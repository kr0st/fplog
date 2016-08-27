#include "utils.h"

#include <stdio.h>
#include <time.h>
#include <boost/algorithm/string.hpp>


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

#define snprintf _snprintf

#else

//TODO: Linux implementation
//some hints at http://linux.die.net/man/3/gmtime and http://linux.die.net/man/3/timelocal
//returns timezone in minutes, not hours!
//check out console cmd: date +%:z
static int get_system_timezone_impl()
{
    FILE* tz = 0;
    char cmd[] = "date +%:z";
    char line[256];
    memset(line, 0, sizeof(line));

    tz = popen(cmd, "r");
        
    if (tz != 0)
    {
        if (fgets(line, 256, tz))
        {
            char hours[256];
            char minutes[256];
            
            memset(hours, 0, sizeof(hours));
            memset(minutes, 0, sizeof(minutes));
            
            int separator = -1;
            for (int i = 0; i < sizeof(line); ++i)
                if (line[i] == ':')
                {
                    separator = i;
                    break;
                }

            if (separator > 0)
            {
                int len = strlen(line);
                if (len > 0)
                {
                    memcpy(hours, line, separator);
                    memcpy(minutes, line + separator + 1, len - separator - 1);
                    
                    int h = std::stoi(hours), m = std::stoi(minutes);
                    return 60 * h + m;
                }
            }
        }
    }

    return 0;
}

static unsigned long long get_msec_time_impl()
{
    struct timespec tp;
    int res = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (res != 0)
        return 0;
    return tp.tv_sec * 1000 + (tp.tv_nsec / 1000000);
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
        snprintf(str, sizeof(str) - 1, "%s%02d%02d", hours > 0 ? "+" : "-", abs(hours), abs(minutes));
    else
        snprintf(str, sizeof(str) - 1, "%s%02d", hours > 0 ? "+" : "-", abs(hours));

    return str;
}

std::string get_iso8601_timestamp()
{
    time_t elapsed_time(time(0));
    struct tm* tm(localtime(&elapsed_time));
    char timestamp[200] = {0};

    static int tz = get_system_timezone();
    static int call_counter = 0;

    call_counter++;

    if (call_counter > 200)
    {
        call_counter = 0;
        tz = get_system_timezone();
    }

    snprintf(timestamp, sizeof(timestamp) - 1, "%04d-%02d-%02dT%02d:%02d:%02d.%lld%s",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, get_msec_time() % 1000,
        timezone_from_minutes_to_iso8601(tz).c_str());

    return timestamp;
}
/**
 * characters used for Base64 encoding
 */  
const char *BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * encode three bytes using base64 (RFC 3548)
 *
 * @param triple three bytes that should be encoded
 * @param result buffer of four characters where the result is stored
 */  
void _base64_encode_triple(unsigned char triple[3], char result[4])
{
    int tripleValue, i;

    tripleValue = triple[0];
    tripleValue *= 256;
    tripleValue += triple[1];
    tripleValue *= 256;
    tripleValue += triple[2];

    for (i=0; i<4; i++)
    {
        result[3-i] = BASE64_CHARS[tripleValue%64];
        tripleValue /= 64;
    }
} 

/**
 * encode an array of bytes using Base64 (RFC 3548)
 *
 * @param source the source buffer
 * @param sourcelen the length of the source buffer
 * @param target the target buffer
 * @param targetlen the length of the target buffer
 * @return 1 on success, 0 otherwise
 */  
bool base64_encode(const void* src, size_t sourcelen, char* target, size_t targetlen)
 {
    unsigned char *source = (unsigned char*)src;

    /* check if the result will fit in the target buffer */
    if ((sourcelen+2)/3*4 > targetlen-1)
        return false;

    /* encode all full triples */
    while (sourcelen >= 3)
    {
        _base64_encode_triple(source, target);
        sourcelen -= 3;
        source += 3;
        target += 4;
    }

    /* encode the last one or two characters */
    if (sourcelen > 0)
    {
        unsigned char temp[3];
        memset(temp, 0, sizeof(temp));
        memcpy(temp, source, sourcelen);
        _base64_encode_triple(temp, target);

        target[3] = '=';
        if (sourcelen == 1)
            target[2] = '=';

        target += 4;
    }

    /* terminate the string */
    target[0] = 0;

    return true;
} 

/**
 * determine the value of a base64 encoding character
 *
 * @param base64char the character of which the value is searched
 * @return the value in case of success (0-63), -1 on failure
 */  
int _base64_char_value(char base64char)
{
    if (base64char >= 'A' && base64char <= 'Z')
        return base64char-'A';
    if (base64char >= 'a' && base64char <= 'z')
        return base64char-'a'+26;
    if (base64char >= '0' && base64char <= '9')
        return base64char-'0'+2*26;
    if (base64char == '+')
        return 2*26+10;
    if (base64char == '/')
        return 2*26+11;

    return -1;
} 

/**
 * decode a 4 char base64 encoded byte triple
 *
 * @param quadruple the 4 characters that should be decoded
 * @param result the decoded data
 * @return lenth of the result (1, 2 or 3), 0 on failure
 */  
int _base64_decode_triple(char quadruple[4], unsigned char *result)
{
    int i, triple_value, bytes_to_decode = 3, only_equals_yet = 1;
    int char_value[4];

    for (i=0; i<4; i++)
        char_value[i] = _base64_char_value(quadruple[i]);

    /* check if the characters are valid */
    for (i=3; i>=0; i--)
    {
        if (char_value[i]<0)
        {
            if (only_equals_yet && quadruple[i]=='=')
            {
                /* we will ignore this character anyway, make it something
                * that does not break our calculations */
                char_value[i]=0;
                bytes_to_decode--;
                continue;
            }

            return 0;
        }
        /* after we got a real character, no other '=' are allowed anymore */
        only_equals_yet = 0;
    }

    /* if we got "====" as input, bytes_to_decode is -1 */
    if (bytes_to_decode < 0)
        bytes_to_decode = 0;

    /* make one big value out of the partial values */
    triple_value = char_value[0];
    triple_value *= 64;
    triple_value += char_value[1];
    triple_value *= 64;
    triple_value += char_value[2];
    triple_value *= 64;
    triple_value += char_value[3];

    /* break the big value into bytes */
    for (i=bytes_to_decode; i<3; i++)
        triple_value /= 256;
    
    for (i=bytes_to_decode-1; i>=0; i--)
    {
        result[i] = triple_value%256;
        triple_value /= 256;
    }

    return bytes_to_decode;
} 

/**
 * decode base64 encoded data
 *
 * @param source the encoded data (zero terminated)
 * @param target pointer to the target buffer
 * @param targetlen length of the target buffer
 * @return length of converted data on success, -1 otherwise
 */  
size_t base64_decode(const char* source, void* dest, size_t targetlen)
 {
    unsigned char* target = (unsigned char*)dest;
    char *src, *tmpptr;
    char quadruple[4], tmpresult[3];
    int i, tmplen = 3;
    size_t converted = 0;

    /* concatinate '===' to the source to handle unpadded base64 data */
    src = (char *)malloc(strlen(source)+5);
    if (src == NULL)
        return -1;
    
    strcpy(src, source);
    strcat(src, "====");
    tmpptr = src;

    /* convert as long as we get a full result */
    while (tmplen == 3)
    {
        /* get 4 characters to convert */
        for (i=0; i<4; i++)
        {
            /* skip invalid characters - we won't reach the end */
            while (*tmpptr != '=' && _base64_char_value(*tmpptr)<0)
                tmpptr++;

            quadruple[i] = *(tmpptr++);
        }

        /* convert the characters */
        tmplen = _base64_decode_triple(quadruple, (unsigned char*)tmpresult);

        /* check if the fit in the result buffer */
        if ((int)targetlen < tmplen)
        {
            free(src);
            return -1;
        }

        /* put the partial result in the result buffer */
        memcpy(target, tmpresult, tmplen);
        target += tmplen;
        targetlen -= tmplen;
        converted += tmplen;
    }

    free(src);
    return converted;
}

size_t base64_encoded_length(size_t non_encoded_length) { return (non_encoded_length+2)/3*4 + 1; }

std::string& escape_quotes(std::string& str)
{
    boost::replace_all(str, "\"", "\\\"");
    return str;
}

};
