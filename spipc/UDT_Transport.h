#pragma once

#include <fplog_transport.h>
#include "spipc.h"

#ifdef SPIPC_EXPORT
#define SPIPC_API __declspec(dllexport)
#else
#define SPIPC_API __declspec(dllimport)
#endif

#ifdef _LINUX
#define SPIPC_API 
#endif

namespace spipc
{
    class SPIPC_API UDT_Transport: public fplog::Transport_Interface
    {
        public:

            virtual void connect(const Params& params);
            virtual void disconnect();

            virtual size_t read(void* buf, size_t buf_size, size_t timeout);
            virtual size_t write(const void* buf, size_t buf_size, size_t timeout);

            UDT_Transport();
            ~UDT_Transport();


        private:

            class UDT_Transport_Impl;
            UDT_Transport_Impl* impl_;
    };
};
