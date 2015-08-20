#pragma once
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <algorithm>

namespace generic_util
{

static inline bool find_str_no_case(const std::string& search_where, const std::string& search_what)
{
  auto it = std::search(
    search_where.begin(), search_where.end(),
    search_what.begin(), search_what.end(),
    [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
  );
  return (it != search_where.end() );
}

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
