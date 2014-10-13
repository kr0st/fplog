#pragma once
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

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

bool base64_encode(const void* source, size_t sourcelen, char* dest, size_t destlen);
size_t base64_decode(const char* source, void* dest, size_t destlen);
size_t base64_encoded_length(size_t non_encoded_length);

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

std::string& escape_quotes(std::string& str);

};
