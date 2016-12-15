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
    
    class Message
    {
        private:
        
            fplog::Message m;
    };

};

#endif /* fpylog_h */
