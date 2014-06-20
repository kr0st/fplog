#pragma once
#include <string>
#include <thread>
#include <vector>
#include "../spipc/spipc.h"

namespace fplogd {
    
typedef void (*Start_Stop_Notification) (void);

//Could be hanging for 12 seconds in case service is already running,
//better to subscribe for notification.
bool is_started();
std::string get_lock_file_name();

void notify_when_started(Start_Stop_Notification callback);
void notify_when_stopped(Start_Stop_Notification callback);

template <class T> void start_notify_thread(void (T::*callback) (void), T* instance)
{
    while (!is_started())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    (instance->*callback)();
};

template <class T> void stop_notify_thread(void (T::*callback) (void), T* instance)
{
    while (is_started())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    (instance->*callback)();
};

template <class T> void notify_when_started(void (T::*callback) (void), T* instance)
{
    if (!instance || !callback)
        return;

    std::thread notifier(start_notify_thread<T>, callback, instance);
    notifier.detach();
}

template <class T> void notify_when_stopped(void (T::*callback) (void), T* instance)
{
    if (!instance || !callback)
        return;

    std::thread notifier(stop_notify_thread<T>, callback, instance);
    notifier.detach();
}

struct Channel_Data
{
    spipc::UID uid;
    std::string app_name;
};

std::vector<Channel_Data> get_registered_channels();

void start();
void stop();

};
