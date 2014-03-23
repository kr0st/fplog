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

};