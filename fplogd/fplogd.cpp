#include "targetver.h"
#include "fplogd.h"

#include <queue>
#include <mutex>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#ifdef WIN32
#include <Shlobj.h>
#endif

static char* g_lock_file_name = ".fplogd_lock";
static char* g_config_file_name = "fplogd.ini";

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
    file_lock flock(get_lock_file_name().c_str());
    if (flock.timed_lock(boost::get_system_time() + boost::posix_time::seconds(12)))
    {
        flock.unlock();
        return false;
    }

    return true;
}

std::string get_lock_file_name()
{
    return (get_home_dir() + std::string(g_lock_file_name));
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

std::vector<Channel_Data> get_registered_channels()
{
    using boost::property_tree::ptree;
    ptree pt;
    std::vector<Channel_Data> res;

    read_ini(get_home_dir() + g_config_file_name, pt);

    for (auto& section: pt)
    {
        Channel_Data data;

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
        overload_checker_(&Impl::overload_prevention, this)
        {
        };

        void start()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            should_stop_ = false;

            std::vector<Channel_Data> channels(get_registered_channels());
            for each (auto channel in channels)
            {
                Thread_Data* worker = new Thread_Data();
                
                worker->app_name = channel.app_name;
                worker->uid = channel.uid;
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

                if (mq_.size() > 0)
                {
                    mq_.pop();
                    str = mq_.front();
                    delete str;
                }
            } while (str);
        }


    private:

        struct Thread_Data
        {
            Thread_Data(): thread(0) {}

            std::thread* thread;
            spipc::UID uid;
            std::string app_name;
        };

        void ipc_listener(Thread_Data* data)
        {
            spipc::IPC ipc;
            ipc.connect(data->uid);

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
                catch(sprot::exceptions::Buffer_Overflow& e)
                {
                    buf_sz = e.get_required_size() + 1;
                    delete [] buf;
                    buf = new char [buf_sz];
                }
                catch(sprot::exceptions::Exception&)
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

        std::recursive_mutex mutex_;
        std::queue<std::string*> mq_;
        std::thread overload_checker_;
        std::vector<Thread_Data*> pool_;
        volatile bool should_stop_;
};

static Impl g_impl;

void start()
{
    g_impl.start();
}

void stop()
{
    g_impl.stop();
}

};
