#include "socket_transport.h"
#include <chrono>

using namespace std::chrono;

namespace spipc {

class WSA_Up_Down
{
    public:

        WSA_Up_Down()
        {
			#ifndef _LINUX
            
			WSADATA wsaData;
            if (WSAStartup(0x202, &wsaData))
                THROW(fplog::exceptions::Connect_Failed);
			
			#endif
        }

        ~WSA_Up_Down()
        {
			#ifndef _LINUX
			
            WSACleanup();
			
			#endif
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

		#ifndef _LINUX

        error += ("Connect failed, socket error = " + std::to_string(WSAGetLastError()));

		#else

		error += ("Connect failed, socket error = " + std::to_string(errno));

		#endif

		THROWM(fplog::exceptions::Connect_Failed, error.c_str());
    }

    sockaddr_in listen_addr;
    listen_addr.sin_family=AF_INET;
    listen_addr.sin_port=htons((u_short)uid.high);
    
    if (localhost_)
    {
        ((char*)&(listen_addr.sin_addr.s_addr))[0] = 127;
        ((char*)&(listen_addr.sin_addr.s_addr))[1] = 0;
        ((char*)&(listen_addr.sin_addr.s_addr))[2] = 0;
        ((char*)&(listen_addr.sin_addr.s_addr))[3] = 1;
    }
    else
    {
        ((char*)&(listen_addr.sin_addr.s_addr))[0] = 0;
        ((char*)&(listen_addr.sin_addr.s_addr))[1] = 0;
        ((char*)&(listen_addr.sin_addr.s_addr))[2] = 0;
        ((char*)&(listen_addr.sin_addr.s_addr))[3] = 0;
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
            THROW(fplog::exceptions::Connect_Failed);
        }

        listen_addr.sin_port=htons((u_short)uid.low);
        if (0 != bind(socket_, (sockaddr*)&listen_addr, sizeof(listen_addr)))
        {
            shutdown(socket_, SD_BOTH);
            closesocket(socket_);
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
		#ifndef _LINUX
		
        res = WSAGetLastError();
		
		#else
		
		res = errno;
		
		#endif
        
		//TODO: do something meaningful with the error
    }

    if (SOCKET_ERROR == closesocket(socket_))
    {
		#ifndef _LINUX
		
        res = WSAGetLastError();
		
		#else
		
		res = errno;
		
		#endif
		
        //TODO: do something meaningful with the error
    }

    connected_ = false;
}

#ifndef socklen_t
typedef int socklen_t;
#endif

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
 
#ifndef _LINUX

    fdset.fd_count = 1;
    fdset.fd_array[0] = socket_;

#else

	FD_ZERO(&fdset);
	FD_SET(socket_, &fdset);

#endif

    timeval to;
    to.tv_sec = timeout / 1000;
    to.tv_usec = (timeout % 1000) * 1000;

    int res = select(socket_ + 1, &fdset, 0, 0, &to);
    if (res == 0)
        THROW(fplog::exceptions::Timeout);
    if (res != 1)
        THROW(fplog::exceptions::Read_Failed);

    res = recvfrom((int)socket_, (char*)buf, buf_size, 0, (sockaddr*)&remote_addr, (socklen_t *)&addr_len);
    if (res != SOCKET_ERROR)
    {
        remote_addr.sin_port = ntohs(remote_addr.sin_port);
        
        if ((remote_addr.sin_port != uid_.high) && (remote_addr.sin_port != uid_.low))
            goto retry;

        if (!localhost_)
            if (memcmp(&(remote_addr.sin_addr.s_addr), ip_, sizeof(ip_)) != 0)
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
        ((char*)&(remote_addr.sin_addr.s_addr))[0] = 127;
        ((char*)&(remote_addr.sin_addr.s_addr))[1] = 0;
        ((char*)&(remote_addr.sin_addr.s_addr))[2] = 0;
        ((char*)&(remote_addr.sin_addr.s_addr))[3] = 1;
    }
    else
    {
        ((char*)&(remote_addr.sin_addr.s_addr))[0] = ip_[0];
        ((char*)&(remote_addr.sin_addr.s_addr))[1] = ip_[1];
        ((char*)&(remote_addr.sin_addr.s_addr))[2] = ip_[2];
        ((char*)&(remote_addr.sin_addr.s_addr))[3] = ip_[3];
    }
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = (u_short)(high_uid_ ? uid_.low : uid_.high);
    
    if (!localhost_)
        remote_addr.sin_port = (u_short)uid_.high;

    remote_addr.sin_port = htons(remote_addr.sin_port);
    
    fd_set fdset;
	
#ifndef _LINUX

    fdset.fd_count = 1;
    fdset.fd_array[0] = socket_;

#else

	FD_ZERO(&fdset);
	FD_SET(socket_, &fdset);

#endif


    timeval to;
    to.tv_sec = timeout / 1000;
    to.tv_usec = (timeout % 1000) * 1000;

    int res = select(socket_ + 1, 0, &fdset, 0, &to);
    if (res == 0)
        THROW(fplog::exceptions::Timeout);
    if (res != 1)
        THROW(fplog::exceptions::Write_Failed);

    res = sendto(socket_, (char*)buf, buf_size, 0, (sockaddr*)&remote_addr, addr_len);
    if (res != SOCKET_ERROR)
        return res;
    else
    {
        int e = errno;
        THROW(fplog::exceptions::Write_Failed);
    }

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
