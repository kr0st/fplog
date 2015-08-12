#pragma once

#include <fplog_exceptions.h>
#include <fplog_transport.h>
#include <mutex>
#include "spipc.h"

#ifdef SPIPC_EXPORT
#define SPIPC_API __declspec(dllexport)
#else
#define SPIPC_API __declspec(dllimport)
#endif

namespace spipc {

class SPIPC_API Shared_Memory_Transport: public fplog::Transport_Interface
{
    public:

        Shared_Memory_Transport();
        virtual ~Shared_Memory_Transport();

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);
        static void global_init();


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

class SPIPC_API Shared_Memory_IPC_Transport: public Shared_Memory_Transport
{
    public:

        virtual ~Shared_Memory_IPC_Transport(){};

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

        void connect(const Params& params);


    private:

       UID private_channel_id_;
};

};
