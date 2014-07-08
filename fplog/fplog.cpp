#include "fplog.h"

namespace fplog
{

const char* Prio::emergency = "emergency"; //system is unusable
const char* Prio::alert = "alert"; //action must be taken immediately
const char* Prio::critical = "critical"; //critical conditions
const char* Prio::error = "error"; //error conditions
const char* Prio::warning = "warning"; //warning conditions
const char* Prio::notice = "notice"; //normal but significant condition
const char* Prio::info = "info"; //informational
const char* Prio::debug = "debug"; //debug/trace info for developers

bool Priority_Filter::should_pass(JSONNode& msg)
{
    JSONNode::iterator it(msg.find("priority"));
    if (it != msg.end())
        return (prio_.find(it->as_string()) != prio_.end());

    return false;
}

};
