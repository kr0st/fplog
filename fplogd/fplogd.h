#pragma once
#include <string>
#include <thread>
#include <vector>
#include "../spipc/spipc.h"

namespace fplogd {
    
typedef void (*Start_Notification) (void);

//Could be hanging for 12 seconds in case service is already running,
//better to subscribe for notification.
bool is_started();
std::string get_lock_file_name();
void notify_when_started(Start_Notification callback);

template <class T> void thread_worker(void (T::*callback) (void), T* instance)
{
    while (!is_started())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    (instance->*callback)();
};

template <class T> void notify_when_started(void (T::*callback) (void), T* instance)
{
    if (!instance || !callback)
        return;

    std::thread notifier(thread_worker<T>, callback, instance);
    notifier.detach();
}

std::vector<spipc::UID> get_registered_channels();

};
