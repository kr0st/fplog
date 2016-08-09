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

#ifdef _LINUX
#define SPIPC_API 
#endif

namespace spipc {

class SPIPC_API IPC: public fplog::Transport_Interface
{
    public:

        IPC(fplog::Transport_Interface* transport = 0, size_t MTU = 1221);
        virtual ~IPC();
        virtual size_t read(void* buf, size_t buf_size, size_t timeout = fplog::Transport_Interface::infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = fplog::Transport_Interface::infinite_wait);
        void connect(const fplog::UID& private_channel);
        void connect(const Params& params);


    private:

        fplog::UID private_channel_id_;
        std::recursive_mutex mutex_;
        fplog::Transport_Interface* transport_;
        fplog::Transport_Interface* protocol_;
        bool own_transport_;
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

}};