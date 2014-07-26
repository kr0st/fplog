#include "utils.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>

static int get_system_timezone_impl()
{
    TIME_ZONE_INFORMATION tz = {0};
    unsigned int res = GetTimeZoneInformation(&tz);

    if ((res == 0) || (res == TIME_ZONE_ID_INVALID)) return 0;
    if (res == 2) return -1 * (tz.DaylightBias + tz.Bias);
    
    return -1 * tz.Bias;
}

static unsigned long long get_msec_time_impl()
{
    __time64_t elapsed_seconds(_time64(0));

    unsigned long long result = elapsed_seconds > 0 ? elapsed_seconds * 1000 : 0;

    if (result == 0)
        return result;

    SYSTEMTIME st = {0};
    GetSystemTime(&st);
    result += st.wMilliseconds;

    return result;
}

#else

//TODO: Linux implementation
//some hints at http://linux.die.net/man/3/gmtime and http://linux.die.net/man/3/timelocal
static int get_system_timezone_impl()
{
}

//TODO: Linux implementation
//Use clock_gettime(CLOCK_MONOTONIC)
static unsigned long long get_msec_time_impl()
{
}

#endif

namespace generic_util
{

unsigned long long get_msec_time() { return get_msec_time_impl(); }
int get_system_timezone() { return get_system_timezone_impl(); }

std::string timezone_from_minutes_to_iso8601(int tz_minute_bias)
{
    if (tz_minute_bias == 0) return "";

    int hours = tz_minute_bias / 60;
    int minutes = tz_minute_bias % 60;

    char str[25] = {0};

    if (minutes != 0)
        _snprintf(str, sizeof(str) - 1, "%s%02d%02d", hours > 0 ? "+" : "-", abs(hours), abs(minutes));
    else
        _snprintf(str, sizeof(str) - 1, "%s%02d", hours > 0 ? "+" : "-", abs(hours));

    return str;
}

std::string get_iso8601_timestamp()
{
    time_t elapsed_time(time(0));
    struct tm* tm(localtime(&elapsed_time));
    char timestamp[200] = {0};

    _snprintf(timestamp, sizeof(timestamp) - 1, "%04d-%02d-%02dT%02d:%02d:%02d.%lld%s",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, get_msec_time() % 1000,
        timezone_from_minutes_to_iso8601(get_system_timezone()).c_str());

    return timestamp;
}

};
