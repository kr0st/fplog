#pragma once

#include <common/fplog_transport.h>

namespace fpcollect {

    class Mongo_Stub : public fplog::Transport_Interface
    {
    public:

        Mongo_Stub(){}

        void connect(const Params& params);
        void disconnect();

        //Reading is not implemented, this is write-only transport
        size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) { return 0; };
        size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

        virtual ~Mongo_Stub();

    };

};