#pragma once

#include <limits.h>
#include <string>
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>

#ifdef _DEBUG
#define private public
#endif

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#ifdef WIN32
#define __SHORT_FORM_OF_FILE_WIN__ \
    (strrchr(__FILE__,'\\') \
    ? strrchr(__FILE__,'\\')+1 \
    : __FILE__ \
    )
#define __SHORT_FORM_OF_FILE__ __SHORT_FORM_OF_FILE_WIN__
#else
#define __SHORT_FORM_OF_FILE_NIX__ \
    (strrchr(__FILE__,'/') \
    ? strrchr(__FILE__,'/')+1 \
    : __FILE__ \
    )
#define __SHORT_FORM_OF_FILE__ __SHORT_FORM_OF_FILE_NIX__
#endif

#define THROW(exception_type) { throw exception_type(__FUNCTION__, __SHORT_FORM_OF_FILE__, __LINE__); }
#define THROWM(exception_type, message) { throw exception_type(__FUNCTION__, __SHORT_FORM_OF_FILE__, __LINE__, message); }

namespace sprot
{
    class Transport_Interface
    {
        public:

            static const size_t infinite_wait = static_cast<size_t>(-1);

            virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
            virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
    };

    class Protocol: public Transport_Interface
    {
        public:

            //Set to manual, sprot will throw exceptions
            //on trying to read/write in an inappropriate mode.
            //Auto is default which will switch modes automatically,
            //based on what request is executed - read or write.
            struct Switching
            {
                enum Type
                {
                    Auto = 0x029A,
                    Manual
                };
            };

            struct Mode
            {
                enum Type
                {
                    Client = 0xA920,
                    Server,
                    Undefined
                };
            };

            struct Frame
            {
                enum Type
                {
                    ACK = 0x0a,
                    NACK,
                    SEQBEGIN,
                    SEQEND,
                    SETSEND,
                    SETRECV,
                    DATA,
                    UNDEF //Unknown frame type.
                };

                Type type = UNDEF;
            };

            struct Data_Frame: public Frame
            {
                unsigned char* data = 0;
                size_t data_size = 0;
                unsigned char sequence_num = 0;
            };

            Protocol(Transport_Interface* transport, Switching::Type switching = Switching::Auto, size_t recv_buf_reserve = 3 * 1024 * 1024);

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

            //Basically waiting means working in server mode waiting to receive a specific frame - 
            //mode switch frame and exiting in case the frame is received or timeout reached.
            void wait_send_mode(size_t timeout = infinite_wait);
            void wait_recv_mode(size_t timeout = infinite_wait);


        private:

            Transport_Interface* transport_;
            Switching::Type switching_;
            Mode::Type current_mode_;
            bool is_sequence_;
            std::vector<unsigned char> buf_;
            size_t recv_buf_reserve_;
            unsigned char sequence_num_;

            static bool crc_check(const unsigned char* buf, size_t length);
            void send_control_frame(Frame::Type frame);

            //Returns true when more frames are needed before returning data to upper layer,
            //false means whole data portion was received, read should stop and return data to upper layer.
            bool on_frame(unsigned char* buf, size_t recv_length, size_t max_capacity);
            
            bool on_seqbegin();
            bool on_seqend(unsigned char* buf, size_t max_capacity);

            bool on_setsend();
            bool on_setrecv();

            bool on_data(Data_Frame& frame);

            void send_frame(Frame::Type type, const unsigned char* buf = 0, size_t length = 0);
            void send_data(const unsigned char* data, size_t length);

            void complete_read(unsigned char* buf, size_t max_capacity);
            void reset();

            Data_Frame make_data_frame(unsigned char* buf, size_t length);
    };

namespace util
{
    unsigned char crc7(const unsigned char* buf, size_t length);
}};

namespace sprot { namespace exceptions
{
    class Exception
    {
        public:

            Exception(const char* facility, const char* file = "", int line = 0, const char* message = ""):
            facility_(facility),
            message_(message),
            file_(file),
            line_(line)
            {
            }


        protected:

            std::string facility_;
            std::string message_;
            std::string file_;
            int line_;


        private:

            Exception();
    };

    class Incorrect_Mode: public Exception
    {
        public:

            Incorrect_Mode(const char* facility, const char* file = "", int line = 0, const char* message = "Tried write in recv mode or read in send mode."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Write_Failed: public Exception
    {
        public:

            Write_Failed(const char* facility, const char* file = "", int line = 0, const char* message = "Write operation failed."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Incorrect_Parameter: public Exception
    {
        public:

            Incorrect_Parameter(const char* facility, const char* file = "", int line = 0, const char* message = "Parameter supplied to function has incorrect value."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Not_Implemented: public Exception
    {
        public:

            Not_Implemented(const char* facility, const char* file = "", int line = 0, const char* message = "Feature not implemented yet."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Buffer_Overflow: public Exception
    {
        public:

            Buffer_Overflow(const char* facility, const char* file = "", int line = 0, const char* message = "Buffer too small."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Invalid_Frame: public Exception
    {
        public:

            Invalid_Frame(const char* facility, const char* file = "", int line = 0, const char* message = "Received an invalid frame."):
            Exception(facility, file, line, message)
            {
            }
    };
}
};