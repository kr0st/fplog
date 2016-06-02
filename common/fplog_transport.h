#pragma once

#include <map>
#include <string>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include <fplog_exceptions.h>

namespace fplog {

class Transport_Interface
{
    public:

        typedef std::pair<std::string, std::string> Param;
        typedef std::map<std::string, std::string> Params;
        
        static Params empty_params;
        static const size_t infinite_wait = static_cast<size_t>(-1);

        virtual void connect(const Params& params) {}
        virtual void disconnect() {}

        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait) = 0;

        virtual ~Transport_Interface(){};
};

class Transport_Factory
{
    public:

        virtual Transport_Interface* create(const Transport_Interface::Params& params) = 0;
};

struct UID
{
    UID(): high(0), low(0) {}
    bool operator== (const UID& rhs) const { return ((high == rhs.high) && (low == rhs.low)); }
    bool operator< (const UID& rhs) const
    {
        if (*this == rhs)
            return false;

        if (high < rhs.high)
            return true;

        if (high == rhs.high)
            return (low < rhs.low);

        return false;
    }

    std::string to_string(UID& uid)
    {
        return (std::to_string(uid.high) + "_" + std::to_string(uid.low));
    }

    #undef max
    UID UID::from_string(std::string& str)
    {
        unsigned long long limit = std::numeric_limits<unsigned long long>::max();
        high = limit;
        low = limit;

        try
        {
            boost::char_separator<char> sep("_");
            boost::tokenizer<boost::char_separator<char>> tok(str, sep);
            for(boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin(); it != tok.end(); ++it)
            {
                if (high == limit)
                {
                    high = boost::lexical_cast<unsigned long long>(*it);
                    continue;
                }

                if (low == limit)
                {
                    low = boost::lexical_cast<unsigned long long>(*it);
                    continue;
                }

                THROW(fplog::exceptions::Invalid_Uid);
            }
        }
        catch(boost::exception&)
        {
            THROW(fplog::exceptions::Invalid_Uid);
        }

        return *this;
    }

    unsigned long long high;
    unsigned long long low;
};

};
