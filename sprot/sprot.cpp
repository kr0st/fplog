// sprot.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "sprot.h"

namespace sprot
{

    bool Protocol::crc_check(const char* buf, size_t length)
    {
        //Reworked sample code from http://www.pololu.com/docs/0J44/6.7.6
        static unsigned char crc_table[256] = { 0 };

        auto get_byte_crc = [](unsigned char byte) -> unsigned char
        {
            const unsigned char crc7_poly = 0x91;

            for (unsigned char j = 0; j < 8; ++j)
            {
                if (byte & 1)
                    byte ^= crc7_poly;
                byte >>= 1;
            }

            return byte;
        };

        auto build_crc_table = [&]() -> bool
        {
            for (int i = 0; i < 256; ++i)
                crc_table[i] = get_byte_crc(i);

            return true;
        };

        static bool one_time_function_call = build_crc_table();
        unsigned char crc = 0;

        if (length > 0)
        {
            size_t size = length - 1;
            for (size_t i = 0; i < size; ++i)
                crc = crc_table[crc ^ static_cast<unsigned char>(buf[i])];

            return (crc == buf[size]);
        }

        return false;
    }

namespace testing
{
    void crc_test()
    {
        char buf[2] = { 0x0c, 106 };
        printf("crc check = %d\n", sprot::Protocol::crc_check(buf, 2));
    }

    void run_all_tests()
    {
        crc_test();
    }
}};

int _tmain(int argc, _TCHAR* argv[])
{
    sprot::testing::run_all_tests();

	return 0;
}

