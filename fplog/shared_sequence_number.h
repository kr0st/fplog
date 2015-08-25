#pragma once

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <fplog_exceptions.h>
#include <mutex>

namespace fplog {

class Shared_Sequence_Number
{
    public:

        Shared_Sequence_Number();
        virtual ~Shared_Sequence_Number();

        unsigned long long read();

    private:
        
        boost::interprocess::shared_memory_object* shared_mem_;
        boost::interprocess::mapped_region* mapped_mem_region_;
        unsigned char* buf_;

        boost::interprocess::named_mutex condition_mutex_;
};

};
