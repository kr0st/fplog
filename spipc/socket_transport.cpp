#include "socket_transport.h"
#include "spipc.h"
#include <chrono>

using namespace std::chrono;

namespace spipc {

void Socket_Transport::connect(const Params& params)
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
    uid.from_string(uidstr);

    if ((uid.high > 65535) || (uid.high < 1))
        THROW(exceptions::Invalid_Uid);
    
    if ((uid.low > 65535) || (uid.low < 1))
        THROW(exceptions::Invalid_Uid);

    disconnect();
    
    WSADATA wsaData;
    if (WSAStartup(0x202, &wsaData))
        THROW(fplog::exceptions::Connect_Failed);
    
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
    listen_addr.sin_port=htons(uid.high);

    listen_addr.sin_addr.S_un.S_un_b.s_b1 = 127;
    listen_addr.sin_addr.S_un.S_un_b.s_b2 = 0;
    listen_addr.sin_addr.S_un.S_un_b.s_b3 = 0;
    listen_addr.sin_addr.S_un.S_un_b.s_b4 = 1;

    if (0 != bind(socket_, (sockaddr*)&listen_addr, sizeof(listen_addr)))
    {
        listen_addr.sin_port=htons(uid.low);
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

    shutdown(socket_, SD_BOTH);
    closesocket(socket_);
    WSACleanup();

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
    
    remote_addr.sin_addr.S_un.S_un_b.s_b1 = 127;
    remote_addr.sin_addr.S_un.S_un_b.s_b2 = 0;
    remote_addr.sin_addr.S_un.S_un_b.s_b3 = 0;
    remote_addr.sin_addr.S_un.S_un_b.s_b4 = 1;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = high_uid_ ? uid_.low : uid_.high;
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
