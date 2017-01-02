//
//  fpylog.h
//  fpylog
//
//  Created by Rostislav Kuratch on 06/12/16.
//  Copyright © 2016 Rostislav Kuratch. All rights reserved.
//

#ifndef fpylog_h
#define fpylog_h

#include <string>
#include <fplog.h>
#include <boost/python.hpp>

#ifdef FPYLOG_EXPORT

#ifdef _LINUX
#define FPYLOG_API __attribute__ ((visibility("default")))
#else
#define FPYLOG_API __declspec(dllexport)
#endif

#else

#ifdef _LINUX
#define FPYLOG_API
#else
#define FPYLOG_API __declspec(dllimport)
#endif

#endif


namespace fpylog
{
    struct FPYLOG_API World
    {
        void set(std::string msg) { this->msg = msg; }
        std::string greet() { return msg; }
        std::string msg;
    };
    
    class File
    {
        public:
        
            File(const char* prio, const char* name, boost::python::list& content);
            ~File() { delete file_; }
            fplog::Message as_message() { return file_->as_message(); }


        private:
        
            File();
            File(File& rhs);
        
            fplog::File* file_;
    };
    
    void initlog(const char* appname, const char* facility, const char* uid, bool use_async, fplog::Filter_Base* filter);

};

#endif /* fpylog_h */
