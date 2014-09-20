#include "fplog.h"
#include "utils.h"
#include <mutex>

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
const char* Message::Mandatory_Fields::appname = "appname"; //name of application or service using this logging library, needed for fplog IPC

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
const char* Message::Optional_Fields::warning = "warning"; //contains warning for the user in case there was an issue with this specific log message

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
    {
        JSONNode::iterator it(msg_.find_nocase(param.name()));
        if (it != msg_.end())
            *it=param;
        else
            msg_.push_back(param);
    }

    return *this;
}

bool Message::is_valid(JSONNode& param)
{
    if (!validate_params_)
        return true;

    for(JSONNode::iterator it(param.begin()); it != param.end(); ++it)
    {
        if (!is_valid(*it))
            return false;

        std::string lowercased(it->name());
        generic_util::trim(lowercased);
        std::transform(lowercased.begin(), lowercased.end(), lowercased.begin(), ::tolower);

        bool invalid(std::find(reserved_names_.begin(), reserved_names_.end(), lowercased) != reserved_names_.end());
        if (invalid)
        {
            set(Optional_Fields::warning, "Some parameters are missing from this log message because they were malformed.");
            return false;
        }
    }

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
        return set(Mandatory_Fields::timestamp, timestamp);

    return set(Mandatory_Fields::timestamp, generic_util::get_iso8601_timestamp().c_str());
}

Message& Message::set_file(const char* name)
{
    if (name)
        return set(Optional_Fields::file, name);

    return *this;
}

File::File(const char* prio, const char* name, const void* content, size_t size):
msg_(prio, Facility::user)
{
    if (!name)
        return;

    msg_.set_file(name);

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

    validate_params_ = false;
            
    try
    {
        add(blob);
    }
    catch(std::exception& e)
    {
        validate_params_ = true;
        throw e;
    }
    catch(fplog::exceptions::Generic_Exception& e)
    {
        validate_params_ = true;
        throw e;
    }

    validate_params_ = true;

    return *this;
}

void Message::one_time_init()
{
    reserved_names_.push_back(Mandatory_Fields::appname);
    reserved_names_.push_back(Mandatory_Fields::facility);
    reserved_names_.push_back(Mandatory_Fields::hostname);
    reserved_names_.push_back(Mandatory_Fields::priority);
    reserved_names_.push_back(Mandatory_Fields::timestamp);

    reserved_names_.push_back(Optional_Fields::blob);
    reserved_names_.push_back(Optional_Fields::class_name);
    reserved_names_.push_back(Optional_Fields::component);
    reserved_names_.push_back(Optional_Fields::encrypted);
    reserved_names_.push_back(Optional_Fields::file);
    reserved_names_.push_back(Optional_Fields::method);
    reserved_names_.push_back(Optional_Fields::line);
    reserved_names_.push_back(Optional_Fields::module);
    reserved_names_.push_back(Optional_Fields::options);
    reserved_names_.push_back(Optional_Fields::text);
    reserved_names_.push_back(Optional_Fields::warning);
}

std::vector<std::string> Message::reserved_names_;

/************************* fplog client API implementation *************************/

class Fplog_Impl
{
    public:

        Fplog_Impl():
        appname_("noname"),
        facility_(Facility::user),
        inited_(false),
        own_transport_(true)
        {
            Message::one_time_init();
        }

        const char* get_facility()
        {
            return facility_.c_str();
        }

        void openlog(const char* facility, Filter_Base* filter)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (facility)
                facility_ = facility;
            else
                return;
        }

        void initlog(const char* appname, fplog::Transport_Interface* transport)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (inited_)
                return;

            if (appname)
                appname_ = appname;
            else
                return;

            if (transport)
            {
                own_transport_ = false;
                transport_ = transport;
            }
            else
            {
                own_transport_ = true;
            }
        }

        void write(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            msg.set(Message::Mandatory_Fields::appname, appname_);
            printf("%s\n", msg.as_string().c_str());
        }


    private:

        
        bool inited_;
        bool own_transport_;
        
        std::string appname_;
        std::string facility_;

        std::recursive_mutex mutex_;
        fplog::Transport_Interface* transport_;
};

static Fplog_Impl g_fplog_impl;


void write(Message& msg)
{
    g_fplog_impl.write(msg);
}

void initlog(const char* appname, fplog::Transport_Interface* transport)
{
    return g_fplog_impl.initlog(appname, transport);
}

void openlog(const char* facility, Filter_Base* filter)
{
    return g_fplog_impl.openlog(facility, filter);
}

void closelog()
{
}

const char* get_facility()
{
    return g_fplog_impl.get_facility();
}

};
