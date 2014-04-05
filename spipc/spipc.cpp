#include "targetver.h"
#include "spipc.h"
#include <process.h>

namespace spipc {

const char* g_shared_mem_name = "mem_trans_shared_buff";
const char* g_data_avail_condition_name = "mem_trans_data_avail";
const char* g_buf_empty_condition_name = "mem_trans_buffer_empty";
const char* g_condition_mutex_name = "mem_trans_cond_mutex";
extern const size_t g_shared_mem_size = 1024 * 1024;

void global_init()
{
    boost::interprocess::named_condition::remove(g_data_avail_condition_name);
    boost::interprocess::named_condition::remove(g_buf_empty_condition_name);
    boost::interprocess::named_mutex::remove(g_condition_mutex_name);
    boost::interprocess::shared_memory_object::remove(g_shared_mem_name);

    boost::interprocess::shared_memory_object shared_mem(boost::interprocess::open_or_create, g_shared_mem_name, boost::interprocess::read_write);
    shared_mem.truncate(g_shared_mem_size);
    boost::interprocess::mapped_region mapped_mem_region(shared_mem, boost::interprocess::read_write);
    memset(static_cast<unsigned char*>(mapped_mem_region.get_address()), 0, g_shared_mem_size);
}

Shared_Memory_Transport::~Shared_Memory_Transport()
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    delete mapped_mem_region_;

    buf_ = 0;
    mapped_mem_region_ = 0;
}

Shared_Memory_Transport::Shared_Memory_Transport():
shared_mem_(boost::interprocess::open_or_create, g_shared_mem_name, boost::interprocess::read_write),
data_available_(boost::interprocess::open_or_create, g_data_avail_condition_name),
buffer_empty_(boost::interprocess::open_or_create, g_buf_empty_condition_name),
condition_mutex_(boost::interprocess::open_or_create, g_condition_mutex_name),
mapped_mem_region_(0),
buf_(0)
{
    reset();
}

size_t Shared_Memory_Transport::get_buf_size()
{
    size_t sz = 0;
    memcpy(&sz, buf_, sizeof(size_t));
    return sz;
}

void Shared_Memory_Transport::reset()
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    shared_mem_.truncate(g_shared_mem_size);
    
    delete mapped_mem_region_;
    mapped_mem_region_ = new boost::interprocess::mapped_region(shared_mem_, boost::interprocess::read_write);
    buf_ = static_cast<unsigned char*>(mapped_mem_region_->get_address());
}

size_t Shared_Memory_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    while (get_buf_size() != 0)
        if (!buffer_empty_.timed_wait(lock, to))
        {
            data_available_.notify_all();
            return 0;
        }

    unsigned char* shared = buf_;
    memcpy(shared, &buf_size, sizeof(size_t));
    shared += sizeof(size_t);

    memcpy(shared, buf, buf_size);
    data_available_.notify_all();

    return buf_size;
}

void Shared_Memory_Transport::set_buf_size(size_t size)
{
    memcpy(buf_, &size, sizeof(size_t));
}

size_t Shared_Memory_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    while (get_buf_size() == 0)
        if (!data_available_.timed_wait(lock, to))
        {
            buffer_empty_.notify_all();
            return 0;
        }

    size_t read_bytes = buf_size > get_buf_size() ? get_buf_size() : buf_size;
    memcpy(buf, buf_ + sizeof(size_t), read_bytes);
    set_buf_size(0);
    buffer_empty_.notify_all();

    return read_bytes;
}

class IPC::Shared_Memory_IPC_Transport: public Shared_Memory_Transport
{
    public:

        virtual ~Shared_Memory_IPC_Transport(){};

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

        void register_private_channel(const UUID& chan_id);

    private:

       UUID private_channel_id_;
};

size_t IPC::Shared_Memory_IPC_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::vector<unsigned char> tmp;
    tmp.resize(buf_size + sizeof(UUID) + sizeof(size_t) + sizeof(int));

    int pid = _getpid();
    size_t tid = std::this_thread::get_id().hash();

    memcpy(&(tmp[0]), &private_channel_id_, sizeof(UUID));
    memcpy(&(tmp[sizeof(UUID)]), &pid, sizeof(int));
    memcpy(&(tmp[sizeof(UUID) + sizeof(int)]), &tid, sizeof(size_t));
    memcpy(&(tmp[sizeof(UUID) + sizeof(size_t) + sizeof(int)]), buf, buf_size);

    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    while (get_buf_size() != 0)
        if (!buffer_empty_.timed_wait(lock, to))
        {
            data_available_.notify_all();
            return 0;
        }

    unsigned char* shared = buf_;
    size_t tmp_sz = tmp.size();
    memcpy(shared, &tmp_sz, sizeof(size_t));
    shared += sizeof(size_t);

    memcpy(shared, &(tmp[0]), tmp_sz);
    data_available_.notify_all();

    return buf_size;
}

size_t IPC::Shared_Memory_IPC_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);

    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    UUID empty_uuid;
    if (private_channel_id_ == empty_uuid)
        THROW(exceptions::No_Receiver);

start_read:

    while (get_buf_size() == 0)
        if (!data_available_.timed_wait(lock, to))
        {
            buffer_empty_.notify_all();
            return 0;
        }

    size_t read_bytes = buf_size > get_buf_size() ? get_buf_size() : buf_size;
    if (read_bytes <= (sizeof(UUID) + sizeof(int) + sizeof(size_t)))
        THROW(sprot::exceptions::Invalid_Frame);
    
    if (memcmp(buf_ + sizeof(size_t), &private_channel_id_, sizeof(UUID)) == 0)
    {
        int this_pid = _getpid();
        size_t this_tid = std::this_thread::get_id().hash();

        int read_pid = 0;
        size_t read_tid = 0;

        memcpy(&read_pid, buf_ + sizeof(size_t) + sizeof(UUID), sizeof(int));
        memcpy(&read_tid, buf_ + sizeof(size_t) + sizeof(UUID) + sizeof(int), sizeof(size_t));

        if ((this_pid == read_pid) && (this_tid == read_tid))
        {
            lock.unlock();
            data_available_.notify_all();
            lock.lock();

            goto start_read;
        }

        read_bytes -= (sizeof(UUID) + sizeof(int) + sizeof(size_t));
        memcpy(buf, &(buf_[sizeof(size_t) + sizeof(UUID) + sizeof(int) + sizeof(size_t)]), read_bytes);
        set_buf_size(0);
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

void IPC::Shared_Memory_IPC_Transport::register_private_channel(const UUID& chan_id)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    private_channel_id_ = chan_id;
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

    return protocol_->write(buf, buf_size, timeout);
}

void IPC::connect(const UUID& private_channel)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    private_channel_id_ = private_channel;
    
    transport_->register_private_channel(private_channel_id_);

    UUID empty;
    if (private_channel == empty)
        THROW(sprot::exceptions::Incorrect_Parameter);
}

};
