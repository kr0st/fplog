#pragma once

#include <map>
#include <string>

namespace fplog {

class Transport_Interface
{
    public:

        typedef std::map<std::string, std::string> Params;
        
        static Params empty_params;
        static const size_t infinite_wait = static_cast<size_t>(-1);

        virtual void connect(const Params& params) {}
        virtual void disconnect() {}

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;

        virtual ~Transport_Interface(){};
};

};
