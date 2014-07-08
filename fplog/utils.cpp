#include "utils.h"

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

#else

//TODO: Linux implementation
//some hints at http://linux.die.net/man/3/gmtime and http://linux.die.net/man/3/timelocal
static int get_system_timezone_impl()
{
}

#endif

namespace generic_util
{

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

};
