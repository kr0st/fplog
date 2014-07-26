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

const char* Facility::system = "system"; //message from some system component
const char* Facility::user = "user"; //message from user-level component
const char* Facility::security = "security"; //security or authorization related message
const char* Facility::fplog = "fplog"; //message from fplog directly, could inform about log system malfunction

const char* Message::Mandatory_Fields::facility = "facility"; //according to syslog
const char* Message::Mandatory_Fields::priority = "priority"; //same as severity for syslog
const char* Message::Mandatory_Fields::timestamp = "timestamp"; //ISO8601 timestamp with milliseconds and timezone
const char* Message::Mandatory_Fields::hostname = "hostname"; //IP address or any specific sending device id, added by fplogd before sending
const char* Message::Mandatory_Fields::text = "text"; //log message text

const char* Message::Optional_Fields::component = "component"; //package name or any logical software component
const char* Message::Optional_Fields::class_name = "class"; //class name if OOP is used
const char* Message::Optional_Fields::method = "method"; //method of a given class if OOP is used or just a function name
const char* Message::Optional_Fields::module = "module"; //source file name
const char* Message::Optional_Fields::line = "line"; //line number in the above mentioned source file
const char* Message::Optional_Fields::options = "options"; //for example encryption method + options when encryption is in use
const char* Message::Optional_Fields::encrypted = "encrypted"; //true/false, if true then Text field contains encrypted JSON values - 
                                                               //the rest of the log message including the decrypted version of Text field
const char* Message::Optional_Fields::file = "file"; //filename when sending a file inside the log message

Message::Message(const char* prio, const char* text):
msg_(JSON_NODE)
{
    if (prio)
        msg_.push_back(JSONNode(Message::Mandatory_Fields::priority, prio));
    else
        msg_.push_back(JSONNode(Message::Mandatory_Fields::priority, Prio::debug));

    if (text)
        msg_.push_back(JSONNode(Message::Mandatory_Fields::text, text));
}

Message& Message::add(const char* param_name, int param)
{
    if (param_name)
        msg_.push_back(JSONNode(param_name, param));

    return *this;
}

Message& Message::add(const char* param_name, long long param)
{
    if (param_name)
        msg_.push_back(JSONNode(param_name, param));

    return *this;
}

Message& Message::add(const char* param_name, double param)
{
    if (param_name)
        msg_.push_back(JSONNode(param_name, param));

    return *this;
}

Message& Message::add(const char* param_name, std::string& param)
{
    if (param_name)
        msg_.push_back(JSONNode(param_name, param));

    return *this;
}

Message& Message::add(const char* param_name, const char* param)
{
    if (param_name && param)
        msg_.push_back(JSONNode(param_name, param));

    return *this;
}

Message& Message::add(JSONNode& param)
{    
    msg_.push_back(param);
    return *this;
}

std::string Message::as_string()
{
    return msg_.write();
}

JSONNode& Message::as_json()
{
    return msg_;
}

void write(Message& msg)
{
    printf("%s\n", msg.as_string().c_str());
}

bool Priority_Filter::should_pass(Message& msg)
{
    /*JSONNode::iterator it(msg.find("priority"));
    if (it != msg.end())
        return (prio_.find(it->as_string()) != prio_.end());*/

    return false;
}

};
