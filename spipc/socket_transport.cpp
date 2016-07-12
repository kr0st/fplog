#include "socket_transport.h"
#include "spipc.h"
#include <chrono>

using namespace std::chrono;

namespace spipc {

class WSA_Up_Down
{
    public:

        WSA_Up_Down()
        {
            WSADATA wsaData;
            if (WSAStartup(0x202, &wsaData))
                THROW(fplog::exceptions::Connect_Failed);
        }

        ~WSA_Up_Down()
        {
            WSACleanup();
        }
};

void Socket_Transport::connect(const Params& params)
{
    static WSA_Up_Down sock_initer;

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::string uidstr;

    if (params.find("uid") == params.end())
    {
        if (params.find("UID") == params.end())
        {
            THROW(fplog::exceptions::Invalid_Uid);
        }
        else
            uidstr = (*params.find("UID")).second;
    }
    else
        uidstr = (*params.find("uid")).second;

    localhost_ = true;
    auto get_ip = [this](const std::string& ip)
    {
        int count = 0; 
        unsigned char addr[4];

        try
        {
            boost::char_separator<char> sep(".");
            boost::tokenizer<boost::char_separator<char>> tok(ip, sep);

            for(boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin(); it != tok.end(); ++it)
            {
                if (count >= 4)
                    THROW(fplog::exceptions::Incorrect_Parameter);
                std::string token(*it);
                unsigned int byte = boost::lexical_cast<unsigned int>(token);
                if (byte > 255)
                    THROW(fplog::exceptions::Incorrect_Parameter);
                addr[count] = byte;
                count++;
            }
        }
        catch(boost::exception&)
        {
            THROW(fplog::exceptions::Incorrect_Parameter);
        }

        memcpy(ip_, addr, sizeof(ip_));
    };

    if (params.find("ip") != params.end())
    {
        get_ip(params.find("ip")->second);
        localhost_ = false;
    }

    if (params.find("IP") != params.end())
    {
        get_ip(params.find("IP")->second);
        localhost_ = false;
    }

    const unsigned char localhost[4] = {127, 0, 0, 1};
    if (memcmp(ip_, localhost, sizeof(localhost)) == 0)
        localhost_ = true;

    fplog::UID uid;
    uid.from_string(uidstr);

    if ((uid.high > 65535) || (uid.high < 1))
        THROW(fplog::exceptions::Invalid_Uid);
    
    if ((uid.low > 65535) || (uid.low < 1))
        THROW(fplog::exceptions::Invalid_Uid);

    disconnect();
    
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET)
    {
        std::string error;
        error += ("Connect failed, socket error = " + std::to_string(WSAGetLastError()));
        WSACleanup();
        THROWM(fplog::exceptions::Connect_Failed, error.c_str());
    }

    sockaddr_in listen_addr;
    listen_addr.sin_family=AF_INET;
    listen_addr.sin_port=htons((u_short)uid.high);
    
    if (localhost_)
    {
        listen_addr.sin_addr.S_un.S_un_b.s_b1 = 127;
        listen_addr.sin_addr.S_un.S_un_b.s_b2 = 0;
        listen_addr.sin_addr.S_un.S_un_b.s_b3 = 0;
        listen_addr.sin_addr.S_un.S_un_b.s_b4 = 1;
    }
    else
    {
        listen_addr.sin_addr.S_un.S_un_b.s_b1 = 0;
        listen_addr.sin_addr.S_un.S_un_b.s_b2 = 0;
        listen_addr.sin_addr.S_un.S_un_b.s_b3 = 0;
        listen_addr.sin_addr.S_un.S_un_b.s_b4 = 0;
    }

    // Set the exclusive address option
    int opt_val = 1;
    
    #ifndef _LINUX
    
    setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *) &opt_val, sizeof(opt_val));
    
    #else
    
    opt_val = 0;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (char *) &opt_val, sizeof(opt_val));
    setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT, (char *) &opt_val, sizeof(opt_val));
 
    #endif

    if (0 != bind(socket_, (sockaddr*)&listen_addr, sizeof(listen_addr)))
    {
        if (!localhost_)
        {
            shutdown(socket_, SD_BOTH);
            closesocket(socket_);
            WSACleanup();
            THROW(fplog::exceptions::Connect_Failed);
        }

        listen_addr.sin_port=htons((u_short)uid.low);
        if (0 != bind(socket_, (sockaddr*)&listen_addr, sizeof(listen_addr)))
        {
            shutdown(socket_, SD_BOTH);
            closesocket(socket_);
            WSACleanup();
            THROW(fplog::exceptions::Connect_Failed);
        }
    }
    else
        high_uid_ = true;

    uid_ = uid;
    connected_ = true;
}

void Socket_Transport::disconnect()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!connected_)
        return;

    int res = 0;
    if (SOCKET_ERROR == shutdown(socket_, SD_BOTH))
    {
        res = WSAGetLastError();
        //TODO: do something meaningful with the error
    }

    if (SOCKET_ERROR == closesocket(socket_))
    {
        res = WSAGetLastError();
        //TODO: do something meaningful with the error
    }

    connected_ = false;
}

size_t Socket_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!connected_)
        THROW(fplog::exceptions::Read_Failed);
    
    time_point<system_clock, system_clock::duration> timer_start(system_clock::now());
    auto check_time_out = [&timeout, &timer_start]()
    {
        time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
        system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000);
        if (timer_stop - timer_start >= converted_timeout)
            THROW(fplog::exceptions::Timeout);
    };

retry:
    
    check_time_out();

    sockaddr_in remote_addr;
    int addr_len = sizeof(remote_addr);
    
    fd_set fdset;
    fdset.fd_count = 1;
    fdset.fd_array[0] = socket_;

    timeval to;
    to.tv_sec = timeout / 1000;
    to.tv_usec = (timeout - to.tv_sec * 1000) * 1000;

    int res = select(0, &fdset, 0, 0, &to);
    if (res == 0)
        THROW(fplog::exceptions::Timeout);
    if (res != 1)
        THROW(fplog::exceptions::Read_Failed);

    res = recvfrom(socket_, (char*)buf, buf_size, 0, (sockaddr*)&remote_addr, &addr_len);
    if (res != SOCKET_ERROR)
    {
        remote_addr.sin_port = ntohs(remote_addr.sin_port);
        if ((remote_addr.sin_port != uid_.high) && (remote_addr.sin_port != uid_.low))
            goto retry;

        if (!localhost_)
            if (memcmp(&(remote_addr.sin_addr.S_un.S_un_b), ip_, sizeof(ip_)) != 0)
                goto retry;

        return res;
    }
    else
        THROW(fplog::exceptions::Read_Failed);
    
    return 0;
}

size_t Socket_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!connected_)
        THROW(fplog::exceptions::Write_Failed);

    sockaddr_in remote_addr;
    int addr_len = sizeof(remote_addr);

    if (localhost_)
    {
        remote_addr.sin_addr.S_un.S_un_b.s_b1 = 127;
        remote_addr.sin_addr.S_un.S_un_b.s_b2 = 0;
        remote_addr.sin_addr.S_un.S_un_b.s_b3 = 0;
        remote_addr.sin_addr.S_un.S_un_b.s_b4 = 1;
    }
    else
    {
        remote_addr.sin_addr.S_un.S_un_b.s_b1 = ip_[0];
        remote_addr.sin_addr.S_un.S_un_b.s_b2 = ip_[1];
        remote_addr.sin_addr.S_un.S_un_b.s_b3 = ip_[2];
        remote_addr.sin_addr.S_un.S_un_b.s_b4 = ip_[3];
    }
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = (u_short)(high_uid_ ? uid_.low : uid_.high);
    
    if (!localhost_)
        remote_addr.sin_port = (u_short)uid_.high;

    remote_addr.sin_port = htons(remote_addr.sin_port);

    fd_set fdset;
    fdset.fd_count = 1;
    fdset.fd_array[0] = socket_;

    timeval to;
    to.tv_sec = timeout / 1000;
    to.tv_usec = (timeout - to.tv_sec * 1000) * 1000;

    int res = select(0, 0, &fdset, 0, &to);
    if (res == 0)
        THROW(fplog::exceptions::Timeout);
    if (res != 1)
        THROW(fplog::exceptions::Write_Failed);

    res = sendto(socket_, (char*)buf, buf_size, 0, (sockaddr*)&remote_addr, addr_len);
    if (res != SOCKET_ERROR)
        return res;
    else
        THROW(fplog::exceptions::Write_Failed);

    return 0;
}

Socket_Transport::Socket_Transport():
connected_(false),
high_uid_(false)
{
}

Socket_Transport::~Socket_Transport()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    disconnect();
}

};
