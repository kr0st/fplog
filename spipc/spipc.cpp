
#include "targetver.h"
#include "spipc.h"

#include <stdio.h>
#include <tchar.h>


int _tmain(int argc, _TCHAR* argv[])
{
	return 0;
}

Memory_Transport::Memory_Transport()
{
    reset();
}

void Memory_Transport::reset()
{
    std::vector<unsigned char> empty;
    buf_.swap(empty);
    buf_.reserve(buf_reserve_);
}

size_t Memory_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    buf_.resize(0);
    std::lock_guard<std::recursive_mutex> lock(write_protector_);
    buf_.insert(buf_.end(), (unsigned char*)buf, (unsigned char*)buf + buf_size);
    data_available_.notify_all();
    return buf_size;
}

size_t Memory_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(write_protector_);
    static std::mutex mutex;
    std::unique_lock<std::mutex> data_avail_lock(mutex);
    data_available_.wait(data_avail_lock);
    size_t read_bytes = buf_size > buf_.size() ? buf_.size() : buf_size;
    memcpy(buf, &(buf_[0]), read_bytes);
    return read_bytes;
}
