#include "targetver.h"
#include "spipc.h"

#include <process.h>
#include <chrono>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

namespace spipc {

const char* g_shared_mem_name = "mem_trans_shared_buff";
const char* g_data_avail_condition_name = "mem_trans_data_avail";
const char* g_buf_empty_condition_name = "mem_trans_buffer_empty";
const char* g_condition_mutex_name = "mem_trans_cond_mutex";
extern const size_t g_shared_mem_size = 1024;

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

UID UID::from_string(std::string& str)
{
    unsigned long long limit = std::numeric_limits<unsigned long long>::max();
    high = limit;
    low = limit;

    try
    {
        boost::char_separator<char> sep("_");
        boost::tokenizer<boost::char_separator<char>> tok(str, sep);
        for(boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin(); it != tok.end(); ++it)
        {
            if (high == limit)
            {
                high = boost::lexical_cast<unsigned long long>(*it);
                continue;
            }

            if (low == limit)
            {
                low = boost::lexical_cast<unsigned long long>(*it);
                continue;
            }

            THROW(exceptions::Invalid_Uid);
        }
    }
    catch(boost::exception&)
    {
        THROW(exceptions::Invalid_Uid);
    }

    return *this;
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

        void register_private_channel(const UID& chan_id);

    private:

       UID private_channel_id_;
};

size_t IPC::Shared_Memory_IPC_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::vector<unsigned char> tmp;
    tmp.resize(buf_size + sizeof(UID) + sizeof(size_t) + sizeof(int) + sizeof(long long));

    int pid = _getpid();
    size_t tid = std::this_thread::get_id().hash();

    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long timestamp = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

    memcpy(&(tmp[0]), &private_channel_id_, sizeof(UID));
    memcpy(&(tmp[sizeof(UID)]), &pid, sizeof(int));
    memcpy(&(tmp[sizeof(UID) + sizeof(int)]), &tid, sizeof(size_t));
    memcpy(&(tmp[sizeof(UID) + sizeof(int) + sizeof(size_t)]), &timestamp, sizeof(long long));
    memcpy(&(tmp[sizeof(UID) + sizeof(size_t) + sizeof(int) + sizeof(long long)]), buf, buf_size);

    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));

    while (get_buf_size() != 0)
    {
        long long read_timestamp = 0;
        memcpy(&read_timestamp, buf_ + sizeof(size_t) + sizeof(UID) + sizeof(int) + sizeof(size_t), sizeof(long long));

        //Purge buffer in case content is older than the default single operation timeout
        if (abs(timestamp - read_timestamp) > sprot::Protocol::Timeout::Operation)
        {
            set_buf_size(0);
            break;
        }

        if (!buffer_empty_.timed_wait(lock, to))
        {
            data_available_.notify_all();
            return 0;
        }
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
    UID empty_UID;
    if (private_channel_id_ == empty_UID)
        THROW(exceptions::No_Receiver);

start_read:

    if (boost::get_system_time() >= to)
        return 0;

    while (get_buf_size() == 0)
        if (!data_available_.timed_wait(lock, to))
        {
            buffer_empty_.notify_all();
            return 0;
        }

    size_t read_bytes = buf_size > get_buf_size() ? get_buf_size() : buf_size;
    if (read_bytes <= (sizeof(UID) + sizeof(int) + sizeof(size_t) + sizeof(long long)))
        THROW(sprot::exceptions::Invalid_Frame);

    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long current_timestamp = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();
    long long read_timestamp = 0;

    memcpy(&read_timestamp, buf_ + sizeof(size_t) + sizeof(UID) + sizeof(int) + sizeof(size_t), sizeof(long long));
    
    if (memcmp(buf_ + sizeof(size_t), &private_channel_id_, sizeof(UID)) == 0)
    {
        int this_pid = _getpid();
        size_t this_tid = std::this_thread::get_id().hash();

        int read_pid = 0;
        size_t read_tid = 0;

        memcpy(&read_pid, buf_ + sizeof(size_t) + sizeof(UID), sizeof(int));
        memcpy(&read_tid, buf_ + sizeof(size_t) + sizeof(UID) + sizeof(int), sizeof(size_t));

        if ((this_pid == read_pid) && (this_tid == read_tid))
        {
            lock.unlock();
            data_available_.notify_all();
            lock.lock();

            goto start_read;
        }

        read_bytes -= (sizeof(UID) + sizeof(int) + sizeof(size_t) + sizeof(long long));
        memcpy(buf, &(buf_[sizeof(size_t) + sizeof(UID) + sizeof(int) + sizeof(size_t) + sizeof(long long)]), read_bytes);
        set_buf_size(0);
        buffer_empty_.notify_all();
    }
    else
    {
        //Purge buffer in case content is older than the default single operation timeout
        if (abs(current_timestamp - read_timestamp) > sprot::Protocol::Timeout::Operation)
        {
            set_buf_size(0);
            lock.unlock();
            buffer_empty_.notify_all();
            lock.lock();
            goto start_read;
        }

        lock.unlock();
        data_available_.notify_all();
        lock.lock();
        goto start_read;
    }

    return read_bytes;
}

void IPC::Shared_Memory_IPC_Transport::register_private_channel(const UID& chan_id)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    private_channel_id_ = chan_id;
}

IPC::IPC()
{
    transport_ = new IPC::Shared_Memory_IPC_Transport();
    protocol_ = new sprot::Protocol(transport_, sprot::Protocol::Switching::Auto, g_shared_mem_size / 2, g_shared_mem_size / 3);
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

void IPC::connect(const UID& private_channel)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    private_channel_id_ = private_channel;
    
    transport_->register_private_channel(private_channel_id_);

    UID empty;
    if (private_channel == empty)
        THROW(sprot::exceptions::Incorrect_Parameter);
}

};
