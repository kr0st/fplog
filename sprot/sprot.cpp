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

    Protocol::Protocol(fplog::Transport_Interface* transport, size_t MTU):
    transport_(transport),
    sequence_num_(0),
    MTU_(MTU),
    terminating_(false),
    evil_twin_(0),
    twin_size_(0)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (!transport_)
            THROW(fplog::exceptions::Incorrect_Parameter);

        frame_buf_ = new unsigned char [MTU_ + Frame::overhead];
    }

    Protocol::~Protocol()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        terminating_ = true;

        delete [] frame_buf_;
        frame_buf_ = 0;
    }

    size_t Protocol::read(void* buf, size_t buf_size, size_t timeout)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (terminating_)
            return 0;

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        int full_retries = 5;
        int bytes_read = 0;
        unsigned char* ptr = (unsigned char*)buf;
    
    full_read_retry:

        bytes_read = 0;
        sequence_num_ = 0;
        ptr = (unsigned char*)buf;

        auto check_time_out = [&timeout, &timer_start]()
        {
            time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
            system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
            if (timer_stop - timer_start >= converted_timeout)
                THROW(fplog::exceptions::Timeout);
        };

        bool multipart = false, last = false;
        int retry_count = 5;
        Frame recv_frame;

        auto frame_read = [check_time_out, &recv_frame, &full_retries, &retry_count, this]()
        {
            while (true)
            {
                check_time_out();

                try
                {
                    recv_frame = read_frame();
                    break;
                }
                catch (sprot::exceptions::Wrong_Sequence& e)
                {
                    if (full_retries == 0)
                        THROW(fplog::exceptions::Read_Failed);

                    full_retries--;
                    return false;
                }
                catch (fplog::exceptions::Generic_Exception& e)
                {
                    if (full_retries == 0)
                        THROW(fplog::exceptions::Read_Failed);

                    if (retry_count == 0)
                    {
                        full_retries--;
                        return false;
                    }

                    retry_count--;
                }
            }

            return true;
        };

        while (true)
        {
            check_time_out();

            if (!frame_read())
                goto full_read_retry;

            if ((recv_frame.type != Frame::DATA_FIRST) &&
                (recv_frame.type != Frame::DATA_SINGLE) &&
                (recv_frame.type != Frame::DATA_LAST))
            {
                if (full_retries == 0)
                    THROW(fplog::exceptions::Read_Failed);
                full_retries--;
                goto full_read_retry;
            }

            if (recv_frame.type == Frame::DATA_FIRST)
                multipart = true;

            if (recv_frame.type == Frame::DATA_LAST)
                last = true;

            bytes_read += recv_frame.data.size();
            if (bytes_read > buf_size)
                THROW(fplog::exceptions::Buffer_Overflow);

            memcpy(ptr, &recv_frame.data[0], recv_frame.data.size());
            ptr += recv_frame.data.size();

            Frame last_frame(recv_frame);

            sequence_num_++;
            Frame send_frame = make_frame(Frame::ACK);

            while (true)
            {
                check_time_out();

                try
                {
                    write_frame(send_frame);
                    break;
                }
                catch (fplog::exceptions::Generic_Exception& e)
                {
                    if (retry_count == 0)
                        THROW(fplog::exceptions::Read_Failed);
                    retry_count--;
                }
            }

            sequence_num_++;
            if (!frame_read())
            {
                auto duplicate_frame = [this, &bytes_read, buf] ()
                {
                    twin_size_ = bytes_read;
                    if (evil_twin_)
                        delete [] evil_twin_;
                    evil_twin_ = new unsigned char[twin_size_];
                    memcpy(evil_twin_, buf, twin_size_);
                };

                //First we will try to guess if we get all the data except the last ACK
                //if we did get all the data - return successfully anyway.
                if ((last_frame.type == Frame::DATA_SINGLE) && !multipart)
                {
                    duplicate_frame();
                    return bytes_read;
                }

                if ((last_frame.type == Frame::DATA_LAST) && multipart)
                {
                    duplicate_frame();
                    return bytes_read;
                }

                goto full_read_retry;
            }

            auto detect_evil_presence = [this, &bytes_read, buf]()
            {
                if (twin_size_ && evil_twin_)
                {
                    bool got_duplicate = false;

                    if (bytes_read == twin_size_)
                        if (memcmp(evil_twin_, buf, twin_size_) == 0)
                            got_duplicate = true;

                    twin_size_ = 0;
                    delete [] evil_twin_;
                    evil_twin_ = 0;

                    if (got_duplicate)
                        return true;
                }

                return false;
            };

            if (recv_frame.type == Frame::ACK)
            {
                if (!multipart)
                {
                    if (detect_evil_presence())
                        goto full_read_retry;

                    return bytes_read;
                }

                if (multipart && last)
                {
                    if (detect_evil_presence())
                        goto full_read_retry;

                    return bytes_read;
                }
            }

            sequence_num_++;
        }

        return bytes_read;
    }

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (terminating_)
            return 0;

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());
        int full_retries = 5;

    full_write_retry:

        sequence_num_ = 0;

        auto check_time_out = [&timeout, &timer_start]()
        {
            time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
            system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
            if (timer_stop - timer_start >= converted_timeout)
                THROW(fplog::exceptions::Timeout);
        };

        if (buf_size <= MTU_)
        {
            write_data(buf, buf_size, Frame::DATA_SINGLE, timeout);
            return buf_size;
        }

        unsigned char* ptr = (unsigned char*)buf;
        int bytes_left = buf_size;

        try
        {
            while (bytes_left > 0)
            {
                check_time_out();

                int to_copy = (bytes_left < MTU_) ? bytes_left : MTU_;

                if (bytes_left <= MTU_)
                    write_data(ptr, to_copy, Frame::DATA_LAST);
                else
                    if (bytes_left == buf_size)
                        write_data(ptr, to_copy, Frame::DATA_FIRST);
                    else
                        write_data(ptr, to_copy, Frame::DATA_SINGLE);

                ptr += to_copy;
                int copied = ptr - (unsigned char*)buf;
                bytes_left = buf_size - copied;
            }
        }
        catch(fplog::exceptions::Generic_Exception& e)
        {
            if (full_retries == 0)
                THROW(fplog::exceptions::Write_Failed);
            full_retries--;
            goto full_write_retry;
        }

        return buf_size;
    }

    Protocol::Frame Protocol::read_frame()
    {
        fill_frame_buf('R');

        const size_t max_len = MTU_ + Frame::overhead;
        size_t recv_size = transport_->read(frame_buf_, max_len, Timeout::Operation);
        if ((recv_size == 0) || (recv_size > max_len))
            THROW(exceptions::Invalid_Frame);
        Frame frame = make_frame(frame_buf_, recv_size);
        return frame;
    }

    void Protocol::write_frame(const Frame& frame)
    {
        fill_frame_buf('W');

        if (frame.data.size() > MTU_)
            THROW(fplog::exceptions::Buffer_Overflow);

        unsigned char* fptr = frame_buf_;
        fptr[0] = frame.type;
        fptr++;

        memcpy(fptr, &frame.sequence, sizeof(frame.sequence));
        fptr += sizeof(frame.sequence);

        unsigned short data_sz = frame.data.size();
        if (data_sz > 0)
        {
            memcpy(fptr, &data_sz, sizeof(data_sz));
            fptr += sizeof(data_sz);

            memcpy(fptr, &frame.data[0], data_sz);
            fptr += data_sz;
        }

        *fptr = frame.crc;
        fptr++;

        size_t proper_data_sz = frame.data.size();
        size_t write_sz = fptr - frame_buf_;

        size_t written = transport_->write(frame_buf_, write_sz, Timeout::Operation);
        if (write_sz != written)
            THROW(exceptions::Invalid_Frame);
    }

    void Protocol::write_data(const void* buf, size_t buf_size, Frame::Type type, size_t timeout)
    {
        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        auto check_time_out = [&timeout, &timer_start]()
        {
            time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
            system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
            if (timer_stop - timer_start >= converted_timeout)
                THROW(fplog::exceptions::Timeout);
        };

        int retry_count = 5;

        Frame send_frame = make_frame(type, (unsigned char*)buf, buf_size);

        while (true)
        {
            check_time_out();

            try
            {
                write_frame(send_frame);
                break;
            }
            catch (fplog::exceptions::Generic_Exception& e)
            {
                if (retry_count == 0)
                    THROW(fplog::exceptions::Write_Failed);

                retry_count--;
            }
        }

        Frame recv_frame;
        sequence_num_++;
        retry_count = 5;

        auto frame_read = [&recv_frame, &retry_count, this, &check_time_out]()
        {
            while (true)
            {
                check_time_out();

                try
                {
                    recv_frame = read_frame();
                    break;
                }
                catch (sprot::exceptions::Wrong_Sequence& e)
                {
                    return false;
                }
                catch (fplog::exceptions::Generic_Exception& e)
                {
                    if (retry_count == 0)
                        THROW(fplog::exceptions::Read_Failed);

                        retry_count--;
                }
            }

            return true;
        };

        if (!frame_read())
            THROW(exceptions::Invalid_Frame);

        if (recv_frame.type != Frame::ACK)
            THROW(exceptions::Invalid_Frame);

        sequence_num_++;
        retry_count = 5;

        send_frame = make_frame(Frame::ACK);

        while (true)
        {
            check_time_out();

            try
            {
                write_frame(send_frame);
                sequence_num_++;
                break;
            }
            catch (fplog::exceptions::Generic_Exception& e)
            {
                if (retry_count == 0)
                    THROW(fplog::exceptions::Write_Failed);

                retry_count--;
            }
        }
    }

    Protocol::Frame Protocol::make_frame(const Frame::Type type, unsigned char* data, size_t data_length)
    {
        Frame frame;
        frame.type = type;

        if (data_length > MTU_)
            THROW(fplog::exceptions::Buffer_Overflow);

        frame.data.reserve(data_length);
        for (auto i = 0; i < data_length; ++i)
            frame.data.push_back(data[i]);
        frame.sequence = sequence_num_;

        unsigned char* fptr = frame_buf_;
        fptr[0] = frame.type;
        fptr++;

        memcpy(fptr, &frame.sequence, sizeof(frame.sequence));
        fptr += sizeof(frame.sequence);

        unsigned short data_sz = frame.data.size();
        if (data_sz > 0)
        {
            memcpy(fptr, &data_sz, sizeof(data_sz));
            fptr += sizeof(data_sz);

            memcpy(fptr, &frame.data[0], data_sz);
            fptr += data_sz;
        }

        frame.crc = util::crc7(frame_buf_, fptr - frame_buf_);
        return frame;
    }

    Protocol::Frame Protocol::make_frame(const unsigned char* buf, size_t length)
    {
        Frame frame;
        unsigned char* fptr = (unsigned char*)buf;
        frame.type = (Protocol::Frame::Type)fptr[0];
        fptr++;

        memcpy(&frame.sequence, fptr, sizeof(frame.sequence));
        fptr += sizeof(frame.sequence);

        if (sequence_num_ != frame.sequence)
            THROW(exceptions::Wrong_Sequence);

        if ((frame.type == Frame::DATA_FIRST) ||
            (frame.type == Frame::DATA_SINGLE) ||
            (frame.type == Frame::DATA_LAST))
        {
            unsigned short data_sz = 0;
            memcpy(&data_sz, fptr, sizeof(data_sz));
            fptr += sizeof(data_sz);

            frame.data.resize(data_sz);
            memcpy(&frame.data[0], fptr, data_sz);
            fptr += data_sz;
        }

        frame.crc = *fptr;

        if (frame.crc != util::crc7(frame_buf_, fptr - frame_buf_))
            THROW(exceptions::Invalid_Frame);

        return frame;
    }

    void Protocol::fill_frame_buf(const char c)
    {
        memset(frame_buf_, c, MTU_ + Frame::overhead);
    }
};
