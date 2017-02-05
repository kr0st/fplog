#pragma once

#include <fplog_exceptions.h>
#include <mutex>

namespace fplog {

class Shared_Sequence_Number
{
    public:

        Shared_Sequence_Number();
        virtual ~Shared_Sequence_Number();

        unsigned long long int read();

    private:
    
        class Impl;
        Impl *impl_;
    };

};
