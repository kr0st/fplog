#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef _LINUX
#include "targetver.h"
#endif

#include <string>
#include "fplogd.h"
#include "../fplog/fplog.h"
#include <libjson/libjson.h>
#include "Transport_Factory.h"
#include "Queue_Controller.h"

#include <fstream> 
#include <stdio.h>

#ifndef _LINUX
#include <conio.h>
#else

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int _getch() {
  struct termios oldt,
                 newt;
  int            ch;
  tcgetattr( STDIN_FILENO, &oldt );
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
  ch = getchar();
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
  return ch;
}

#endif

#include <queue>
#include <mutex>
#include <fplog_exceptions.h>
#include <utils.h>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>


#ifdef WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <Shlobj.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#define MAX_PATH 255
#endif

#ifdef WIN32
static char* g_process_name = "fplogd.exe";
#else
static char* g_process_name = "fplogd";

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#endif

static char* g_config_file_name = "fplogd.ini";

static char* g_emergency_config_setting_name = "log_file";
static char* g_emergency_log_file_name = "fplogd.log";

static char* g_config_file_channels_section_name = "channels";
static char* g_config_file_transport_section_name = "transport";
static char* g_config_file_misc_section_name = "misc";

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
 
    FILE* proc = 0;
    std::string cmd;
    cmd = "ps cax | grep " + std::string(processName);

    char line[256];
    memset(line, 0, sizeof(line));

    proc = popen(cmd.c_str(), "r");
  
    if (proc != 0)
    {
        if (fgets(line, 256, proc))
        {
            pclose(proc);
            std::string str(line);
            return (str.find(processName) != std::string::npos);
        }
        else
            pclose(proc);
    }

    return false;
#endif
}

static std::string get_home_dir()
{
    char path[MAX_PATH + 3];
    memset(path, 0, sizeof(path));
#ifdef WIN32
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) 
    {
        path[strlen(path)] = '\\';
        return path;
    }
    else
        return "./";
#else
    char *home = getenv("HOME");
    if (home)
        sprintf(path, "%s", home);
    else
    {
        struct passwd *pw = getpwuid(getuid());
        sprintf(path, "%s", pw->pw_dir);
    }
    if (path[strlen(path) - 1] != '/')
        path[strlen(path)] = '/';

    return path;
#endif
}

#ifndef _LINUX
void GetPrimaryIp(char* buffer, size_t buflen) 
{
    WSADATA wsaData;
    if (WSAStartup(0x202, &wsaData))
        THROW(fplog::exceptions::Connect_Failed);

    assert(buflen >= 16);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock != -1);

    const char* kGoogleDnsIp = "8.8.8.8";
    uint16_t kDnsPort = 53;
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(kDnsPort);

    int err = connect(sock, (const sockaddr*) &serv, sizeof(serv));
    assert(err != -1);

    sockaddr_in name;
    int namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*) &name, &namelen);
    assert(err != -1);

    const char* p = InetNtopA(AF_INET, &name.sin_addr, buffer, buflen);
    assert(p);

    closesocket(sock);

    WSACleanup();
}
#else

void GetPrimaryIp(char* buffer, size_t buflen) 
{
    assert(buflen >= 16);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock != -1);

    const char* kGoogleDnsIp = "8.8.8.8";
    uint16_t kDnsPort = 53;
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(kDnsPort);

    int err = connect(sock, (const sockaddr*) &serv, sizeof(serv));
    assert(err != -1);

    sockaddr_in name;
    int namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*) &name, &namelen);
    assert(err != -1);

    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, buflen);
    assert(p);

    shutdown(sock, SHUT_RDWR);
    close(sock);
}

#endif

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

fplog::Transport_Interface::Params get_misc_config()
{
    using boost::property_tree::ptree;
    ptree pt;
    fplog::Transport_Interface::Params res;

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& section: pt)
    {
        if (section.first.find(g_config_file_misc_section_name) == std::string::npos)
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
            std::string str_uid(key.second.get_value<std::string>());
            data.uid.from_string(str_uid);
            data.app_name = key.first;
            res.push_back(data);
        }
    }

    return res;
}

std::string get_config_key_value(const std::string& section, const std::string& key)
{
    using boost::property_tree::ptree;
    ptree pt;
    std::string res = "";

    std::string home(get_home_dir());
    read_ini(home + g_config_file_name, pt);

    for (auto& ini_section : pt)
    {
        if (ini_section.first.find(section) == std::string::npos)
            continue;

        for (auto& ini_key : ini_section.second)
        {
            std::string trimmed_key = ini_key.first;
            if (generic_util::find_str_no_case(generic_util::trim(trimmed_key), key))
            {
                res = ini_key.second.get_value<std::string>();
                break;
            }
        }
    }

    return res;
}

std::string get_log_error_file_full_path()
{
    std::string emergency_log_path = get_config_key_value(g_config_file_misc_section_name, g_emergency_config_setting_name);
    
    if (emergency_log_path.empty())
        emergency_log_path = get_home_dir() + g_emergency_log_file_name;

    return emergency_log_path;
}

class Impl
{
    public:

        Impl():
        should_stop_(false),
        log_transport_(0),
        overload_checker_(&Impl::overload_prevention, this),
        mq_reader_(&Impl::mq_reader, this),
        protocol_(0)
        {
        };

        void set_log_transport(fplog::Transport_Interface* transport, fplog::Transport_Interface* protocol)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            log_transport_ = transport;
            protocol_ = protocol;
        }

        void start()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            should_stop_ = false;
            batch_size_ = 30;

            fplog::Transport_Interface::Params misc(get_misc_config());
            for (auto& param : misc)
            {
                if (generic_util::find_str_no_case(param.first, "batch_size"))
                {
                    int batch_sz = std::stoi(param.second);
                    
                    if ((batch_sz > 0) && (batch_sz < 1000))
                        batch_size_ = batch_sz;
                }
                
                if (generic_util::find_str_no_case(param.first, "hostname"))
                {
                    hostname_ = "auto";

                    if (generic_util::find_str_no_case(param.second, "auto"))
                    {
                        char ip_str[255], comp_name[255];
                        memset(ip_str, 0, sizeof(ip_str));
                        memset(comp_name, 0, sizeof(comp_name));

                        GetPrimaryIp(ip_str, sizeof(ip_str));
                        unsigned long name_len = sizeof(comp_name);
                        gethostname(comp_name, name_len);

                        if (std::string(ip_str).empty() && std::string(comp_name).empty())
                        {
                            THROWM(fplog::exceptions::Incorrect_Parameter, "Cannot resolve hostname, please set it manually in ini file.");
                        }
                        else
                            hostname_ = std::string(comp_name) + "/" + std::string(ip_str);
                    }
                    else
                        hostname_ = param.second;

                    if (hostname_.compare("auto") == 0)
                    {
                        THROWM(fplog::exceptions::Incorrect_Parameter, "Cannot resolve hostname, please set it manually in ini file.");
                    }
                }
            }

            std::vector<Channel_Data> channels(get_registered_channels());
            for (auto channel : channels)
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

            delete protocol_;
            delete log_transport_;
        }


    private:

        std::string hostname_;
        int batch_size_;

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

            params["type"] = "ip";
            params["ip"] = "127.0.0.1";
            params["uid"] = data->uid;
            
            ipc.connect(params);

            size_t buf_sz = 2048;
            char *buf = new char [buf_sz];
            std::string emergency_log_file_path = get_log_error_file_full_path();

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
                        delete[] buf;
                        buf = new char[buf_sz];
                    }
                }
                catch(fplog::exceptions::Buffer_Overflow&)
                {
                    buf_sz *= 2;
                    delete [] buf;
                    buf = new char [buf_sz];
                }
                catch(fplog::exceptions::Timeout&)
                {
                    {
                        std::lock_guard<std::recursive_mutex> lock(mutex_);
                        if (should_stop_)
                        {
                            delete [] buf;
                            return;
                        }
                    }
                    continue;
                }
                catch(fplog::exceptions::Generic_Exception& e)
                {
                    fplog::Message error_msg = FPL_ERROR((std::string("Error from IPC: %s") + std::string(", app = ") + data->app_name + std::string(", uid = ") + data->uid).c_str(),
                        e.what().c_str()).set(fplog::Message::Mandatory_Fields::appname, "fplogd").add(fplog::Message::Optional_Fields::sequence, 0).set(fplog::Message::Mandatory_Fields::facility, fplog::Facility::fplog);
                    std::string error_str = error_msg.as_string();
                    append_hostname(&error_str);

                    if (log_transport_ && protocol_)
                    {
                        try
                        {
                            protocol_->write(error_str.c_str(), error_str.size() + 1, 200);
                        }
                        catch(...)
                        {
                            std::ofstream file(emergency_log_file_path, std::ios::app);
                            if (file.is_open())
                            {
                                file << error_str + "\n";
                                file.close();
                            }
                        }
                    }

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

            for (auto worker : pool_)
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

        void append_hostname(std::string* str)
        {
            if (!str)
                return;

            //Hackish way of adding json representation of hostname to the log message.
            int pos = str->rfind('}');
            if ((pos > 0) && (pos != std::string::npos))
            {
                (*str)[pos] = ',';
                (*str) += "\"hostname\":\"";
                (*str) += hostname_;
                (*str) += "\"}";
            }
        }

        void mq_reader()
        {
            std::string emergency_log_file_path = get_log_error_file_full_path();
            std::vector<std::string*> batch;

            size_t batch_flush_counter = 0;
            
            while(true)
            {
                std::string* str = 0;
                bool send_batch = false;

                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    if (should_stop_)
                        return;

                    while (!mq_.empty() && (batch.size() < batch_size_) && !send_batch)
                    {
                        str = mq_.front();
                        
                        //This is needed for sending larger messages - large messages are sent independently,
                        //separate from the batch, i.e. large message cannot be part of the batch along with other messages
                        //because in that case batch byte size could become too great to be optimal for sending over any transport.
                        if (str->length() >= (batch_size_ * 300 / 2))
                        {
                            if (batch.size() == 0)
                            {
                                send_batch = true;
                                mq_.pop();
                                batch.push_back(str);
                            }
                            else
                            {
                                send_batch = true;
                            }
                        }
                        else
                        {
                            mq_.pop();
                            batch.push_back(str);
                        }
                    }
                }

                if ((batch.size() < batch_size_) && !send_batch)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                    if (batch.size() > 0)
                        batch_flush_counter++;
                    
                    if (batch_flush_counter < 300)
                        continue;
                    else
                        batch_flush_counter = 0; //we are sending messages even despite the fact that we do not have a full batch
                        //this is done in order to prevent situations when some messages are not sent because they were last and thus stuck in the queue
                }
                else
                    batch_flush_counter = 0;

                JSONNode json_batch(JSON_ARRAY);
                for (auto item: batch)
                    json_batch.push_back(fplog::Message(*item).as_json());

                str = new std::string(fplog::Message(fplog::Prio::critical, "fplog").add_batch(json_batch).as_string());

                if (str)
                {
                    {
                        std::vector<std::string*> empty_batch;
                        
                        for (auto item: batch)
                            delete item;

                        batch.swap(empty_batch);
                    }

                    std::auto_ptr<std::string> str_ptr(str);
                    int retries = 5;
                    
                    append_hostname(str);

                retry:

                    try
                    {
                        if (log_transport_ && protocol_)
                        {
                            size_t timeout = 400;
                            size_t str_len = str->size();

                            protocol_->write(str->c_str(), str_len + 1, timeout * (1 + str_len / 3096)) * (6 - retries);
                        }
                        else
                        {
                            THROW(fplog::exceptions::Transport_Missing);
                        }
                    }
                    catch(fplog::exceptions::Generic_Exception& e)
                    {
                        if (retries <= 0)
                        {
                            fplog::Message error_msg = FPL_ERROR(std::string("Error: " + e.what() + " Log message:" + *str).c_str()).set(fplog::Message::Mandatory_Fields::appname, "fplogd").add(fplog::Message::Optional_Fields::sequence, 0).set(fplog::Message::Mandatory_Fields::facility, fplog::Facility::fplog);
                            std::string error_str = error_msg.as_string();
                            append_hostname(&error_str);

                            int retries_error = 5;

                        retry_error:

                            try
                            {
                                if (log_transport_ && protocol_)
                                {
                                    protocol_->write(error_str.c_str(), error_str.size() + 1, 200);
                                }
                                else
                                {
                                    THROW(fplog::exceptions::Transport_Missing);
                                }
                            }
                            catch (fplog::exceptions::Generic_Exception& e)
                            {
                                if (retries_error <= 0)
                                {
                                    std::ofstream file(emergency_log_file_path, std::ios::app);
                                    if (file.is_open())
                                    {
                                        file << error_str + "\n";
                                        file.close();
                                    }
                                }
                                else
                                {
                                    retries_error--;
                                    goto retry_error;
                                }
                            }
                        }
                        else
                        {
                            retries--;
                            goto retry;
                        }
                    }
                }
                else
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::recursive_mutex mutex_;
        Queue_Controller mq_;

        std::thread overload_checker_;
        std::thread mq_reader_;

        std::vector<Thread_Data*> pool_;
        volatile bool should_stop_;
        fplog::Transport_Interface* log_transport_;
        fplog::Transport_Interface* protocol_;
};

static Impl g_impl;

void start()
{    
    Transport_Factory factory;
    fplog::Transport_Interface::Params params(get_log_transort_config());
    fplog::Transport_Interface* trans = factory.create(params);
	
	fplog::Transport_Interface* protocol = 0;
	
	for (auto param : params)
	{
		if (generic_util::find_str_no_case(param.first, "protocol"))
		{
			if (generic_util::find_str_no_case(param.second, "vsprot"))
			{
				protocol = new vsprot::Protocol(trans);
			}
			else
				protocol = new sprot::Protocol(trans);
		}
	}
	
	if (!protocol)
		protocol = new sprot::Protocol(trans);

    if (trans)
    {
        trans->connect(params);
        g_impl.set_log_transport(trans, protocol);
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
