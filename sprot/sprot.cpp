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

    Protocol::Protocol(Transport_Interface* transport, Switching::Type switching, size_t recv_buf_reserve):
    transport_(transport),
    switching_(switching),
    current_mode_(Mode::Undefined),
    is_sequence_(false),
    recv_buf_reserve_(recv_buf_reserve),
    sequence_num_(0)
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
                looping = on_frame(static_cast<unsigned char*>(buf), recv_sz, buf_size);
            else
                send_frame(Frame::NACK);
        };

        size_t read_data = buf_.size();
        reset();

        return read_data;
    }
    
    bool Protocol::on_frame(unsigned char* buf, size_t recv_length, size_t max_capacity)
    {
        if (recv_length == 0)
            return true;

        if (!buf)
            THROW(exceptions::Incorrect_Parameter);

        switch (buf[0])
        {
            case Frame::ACK: break;
            case Frame::NACK: break;
            case Frame::SEQBEGIN: return on_seqbegin();
            case Frame::SEQEND: return on_seqend(buf, max_capacity);
            case Frame::SETSEND: return on_setsend();
            case Frame::SETRECV: return on_setrecv();
            case Frame::DATA: return on_data(make_data_frame(buf, recv_length));
        }

        send_frame(Frame::ACK);
        return true;
    }

    bool Protocol::on_seqbegin()
    {
        is_sequence_ = true;
        send_frame(Frame::ACK);
        return true;
    }

    bool Protocol::on_seqend(unsigned char* buf, size_t max_capacity)
    {
        is_sequence_ = false;
        send_frame(Frame::ACK);
        complete_read(buf, max_capacity);
        return false;
    }

    void Protocol::reset()
    {
        current_mode_ = Mode::Undefined;
        is_sequence_ = false;
        sequence_num_ = 0;
        
        std::vector<unsigned char> empty;
        buf_.swap(empty);
        buf_.reserve(recv_buf_reserve_);
    }

    bool Protocol::on_setsend()
    {
        send_frame(Frame::ACK);
        reset();
        current_mode_ = Mode::Client;
        return false;
    }

    bool Protocol::on_data(Data_Frame& frame)
    {
        //Received the same frame twice, just reply ACK & do nothing
        if (sequence_num_ == frame.sequence_num)
        {
            send_frame(Frame::ACK);
            return true;
        }

        if ((unsigned char)(sequence_num_ + 1) != frame.sequence_num)
        {
            send_frame(Frame::NACK);
            return true;
        }

        sequence_num_ = frame.sequence_num;
        send_frame(Frame::ACK);

        if (is_sequence_)
        {
            buf_.insert(buf_.end(), frame.data, frame.data + frame.data_size);
            return true;
        }
        else
        {
            complete_read(frame.data, frame.data_size);
            return false;
        }
    }

    bool Protocol::on_setrecv()
    {
        send_frame(Frame::ACK);
        reset();
        current_mode_ = Mode::Server;
        return true;
    }

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        THROW(exceptions::Not_Implemented);
    }

    void Protocol::complete_read(unsigned char* buf, size_t max_capacity)
    {
        if (max_capacity < buf_.size())
            THROW(exceptions::Buffer_Overflow);

        if (!buf_.empty())
            memcpy(buf, &*buf_.begin(), buf_.size());
    }

    void Protocol::send_data(const unsigned char* data, size_t length)
    {
        THROW(exceptions::Not_Implemented);
    }

    void Protocol::send_frame(Frame::Type type, const unsigned char* buf, size_t length)
    {
        if (type == Frame::DATA)
        {
            if (!buf || (length == 0))
                THROW(exceptions::Incorrect_Parameter);

            send_data(buf, length);
        }

        unsigned char frame[2];
        frame[0] = type;
        frame[1] = util::crc7(frame, 1);

        transport_->write(frame, sizeof(frame));
    }

    Protocol::Data_Frame Protocol::make_data_frame(unsigned char* buf, size_t length)
    {
        if (length < 3)
            THROW(exceptions::Invalid_Frame);

        length -= 3;

        Data_Frame frame;
        frame.type = static_cast<Frame::Type>(buf[0]);
        frame.data = buf + 2;
        frame.data_size = length;
        frame.sequence_num = buf[1];

        return frame;
    }

namespace testing
{
    class Dummy_Transport: public Transport_Interface
    {
        public:

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                static unsigned char seq = 0;

                unsigned char data[6] = {0};

                if (seq == 0)
                {
                    seq++;

                    data[0] = 0x0c;
                    data[1] = 0x6a;

                    memcpy(buf, data, 2);
                    return 2;
                }

                if (seq == 1)
                {
                    seq++;

                    data[0] = 0x10;
                    data[1] = 1;

                    data[2] = 0xde;
                    data[3] = 0xad;
                    data[4] = util::crc7(data, 4);

                    memcpy(buf, data, sizeof(data));
                    return 5;
                }

                if (seq == 2)
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

                if (seq == 3)
                {
                    seq++;

                    data[0] = 0x0d;
                    data[1] = 0x2b;

                    memcpy(buf, data, 2);
                    return 2;
                }

                return 0;
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                return buf_size;
            }
    };

    bool proto_test()
    {
        Dummy_Transport transport;
        Protocol proto(&transport);
        
        char buf[256] = {0};
        char ethalon[256] = {0xde, 0xad, 0xbe, 0xee, 0xef};

        if (proto.read(buf, sizeof(buf)) != 5)
        {
            printf("proto.read() returned incorrect size.\n");
            return false;
        }

        if (memcmp(buf, ethalon, 5) != 0)
        {
            printf("proto.read() got incorrect data.\n");
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
