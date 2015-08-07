#include "targetver.h"

#include <windows.h>
#include <tchar.h>

#include "sprot.h"

#include <mutex>
#include <thread>

namespace sprot { namespace testing
{
    bool proto_test()
    {
        return true;
    }

    bool crc_test()
    {
        {
            unsigned char buf[2] = { 0x0c, 0x6a };
            if (!sprot::Protocol::crc_check(buf, 2))
                return false;
        }

        {
            unsigned char buf[2] = { 0x0a, 0xcc };
            if (sprot::Protocol::crc_check(buf, 2))
                return false;
        }

        {
            unsigned char buf[2] = { 0x0f, 0x38 };
            if (!sprot::Protocol::crc_check(buf, 2))
                return false;
        }

        return true;
    }

    void run_all_tests()
    {
        if (!crc_test())
            printf("crc_test failed.\n");
        
        if (!proto_test())
            printf("proto_test failed.\n");
        
        printf("tests finished.\n");
    }
}};

int _tmain(int argc, _TCHAR* argv[])
{
    sprot::testing::run_all_tests();

	return 0;
}
