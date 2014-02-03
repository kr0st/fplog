// sprot.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "sprot.h"


namespace sprot
{
    unsigned char util::crc7(const unsigned char* buf, size_t length)
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

        for (size_t i = 0; i < length; ++i)
            crc = crc_table[crc ^ buf[i]];

        return crc;
    }

    bool Protocol::crc_check(const unsigned char* buf, size_t length)
    {
        if (length < 1)
            return false;

        return (util::crc7(buf, length - 1) == buf[length - 1]);
    }

    void Protocol::send_control_frame(Frame::Type frame)
    {
        unsigned char buf[2];
        
        buf[0] = frame;
        buf[1] = util::crc7(buf, 1);

        if (transport_->write(buf, sizeof(buf)) != sizeof(buf))
            THROW(exceptions::Write_Failed);
    }

    Protocol::Protocol(Transport_Interface* transport, Switching::Type switching):
    transport_(transport),
    switching_(switching),
    current_mode_(Mode::Undefined),
    is_sequence_(false)
    {
        if (!transport_)
            THROW(exceptions::Incorrect_Parameter);
    }

    size_t Protocol::read(void* buf, size_t buf_size, size_t timeout)
    {
        if (!buf)
            THROW(exceptions::Incorrect_Parameter);

        if (timeout != Protocol::infinite_wait)
            THROW(exceptions::Not_Implemented);

        if (current_mode_ == Mode::Undefined)
            current_mode_ = Mode::Server;

        if (current_mode_ == Mode::Client)
        {
            if (switching_ == Switching::Auto)
                ; //TODO: Here we should launch mode switch
            else
                THROW(exceptions::Incorrect_Mode);
        }

        bool looping = true;
        while(looping)
        {
            size_t recv_sz = transport_->read(buf, buf_size);
            if (crc_check(static_cast<unsigned char*>(buf), recv_sz))
                looping = on_frame(static_cast<unsigned char*>(buf), recv_sz);
        };
    }
    
    bool Protocol::on_frame(const unsigned char* buf, size_t length)
    {
        return true;
    }

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        THROW(exceptions::Not_Implemented);
    }


namespace testing
{
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
        printf("tests finished.\n");
    }
}};

int _tmain(int argc, _TCHAR* argv[])
{
    sprot::testing::run_all_tests();

	return 0;
}

