#include "targetver.h"
#include "fplogd.h"

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/thread_time.hpp>

#ifdef WIN32
#include <Shlobj.h>
#endif

static char* g_lock_file_name = ".fplogd_lock";
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

        void start_notification(){ callback_(); delete this; }
        void set_callback(Start_Notification callback) { callback_ = callback; }

    private:

        Start_Notification callback_;
};

void notify_when_started(Start_Notification callback)
{
    Notification_Helper *helper = new Notification_Helper();
    helper->set_callback(callback);
    notify_when_started<Notification_Helper>(&Notification_Helper::start_notification, helper);
}

};
