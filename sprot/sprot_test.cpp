#include "targetver.h"

#include <windows.h>
#include <tchar.h>

#include "sprot.h"

#include <mutex>
#include <thread>

namespace sprot { namespace testing
{

    unsigned char* random_data_small = 0;
    unsigned char* random_data_big = 0;

    const size_t default_mtu = 1024;
    const size_t small_data_size = 213; //Less than default MTU
    const size_t big_data_size = (1024 - 3) * 3 + 123; //3 MTUs + 123 bytes = seq begin frame, 4 data frames, seq end frame

    //Emulating sending given frames and receiving given replies:
    //seqbegin - ack
    //dataframe #1 - ack
    //dataframe #2 - nack, nack, ack
    //dataframe #3 - nack, ack
    //dataframe #4 - ack
    //seqend - nack, nack, nack, ack
    class Write_Retransmit_Test_Transport: public fplog::Transport_Interface
    {
        public:

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                static unsigned call_count = 0;
                unsigned char frame[2];

                if ((call_count == 0) || (call_count == 1) || (call_count == 4) || (call_count == 6) || (call_count == 7) || (call_count == 11))
                {
                    frame[0] = 0x0a;
                    frame[1] = 0x5f;
                }
                else
                {
                    frame[0] = 0x0b;
                    frame[1] = 0x1e;
                }
                
                memcpy(buf, frame, sizeof(frame));
                call_count++;
                return sizeof(frame);
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                static unsigned counter = 0;

                if (counter == 0) //Seq begin
                {
                    counter++;
                    if ((buf_size == 2) && (((unsigned char*)buf)[0] == 0x0c) && (((unsigned char*)buf)[1] == 0x6a))
                        return buf_size;
                    else
                        THROW(fplog::exceptions::Write_Failed);
                }

                static size_t composite_frame_size = 0;

                if (counter == 1)
                {
                    counter++;
                    if ((buf_size == default_mtu) && (((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == 1) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_big + 0 * (default_mtu - 3), buf_size - 3) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    composite_frame_size += (buf_size - 3);
                    return buf_size;
                }

                if ((counter > 1) && (counter < 5))
                {
                    counter++;
                    if ((buf_size == default_mtu) && (((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == 2) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_big + 1 * (default_mtu - 3), buf_size - 3) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    if ((counter == 5))
                        composite_frame_size += (buf_size - 3);
                    
                    return buf_size;
                }

                if ((counter > 4) && (counter < 7))
                {
                    counter++;
                    if ((buf_size == default_mtu) && (((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == 3) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_big + 2 * (default_mtu - 3), buf_size - 3) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    if ((counter == 7))
                        composite_frame_size += (buf_size - 3);
                    
                    return buf_size;
                }

                if (counter == 7)
                {
                    counter++;
                    if ((((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == 4) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_big + 3 * (default_mtu - 3), buf_size - 3) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    composite_frame_size += (buf_size - 3);
                    return buf_size;
                }

                if ((counter == 8) && (composite_frame_size != big_data_size))
                    THROW(fplog::exceptions::Write_Failed);

                if ((counter > 7) && (counter < 12))
                {
                    counter++;
                    if ((buf_size == 2) && (((unsigned char*)buf)[0] == 0x0d) && (((unsigned char*)buf)[1] == 0x2b))
                        return buf_size;
                    else
                        THROW(fplog::exceptions::Write_Failed);
                }

                return buf_size;
            }
    };

    class Write_Test_Transport: public fplog::Transport_Interface
    {
        public:

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                unsigned char frame[2];
                frame[0] = 0x0a;
                frame[1] = 0x5f;
                memcpy(buf, frame, sizeof(frame));
                return sizeof(frame);
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                static unsigned counter = 0;

                if (counter == 0) //Received data frame of size < MTU-3
                {
                    counter++;
                    if ((buf_size == (small_data_size + 3)) && (((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == 1) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_small, small_data_size) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    return buf_size;
                }

                if (counter == 1) //Seq begin
                {
                    counter++;
                    if ((buf_size == 2) && (((unsigned char*)buf)[0] == 0x0c) && (((unsigned char*)buf)[1] == 0x6a))
                        return buf_size;
                    else
                        THROW(fplog::exceptions::Write_Failed);
                }

                static size_t composite_frame_size = 0;

                if ((counter > 1) && (counter < 6)) //Bigger data frame parts 1, 2, 3 and 4
                {
                    counter++;
                    if (((buf_size == default_mtu) || (counter == 6)) && (((unsigned char*)buf)[0] == 0x10) && (((unsigned char*)buf)[1] == (counter - 2)) && Protocol::crc_check((unsigned char*)buf, buf_size))
                    {
                        if (memcmp((unsigned char*)buf + 2, random_data_big + (counter - 3) * (default_mtu - 3), buf_size - 3) != 0)
                            THROW(fplog::exceptions::Write_Failed);
                    }
                    else
                        THROW(fplog::exceptions::Write_Failed);

                    composite_frame_size += (buf_size - 3);
                    return buf_size;
                }

                if ((counter == 6) && (composite_frame_size != big_data_size))
                    THROW(fplog::exceptions::Write_Failed);

                if (counter == 6)
                {
                    counter++;
                    if ((buf_size == 2) && (((unsigned char*)buf)[0] == 0x0d) && (((unsigned char*)buf)[1] == 0x2b))
                        return buf_size;
                    else
                        THROW(fplog::exceptions::Write_Failed);
                }

                return buf_size;
            }
    };


    //Test description
    //send: seqbegin
    //recv: ack
    //send: seqbegin
    //recv: ack
    //send: frame #1
    //recv: ack
    //send: frame #1
    //recv: ack
    //send: frame #1 with incorrect crc
    //recv: nack
    //send: frame #2
    //recv: ack
    //send: frame #1 with incorrect seq number
    //recv: nack
    //send: seqend with wrong crc
    //recv: nack
    //send: seqend
    //recv: ack
    //send: frame #3 with incorrect seq number
    //recv: nack
    //send: frame #3 with incorrect crc
    //recv: nack
    //send: frame #3
    //recv: ack
    class Read_Test_Transport: public fplog::Transport_Interface
    {
        public:

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                static unsigned char seq = 0;

                unsigned char data[6] = {0};

                //First sending sequence start frame
                if (seq == 0)
                {
                    seq++;

                    data[0] = 0x0c;
                    data[1] = 0x6a;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //...and do it again to see if it breaks something
                if (seq == 1)
                {
                    seq++;

                    data[0] = 0x0c;
                    data[1] = 0x6a;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //Then sending the first data frame.
                if (seq == 2)
                {
                    seq++;

                    data[0] = 0x10;

                    //Take note that frame sequence numbers start with 1.
                    data[1] = 1;

                    data[2] = 0xde;
                    data[3] = 0xad;
                    data[4] = util::crc7(data, 4);

                    memcpy(buf, data, sizeof(data));
                    return 5;
                }

                //Repeating the first data frame.
                if (seq == 3)
                {
                    seq++;

                    data[0] = 0x10;

                    //Take note that frame sequence numbers start with 1.
                    data[1] = 1;

                    data[2] = 0xde;
                    data[3] = 0xad;
                    data[4] = util::crc7(data, 4);

                    memcpy(buf, data, sizeof(data));
                    return 5;
                }

                //Sending the first data frame once more now with broken crc.
                if (seq == 4)
                {
                    seq++;

                    data[0] = 0x10;

                    //Take note that frame sequence numbers start with 1.
                    data[1] = 1;

                    data[2] = 0xde;
                    data[3] = 0xad;
                    data[4] = 0x88;

                    memcpy(buf, data, sizeof(data));
                    return 5;
                }

                //Second data frame
                if (seq == 5)
                {
                    seq++;

                    data[0] = 0x10;
                    data[1] = 2;

                    data[2] = 0xbe;
                    data[3] = 0xee;
                    data[4] = 0xef;
                    data[5] = util::crc7(data, 5);

                    memcpy(buf, data, sizeof(data));
                    return 6;
                }

                //First data frame returns with incorrect sequence number.
                if (seq == 6)
                {
                    seq++;

                    data[0] = 0x10;

                    //Take note that frame sequence numbers start with 1.
                    data[1] = 1;

                    data[2] = 0xde;
                    data[3] = 0xad;
                    data[4] = util::crc7(data, 4);

                    memcpy(buf, data, sizeof(data));
                    return 5;
                }

                //Sending sequence end with wrong crc
                if (seq == 7)
                {
                    seq++;

                    data[0] = 0x0d;
                    data[1] = 0x3b;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //Finally normal sequence end
                if (seq == 8)
                {
                    seq++;

                    data[0] = 0x0d;
                    data[1] = 0x2b;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //Sending a standalone data frame with incorrect sequence number
                if (seq == 9)
                {
                    seq++;

                    data[0] = 0x10;

                    data[1] = 2;

                    data[2] = 0x45;
                    data[3] = 0xe9;
                    data[4] = 0xa6;
                    data[5] = util::crc7(data, 5);

                    memcpy(buf, data, sizeof(data));
                    return 6;
                }

                //Sending a standalone data frame with incorrect crc
                if (seq == 10)
                {
                    seq++;

                    data[0] = 0x10;
                    data[1] = 1;

                    data[2] = 0x45;
                    data[3] = 0xe9;
                    data[4] = 0xa6;
                    data[5] = 0x56;

                    memcpy(buf, data, sizeof(data));
                    return 6;
                }

                //Sending a correct standalone data frame
                if (seq == 11)
                {
                    seq++;

                    data[0] = 0x10;

                    //Each read (when it's completed) resets sequence number counter.
                    //Same story with write - basically sequence numbers are needed
                    //only inside one read-write session to make sure there are no
                    //double reads of the same data frame.
                    data[1] = 1;

                    data[2] = 0x45;
                    data[3] = 0xe9;
                    data[4] = 0xa6;
                    data[5] = util::crc7(data, 5);

                    memcpy(buf, data, sizeof(data));
                    return 6;
                }

                //Sending switch to client mode command
                if (seq == 12)
                {
                    seq++;

                    data[0] = 0x0e;
                    data[1] = 0x79;

                    memcpy(buf, data, sizeof(data));
                    return 2;
                }

                //Sending switch to server mode command
                if (seq == 13)
                {
                    seq++;

                    data[0] = 0x0f;
                    data[1] = 0x38;

                    memcpy(buf, data, sizeof(data));
                    return 2;
                }

                return 0;
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                if (buf_size != 2)
                    THROW(fplog::exceptions::Read_Failed);

                static unsigned counter = 0;
                
                const unsigned char ack[] = { 0x0a, 0x5f };
                const unsigned char nack[] = { 0x0b, 0x1e };

                if ((counter == 4) || (counter == 6) || (counter == 7) || (counter == 9) || (counter == 10))
                {
                    counter++;
                    if (memcmp(buf, nack, 2) != 0)
                        THROW(fplog::exceptions::Read_Failed);
                    return buf_size;
                }

                counter++;
                if (memcmp(buf, ack, 2) != 0)
                    THROW(fplog::exceptions::Read_Failed);

                return buf_size;
            }
    };

    bool proto_test()
    {
        random_data_small = new unsigned char[small_data_size];
        random_data_big = new unsigned char[big_data_size];

        for (int i = 0; i < small_data_size; ++i)
            random_data_small[i] = (unsigned char) rand();

        for (int i = 0; i < big_data_size; ++i)
            random_data_big[i] = (unsigned char) rand();

        Read_Test_Transport read_test_transport;
        Protocol read_test_protocol(&read_test_transport);
        
        char buf[256] = {0};
        char ethalon[256] = {0xde, 0xad, 0xbe, 0xee, 0xef};
        char ethalon2[256] = {0x45, 0xe9, 0xa6};

        try
        {
            if (read_test_protocol.read(buf, sizeof(buf)) != 5)
            {
                printf("read_test_protocol.read() returned incorrect size.\n");
                return false;
            }

            if (memcmp(buf, ethalon, 5) != 0)
            {
                printf("read_test_protocol.read() got incorrect data.\n");
                return false;
            }

            if (read_test_protocol.read(buf, sizeof(buf)) != 3)
            {
                printf("read_test_protocol.read() returned incorrect size.\n");
                return false;
            }

            if (memcmp(buf, ethalon2, 3) != 0)
            {
                printf("read_test_protocol.read() got incorrect data.\n");
                return false;
            }

            if (!read_test_protocol.wait_send_mode())
            {
                printf("read_test_protocol.wait_send_mode() failed.\n");
                return false;
            }

            if (!read_test_protocol.wait_recv_mode())
            {
                printf("read_test_protocol.wait_recv_mode() failed.\n");
                return false;
            }
        }
        catch (fplog::exceptions::Read_Failed)
        {
            printf("read_test_protocol.read() failed.\n");
            return false;
        }

        Write_Test_Transport write_test_transport;
        Protocol write_test_protocol(&write_test_transport);

        try
        {
            write_test_protocol.write(random_data_small, small_data_size);
            write_test_protocol.write(random_data_big, big_data_size);
        }
        catch(fplog::exceptions::Write_Failed)
        {
            printf("write_test_protocol.write() failed.\n");
            return false;
        }
        catch(fplog::exceptions::Timeout)
        {
            printf("write_test_protocol.write() failed with timeout.\n");
            return false;
        }

        Write_Retransmit_Test_Transport write_retransmit_test_transport;
        Protocol write_retransmit_test_protocol(&write_retransmit_test_transport);

        try
        {
            write_retransmit_test_protocol.write(random_data_big, big_data_size);
        }
        catch(fplog::exceptions::Write_Failed)
        {
            printf("write_retransmit_test_protocol.write() failed.\n");
            return false;
        }
        catch(fplog::exceptions::Timeout)
        {
            printf("write_retransmit_test_protocol.write() failed with timeout.\n");
            return false;
        }

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
