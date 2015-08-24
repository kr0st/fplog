#pragma once

#include <limits.h>
#include <string>
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <fplog_exceptions.h>
#include <fplog_transport.h>

#ifdef SPROT_EXPORT
#define SPROT_API __declspec(dllexport)
#else
#define SPROT_API __declspec(dllimport)
#endif

#ifdef _DEBUG
#define private public
#endif

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

namespace sprot
{
    class SPROT_API Protocol: public fplog::Transport_Interface
    {
        public:

            struct Frame
            {
                enum Type
                {
                    ACK = 0x0a,
                    DATA_SINGLE,
                    DATA_FIRST,
                    DATA_LAST,
                    UNDEF //Unknown frame type.
                };

                Type type = UNDEF;
                std::vector<unsigned char> data;
                unsigned short sequence;
                unsigned char crc;

                static const int overhead = 6;
            };

            struct Timeout
            {
                enum Type
                {
                    Operation = 300 //ms
                };
            };

            Protocol(fplog::Transport_Interface* transport, size_t MTU = 1024);
            ~Protocol();

            virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
            virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);


        private:

            static const size_t default_timeout = 200; //ms

            unsigned char* evil_twin_;
            size_t twin_size_;

            fplog::Transport_Interface* transport_;
            unsigned short sequence_num_;
            bool terminating_;

            size_t MTU_;
            size_t recv_buf_reserve_;

            unsigned char* frame_buf_;
            std::recursive_mutex mutex_;

            Frame make_frame(const Frame::Type type, unsigned char* data = 0, size_t data_length = 0);
            Frame make_frame(const unsigned char* buf, size_t length);

            Frame read_frame();

            void write_frame(const Frame& frame);
            void write_data(const void* buf, size_t buf_size, Frame::Type type = Frame::Type::DATA_SINGLE, size_t timeout = Timeout::Operation);

            static bool crc_check(const unsigned char* buf, size_t length);

            void fill_frame_buf(const char c);
    };

namespace util
{
    SPROT_API unsigned char crc7(const unsigned char* buf, size_t length);
}};

namespace sprot { namespace exceptions
{
    class Exception: public fplog::exceptions::Generic_Exception
    {
        public:

            Exception(const char* facility, const char* file = "", int line = 0, const char* message = ""):
            Generic_Exception(facility, file, line, message)
            {
            }

        private:

            Exception();
    };

    class Invalid_Frame: public Exception
    {
        public:

            Invalid_Frame(const char* facility, const char* file = "", int line = 0, const char* message = "Received an invalid frame."):
            Exception(facility, file, line, message)
            {
            }
    };

    class Wrong_Sequence: public Invalid_Frame
    {
        public:

            Wrong_Sequence(const char* facility, const char* file = "", int line = 0, const char* message = "Received frame with wrong sequence number."):
            Invalid_Frame(facility, file, line, message)
            {
            }
    };
    
    class Wrong_Type: public Invalid_Frame
    {
        public:

            Wrong_Type(const char* facility, const char* file = "", int line = 0, const char* message = "Got frame of unexpected type."):
            Invalid_Frame(facility, file, line, message)
            {
            }
    };

    class Crc_Failed: public Invalid_Frame
    {
        public:

            Crc_Failed(const char* facility, const char* file = "", int line = 0, const char* message = "Received a corrupted frame (CRC failure)."):
            Invalid_Frame(facility, file, line, message)
            {
            }
    };
}};
