#pragma once
#include <string>

namespace generic_util
{

//Returns difference in minutes from UTC
//Example: for UTC+3 function returns 180
int get_system_timezone();

//Converts timezone bias in minutes (returned by get_system_timezone() for example) to iso8601 representation
std::string timezone_from_minutes_to_iso8601(int tz_minute_bias);

};
