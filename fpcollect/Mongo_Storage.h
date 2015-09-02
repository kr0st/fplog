#pragma once

#include <common/fplog_transport.h>

namespace mongo {
class DBClientConnection;
};

namespace fpcollect {

class Mongo_Storage: public fplog::Transport_Interface
{
    public:

        Mongo_Storage(): connection_(0) {}

        void connect(const Params& params);
        void disconnect();

        //Reading is not implemented, this is write-only transport
        size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) { return 0; };
        size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

        virtual ~Mongo_Storage(){ disconnect(); delete connection_; };


    private:

        mongo::DBClientConnection* connection_;
        std::string collection_;
};

};
