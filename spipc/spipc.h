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

struct UUID
{
    UUID(): high(0), low(0) {}
    bool operator== (const UUID& rhs) const { return ((high == rhs.high) && (low == rhs.low)); }

    unsigned long long high;
    unsigned long long low;
};

class Shared_Memory_Transport: public sprot::Transport_Interface
{
    public:

        Shared_Memory_Transport();
        virtual ~Shared_Memory_Transport();

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);


    protected:

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

class IPC: public sprot::Transport_Interface
{
    public:

        IPC();
        virtual ~IPC();
        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);
        void connect(const UUID& uuid);


    private:

        UUID registered_id_;
        std::recursive_mutex mutex_;
        class Shared_Memory_IPC_Transport;
        Shared_Memory_IPC_Transport* transport_;
        sprot::Protocol* protocol_;
};

namespace exceptions {

class No_Receiver: public sprot::exceptions::Exception
{
    public:

        No_Receiver(const char* facility, const char* file = "", int line = 0, const char* message = "No receiver registered to IPC transport."):
        Exception(facility, file, line, message)
        {
        }
};

}};