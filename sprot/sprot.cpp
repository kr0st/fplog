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

    Protocol::Protocol(fplog::Transport_Interface* transport, size_t MTU, int frames_before_ack):
    transport_(transport),
    sequence_num_(0),
    MTU_(MTU),
    terminating_(false),
    evil_twin_(0),
    twin_size_(0),
    ack_after_(frames_before_ack),
    frame_num_(0)
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
        int frame_num = 0;

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (terminating_)
            return 0;

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        int full_retries = 100;
        size_t bytes_read = 0;
        unsigned char* ptr = (unsigned char*)buf;
    
    full_read_retry:

        bytes_read = 0;
        sequence_num_ = 0;
        frame_num = 0;
        
        ptr = (unsigned char*)buf;

        auto check_time_out = [&timeout, &timer_start]()
        {
            auto timer_start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timer_start);
            auto timer_stop_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
            std::chrono::milliseconds timeout_ms(timeout);
            
            if (timer_stop_ms - timer_start_ms >= timeout_ms)
                THROW(fplog::exceptions::Timeout);
        };

        bool multipart = false, last = false;
        int retry_count = 5;
        Frame recv_frame, prev_frame;

        auto frame_read = [check_time_out, &recv_frame, &prev_frame, &full_retries, &retry_count, &frame_num, this]()
        {
            while (true)
            {
                check_time_out();

                try
                {
                    recv_frame = read_frame();
                    break;
                }
                catch (sprot::exceptions::Wrong_Sequence&)
                {
                    if (full_retries == 0)
                        THROW(fplog::exceptions::Read_Failed);

                    full_retries--;
                    return false;
                }
                catch (fplog::exceptions::Generic_Exception&)
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
            else
            {
                frame_num++;
            }
            
            if (recv_frame.type == Frame::DATA_FIRST)
                multipart = true;

            if (recv_frame.type == Frame::DATA_LAST)
                last = true;
                
            if ((recv_frame.type == Frame::DATA_FIRST) && (prev_frame.type == recv_frame.type))
                continue;
             
            bool should_copy_data = true;
   
            if ((recv_frame.type == Frame::DATA_LAST) && (prev_frame.type == recv_frame.type))
            {
                should_copy_data = false;
            }
            
            if (should_copy_data)
            {
                bytes_read += recv_frame.data.size();
                if (bytes_read > buf_size)
                    THROW(fplog::exceptions::Buffer_Overflow);

                memcpy(ptr, &recv_frame.data[0], recv_frame.data.size());
                ptr += recv_frame.data.size();            
            }
            
            prev_frame = recv_frame;
            Frame last_frame(recv_frame);

            bool ack_sent = false;
            if ((frame_num % ack_after_ == 0) || (recv_frame.type == Frame::DATA_LAST) || (!multipart))
            {
                sequence_num_++;
                Frame send_frame = make_frame(Frame::ACK);

                while (true)
                {
                    check_time_out();

                    try
                    {
                        write_frame(send_frame);
                        ack_sent = true;
                        break;
                    }
                    catch (fplog::exceptions::Generic_Exception&)
                    {
                        if (retry_count == 0)
                            THROW(fplog::exceptions::Read_Failed);
                        retry_count--;
                    }
                }
            }
            
            if (ack_sent)
            {
                frame_num = 0;
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

                    duplicate_frame();

                    //First we will try to guess if we get all the data except the last ACK
                    //if we did get all the data - return successfully anyway.

                    //however this specific case was disabled because it seem to worsens the situation
                    //and it is better to fail the whole read than to try such kind of recovery
                    //if ((last_frame.type == Frame::DATA_SINGLE) && !multipart)
                        //return bytes_read;

                    //this is much safer assumption, thus is still enabled
                    if ((last_frame.type == Frame::DATA_LAST) && multipart)
                        return bytes_read;

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
        }

        return bytes_read;
    }

    class Assign_On_Destroy
    {
        public:
        
            Assign_On_Destroy(int& x, int y): x_(x), y_(y) {}
            ~Assign_On_Destroy() { x_ = y_; }

        private:
        
            int& x_;
            int y_;
    };

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (terminating_)
            return 0;

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());
        int full_retries = 100;

    full_write_retry:

        sequence_num_ = 0;

        auto check_time_out = [&timeout, &timer_start]()
        {
            auto timer_start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timer_start);
            auto timer_stop_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
            std::chrono::milliseconds timeout_ms(timeout);
            
            if (timer_stop_ms - timer_start_ms >= timeout_ms)
                THROW(fplog::exceptions::Timeout);
        };

        if (buf_size <= MTU_)
        {
            Assign_On_Destroy acks(ack_after_, ack_after_);
            ack_after_ = 1;

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

                int to_copy = (bytes_left < (int)MTU_) ? bytes_left : (int)MTU_;

                if (bytes_left <= (int)MTU_)
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
        catch(fplog::exceptions::Generic_Exception&)
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

        unsigned short data_sz = (unsigned short)frame.data.size();
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

        auto check_time_out = [&timeout, &timer_start, this]()
        {
            auto timer_start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timer_start);
            auto timer_stop_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
            std::chrono::milliseconds timeout_ms(timeout);
            
            if (timer_stop_ms - timer_start_ms >= timeout_ms)
            {
                frame_num_ = 0;
                THROW(fplog::exceptions::Timeout);
            }
        };

        int retry_count = 5;

        Frame send_frame = make_frame(type, (unsigned char*)buf, buf_size);

        while (true)
        {
            check_time_out();

            try
            {
                write_frame(send_frame);
                frame_num_++;
                break;
            }
            catch (fplog::exceptions::Generic_Exception&)
            {
                if (retry_count == 0)
                {
                    frame_num_ = 0;
                    THROW(fplog::exceptions::Write_Failed);
                }
                retry_count--;
            }
        }

        Frame recv_frame;
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
                catch (sprot::exceptions::Wrong_Sequence&)
                {
                    frame_num_ = 0;
                    return false;
                }
                catch (fplog::exceptions::Generic_Exception&)
                {
                    if (retry_count == 0)
                    {
                        frame_num_ = 0;
                        THROW(fplog::exceptions::Read_Failed);
                    }
                        retry_count--;
                }
            }

            return true;
        };

        if ((frame_num_ % ack_after_ == 0) || (type == Frame::DATA_LAST))
        {
            sequence_num_++;
            
            if (!frame_read())
            {
                frame_num_ = 0;
                THROW(exceptions::Invalid_Frame);
            }

            if (recv_frame.type != Frame::ACK)
            {
                frame_num_ = 0;
                THROW(exceptions::Invalid_Frame);
            }
            
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
                    frame_num_ = 0;
                    break;
                }
                catch (fplog::exceptions::Generic_Exception&)
                {
                    if (retry_count == 0)
                    {
                        frame_num_ = 0;
                        THROW(fplog::exceptions::Write_Failed);
                    }
                    retry_count--;
                }
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
        for (size_t i = 0; i < data_length; ++i)
            frame.data.push_back(data[i]);
        frame.sequence = sequence_num_;

        unsigned char* fptr = frame_buf_;
        fptr[0] = frame.type;
        fptr++;

        memcpy(fptr, &frame.sequence, sizeof(frame.sequence));
        fptr += sizeof(frame.sequence);

        unsigned short data_sz = (unsigned short)frame.data.size();
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

namespace vsprot
{
    Protocol::Protocol(fplog::Transport_Interface* transport, size_t MTU): transport_(transport), MTU_(MTU)
    {
        write_buffer_.reserve(MTU_);
        read_buffer_.reserve(MTU_);
        
        header_[0] = 0xDE;
        header_[1] = 0xAD;
        header_[2] = 0xBE;
        header_[3] = 0xEF;
        header_[4] = 0x02;
        header_[5] = 0x9A;
    }

    bool validate_read_buffer(char* buf, size_t sz, char* header)
    {
        if (!buf)
            return true;

        for (char* ptr = buf; ptr < (buf + sz - 10); ptr++)
        {
            if (memcmp(ptr, header, 6) == 0)
            {
                ptr += 6;
                
                size_t frame_sz = 0;
                memcpy(&frame_sz, ptr, 4);
                
                ptr += 4;
                
                for (char* ptr2 = ptr; (ptr2 < (buf + sz - 6)) && (ptr2 < (ptr + frame_sz - 6)); ptr2++)
                {
                    if (memcmp(ptr2, header, 6) == 0)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

	int Protocol::read_frame(char* buf, size_t buf_size)
	{
        size_t _100_MB = 100 * 1024 * 1024;
        size_t frame_size = 0;
        
		if (read_buffer_.size() > (sizeof(header_) + 4))
        {
			if (memcmp(&(read_buffer_[0]), header_, sizeof(header_)) == 0)
			{                    
				memcpy(&frame_size, &(read_buffer_[0]) + sizeof(header_), 4);
				if (frame_size >= _100_MB)
				{
					{
						std::vector<char> dummy;
						read_buffer_.swap(dummy);
					}

					return 0;
				}

				size_t sz_buf = read_buffer_.size();
				if ( sz_buf < (frame_size + sizeof(header_) + 4))
					return 0;

				if (frame_size > buf_size)
					return ((int)buf_size - (int)frame_size);

				memcpy(buf, &(read_buffer_[0]) + sizeof(header_) + 4, frame_size);
				size_t new_size = read_buffer_.size() - sizeof(header_) - 4 - frame_size;

				std::vector<char> new_buffer;
				new_buffer.resize(new_size);
				memcpy(&(new_buffer[0]), &(read_buffer_[0]) + sizeof(header_) + 4 + frame_size, new_size);

				read_buffer_ = new_buffer;
				
				if ((read_buffer_.size() >= (sizeof(header_) + 4)))
				{
					if (memcmp(&(read_buffer_[0]), header_, sizeof(header_)) != 0)
						read_buffer_.clear();
				}

				return frame_size;
			}
		}
		
		return 0;
	}
	
	size_t Protocol::internal_read(size_t timeout)
	{
        char* temp_buf = new char[MTU_];
        size_t bytes_read = 0;

        try
        {
            bytes_read = transport_->read(temp_buf, MTU_, timeout);
            
            if (bytes_read > 0)
            {
                std::vector<char> new_buffer;
                new_buffer.resize(bytes_read);
                memcpy(&(new_buffer[0]), temp_buf, bytes_read);
				read_buffer_.insert(read_buffer_.end(), new_buffer.begin(), new_buffer.end());
            }
        }
        catch(fplog::exceptions::Timeout&)
        {
            std::vector<char> dummy;
            read_buffer_.swap(dummy);
            
            delete [] temp_buf;
			
			throw;
        }
		
		delete [] temp_buf;
        return bytes_read;
	}
	
	size_t Protocol::read(void* buf, size_t buf_size, size_t timeout)
	{
        if (!buf)
            return 0;

        if (!transport_)
            return 0;

        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        auto check_time_out = [&timeout, &timer_start]()
        {
            auto timer_start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timer_start);
            auto timer_stop_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
            std::chrono::milliseconds timeout_ms(timeout);
            
            if (timer_stop_ms - timer_start_ms >= timeout_ms)
                THROW(fplog::exceptions::Timeout);
        };
		
		while (true)
		{
			int bytes_read = read_frame((char*)buf, buf_size);
			
			if (bytes_read > 0)
				return bytes_read;
		
			if (bytes_read < 0)
				THROW(fplog::exceptions::Buffer_Overflow);
			
			check_time_out();
			internal_read(timeout);
		}
		
		return 0;
	}

    size_t Protocol::write(const void* buf, size_t buf_size, size_t timeout)
    {
        if (!buf)
            return 0;

        if (!transport_)
            return 0;
          
		if (buf_size == 0)
			return 0;
        //just for testing to make sure if there is no data,
        //the protocol is to blame, not something else
        //return transport_->write(buf, buf_size, timeout);

        {
            std::vector<char> dummy;
            write_buffer_.swap(dummy);
        }
        
        write_buffer_.resize(buf_size + sizeof(header_) + 4); //4 bytes is frame size that follows the header
        char* ptr = &(write_buffer_[0]), *start_ptr = ptr, *end_ptr = &(write_buffer_[write_buffer_.size() - 1]);
        
        memcpy(ptr, header_, sizeof(header_));
        ptr += sizeof(header_);
        
        memcpy(ptr, &buf_size, sizeof(buf_size));
        ptr += sizeof(buf_size);
        memcpy(ptr, buf, buf_size);
        
        time_point<system_clock, system_clock::duration> timer_start(system_clock::now());

        auto check_time_out = [&timeout, &timer_start]()
        {
            auto timer_start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timer_start);
            auto timer_stop_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
            std::chrono::milliseconds timeout_ms(timeout);

            if (timer_stop_ms - timer_start_ms >= timeout_ms)
                THROW(fplog::exceptions::Timeout);
        };

        ptr = start_ptr;
        size_t bytes_to_write = 1 + end_ptr - start_ptr;

        while (bytes_to_write > 0)
        {
            check_time_out();
            
            size_t bytes = bytes_to_write;
            if (bytes > MTU_)
                bytes = MTU_;

            size_t written = transport_->write(ptr, bytes, timeout);
            ptr += written;

            bytes_to_write -= written;
        }

        return buf_size;
    }
};
