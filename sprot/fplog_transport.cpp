#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "fplog_transport.h"

namespace fplog {
    
#undef max
UID UID::from_string(const std::string& str)
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

UID UID::Helper::from_string(const std::string &str)
{
    UID uid;
    return uid.from_string(str);
}

};
