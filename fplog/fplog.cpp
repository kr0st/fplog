#include "fplog.h"
#include "utils.h"
#include <sprot/sprot.h>

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

const char* Message::Optional_Fields::text = "text"; //log message text
const char* Message::Optional_Fields::component = "component"; //package name or any logical software component
const char* Message::Optional_Fields::class_name = "class"; //class name if OOP is used
const char* Message::Optional_Fields::method = "method"; //method of a given class if OOP is used or just a function name
const char* Message::Optional_Fields::module = "module"; //source file name
const char* Message::Optional_Fields::line = "line"; //line number in the above mentioned source file
const char* Message::Optional_Fields::options = "options"; //for example encryption method + options when encryption is in use
const char* Message::Optional_Fields::encrypted = "encrypted"; //true/false, if true then Text field contains encrypted JSON values - 
                                                               //the rest of the log message including the decrypted version of Text field
const char* Message::Optional_Fields::file = "file"; //filename when sending a file inside the log message
const char* Message::Optional_Fields::/*the*/blob = "blob"; //used when attaching binary fields to the message, resulting JSON object will look
                                                            //like this: "blob_name":{ "blob":"xckjhKJSHDKDSdJKShdsdsgr=" }
                                                            //where "xckjhKJSHDKDSdJKShdsdsgr=" is base64 encoded binary object

Message::Message(const char* prio, const char *facility, const char* format, ...):
msg_(JSON_NODE)
{
    set_timestamp();
    set(Mandatory_Fields::priority, prio ? prio : Prio::debug);
    set(Mandatory_Fields::facility, facility ? facility : Facility::user);

    if (format)
    {
        va_list aptr;
        va_start(aptr, format);

        char buffer[2048] = {0};
        vsnprintf(buffer, sizeof(buffer) - 1, format, aptr);
        set_text(buffer);

        va_end(aptr);
    }
}

Message& Message::set_module(std::string& module)
{
    return set(Optional_Fields::module, module);
}

Message& Message::set_module(const char* module)
{
    return set(Optional_Fields::module, module);
}

Message& Message::set_line(int line)
{
    return set(Optional_Fields::line, line);
}

Message& Message::set_text(std::string& text)
{
    return set(Optional_Fields::text, text);
}

Message& Message::set_text(const char* text)
{
    return set(Optional_Fields::text, text);
}

Message& Message::set_class(std::string& class_name)
{
    return set(Optional_Fields::class_name, class_name);
}

Message& Message::set_class(const char* class_name)
{
    return set(Optional_Fields::class_name, class_name);
}

Message& Message::set_method(std::string& method)
{
    return set(Optional_Fields::method, method);
}

Message& Message::set_method(const char* method)
{
    return set(Optional_Fields::method, method);
}

Message& Message::add(JSONNode& param)
{
    if (is_valid(param))
        msg_.push_back(param);
    return *this;
}

bool Message::is_valid(JSONNode& param)
{
    return true;
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

Message& Message::set_timestamp(const char* timestamp)
{
    if (timestamp)
        return add(Mandatory_Fields::timestamp, timestamp);

    return add(Mandatory_Fields::timestamp, generic_util::get_iso8601_timestamp().c_str());
}

File::File(const char* prio, const char* name, const void* content, size_t size):
msg_(prio, Facility::user)
{
    if (!name)
        return;

    msg_.add(Message::Optional_Fields::file, name);

    if (size > 0)
    {
        size_t dest_len = generic_util::base64_encoded_length(size);
        buf_ = new char [dest_len + 1];
        memset(buf_, 0, dest_len + 1);
        generic_util::base64_encode(content, size, buf_, dest_len);

        msg_.set_text(buf_);

        delete [] buf_;
    }
}

Message& Message::add_binary(const char* param_name, const void* buf, size_t buf_size_bytes)
{
    if (!param_name || !buf || !buf_size_bytes)
        return *this;

    JSONNode blob(JSON_NODE);
    blob.set_name(param_name);

    size_t dest_len = generic_util::base64_encoded_length(buf_size_bytes);
    char* base64 = new char [dest_len + 1];
    memset(base64, 0, dest_len + 1);
    generic_util::base64_encode(buf, buf_size_bytes, base64, dest_len);

    blob.push_back(JSONNode("blob", base64));

    delete [] base64;

    return add(blob);
}

};
