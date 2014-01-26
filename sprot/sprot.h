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

#define THROW(exception_type, message) { throw exception_type(__FUNCTION__, message, __SHORT_FORM_OF_FILE__, __LINE__); }

namespace sprot
{
    class Transport_Interface
    {
    public:

        virtual size_t read(void* buf, size_t buf_size) = 0;
        virtual size_t write(const void* buf, size_t buf_size) = 0;
    };

    class Exception
    {
    public:

        Exception(const char* facility, const char* message, const char* file = "", int line = 0):
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
            std::string function_;


        private:

            Exception();
    };
};
