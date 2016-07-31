#include "Mongo_Stub.h"
#include <common/fplog_exceptions.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

#include <mongo/client/dbclient.h>
#include <utils.h>

namespace fpcollect {

    Mongo_Stub::~Mongo_Stub()
    {
    }

    void Mongo_Stub::connect(const Params& params)
    {
    }

    void Mongo_Stub::disconnect()
    {
    }

    size_t Mongo_Stub::write(const void* buf, size_t buf_size, size_t timeout)
    {
        static unsigned long long int msg_counter = 0;
        static unsigned long long int clocks = 0;
        static unsigned long long int ref_clocks = 0;

        msg_counter++;

        if (msg_counter % 100 == 0)
        {
            clocks = generic_util::get_msec_time();
            if (ref_clocks == 0)
                ref_clocks = clocks;
        }

        if (clocks % 10000 < 1000)
            std::cout << (msg_counter / ((clocks - ref_clocks + 1001) / 1000)) << "msg/s" << std::endl;

        return buf_size;
    }

};
