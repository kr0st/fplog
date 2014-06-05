#pragma once
#include <string>

namespace fplogd {
    
typedef void (*Start_Notification) (void);

//Could be hanging for 12 seconds in case service is already running,
//better to subscribe for notification.
bool is_started();
std::string get_lock_file_name();
void notify_when_started(Start_Notification callback);

template <class T> void notify_when_started(void (T::*callback) (void), T* instance)
{
    instance->*callback;
}

};
