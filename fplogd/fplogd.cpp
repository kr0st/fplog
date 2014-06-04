#include "targetver.h"
#include "fplogd.h"

namespace fplogd {

bool is_started()
{
    return true;
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