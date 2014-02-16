// sprot.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "sprot.h"
#include <windows.h>

using namespace std::chrono;

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

    Protocol::Protocol(Transport_Interface* transport, Switching::Type switching, size_t recv_buf_reserve, size_t MTU):
    transport_(transport),
    switching_(switching),
    current_mode_(Mode::Undefined),
    is_sequence_(false),
    recv_buf_reserve_(recv_buf_reserve),
    sequence_num_(0),
    out_buf_(0),
    out_buf_sz_(0),
    MTU_(MTU - 3) //3 bytes of overhead for data frame
    {
        if (!transport_)
            THROW(exceptions::Incorrect_Parameter);
    }

    size_t Protocol::read_impl(void* buf, size_t buf_size, size_t timeout, bool no_mode_check)
    {
        if (!buf)
            THROW(exceptions::Incorrect_Parameter);

        if (!no_mode_check)
        {
            if (current_mode_ == Mode::Undefined)
                current_mode_ = Mode::Server;

            if (current_mode_ == Mode::Client)
            {
                if (switching_ == Switching::Auto)
                {
                    if (!wait_recv_mode(timeout))
                        THROW(exceptions::Incorrect_Mode);
                }
                else
                    THROW(exceptions::Incorrect_Mode);
            }
        }

        out_buf_ = static_cast<unsigned char*>(buf);
        out_buf_sz_ = buf_size;
        
        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        bool looping = true;
        while(looping)
        {
            time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
            system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
            if (timer_stop - timer_start >= converted_timeout)
                THROW(exceptions::Timeout);

            size_t recv_sz = transport_->read(buf, buf_size, timeout);            
            if (crc_check(static_cast<unsigned char*>(buf), recv_sz))
                looping = on_frame(static_cast<unsigned char*>(buf), recv_sz, buf_size);
            else
                send_frame(Frame::NACK);
        };

        size_t read_data = buf_.size();
        reset();

        return read_data;
    }
    
    size_t Protocol::write_impl(const void* buf, size_t buf_size, size_t timeout, bool no_mode_check)
    {
        if (!buf)
            THROW(exceptions::Incorrect_Parameter);

        if (!no_mode_check)
        {
            if (current_mode_ == Mode::Undefined)
                current_mode_ = Mode::Client;

            if (current_mode_ == Mode::Server)
            {
                if (switching_ == Switching::Auto)
                {
                    if (!wait_send_mode(timeout))
                        THROW(exceptions::Incorrect_Mode);
                }
                else
                    THROW(exceptions::Incorrect_Mode);
            }
        }

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());
        size_t op_timeout = 200; //ms

        bool seq_begin_sent = false;
        long data_left = buf_size;
        size_t data_frame_counter = 0;
        
        Frame control_frame;
        Data_Frame data_frame;
        Frame* current_frame = &data_frame;

        do 
        {
            int fail_count = 5;

            if ((data_left > MTU_) && !seq_begin_sent)
            {
                //Sending begin sequence
                current_frame = &control_frame;
                control_frame.type = Frame::SEQBEGIN;
            }

            if ((data_left <= 0) && !seq_begin_sent)
                current_frame = 0;

            if ((data_left <= 0) && seq_begin_sent)
            {
                //Sending end sequence
                current_frame = &control_frame;
                control_frame.type = Frame::SEQEND;
            }

            if (current_frame == &data_frame)
            {
                //Need to send data frame
                data_frame.data = static_cast<const unsigned char*>(buf) + data_frame_counter * MTU_;
                data_frame.data_size = data_left > MTU_ ? MTU_ : data_left;
            }

            if (current_frame == 0)
                break;

            while (fail_count > 0)
            {
                time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
                system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
                if (timer_stop - timer_start >= converted_timeout)
                    THROW(exceptions::Timeout);
            
                try
                {
                    unsigned char temp_buf[256];

                    if (current_frame == &data_frame)
                    {
                        size_t bytes_sent = send_frame(Frame::DATA, op_timeout, data_frame.data, data_frame.data_size);
                        if (transport_->read(temp_buf, sizeof(temp_buf), op_timeout) == 2) //Control frame size is always 2 bytes
                        {
                            if ((temp_buf[0] == Frame::ACK) && crc_check(temp_buf, 1))
                            {
                                data_left -= bytes_sent;
                                data_frame_counter++;
                                break;
                            }
                        }
                    }
                    else
                    {
                        send_frame(current_frame->type, op_timeout);
                        if (transport_->read(temp_buf, sizeof(temp_buf), op_timeout) == 2) //Control frame size is always 2 bytes
                        {
                            if ((temp_buf[0] == Frame::ACK) && crc_check(temp_buf, 1))
                            {
                                if (current_frame->type == Frame::SEQBEGIN)
                                    seq_begin_sent = true;
                                if (current_frame->type == Frame::SEQEND)
                                    current_frame = 0; //Time to exit, data sent
                                break;
                            }
                        }
                    }
                }
                catch (exceptions::Timeout)
                {
                }
                catch (exceptions::Write_Failed)
                {
                }

                fail_count--;
            }

            if (fail_count == 0)
                THROW(exceptions::Write_Failed);

        } while (current_frame != 0);

        reset();

        return buf_size;
    }

    size_t Protocol::read(void* buf, size_t buf_size, size_t timeout)
    {
        reset();
        return read_impl(buf, buf_size, timeout);
    }
    
    bool Protocol::on_frame(const unsigned char* buf, size_t recv_length, size_t max_capacity)
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
            case Frame::SEQEND: return on_seqend();
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

    bool Protocol::on_seqend()
    {
        is_sequence_ = false;
        send_frame(Frame::ACK);
        complete_read();
        return false;
    }

    void Protocol::reset()
    {
        is_sequence_ = false;
        sequence_num_ = 0;
        
        std::vector<unsigned char> empty;
        buf_.swap(empty);
        buf_.reserve(recv_buf_reserve_);

        out_buf_ = 0;
        out_buf_sz_ = 0;
    }

    bool Protocol::on_setsend()
    {
        send_frame(Frame::ACK);
        reset();

        if (current_mode_ == Mode::Client)
            return true;
        
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

        buf_.insert(buf_.end(), frame.data, frame.data + frame.data_size);

        if (is_sequence_)
            return true;
        else
        {
            complete_read();
            return false;
        }
    }

    bool Protocol::on_setrecv()
    {
        send_frame(Frame::ACK);
        reset();

        if (current_mode_ == Mode::Server)
            return true;
        
        current_mode_ = Mode::Server;
        return false;
    }

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        reset();
        return write_impl(buf, buf_size, timeout);
    }

    void Protocol::complete_read()
    {
        if (out_buf_sz_ < buf_.size())
            THROW(exceptions::Buffer_Overflow);

        if (!buf_.empty())
            memcpy(out_buf_, &*buf_.begin(), buf_.size());
    }

    size_t Protocol::send_data(const unsigned char* data, size_t length, size_t timeout)
    {
        size_t frame_sz = length + 3;
        unsigned char* frame = new unsigned char [frame_sz]; //3 bytes overhead for data frame - type, seq #, crc

        frame[0] = Frame::DATA;
        frame[1] = sequence_num_;

        memcpy(&frame[2], data, length);
        frame[frame_sz - 1] = util::crc7(frame, 1);

        if (transport_->write(frame, frame_sz, timeout) != frame_sz)
            THROW(exceptions::Write_Failed);

        return frame_sz;
    }

    size_t Protocol::send_frame(Frame::Type type, size_t timeout, const unsigned char* buf, size_t length)
    {
        if (type == Frame::DATA)
        {
            if (!buf || (length == 0))
                THROW(exceptions::Incorrect_Parameter);

            return send_data(buf, length, timeout);
        }

        unsigned char frame[2];
        frame[0] = type;
        frame[1] = util::crc7(frame, 1);

        if (transport_->write(frame, sizeof(frame), timeout) != sizeof(frame))
            THROW(exceptions::Write_Failed);

        return 1; //Control frame has 1 byte actual payload
    }

    Protocol::Data_Frame Protocol::make_data_frame(const unsigned char* buf, size_t length)
    {
        if (length < 3)
            THROW(exceptions::Invalid_Frame);

        length -= 3; //3 bytes of overhead - type, sequence # and crc
        Data_Frame frame(buf + 2, length, buf[1]);

        return frame;
    }

    bool Protocol::wait_send_mode(size_t timeout)
    {
        if (current_mode_ == Mode::Client)
            return true;
        
        current_mode_ = Mode::Undefined;
        unsigned char buf[50];
        read_impl(buf, sizeof(buf), timeout, true);

        return (current_mode_ == Mode::Client);
    }
    
    bool Protocol::wait_recv_mode(size_t timeout)
    {
        if (current_mode_ == Mode::Server)
            return true;
        
        current_mode_ = Mode::Undefined;
        unsigned char buf[50];
        read_impl(buf, sizeof(buf), timeout, true);

        return (current_mode_ == Mode::Server);
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

                //First sending sequence start frame
                if (seq == 0)
                {
                    seq++;

                    data[0] = 0x0c;
                    data[1] = 0x6a;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //Then sending the first data frame.
                if (seq == 1)
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

                //Second data frame
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

                //Finally sending sequence end
                if (seq == 3)
                {
                    seq++;

                    data[0] = 0x0d;
                    data[1] = 0x2b;

                    memcpy(buf, data, 2);
                    return 2;
                }

                //Sending an ordinary data frame
                if (seq == 4)
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
                if (seq == 5)
                {
                    seq++;

                    data[0] = 0x0e;
                    data[1] = 0x79;

                    memcpy(buf, data, sizeof(data));
                    return 2;
                }

                //Sending switch to server mode command
                if (seq == 6)
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
                return buf_size;
            }
    };

    bool proto_test()
    {
        Dummy_Transport transport;
        Protocol proto(&transport);
        
        char buf[256] = {0};
        char ethalon[256] = {0xde, 0xad, 0xbe, 0xee, 0xef};
        char ethalon2[256] = {0x45, 0xe9, 0xa6};

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

        if (proto.read(buf, sizeof(buf)) != 3)
        {
            printf("proto.read() returned incorrect size.\n");
            return false;
        }

        if (memcmp(buf, ethalon2, 3) != 0)
        {
            printf("proto.read() got incorrect data.\n");
            return false;
        }

        if (!proto.wait_send_mode())
        {
            printf("proto.wait_send_mode() failed.\n");
            return false;
        }

        if (!proto.wait_recv_mode())
        {
            printf("proto.wait_recv_mode() failed.\n");
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
