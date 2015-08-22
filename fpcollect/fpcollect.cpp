#include "targetver.h"
#include "fpcollect.h"

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

fplog::Transport_Interface::Params get_log_transort_config()
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

        }

        void stop()
        {
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                should_stop_ = true;
            }

        }


    private:


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
        }

        std::recursive_mutex mutex_;
        std::queue<std::string*> mq_;

        std::thread overload_checker_;
        std::thread mq_reader_;

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
