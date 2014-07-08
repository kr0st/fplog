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

class Filter_Base
{
    public:

        Filter_Base(const char* filter_id) { if (filter_id) filter_id_ = filter_id; else filter_id_ = ""; }
        virtual bool should_pass(JSONNode& msg) = 0;
        virtual ~Filter_Base();


    protected:

        std::string filter_id_;


    private:

        Filter_Base();
};

//You need to explicitly state messages of which priorities you need to log by using add/remove.
class Priority_Filter: public Filter_Base
{
    public:

        virtual bool should_pass(JSONNode& msg);

        void add(const char* prio){ if (prio) prio_.insert(prio); }
        void remove(const char* prio) { if (prio){ std::set<std::string>::iterator it(prio_.find(prio)); if (it != prio_.end()) { prio_.erase(it); }}}


    private:

        std::set<std::string> prio_;
};

void openlog(const char* facility, Filter_Base* filter = 0);
void closelog();

void add_filter(Filter_Base* filter);
void remove_filter(Filter_Base* filter);

void write(JSONNode& msg);

};
