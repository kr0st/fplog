#include "shared_memory_transport.h"
#include "spipc.h"
#include <chrono>

using namespace std::chrono;

namespace spipc {

static const char* g_shared_mem_name = "mem_trans_shared_buff";
static char* g_data_avail_condition_name = "mem_trans_data_avail";
static const char* g_buf_empty_condition_name = "mem_trans_buffer_empty";
static const char* g_condition_mutex_name = "mem_trans_cond_mutex";
static const size_t g_shared_mem_size = 1024;

void Shared_Memory_Transport::global_init()
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

size_t Shared_Memory_IPC_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::vector<unsigned char> tmp;
    tmp.resize(buf_size + sizeof(fplog::UID) + sizeof(size_t) + sizeof(int) + sizeof(long long));

#ifdef _LINUX
    int pid = getpid();
#else
    int pid = _getpid();
#endif

    size_t tid = std::hash<std::thread::id>()(std::this_thread::get_id());

    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long timestamp = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

    memcpy(&(tmp[0]), &private_channel_id_, sizeof(fplog::UID));
    memcpy(&(tmp[sizeof(fplog::UID)]), &pid, sizeof(int));
    memcpy(&(tmp[sizeof(fplog::UID) + sizeof(int)]), &tid, sizeof(size_t));
    memcpy(&(tmp[sizeof(fplog::UID) + sizeof(int) + sizeof(size_t)]), &timestamp, sizeof(long long));
    memcpy(&(tmp[sizeof(fplog::UID) + sizeof(size_t) + sizeof(int) + sizeof(long long)]), buf, buf_size);

    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);

    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));

    while (get_buf_size() != 0)
    {
        long long read_timestamp = 0;
        memcpy(&read_timestamp, buf_ + sizeof(size_t) + sizeof(fplog::UID) + sizeof(int) + sizeof(size_t), sizeof(long long));

        //Purge buffer in case content is older than the default single operation timeout div by 2
        if (abs(timestamp - read_timestamp) > 1 * sprot::Protocol::Timeout::Operation / 2)
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

size_t Shared_Memory_IPC_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);

    boost::system_time to(boost::get_system_time() + boost::posix_time::millisec(timeout));
    fplog::UID empty_UID;
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

    size_t got_buf_size = get_buf_size();

    if (got_buf_size == 0)
        THROW(fplog::exceptions::Incorrect_Parameter);

    if (got_buf_size <= (sizeof(fplog::UID) + sizeof(int) + sizeof(size_t) + sizeof(long long)))
        THROW(sprot::exceptions::Invalid_Frame);

    size_t read_bytes = got_buf_size - (sizeof(fplog::UID) + sizeof(int) + sizeof(size_t) + sizeof(long long));
    if (read_bytes > buf_size)
        read_bytes = buf_size;

    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long current_timestamp = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();
    long long read_timestamp = 0;

    memcpy(&read_timestamp, buf_ + sizeof(size_t) + sizeof(fplog::UID) + sizeof(int) + sizeof(size_t), sizeof(long long));
    
    if (memcmp(buf_ + sizeof(size_t), &private_channel_id_, sizeof(fplog::UID)) == 0)
    {
#ifdef _LINUX
        int this_pid = getpid();
#else
        int this_pid = _getpid();
#endif

        size_t this_tid = std::hash<std::thread::id>()(std::this_thread::get_id());

        int read_pid = 0;
        size_t read_tid = 0;

        memcpy(&read_pid, buf_ + sizeof(size_t) + sizeof(fplog::UID), sizeof(int));
        memcpy(&read_tid, buf_ + sizeof(size_t) + sizeof(fplog::UID) + sizeof(int), sizeof(size_t));

        if ((this_pid == read_pid) && (this_tid == read_tid))
        {
            lock.unlock();
            data_available_.notify_all();
            lock.lock();

            goto start_read;
        }

        memcpy(buf, &(buf_[sizeof(size_t) + sizeof(fplog::UID) + sizeof(int) + sizeof(size_t) + sizeof(long long)]), read_bytes);
        set_buf_size(0);

        buffer_empty_.notify_all();
    }
    else
    {
        //Purge buffer in case content is older than the default single operation timeout div by 2
        if (abs(current_timestamp - read_timestamp) > 1 * sprot::Protocol::Timeout::Operation / 2)
        {
            set_buf_size(0);
            buffer_empty_.notify_all();
            goto start_read;
        }

        lock.unlock();
        data_available_.notify_all();
        lock.lock();
        goto start_read;
    }

    return read_bytes;
}

void Shared_Memory_IPC_Transport::connect(const Params& params)
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    std::string uidstr;

    if (params.find("uid") == params.end())
    {
        if (params.find("UID") == params.end())
        {
            THROW(fplog::exceptions::Invalid_Uid);
        }
        else
            uidstr = (*params.find("UID")).second;
    }
    else
        uidstr = (*params.find("uid")).second;

    fplog::UID uid;
    private_channel_id_ = uid.from_string(uidstr);
}

};
