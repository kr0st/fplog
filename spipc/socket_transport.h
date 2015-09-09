#pragma once

#include <fplog_exceptions.h>
#include <fplog_transport.h>
#include <winsock2.h>
#include <mutex>
#include "spipc.h"

#ifdef SPIPC_EXPORT
#define SPIPC_API __declspec(dllexport)
#else
#define SPIPC_API __declspec(dllimport)
#endif

namespace spipc {

class SPIPC_API Socket_Transport: public fplog::Transport_Interface
{
    public:

        virtual void connect(const Params& params);
        virtual void disconnect();

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

        Socket_Transport();
        ~Socket_Transport();


    private:

        SOCKET socket_;
        bool connected_;
        std::recursive_mutex mutex_;
        UID uid_;
        bool high_uid_;
        unsigned char ip_[4];
        bool localhost_;
};

};