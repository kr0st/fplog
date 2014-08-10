#include "sprot.h"

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

    Protocol::Protocol(fplog::Transport_Interface* transport, Switching::Type switching, size_t recv_buf_reserve, size_t MTU):
    transport_(transport),
    switching_(switching),
    current_mode_(Mode::Undefined),
    is_sequence_(false),
    recv_buf_reserve_(recv_buf_reserve),
    sequence_num_(0),
    out_buf_(0),
    out_buf_sz_(0),
    MTU_(MTU - 3), //3 bytes of overhead for data frame
    incomplete_read_(false)
    {
        if (!transport_)
            THROW(fplog::exceptions::Incorrect_Parameter);
    }

    size_t Protocol::read_impl(void* buf, size_t buf_size, size_t timeout, bool no_mode_check)
    {
        if (!buf)
            THROW(fplog::exceptions::Incorrect_Parameter);

        out_buf_ = static_cast<unsigned char*>(buf);
        out_buf_sz_ = buf_size;

        if (!incomplete_read_)
        {
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
        
            time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

            bool looping = true;
            while(looping)
            {
                time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
                system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
                if (timer_stop - timer_start >= converted_timeout)
                    THROW(fplog::exceptions::Timeout);

                size_t recv_sz = transport_->read(buf, buf_size, timeout);            
                if (crc_check(static_cast<unsigned char*>(buf), recv_sz))
                    looping = on_frame(static_cast<unsigned char*>(buf), recv_sz, buf_size);
                else
                    send_frame(Frame::NACK);
            }
        }
        else
            complete_read();

        size_t read_data = buf_.size();
        reset();

        return read_data;
    }
    
    size_t Protocol::write_impl(const void* buf, size_t buf_size, size_t timeout, bool no_mode_check)
    {
        if (!buf)
            THROW(fplog::exceptions::Incorrect_Parameter);

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
        size_t op_timeout = Timeout::Operation; //ms

        bool seq_begin_sent = false;
        long data_left = buf_size;
        size_t data_frame_counter = 0;
        
        Frame control_frame;
        Data_Frame data_frame;
        Frame* current_frame = &data_frame;

        sequence_num_ = 1;

        do 
        {
            int fail_count = 5;

            if ((data_left > static_cast<long>(MTU_)) && !seq_begin_sent)
            {
                //Sending begin sequence
                current_frame = &control_frame;
                control_frame.type = Frame::SEQBEGIN;
            }

            if ((data_left > static_cast<long>(MTU_)) && seq_begin_sent)
                current_frame = &data_frame;

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
                data_frame.data_size = data_left > static_cast<long>(MTU_) ? MTU_ : data_left;
                data_frame.sequence_num = sequence_num_;
            }

            if (current_frame == 0)
                break;

            while (fail_count > 0)
            {
                time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
                system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
                if (timer_stop - timer_start >= converted_timeout)
                    THROW(fplog::exceptions::Timeout);
            
                try
                {
                    unsigned char temp_buf[256];

                    if (current_frame == &data_frame)
                    {
                        size_t bytes_sent = send_frame(Frame::DATA, op_timeout, data_frame.data, data_frame.data_size);
                        if (transport_->read(temp_buf, sizeof(temp_buf), op_timeout) == 2) //Control frame size is always 2 bytes
                        {
                            if ((temp_buf[0] == Frame::ACK) && crc_check(temp_buf, 2))
                            {
                                data_left -= (bytes_sent - 3);
                                data_frame_counter++;
                                sequence_num_++;
                                break;
                            }
                        }
                    }
                    else
                    {
                        send_frame(current_frame->type, op_timeout);
                        if (transport_->read(temp_buf, sizeof(temp_buf), op_timeout) == 2) //Control frame size is always 2 bytes
                        {
                            if ((temp_buf[0] == Frame::ACK) && crc_check(temp_buf, 2))
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
                catch (fplog::exceptions::Timeout)
                {
                }
                catch (fplog::exceptions::Write_Failed)
                {
                }

                fail_count--;
            }

            if (fail_count == 0)
                THROW(fplog::exceptions::Write_Failed);

        } while (current_frame != 0);

        reset();

        return buf_size;
    }

    size_t Protocol::read(void* buf, size_t buf_size, size_t timeout)
    {
        if (!incomplete_read_)
            reset();
        
        try
        {
            size_t read = read_impl(buf, buf_size, timeout);
            return read;
        }
        catch (fplog::exceptions::Write_Failed)
        {
            THROW(fplog::exceptions::Read_Failed);
        }

        return 0;
    }
    
    bool Protocol::on_frame(const unsigned char* buf, size_t recv_length, size_t max_capacity)
    {
        if (recv_length == 0)
            return true;

        if (!buf)
            THROW(fplog::exceptions::Incorrect_Parameter);

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
        incomplete_read_ = false;
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
        
        try
        {
            size_t wrote = write_impl(buf, buf_size, timeout);
            return wrote;
        }
        catch(fplog::exceptions::Read_Failed)
        {
            THROW(fplog::exceptions::Write_Failed);
        }

        return 0;
    }

    void Protocol::complete_read()
    {
        incomplete_read_ = true;

        if (out_buf_sz_ < buf_.size())
            throw fplog::exceptions::Buffer_Overflow(__FUNCTION__, __SHORT_FORM_OF_FILE__, __LINE__, "Buffer too small.", buf_.size());

        incomplete_read_ = false;

        if (!buf_.empty())
        {
            memcpy(out_buf_, &*buf_.begin(), buf_.size());

            //Potentially we might have a large chunk of memory used
            //while assemblying the frame before copying it to out buffer
            //therefore we need to really dealloc internal buffer buf_.
            if (buf_.size() > recv_buf_reserve_)
            {
                std::vector<unsigned char> empty;
                buf_.swap(empty);
                buf_.reserve(recv_buf_reserve_);
            }
        }
    }

    size_t Protocol::send_data(const unsigned char* data, size_t length, size_t timeout)
    {
        size_t frame_sz = length + 3;
        unsigned char* frame = new unsigned char [frame_sz]; //3 bytes overhead for data frame - type, seq #, crc

        frame[0] = Frame::DATA;
        frame[1] = sequence_num_;

        memcpy(&frame[2], data, length);
        frame[frame_sz - 1] = util::crc7(frame, frame_sz - 1);

        size_t data_written = transport_->write(frame, frame_sz, timeout);
        if (data_written != frame_sz)
            THROW(fplog::exceptions::Write_Failed);

        return frame_sz;
    }

    size_t Protocol::send_frame(Frame::Type type, size_t timeout, const unsigned char* buf, size_t length)
    {
        if (type == Frame::DATA)
        {
            if (!buf || (length == 0))
                THROW(fplog::exceptions::Incorrect_Parameter);

            return send_data(buf, length, timeout);
        }

        unsigned char frame[2];
        frame[0] = type;
        frame[1] = util::crc7(frame, 1);

        size_t data_written = transport_->write(frame, sizeof(frame), timeout);
        if (data_written != sizeof(frame))
            THROW(fplog::exceptions::Write_Failed);

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
};
