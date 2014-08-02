#pragma once

#include <string>
#include <set>
#include <libjson/libjson.h>
#include <typeinfo>

#ifdef FPLOG_EXPORT
#define FPLOG_API __declspec(dllexport)
#else
#define FPLOG_API __declspec(dllimport)
#endif

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

#define CLASSNAME typeid(*this).name()


namespace fplog
{

struct FPLOG_API Facility
{
    static const char* system; //message from some system component
    static const char* user; //message from user-level component
    static const char* security; //security or authorization related message
    static const char* fplog; //message from fplog directly, could inform about log system malfunction
};

struct FPLOG_API Prio
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

class FPLOG_API Message
{
    public:
        
        struct Mandatory_Fields
        {
            static const char* facility; //according to syslog
            static const char* priority; //same as severity for syslog
            static const char* timestamp; //ISO8601 timestamp with milliseconds and timezone
            static const char* hostname; //IP address or any specific sending device id, added by fplogd before sending
        };

        struct Optional_Fields
        {
            static const char* text; //log message text
            static const char* component; //package name or any logical software component
            static const char* class_name; //class name if OOP is used
            static const char* method; //method of a given class if OOP is used or just a function name
            static const char* module; //source file name
            static const char* line; //line number in the above mentioned source file
            static const char* options; //for example encryption method + options when encryption is in use
            static const char* encrypted; //true/false, if true then Text field contains encrypted JSON values - 
                                          //the rest of the log message including the plaintext version of Text field
            static const char* file; //filename when sending a file inside the log message
        };

        Message(const char* prio, const char *facility, const char* format = 0, ...);

        void set_timestamp(const char* timestamp = 0); //either sets provided timestamp or uses current system date/time if timestamp is 0

        Message& add(const char* param_name, int param){ return add<int>(param_name, param); }
        Message& add(const char* param_name, long long param){ return add<long long>(param_name, param); }
        Message& add(const char* param_name, double param){ return add<double>(param_name, param); }
        Message& add(const char* param_name, std::string& param){ return add<std::string>(param_name, param); }
        Message& add(const char* param_name, const char* param){ return add<const char*>(param_name, param); }
        Message& add(JSONNode& param);

        Message& set_text(std::string& text);
        Message& set_text(const char* text);

        Message& set_class(std::string& class_name);
        Message& set_class(const char* class_name);

        std::string as_string();
        JSONNode& as_json();


    private:

        template <typename T> Message& set(const char* param_name, T param)
        {
            validate_params_ = false;
            try
            {
                add(param_name, param);
            }
            catch(std::exception& e)
            {
                validate_params_ = true;
                throw e;
            }
            catch(sprot::exceptions::Exception& e)
            {
                validate_params_ = true;
                throw e;
            }
        }

        template <typename T> bool is_valid(const char* param_name, T param)
        {
            if (!validate_params_)
                return true;

            return true;
        }

        template <typename T> Message& add(const char* param_name, T param)
        {
            if (param_name && is_valid<T>(param_name, param))
                msg_.push_back(JSONNode(param_name, param));

            return *this;
        }

        JSONNode msg_;
        bool validate_params_;
};

class FPLOG_API Filter_Base
{
    public:

        Filter_Base(const char* filter_id) { if (filter_id) filter_id_ = filter_id; else filter_id_ = ""; }
        virtual bool should_pass(Message& msg) = 0;


    protected:

        std::string filter_id_; //just a human-readable name for the filter


    private:

        Filter_Base();
};

//You need to explicitly state messages of which priorities you need to log by using add/remove.
class FPLOG_API Priority_Filter: public Filter_Base
{
    public:

        virtual bool should_pass(Message& msg);

        void add(const char* prio){ if (prio) prio_.insert(prio); }
        void remove(const char* prio) { if (prio){ std::set<std::string>::iterator it(prio_.find(prio)); if (it != prio_.end()) { prio_.erase(it); }}}


    private:

        std::set<std::string> prio_;
};

FPLOG_API void openlog(const char* facility, Filter_Base* filter = 0);
FPLOG_API void closelog();

FPLOG_API void add_filter(Filter_Base* filter);
FPLOG_API void remove_filter(Filter_Base* filter);
FPLOG_API Filter_Base* find_filter(const char* filter_id);

FPLOG_API void write(Message& msg);
};
