#include "fplog.h"
#include "utils.h"
#include "shared_sequence_number.h"
#include <map>
#include <thread>
#include <queue>
#include <lua.hpp>
#include <spipc/socket_transport.h>
#include <sprot/sprot.h>
#include <mutex>
#include <chaiscript/chaiscript.hpp>
#include <chaiscript/chaiscript_stdlib.hpp>

namespace fplog
{

typedef std::map<std::string, std::shared_ptr<Filter_Base>> Filter_Map;

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
const char* Message::Optional_Fields::sequence = "sequence"; //sequence number that allows to prevent duplicate messages and also to tell
                                                             //which message was first even if timestamps are the same

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

FPLOG_API std::vector<std::string> g_test_results_vector;

class FPLOG_API Fplog_Impl
{
    public:

        Fplog_Impl():
        appname_("noname"),
        inited_(false),
        own_transport_(true),
        test_mode_(false),
        stopping_(false),
        transport_(0),
        mq_reader_(0)
        {
            Message::one_time_init();
        }

        ~Fplog_Impl()
        {
            stop_reading_queue();
            mq_reader_->join();

            std::lock_guard<std::recursive_mutex> lock(mutex_);

            delete mq_reader_;
            delete protocol_;

            if (inited_ && own_transport_)
                delete transport_;
        }

        const char* get_facility()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            return thread_log_settings_table_[std::this_thread::get_id().hash()].facility_.c_str();
        }

        void openlog(const char* facility, Filter_Base* filter)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (facility)
                thread_log_settings_table_[std::this_thread::get_id().hash()].facility_ = facility;
            else
                return;

            if (filter)
                add_filter(filter);
        }

        void initlog(const char* appname, const char* uid, fplog::Transport_Interface* transport)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (!mq_reader_)
                mq_reader_ = new std::thread(&Fplog_Impl::mq_reader, this);
            
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
                transport_ = new spipc::Socket_Transport();

                fplog::Transport_Interface::Params params;
                params["uid"] = uid;

                transport_->connect(params);
                protocol_ = new sprot::Protocol(transport_);

                inited_ = true;
            }
        }

        void closelog()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            std::map<unsigned long long, Logger_Settings>::iterator it = thread_log_settings_table_.find(std::this_thread::get_id().hash());
            if (it != thread_log_settings_table_.end())
                thread_log_settings_table_.erase(it);
        }

        void write(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (stopping_)
                return;

            msg.set(Message::Mandatory_Fields::appname, appname_);
            if (passed_filters(msg))
            {
                msg.add(Message::Optional_Fields::sequence, (long long)sequence_.read());

                if (test_mode_)
                    g_test_results_vector.push_back(msg.as_string());
                else
                    mq_.push(new std::string(msg.as_string()));
            }
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
            Logger_Settings settings = Logger_Settings(thread_log_settings_table_[std::this_thread::get_id().hash()]);
            settings.filter_id_ptr_map[filter_id] = std::shared_ptr<Filter_Base>(filter);

            thread_log_settings_table_[std::this_thread::get_id().hash()] = settings;
        }

        void remove_filter(Filter_Base* filter)
        {
            if (!filter)
                return;

            std::lock_guard<std::recursive_mutex> lock(mutex_);

            Logger_Settings settings(thread_log_settings_table_[std::this_thread::get_id().hash()]);

            std::string filter_id(filter->get_id());
            if (!filter_id.empty())
            {
                settings.filter_id_ptr_map.erase(settings.filter_id_ptr_map.find(filter_id));
                thread_log_settings_table_[std::this_thread::get_id().hash()] = settings;
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

            Logger_Settings settings(thread_log_settings_table_[std::this_thread::get_id().hash()]);
            std::map<std::string, std::shared_ptr<Filter_Base>>::iterator found(settings.filter_id_ptr_map.find(filter_id_trimmed));
            if (found != settings.filter_id_ptr_map.end())
                return found->second.get();

            return 0;
        }

        void set_test_mode(bool mode){ g_test_results_vector.clear(); test_mode_ = mode; }
        void wait_until_queues_are_empty()
        {
            int counter = 0;

            while (counter < 5)
            {
                bool q1_empty = true;

                {
                    std::lock_guard<std::recursive_mutex> lock_q1(mutex_);
                    q1_empty = mq_.empty();
                }

                if (q1_empty)
                {
                    counter++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                else
                {
                    counter = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }


    private:

        Shared_Sequence_Number sequence_;
        bool inited_;
        bool own_transport_;
        bool test_mode_;
        volatile bool stopping_;

        std::string appname_;

        std::queue<std::string*> mq_;
        std::thread* mq_reader_;

        struct Logger_Settings
        {
            Logger_Settings() : facility_(Facility::user) {}
            Filter_Map filter_id_ptr_map;
            std::string facility_;
        };

        std::map<unsigned long long, Logger_Settings> thread_log_settings_table_;

        std::recursive_mutex mutex_;
        std::recursive_mutex mq_reader_mutex_;

        fplog::Transport_Interface* transport_;
        sprot::Protocol* protocol_;

        void stop_reading_queue()
        {
            stopping_ = true;
            std::lock_guard<std::recursive_mutex> lock(mq_reader_mutex_);
        }
        
        void mq_reader()
        {
            std::lock_guard<std::recursive_mutex> queue_lock(mq_reader_mutex_);

            while(!stopping_)
            {
                
                std::string* str = 0;

                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                    if (!mq_.empty() && transport_)
                    {
                        str = mq_.front();
                        mq_.pop();
                    }
                }

                std::auto_ptr<std::string> str_ptr(str);

            retry:

                if (stopping_)
                    return;

                try
                {
                    if (str)
                        protocol_->write(str->c_str(), str->size(), 200);
                    else
                    {
                        if (stopping_)
                            return;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                }
                catch(fplog::exceptions::Generic_Exception)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    goto retry;
                }
            }
        }

        bool passed_filters(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            Logger_Settings settings(thread_log_settings_table_[std::this_thread::get_id().hash()]);
            
            if (settings.filter_id_ptr_map.size() == 0)
                return false;

            bool should_pass = true;

            for (std::map<std::string, std::shared_ptr<Filter_Base>>::iterator it = settings.filter_id_ptr_map.begin(); it != settings.filter_id_ptr_map.end(); ++it)
            {
                should_pass = (should_pass && it->second->should_pass(msg));
                if (!should_pass)
                    break;
            }

            return should_pass;
        }
};

FPLOG_API Fplog_Impl* g_fplog_impl = 0;
std::recursive_mutex g_api_mutex;

void write(Message& msg)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);
    
    if (!g_fplog_impl)
        return;
   
    g_fplog_impl->write(msg);
}

void initlog(const char* appname, const char* uid, fplog::Transport_Interface* transport)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        g_fplog_impl = new Fplog_Impl();

    return g_fplog_impl->initlog(appname, uid, transport);
}

void shutdownlog()
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    delete g_fplog_impl;
    g_fplog_impl = 0;
}

void openlog(const char* facility, Filter_Base* filter)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return;

    return g_fplog_impl->openlog(facility, filter);
}

void closelog()
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return;

    return g_fplog_impl->closelog();
}

const char* get_facility()
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return "";

    return g_fplog_impl->get_facility();
}

void add_filter(Filter_Base* filter)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return;

    return g_fplog_impl->add_filter(filter);
}

void remove_filter(Filter_Base* filter)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return;

    return g_fplog_impl->remove_filter(filter);
}

Filter_Base* find_filter(const char* filter_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_api_mutex);

    if (!g_fplog_impl)
        return 0;

    return g_fplog_impl->find_filter(filter_id);
}

class Lua_Filter::Lua_Filter_Impl
{
    public:

        Lua_Filter_Impl(const char* lua_script):
        lua_script_(lua_script)
        {
            init();
        }
        
        ~Lua_Filter_Impl()
        {
            deinit();
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
                deinit();
                init();
            }

            lua_getglobal(lua_state_, "filter_result");
            if (lua_isboolean(lua_state_, -1))
            {
                bool res = lua_toboolean(lua_state_, -1);
                return res;
            }

            return false;
        }


    private:

        bool inited_;

        lua_State *lua_state_;
        std::recursive_mutex mutex_;

        void init()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            lua_state_ = luaL_newstate();
            luaL_openlibs(lua_state_);

            luaL_dostring(lua_state_, "json = require(\"json\")\n");        

            inited_ = (lua_state_ != 0);
        }

        void deinit()
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

class Chai_Filter::Chai_Filter_Impl
{
    public:

        Chai_Filter_Impl(const char* chai_script):
        chai_script_(chai_script)
        {
            init();
        }
        
        ~Chai_Filter_Impl()
        {
            deinit();
        }

        bool should_pass(Message& msg)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            filter_result_ = false;

            std::string log_msg_escaped(msg.as_string());
            generic_util::escape_quotes(log_msg_escaped);

            std::string std_format("log_msg=\"%s\";\nfplog_message = from_json(log_msg);\n");
        
        retry_parsing:

            int chai_len = log_msg_escaped.length() + 256;
            
            char* chai_script = new char[chai_len];
            memset(chai_script, 0, chai_len);
            std::auto_ptr<char> chai_script_ptr(chai_script);

            _snprintf(chai_script, chai_len - 1, std_format.c_str(), log_msg_escaped.c_str());

            std::string full_script(chai_script + chai_script_);

            try
            {
                chai_->eval(full_script);
            }
            catch (chaiscript::exception::eval_error& e)
            {
                std_format = "var log_msg=\"%s\";\nvar fplog_message = from_json(log_msg);\n";
                goto retry_parsing;
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
                std::cout << full_script << std::endl;
            }

            return filter_result_;
        }


    private:

        bool filter_result_;
        std::recursive_mutex mutex_;
        chaiscript::ChaiScript* chai_;

        void init()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            chai_ = new chaiscript::ChaiScript(chaiscript::Std_Lib::library());
            chai_->add(chaiscript::var(std::ref(filter_result_)), "filter_result");
        }

        void deinit()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            delete chai_;
        }

        std::string chai_script_;
};

Chai_Filter::Chai_Filter(const char* filter_id, const char* chai_script):
Filter_Base(filter_id)
{
    impl_ = new Chai_Filter_Impl(chai_script);
}

bool Chai_Filter::should_pass(Message& msg)
{
    return impl_->should_pass(msg);
}

Chai_Filter::~Chai_Filter()
{
    delete impl_;
}

};
