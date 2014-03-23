#include "targetver.h"
#include "spipc.h"

namespace spipc {

Shared_Memory_Transport::~Shared_Memory_Transport()
{
}

Shared_Memory_Transport::Shared_Memory_Transport():
shared_mem_(boost::interprocess::open_or_create, shared_mem_name_, shared_reserve_),
data_available_(boost::interprocess::open_or_create, "mem_trans_data_avail"),
buffer_empty_(boost::interprocess::open_or_create, "mem_trans_buffer_empty"),
condition_mutex_(boost::interprocess::open_or_create, "mem_trans_cond_mutex"),
allocator_(shared_mem_.get_segment_manager()),
buf_(allocator_)
{
    reset();
}

void Shared_Memory_Transport::reset()
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);

    shared_vector empty(allocator_);
    buf_.swap(empty);
    buf_.reserve(buf_reserve_);
}

size_t Shared_Memory_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    while (buf_.size() != 0)
        if (!buffer_empty_.timed_wait(lock, to))
        {
            data_available_.notify_all();
            return 0;
        }

    buf_.insert(buf_.end(), (unsigned char*)buf, (unsigned char*)buf + buf_size);
    data_available_.notify_all();

    return buf_size;
}

size_t Shared_Memory_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    while (buf_.size() == 0)
        if (!data_available_.timed_wait(lock, to))
        {
            buffer_empty_.notify_all();
            return 0;
        }

    size_t read_bytes = buf_size > buf_.size() ? buf_.size() : buf_size;
    memcpy(buf, &(buf_[0]), read_bytes);
    buf_.resize(0);
    buffer_empty_.notify_all();

    return read_bytes;
}

class IPC::Shared_Memory_IPC_Transport: public Shared_Memory_Transport
{
    public:

        Shared_Memory_IPC_Transport(){ Shared_Memory_Transport(); }
        virtual ~Shared_Memory_IPC_Transport(){};

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        void register_receiver(const UUID& uuid);

    private:

       UUID receiver_id_; 
};

size_t IPC::Shared_Memory_IPC_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    UUID empty_uuid;
    if (receiver_id_ == empty_uuid)
        THROW(exceptions::No_Receiver);

start_read:

    while (buf_.size() == 0)
        if (!data_available_.timed_wait(lock, to))
        {
            buffer_empty_.notify_all();
            return 0;
        }

    size_t read_bytes = buf_size > buf_.size() ? buf_.size() : buf_size;
    if (read_bytes <= sizeof(UUID))
        THROW(sprot::exceptions::Invalid_Frame);
    
    if (memcmp(&(buf_[0]), &receiver_id_, sizeof(UUID)) == 0)
    {
        read_bytes -= sizeof(UUID);
        memcpy(buf, &(buf_[sizeof(UUID)]), read_bytes);
        buf_.resize(0);
        buffer_empty_.notify_all();
    }
    else
    {
        lock.unlock();
        data_available_.notify_all();
        lock.lock();
        goto start_read;
    }

    return read_bytes;
}

void IPC::Shared_Memory_IPC_Transport::register_receiver(const UUID& uuid)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    receiver_id_ = uuid;
}

IPC::IPC()
{
    transport_ = new IPC::Shared_Memory_IPC_Transport();
    protocol_ = new sprot::Protocol(transport_);
}

IPC::~IPC()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    delete protocol_;
    delete transport_;
}

size_t IPC::read(void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return protocol_->read(buf, buf_size, timeout);
}

size_t IPC::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!buf)
        THROW(sprot::exceptions::Incorrect_Parameter);
    if (buf_size == 0)
        return 0;

    const unsigned char* converted_buf = (unsigned char*)buf;
    std::vector<unsigned char> tmp_buf(converted_buf, converted_buf + buf_size);

    const unsigned char* converted_uuid = (unsigned char*)&registered_id_;
    std::vector<unsigned char> uuid(converted_uuid, converted_uuid + sizeof(UUID));

    tmp_buf.insert(tmp_buf.begin(), uuid.begin(), uuid.end());

    return protocol_->write(&(tmp_buf[0]), tmp_buf.size(), timeout);
}

void IPC::connect(const UUID& uuid)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    registered_id_ = uuid;
    
    UUID empty;
    if (uuid == empty)
        THROW(sprot::exceptions::Incorrect_Parameter);
    
    transport_->register_receiver(registered_id_);
}

};
