#pragma once

#include <vector>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/thread/thread.hpp>

#include "..\sprot\sprot.h"

namespace spipc {

class Shared_Memory_Transport: public sprot::Transport_Interface
{
    public:

        Shared_Memory_Transport();
        ~Shared_Memory_Transport();

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);


    private:

        typedef boost::interprocess::allocator<unsigned char, boost::interprocess::managed_shared_memory::segment_manager> uchar_alloc;
        typedef boost::interprocess::vector<unsigned char, uchar_alloc> shared_vector;
        
        static const size_t buf_reserve_ = 1024 * 1024;
        static const size_t shared_reserve_ = buf_reserve_ + 1024;
        const char* shared_mem_name_ = "mem_trans_shared_buff";
        
        boost::interprocess::managed_shared_memory shared_mem_;
        uchar_alloc allocator_;
        shared_vector buf_;
        
        boost::interprocess::named_mutex condition_mutex_;
        boost::interprocess::named_condition data_available_;
        boost::interprocess::named_condition buffer_empty_;

        void reset();
};


};