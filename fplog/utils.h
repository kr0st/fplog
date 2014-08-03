#pragma once
#include <string>

namespace generic_util
{

//Returns difference in minutes from UTC
//Example: for UTC+3 function returns 180
int get_system_timezone();

//Converts timezone bias in minutes (returned by get_system_timezone() for example) to iso8601 representation
std::string timezone_from_minutes_to_iso8601(int tz_minute_bias);

//Returns current local date-time in iso 8601 format including timezone information
std::string get_iso8601_timestamp();

//Milliseconds elapsed since 01-Jan-1970
unsigned long long get_msec_time();

bool base64_encode(const char *source, size_t sourcelen, char *dest, size_t destlen);
size_t base64_decode(const char *source, char *dest, size_t destlen);
size_t base64_encoded_length(size_t non_encoded_length);

};
