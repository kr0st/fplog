#pragma once

#include <vector>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/thread/thread.hpp>

#include <sprot/sprot.h>

#ifdef SPIPC_EXPORT
#define SPIPC_API __declspec(dllexport)
#else
#define SPIPC_API __declspec(dllimport)
#endif

namespace spipc {

extern const char* g_shared_mem_name;
extern const char* g_data_avail_condition_name;
extern const char* g_buf_empty_condition_name;
extern const char* g_condition_mutex_name;
extern const size_t g_shared_mem_size;

SPIPC_API void global_init();

struct SPIPC_API UID
{
    UID(): high(0), low(0) {}
    bool operator== (const UID& rhs) const { return ((high == rhs.high) && (low == rhs.low)); }
    bool operator< (const UID& rhs) const
    {
        if (*this == rhs)
            return false;

        if (high < rhs.high)
            return true;

        if (high == rhs.high)
            return (low < rhs.low);

        return false;
    }

    std::string to_string(UID& uid)
    {
        return (std::to_string(uid.high) + "_" + std::to_string(uid.low));
    }

    UID from_string(std::string& str);

    unsigned long long high;
    unsigned long long low;
};

class SPIPC_API Shared_Memory_Transport: public fplog::Transport_Interface
{
    public:

        Shared_Memory_Transport();
        virtual ~Shared_Memory_Transport();

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);


    protected:
        
        boost::interprocess::shared_memory_object shared_mem_;
        boost::interprocess::mapped_region* mapped_mem_region_;
        unsigned char* buf_;

        boost::interprocess::named_mutex condition_mutex_;
        boost::interprocess::named_condition data_available_;
        boost::interprocess::named_condition buffer_empty_;

        void reset();
        size_t get_buf_size();
        void set_buf_size(size_t size);
};

class Socket_Transport;
class SPIPC_API IPC: public fplog::Transport_Interface
{
    public:

        IPC();
        virtual ~IPC();
        virtual size_t read(void* buf, size_t buf_size, size_t timeout = fplog::Transport_Interface::infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = fplog::Transport_Interface::infinite_wait);
        void connect(const UID& private_channel);
        void connect(const Params& params);


    private:

        UID private_channel_id_;
        std::recursive_mutex mutex_;
        class Shared_Memory_IPC_Transport;
        Socket_Transport* transport_;
        sprot::Protocol* protocol_;
};

namespace exceptions {

class SPIPC_API No_Receiver: public sprot::exceptions::Exception
{
    public:

        No_Receiver(const char* facility, const char* file = "", int line = 0, const char* message = "No receiver registered to IPC transport."):
        Exception(facility, file, line, message)
        {
        }
};

class SPIPC_API Invalid_Uid: public sprot::exceptions::Exception
{
    public:

        Invalid_Uid(const char* facility, const char* file = "", int line = 0, const char* message = "Supplied UID is invalid."):
        Exception(facility, file, line, message)
        {
        }
};

}};