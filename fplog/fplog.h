#pragma once

#include <string>
#include <set>
#include <libjson/libjson.h>

#ifndef __SHORT_FORM_OF_FILE__

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

#endif

namespace fplog
{

struct Facility
{
    static const char* system; //message from some system component
    static const char* user; //message from user-level component
    static const char* security; //security or authorization related message
    static const char* fplog; //message from fplog directly, could inform about log system malfunction
};

struct Prio
{
    static const char* emergency; //system is unusable
    static const char* alert; //action must be taken immediately
    static const char* critical; //critical conditions
    static const char* error; //error conditions
    static const char* warning; //warning conditions
    static const char* notice; //normal but significant condition
    static const char* info; //informational
    static const char* debug; //debug/trace info for developers
};

class Message
{
    public:
        
        struct Mandatory_Fields
        {
            static const char* facility; //according to syslog
            static const char* priority; //same as severity for syslog
            static const char* timestamp; //ISO8601 timestamp with milliseconds and timezone
            static const char* hostname; //IP address or any specific sending device id, added by fplogd before sending
            static const char* text; //log message text
        };

        struct Optional_Fields
        {
            static const char* component; //package name or any logical software component
            static const char* class_name; //class name if OOP is used
            static const char* method; //method of a given class if OOP is used or just a function name
            static const char* module; //source file name
            static const char* line; //line number in the above mentioned source file
            static const char* options; //for example encryption method + options when encryption is in use
            static const char* encrypted; //true/false, if true then Text field contains encrypted JSON values - 
                                          //the rest of the log message including the decrypted version of Text field
            static const char* file; //filename when sending a file inside the log message
        };

        Message(const char* prio);
        void set_timestamp(const char* timestamp = 0); //either sets provided timestamp or uses current system date/time if timestamp is 0

        Message& add(const char* param_name, int param);
        Message& add(const char* param_name, long long param);
        Message& add(const char* param_name, double param);
        Message& add(const char* param_name, std::string& param);
        Message& add(const char* param_name, const char* param);
};

class Filter_Base
{
    public:

        Filter_Base(const char* filter_id) { if (filter_id) filter_id_ = filter_id; else filter_id_ = ""; }
        virtual bool should_pass(Message& msg) = 0;
        virtual ~Filter_Base();


    protected:

        std::string filter_id_; //just a human-readable name for the filter


    private:

        Filter_Base();
};

//You need to explicitly state messages of which priorities you need to log by using add/remove.
class Priority_Filter: public Filter_Base
{
    public:

        virtual bool should_pass(Message& msg);

        void add(const char* prio){ if (prio) prio_.insert(prio); }
        void remove(const char* prio) { if (prio){ std::set<std::string>::iterator it(prio_.find(prio)); if (it != prio_.end()) { prio_.erase(it); }}}


    private:

        std::set<std::string> prio_;
};

void openlog(const char* facility, Filter_Base* filter = 0);
void closelog();

void add_filter(Filter_Base* filter);
void remove_filter(Filter_Base* filter);
Filter_Base* find_filter(const char* filter_id);

void write(Message& msg);

};
