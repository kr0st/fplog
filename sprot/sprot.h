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

            struct Timeout
            {
                enum Type
                {
                    Operation = 200 //ms
                };
            };

            struct Data_Frame: public Frame
            {
                const unsigned char* data = 0;
                size_t data_size = 0;
                unsigned char sequence_num = 0;

                Data_Frame(const unsigned char* buf, size_t size, unsigned char sequence):
                data(buf),
                data_size(size),
                sequence_num(sequence)
                {
                    type = Frame::DATA;
                }

                Data_Frame()
                {
                    type = Frame::DATA;
                }
            };

            Protocol(fplog::Transport_Interface* transport, Switching::Type switching = Switching::Auto, size_t recv_buf_reserve = 3 * 1024, size_t MTU = 1024);

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

            //Basically waiting means working in server mode waiting to receive a specific frame - 
            //mode switch frame and exiting in case the frame is received or timeout reached.
            bool wait_send_mode(size_t timeout = infinite_wait);
            bool wait_recv_mode(size_t timeout = infinite_wait);


        private:

            static const size_t default_timeout = 200; //ms

            fplog::Transport_Interface* transport_;
            Switching::Type switching_;
            Mode::Type current_mode_;
            bool is_sequence_;
            std::vector<unsigned char> buf_;
            
            unsigned char* out_buf_;
            size_t out_buf_sz_;

            size_t recv_buf_reserve_;
            unsigned char sequence_num_;
            size_t MTU_;

            bool incomplete_read_;

            static bool crc_check(const unsigned char* buf, size_t length);

            //Returns true when more frames are needed before returning data to upper layer,
            //false means whole data portion was received, read should stop and return data to upper layer.
            bool on_frame(const unsigned char* buf, size_t recv_length, size_t max_capacity);
            
            bool on_seqbegin();
            bool on_seqend();

            bool on_setsend();
            bool on_setrecv();

            bool on_data(Data_Frame& frame);

            size_t send_frame(Frame::Type type, size_t timeout = default_timeout, const unsigned char* buf = 0, size_t length = 0);
            size_t send_data(const unsigned char* data, size_t length, size_t timeout = default_timeout);

            void complete_read();
            void reset();

            Data_Frame make_data_frame(const unsigned char* buf, size_t length);

            size_t read_impl(void* buf, size_t buf_size, size_t timeout = default_timeout, bool no_mode_check = false);
            size_t write_impl(const void* buf, size_t buf_size, size_t timeout = default_timeout, bool no_mode_check = false);
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

    class Incorrect_Mode: public Exception
    {
        public:

            Incorrect_Mode(const char* facility, const char* file = "", int line = 0, const char* message = "Tried write in recv mode or read in send mode."):
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

}};
