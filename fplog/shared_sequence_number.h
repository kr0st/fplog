#pragma once

#include <fplog_exceptions.h>
#include <mutex>

#ifdef SEQUENCE_EXPORT

#ifdef _LINUX
#define SEQUENCE_API __attribute__ ((visibility("default")))
#else
#define SEQUENCE_API __declspec(dllexport)
#endif

#else

//#ifdef _LINUX
#define SEQUENCE_API
//#else
//#define SEQUENCE_API __declspec(dllimport)
//#endif

#endif

namespace fplog {

class SEQUENCE_API Shared_Sequence_Number
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
