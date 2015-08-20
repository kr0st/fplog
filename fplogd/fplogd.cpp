#include "targetver.h"
#include "fplogd.h"
#include "Transport_Factory.h"

#include <stdio.h>
#include <conio.h>
#include <queue>
#include <mutex>
#include <fplog_exceptions.h>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>


#ifdef WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <Shlobj.h>
#endif

#ifdef WIN32
static char* g_process_name = "fplogd.exe";
#else
static char* g_process_name = "fplogd";
#endif

static char* g_config_file_name = "fplogd.ini";
static char* g_config_file_channels_section_name = "channels";
static char* g_config_file_transport_section_name = "transport";

/*!
\brief Check if a process is running
\param [in] processName Name of process to check if is running
\returns \c True if the process is running, or \c False if the process is not running
*/
bool IsProcessRunning(const char *processName)
{
#ifdef WIN32
    bool exists = false;
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry))
        while (Process32Next(snapshot, &entry))
            if (!_stricmp(entry.szExeFile, processName))
                exists = true;

    CloseHandle(snapshot);
    return exists;
#else
    return false;
#endif
}

static std::string get_home_dir()
{
#ifdef WIN32
    char path[MAX_PATH + 3];
    memset(path, 0, sizeof(path));

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) 
    {
        path[strlen(path)] = '\\';
        return path;
    }
    else
        return "./";
#else
    return "~/";
#endif
}

using namespace boost::interprocess;

namespace fplogd {

bool is_started()
{
    return IsProcessRunning(g_process_name);
}

class Notification_Helper
{
    public:

        void start_stop_notification(){ callback_(); delete this; }
        void set_callback(Start_Stop_Notification callback) { callback_ = callback; }

    private:

        Start_Stop_Notification callback_;
};

void notify_when_started(Start_Stop_Notification callback)
{
    Notification_Helper *helper = new Notification_Helper();
    helper->set_callback(callback);
    notify_when_started<Notification_Helper>(&Notification_Helper::start_stop_notification, helper);
}

void notify_when_stopped(Start_Stop_Notification callback)
{
    Notification_Helper *helper = new Notification_Helper();
    helper->set_callback(callback);
    notify_when_stopped<Notification_Helper>(&Notification_Helper::start_stop_notification, helper);
}

fplog::Transport_Interface::Params get_log_transort_config()
{
    using boost::property_tree::ptree;
    ptree pt;
    fplog::Transport_Interface::Params res;

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& section: pt)
    {
        if (section.first.find(g_config_file_transport_section_name) == std::string::npos)
            continue;

        for (auto& key: section.second)
        {
            fplog::Transport_Interface::Param param(key.first, key.second.get_value<std::string>());
            res.insert(param);
        }
    }

    return res;
}

std::vector<Channel_Data> get_registered_channels()
{
    using boost::property_tree::ptree;
    ptree pt;
    std::vector<Channel_Data> res;

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& section: pt)
    {
        Channel_Data data;

        if (section.first.find(g_config_file_channels_section_name) == std::string::npos)
            continue;

        for (auto& key: section.second)
        {
            data.uid.from_string(key.second.get_value<std::string>());
            data.app_name = key.first;
            res.push_back(data);
        }
    }

    return res;
}

class Impl
{
    public:

        Impl():
        should_stop_(false),
        log_transport_(0),
        overload_checker_(&Impl::overload_prevention, this),
        mq_reader_(&Impl::mq_reader, this)
        {
        };

        void set_log_transport(fplog::Transport_Interface* transport)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            log_transport_ = transport;
        }

        void start()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            should_stop_ = false;

            std::vector<Channel_Data> channels(get_registered_channels());
            for each (auto channel in channels)
            {
                Thread_Data* worker = new Thread_Data();
                
                worker->app_name = channel.app_name;
                worker->uid = channel.uid.to_string(channel.uid);
                worker->thread = new std::thread(&Impl::ipc_listener, this, worker);
                
                pool_.push_back(worker);
            }
        }

        void stop()
        {
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                should_stop_ = true;
            }

            join_all_threads();

            std::string* str = 0;
            do
            {
                str = 0;

                if (!mq_.empty())
                {
                    str = mq_.front();
                    mq_.pop();
                    delete str;
                }
            } while (str);

            delete log_transport_;
        }


    private:

        struct Thread_Data
        {
            Thread_Data(): thread(0) {}

            std::thread* thread;
            std::string uid;
            std::string app_name;
        };

        void ipc_listener(Thread_Data* data)
        {
            spipc::IPC ipc;
            spipc::IPC::Params params;
            params["uid"] = data->uid;
            ipc.connect(params);

            size_t buf_sz = 2048;
            char *buf = new char [buf_sz];

            while(true)
            {
                memset(buf, 0, buf_sz);

                try
                {
                    ipc.read(buf, buf_sz - 1, 1000);

                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    mq_.push(new std::string(buf));

                    if (buf_sz > 2048)
                    {
                        buf_sz = 2048;
                        delete [] buf;
                        buf = new char [buf_sz];
                    }
                }
                catch(fplog::exceptions::Buffer_Overflow& e)
                {
                    buf_sz *= 2;
                    delete [] buf;
                    buf = new char [buf_sz];
                }
                catch(fplog::exceptions::Generic_Exception&)
                {
                    //TODO: handle exceptions (send special log message to queue originating from fplog)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }

                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    if (should_stop_)
                    {
                        delete [] buf;
                        return;
                    }
                }
            }
        }

        void join_all_threads()
        {
            overload_checker_.join();
            mq_reader_.join();

            for each (auto worker in pool_)
            {
                if (!worker)
                    continue;

                if (worker->thread)
                    worker->thread->join();
                
                delete worker->thread;
                delete worker;
            }

            pool_.clear();
        }

        //TODO: need real implementation
        void overload_prevention()
        {
            while(true)
            {
                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    if (should_stop_)
                        return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        void mq_reader()
        {
            while(true)
            {
                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    if (should_stop_)
                        return;
                }

                if (!mq_.empty())
                {
                    std::string* str = mq_.front();
                    std::auto_ptr<std::string> str_ptr(str);
                    mq_.pop();

                    //TODO: exception handling!
                    if (log_transport_)
                        log_transport_->write(str->c_str(), str->size() + 1, 200);
                }
                else
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        std::recursive_mutex mutex_;
        std::queue<std::string*> mq_;

        std::thread overload_checker_;
        std::thread mq_reader_;

        std::vector<Thread_Data*> pool_;
        volatile bool should_stop_;
        fplog::Transport_Interface* log_transport_;
};

class Console_Output: public fplog::Transport_Interface
{
    public:

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) { return 0; }
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
        {
            if (buf && (buf_size > 0))
                printf("%s\n", buf);
            return buf_size;
        }
};

static Impl g_impl;

void start()
{
    Transport_Factory factory;
    fplog::Transport_Interface::Params params(get_log_transort_config());
    fplog::Transport_Interface* trans = factory.create(params);
    if (trans)
    {
        trans->connect(params);
        g_impl.set_log_transport(trans);
        g_impl.start();
    }
}

void stop()
{
    g_impl.stop();
}

};

int main()
{
    fplogd::start();
    _getch();
    fplogd::stop();
}
