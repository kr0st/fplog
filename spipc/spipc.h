#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>

#include "..\sprot\sprot.h"

class Memory_Transport: public sprot::Transport_Interface
{
    public:

        Memory_Transport();
        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

    private:

        std::vector <unsigned char> buf_;
        static const size_t buf_reserve_ = 1024 * 1024;
        std::recursive_mutex write_protector_;
        std::condition_variable data_available_;

        void reset();
};
