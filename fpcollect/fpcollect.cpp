#include "targetver.h"
#include "fpcollect.h"
#include <fplogd/Transport_Factory.h>
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
static char* g_process_name = "fpcollect.exe";
#else
static char* g_process_name = "fpcollect";
#endif

static char* g_config_file_name = "fpcollect.ini";
static char* g_config_file_storage_section_name = "storage";
static char* g_config_file_connection_section_name = "connection_";

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

namespace fpcollect {

fplog::Transport_Interface::Params get_log_storage_config()
{
    using boost::property_tree::ptree;
    ptree pt;
    fplog::Transport_Interface::Params res;

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& section: pt)
    {
        if (section.first.find(g_config_file_storage_section_name) == std::string::npos)
            continue;

        for (auto& key: section.second)
        {
            fplog::Transport_Interface::Param param(key.first, key.second.get_value<std::string>());
            res.insert(param);
        }
    }

    return res;
}

std::vector<fplog::Transport_Interface::Params> get_connections()
{
    using boost::property_tree::ptree;
    ptree pt;
    std::vector<fplog::Transport_Interface::Params> res;

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& section: pt)
    {
        fplog::Transport_Interface::Params params;

        if (section.first.find(g_config_file_connection_section_name) == std::string::npos)
            continue;

        for (auto& key: section.second)
            params[key.first] = key.second.get_value<std::string>();

        res.push_back(params);
    }

    return res;
}

class Impl
{
    public:

        Impl():
        should_stop_(false),
        storage_(0),
        overload_checker_(&Impl::overload_prevention, this),
        mq_reader_(&Impl::mq_reader, this)
        {
        };

        void set_log_storage(fplog::Transport_Interface* transport)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            storage_ = transport;
        }

        void start()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            should_stop_ = false;

            std::vector<fplog::Transport_Interface::Params> params(get_connections());
            for each (auto param in params)
            {
                Thread_Data* worker = new Thread_Data();
                
                worker->params = param;
                worker->thread = new std::thread(&Impl::fplogd_listener, this, worker);
                
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

            delete storage_;
        }


    private:


        struct Thread_Data
        {
            Thread_Data(): thread(0) {}

            std::thread* thread;
            fplog::Transport_Interface::Params params;
        };

        void fplogd_listener(Thread_Data* data)
        {
            fplogd::Transport_Factory factory;
            fplog::Transport_Interface* transport = factory.create(data->params);

            //TODO: notify somehow that one of the connections is not working
            if (!transport)
                return;

            std::auto_ptr<fplog::Transport_Interface> autokill(transport);

            try
            {
                transport->connect(data->params);
            }
            catch(fplog::exceptions::Generic_Exception)
            {
                //TODO: notify that one of the connections failed and logs will not be coming in
                return;
            }

            sprot::Protocol protocol(transport);

            size_t buf_sz = 2048;
            char *buf = new char [buf_sz];

            while(true)
            {
                memset(buf, 0, buf_sz);

                try
                {
                    static std::string old_str;

                    protocol.read(buf, buf_sz - 1, 1000);

                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    
                    std::string new_str(buf);
                    
                    //TODO: this is for duplicates testing only, to rework
                    if (old_str.find(new_str) != std::string::npos)
                    {
                        printf("!!! DUPLICATE !!!\n");
                        continue;
                    }

                    mq_.push(new std::string(buf));

                    old_str = new_str;

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
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
                
                std::string* str = 0;

                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                    if (!mq_.empty() && storage_)
                    {
                        str = mq_.front();
                        mq_.pop();
                    }
                }

                std::auto_ptr<std::string> str_ptr(str);

            retry:

                try
                {
                    if (str)
                        storage_->write(str->c_str(), str->size(), 200);
                    else
                    {
                        if (should_stop_)
                            return;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }
                }
                catch(fplog::exceptions::Generic_Exception)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    goto retry;
                }
            }
        }

        std::recursive_mutex mutex_;
        std::queue<std::string*> mq_;

        std::thread overload_checker_;
        std::thread mq_reader_;

        volatile bool should_stop_;
        fplog::Transport_Interface* storage_;
        std::vector<Thread_Data*> pool_;
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

class Measure_Performance: public fplog::Transport_Interface
{
    public:

        Measure_Performance():
        first_write_(true),
        counter_(0)
        {
        }

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) { return 0; }
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
        {
            if (!buf || (buf_size <= 0))
                return 0;
            
            if (first_write_)
            {
                std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
                sys_time_absolute_ = (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();
                first_write_ = false;
            }

            counter_++;

            if (counter_ % 2500 == 0)
            {
                //Perf monitoring point
                std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));

                unsigned long long elapsed = (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count() - sys_time_absolute_;
                if (elapsed == 0)
                    return buf_size;
                unsigned long long mps = counter_ / elapsed;
                
                printf("Speed = %llu messages per second.\n", mps);
            }
            
            return buf_size;
        }


    private:

        unsigned long long counter_;
        bool first_write_;
        unsigned long long sys_time_absolute_;
};

static Impl g_impl;
static Console_Output g_storage;
//static Measure_Performance g_storage;

void start()
{
    g_impl.set_log_storage(&g_storage);
    g_impl.start();
}

void stop()
{
    g_impl.stop();
}

};

int main()
{
    fpcollect::start();
    _getch();
    fpcollect::stop();
}
