#pragma once
#include <string>
#include <string.h>

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

        virtual size_t read(void* buf, size_t buf_size) = 0;
        virtual size_t write(const void* buf, size_t buf_size) = 0;
    };

    class Protocol: public Transport_Interface
    {
        public:

            size_t read(void* buf, size_t buf_size);
            size_t write(const void* buf, size_t buf_size);

            //Basically waiting means working in server mode waiting to receive a specific frame - 
            //mode switch frame and exiting in case the frame is received or timeout reached.
            void wait_send_mode(size_t timeout);
            void wait_recv_mode(size_t timeout);
    };
};

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
}};