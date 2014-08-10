#pragma once

namespace fplog {

class Transport_Interface
{
    public:

        static const size_t infinite_wait = static_cast<size_t>(-1);

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
};

};
