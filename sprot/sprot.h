#pragma once
#include <limits.h>
#include <string>
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>

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
            };

            Protocol(Transport_Interface* transport, Switching::Type switching = Switching::Auto);

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

            //Basically waiting means working in server mode waiting to receive a specific frame - 
            //mode switch frame and exiting in case the frame is received or timeout reached.
            void wait_send_mode(size_t timeout = infinite_wait);
            void wait_recv_mode(size_t timeout = infinite_wait);


        private:

            Transport_Interface* transport_;
            Switching::Type switching_;
            bool is_sequence_;

            static bool crc_check(const unsigned char* buf, size_t length);
            void send_control_frame(Frame::Type frame);
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

    class Incorrect_Mode : public Exception
    {
        public:

            Incorrect_Mode(const char* facility, const char* file = "", int line = 0, const char* message = "Tried write in recv mode or read in send mode."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Write_Failed : public Exception
    {
        public:

            Write_Failed(const char* facility, const char* file = "", int line = 0, const char* message = "Write operation failed."):
            Exception(facility, file, line, message)
            {
            }
    };
}
};