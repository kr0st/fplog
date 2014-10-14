#include "fplog.h"
#include "utils.h"

#include <map>
#include <thread>
#include <lua.hpp>


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
    JSONNode::iterator it(msg.as_json().find(fplog::Message::Mandatory_Fields::priority));
    if (it != msg.as_json().end())
        return (prio_.find(it->as_string()) != prio_.end());

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

            if (filter)
                add_filter(filter);
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

        void closelog()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            std::map<unsigned long long, Filter_Mapping_Entry> empty_table;
            thread_filters_table_.swap(empty_table);
        }

        void write(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            msg.set(Message::Mandatory_Fields::appname, appname_);
            if (passed_filters(msg))
                printf("%s\n", msg.as_string().c_str());
        }

        void add_filter(Filter_Base* filter)
        {
            if (!filter)
                return;

            std::string filter_id(filter->get_id());
            generic_util::trim(filter_id);
            if (filter_id.empty())
                return;

            std::lock_guard<std::recursive_mutex> lock(mutex_);
            Filter_Mapping_Entry mapping(thread_filters_table_[std::this_thread::get_id().hash()]);
            mapping.filter_id_ptr_map[filter_id] = filter;
            mapping.filter_ptr_id_map[filter] = filter_id;

            thread_filters_table_[std::this_thread::get_id().hash()] = mapping;
        }

        void remove_filter(Filter_Base* filter)
        {
            if (!filter)
                return;

            std::lock_guard<std::recursive_mutex> lock(mutex_);

            Filter_Mapping_Entry mapping(thread_filters_table_[std::this_thread::get_id().hash()]);
            std::string filter_id(mapping.filter_ptr_id_map[filter]);

            if (!filter_id.empty())
            {
                mapping.filter_ptr_id_map.erase(mapping.filter_ptr_id_map.find(filter));
                mapping.filter_id_ptr_map.erase(mapping.filter_id_ptr_map.find(filter_id));

                thread_filters_table_[std::this_thread::get_id().hash()] = mapping;
            }
        }

        Filter_Base* find_filter(const char* filter_id)
        {
            if (!filter_id)
                return 0;

            std::string filter_id_trimmed(filter_id);
            generic_util::trim(filter_id_trimmed);
            if (filter_id_trimmed.empty())
                return 0;

            std::lock_guard<std::recursive_mutex> lock(mutex_);

            Filter_Mapping_Entry mapping(thread_filters_table_[std::this_thread::get_id().hash()]);
            std::map<std::string, Filter_Base*>::iterator found(mapping.filter_id_ptr_map.find(filter_id_trimmed));
            if (found != mapping.filter_id_ptr_map.end())
                return found->second;

            return 0;
        }


    private:

        bool inited_;
        bool own_transport_;
        
        std::string appname_;
        std::string facility_;

        struct Filter_Mapping_Entry
        {
            std::map<Filter_Base*, std::string> filter_ptr_id_map;
            std::map<std::string, Filter_Base*> filter_id_ptr_map;
        };

        std::map<unsigned long long, Filter_Mapping_Entry> thread_filters_table_;

        std::recursive_mutex mutex_;
        fplog::Transport_Interface* transport_;

        bool passed_filters(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            Filter_Mapping_Entry mapping(thread_filters_table_[std::this_thread::get_id().hash()]);
            
            if (mapping.filter_ptr_id_map.size() == 0)
                return false;

            bool should_pass = true;

            for (std::map<Filter_Base*, std::string>::iterator it = mapping.filter_ptr_id_map.begin(); it != mapping.filter_ptr_id_map.end(); ++it)
            {
                should_pass = (should_pass && it->first->should_pass(msg));
                if (!should_pass)
                    break;
            }

            return should_pass;
        }
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
    return g_fplog_impl.closelog();
}

const char* get_facility()
{
    return g_fplog_impl.get_facility();
}

void add_filter(Filter_Base* filter)
{
    return g_fplog_impl.add_filter(filter);
}

void remove_filter(Filter_Base* filter)
{
    return g_fplog_impl.remove_filter(filter);
}

Filter_Base* find_filter(const char* filter_id)
{
    return g_fplog_impl.find_filter(filter_id);
}

class Lua_Filter::Lua_Filter_Impl
{
    public:

        Lua_Filter_Impl(const char* lua_script):
        lua_script_(lua_script)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            static bool inited_ = one_time_init();
        }

        bool should_pass(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            std::string log_msg_escaped(msg.as_string());
            generic_util::escape_quotes(log_msg_escaped);

            const char* format = "log_msg=\"%s\"\nfplog_message = json.decode(log_msg)\n";
            int lua_len = log_msg_escaped.length() + 256;
            
            char* lua_script = new char[lua_len];
            memset(lua_script, 0, lua_len);
            std::auto_ptr<char> lua_script_ptr(lua_script);

            _snprintf(lua_script, lua_len - 1, format, log_msg_escaped.c_str());

            std::string full_script(lua_script + lua_script_);
            luaL_dostring(lua_state_, full_script.c_str());

            std::string lua_error(get_lua_error());
            if (!lua_error.empty())
            {
                printf("lua_err = %s\n", lua_error.c_str());

                one_time_deinit();
                one_time_init();
            }

            //TODO: real implementation here - need to eval lua script result
            return true;
        }


    private:

        static bool inited_;
        class Static_Destructor
        {
            public:

                ~Static_Destructor()
                {
                    Lua_Filter_Impl::one_time_deinit();
                }
        };

        static Static_Destructor static_destructor_;
        static lua_State *lua_state_;
        static std::recursive_mutex mutex_;

        Lua_Filter_Impl();

        bool one_time_init()
        {
            if (lua_state_ != 0)
                return true;

            lua_state_ = luaL_newstate();
            luaL_openlibs(lua_state_);

            luaL_dostring(lua_state_, "json = require(\"json\")\n");        

            return (lua_state_ != 0);
        }

        static void one_time_deinit()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if(lua_state_) lua_close(lua_state_);
            lua_state_ = 0;
        }

        std::string get_lua_error()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (lua_type(lua_state_, 1) == LUA_TSTRING)
            {
                const char* status = lua_tostring(lua_state_, -1);
                std::string err(status ? status : "");
                lua_pop(lua_state_, 1);
                return err;
            }

            return "";
        }

        std::string lua_script_;
};

lua_State* Lua_Filter::Lua_Filter_Impl::lua_state_ = 0;
std::recursive_mutex Lua_Filter::Lua_Filter_Impl::mutex_;
Lua_Filter::Lua_Filter_Impl::Static_Destructor Lua_Filter::Lua_Filter_Impl::static_destructor_;

Lua_Filter::Lua_Filter(const char* filter_id, const char* lua_script):
Filter_Base(filter_id)
{
    impl_ = new Lua_Filter_Impl(lua_script);
}

bool Lua_Filter::should_pass(Message& msg)
{
    return impl_->should_pass(msg);
}

Lua_Filter::~Lua_Filter()
{
    delete impl_;
}

};
