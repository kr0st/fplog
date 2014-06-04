#pragma once

namespace fplogd {
    
typedef void (*Start_Notification) (void);

bool is_started();
void notify_when_started(Start_Notification callback);

template <class T> void notify_when_started(void (T::*callback) (void), T* instance)
{
    instance->*callback;
}

};
