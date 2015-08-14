#include "targetver.h"
#include "spipc.h"

#include <process.h>
#include <chrono>
#include <mutex>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include "socket_transport.h"

namespace spipc {

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

            THROW(exceptions::Invalid_Uid);
        }
    }
    catch(boost::exception&)
    {
        THROW(exceptions::Invalid_Uid);
    }

    return *this;
}

IPC::IPC(fplog::Transport_Interface* transport, size_t MTU)
{
    own_transport_ = (transport == 0);
    transport_ = own_transport_ ? new Socket_Transport() : transport;
    protocol_ = new sprot::Protocol(transport_, MTU);
}

IPC::~IPC()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    delete protocol_;
    
    if (own_transport_)
        delete transport_;
}

size_t IPC::read(void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    size_t bytes = protocol_->read(buf, buf_size, timeout);
    if (bytes == 0)
        THROW(fplog::exceptions::Read_Failed);
    return bytes;
}

size_t IPC::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!buf)
        THROW(fplog::exceptions::Incorrect_Parameter);
    if (buf_size == 0)
        return 0;

    size_t bytes = protocol_->write(buf, buf_size, timeout);
    if (bytes != buf_size)
        THROW(fplog::exceptions::Write_Failed);
    return bytes;
}

void IPC::connect(const UID& private_channel)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    private_channel_id_ = private_channel;

    Params params;
    params["uid"] = private_channel_id_.to_string(private_channel_id_);

    transport_->connect(params);

    UID empty;
    if (private_channel == empty)
        THROW(fplog::exceptions::Incorrect_Parameter);
}

void IPC::connect(const Params& params)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string uidstr;

    if (params.find("uid") == params.end())
    {
        if (params.find("UID") == params.end())
        {
            THROW(exceptions::Invalid_Uid);
        }
        else
            uidstr = (*params.find("UID")).second;
    }
    else
        uidstr = (*params.find("uid")).second;

    UID uid;
    private_channel_id_ = uid.from_string(uidstr);
    transport_->connect(params);
}

};
