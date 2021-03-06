#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef _LINUX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "UDT_Transport.h"

#include <udt.h>
#include <test_util.h>

#include <mutex>
#include <fplog_exceptions.h>
#include <thread>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

using namespace fplog;


namespace spipc
{
    class UDT_Transport::UDT_Transport_Impl
    {
        public:

            UDT_Transport_Impl():
            client_sock_(-1),
            serv_sock_(-1),
            localhost_(false),
            disconnecting_(false),
            connected_(false)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
            }

            ~UDT_Transport_Impl()
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                disconnect();
            }

            void connect(const Params& params)
            {
                static UDTUpDown udt_initer;

                std::lock_guard<std::recursive_mutex> lock(mutex_);
                std::string UIDstr;
                
                disconnecting_ = false;

                if (params.find("uid") == params.end())
                {
                    if (params.find("UID") == params.end())
                    {
                        THROW(fplog::exceptions::Incorrect_Parameter);
                    }
                    else
                        UIDstr = (*params.find("UID")).second;
                }
                else
                    UIDstr = (*params.find("uid")).second;

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
                uid.from_string(UIDstr);

                if ((uid.high > 65535) || (uid.high < 1))
                    THROW(fplog::exceptions::Incorrect_Parameter);
    
                if ((uid.low > 65535) || (uid.low < 1))
                    THROW(fplog::exceptions::Incorrect_Parameter);

                if (check_socket(client_sock_))
                    UDT::close(client_sock_);

                if (check_socket(serv_sock_))
                    UDT::close(serv_sock_);

                init_socket(client_sock_);
                init_socket(serv_sock_);

                uid_ = uid;
            }

            void disconnect()
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                disconnecting_ = true;

                if (check_socket(client_sock_))
                {
                    UDT::close(client_sock_);
                }

                if (check_socket(serv_sock_))
                {
                    UDT::close(serv_sock_);
                }

                init_socket(client_sock_);
                init_socket(serv_sock_);

                connected_ = false;
            }

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                if (disconnecting_)
                    return 0;

                accept_connection();

                UDT::UDSET read_set;
                
                timeval to;
                to.tv_sec = static_cast<long>(timeout / 1000);
                to.tv_usec = static_cast<long>((timeout % 1000) * 1000);

                UD_SET(client_sock_, &read_set);

                if (UDT::select(client_sock_ + 1, &read_set, 0, 0, &to) < 0)
                {
                    int status = UDT::getsockstate(client_sock_);
                    if (status >= BROKEN)
                    {
                        UDT::close(client_sock_);
                        init_socket(client_sock_);
                        connected_ = false;
                    }

                    THROWM(fplog::exceptions::Read_Failed, "Cannot select socket for reading!");
                }

                if (read_set.size() == 0)
                    THROW(fplog::exceptions::Timeout);

                int sz = UDT::recv(client_sock_, (char*)buf, static_cast<int>(buf_size), 0);
                if (sz == UDT::ERROR)
                {
                    int status = UDT::getsockstate(client_sock_);
                    if (status >= BROKEN)
                    {
                        UDT::close(client_sock_);
                        init_socket(client_sock_);
                        connected_ = false;
                    }

                    THROWM(fplog::exceptions::Read_Failed, UDT::getlasterror().getErrorMessage());
                }

                return sz;
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                if (disconnecting_)
                    return 0;
                    
                initiate_connection();

                UDT::UDSET write_set;
                
                timeval to;
                to.tv_sec = static_cast<long>(timeout / 1000);
                to.tv_usec = static_cast<int>((timeout % 1000) * 1000);

                UD_SET(client_sock_, &write_set);
                if (UDT::select(client_sock_ + 1, 0, &write_set, 0, &to) < 0)
                {
                    int status = UDT::getsockstate(client_sock_);
                    if (status >= BROKEN)
                    {
                        UDT::close(client_sock_);
                        init_socket(client_sock_);
                        connected_ = false;
                    }

                    THROWM(fplog::exceptions::Read_Failed, "Cannot select socket for writing!");
                }

                if (write_set.size() == 0)
                    THROW(fplog::exceptions::Timeout);

                int sz = UDT::send(client_sock_, (char*)buf, static_cast<int>(buf_size), 0);
                if (sz == UDT::ERROR)
                {
                    int status = UDT::getsockstate(client_sock_);
                    if (status >= BROKEN)
                    {
                        UDT::close(client_sock_);
                        init_socket(client_sock_);
                        connected_ = false;
                    }

                    THROWM(fplog::exceptions::Write_Failed, UDT::getlasterror().getErrorMessage());
                }

                return sz;
            }


        private:


            std::recursive_mutex mutex_;
            
            UDTSOCKET serv_sock_, client_sock_;
            bool connected_;
            
            fplog::UID uid_;
            bool high_uid_;

            unsigned char ip_[4];
            std::string ip_str_;

            bool localhost_;
            bool disconnecting_;

            bool check_socket(UDTSOCKET& socket)
            {
                if (socket > -1)
                {
                    if (socket == CUDTException::EINVSOCK)
                        return false;
                }
                else
                    return false;

                return true;
            }

            void init_socket(UDTSOCKET& socket)
            {
                socket = -1;
            }

            void accept_connection()
            {
                sockaddr_storage clientaddr;
                int addrlen = sizeof(clientaddr);

                int retries = 5;

                while (!connected_)
                {
                    if (disconnecting_)
                        THROWM(fplog::exceptions::Connect_Failed, "Failed to accept connection.");

                    if (retries <= 0)
                        THROWM(fplog::exceptions::Connect_Failed, "Failed to accept connection.");

                    addrinfo hints;
                    addrinfo* res;

                    memset(&hints, 0, sizeof(struct addrinfo));

                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;

                    std::string port(std::to_string(uid_.high));
                    high_uid_ = true;

                    disconnect();
                    disconnecting_ = false;

                    int err = getaddrinfo(NULL, port.c_str(), &hints, &res);
                    if (err != 0)
                        THROWM(fplog::exceptions::Connect_Failed, ("Port is in use, cannot connect. Error = " + std::to_string(err)).c_str());

                    serv_sock_ = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                    if (UDT::ERROR == UDT::bind(serv_sock_, res->ai_addr, static_cast<int>(res->ai_addrlen)))
                    {
                        UDT::close(serv_sock_);
                        init_socket(serv_sock_);

                        freeaddrinfo(res);

                        THROWM(fplog::exceptions::Connect_Failed, UDT::getlasterror().getErrorMessage());
                    }

                    freeaddrinfo(res);

                    if (UDT::ERROR == UDT::listen(serv_sock_, 10))
                    {
                        UDT::close(serv_sock_);
                        init_socket(serv_sock_);

                        THROWM(fplog::exceptions::Connect_Failed, std::string("Socket cannot accept connections, err = " + std::to_string(UDT::getlasterror().getErrorCode())).c_str());
                    }
re_accept:
                    if (UDT::INVALID_SOCK == (client_sock_ = UDT::accept(serv_sock_, (sockaddr*)&clientaddr, &addrlen)))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        retries--;
                        if (retries <= 0)
                            THROWM(fplog::exceptions::Connect_Failed, "Failed to accept connection.");
                        goto re_accept;
                    }
                    else
                        retries = 5;

                    char clienthost[NI_MAXHOST];
                    char clientservice[NI_MAXSERV];

                    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);

                    std::string ip_address(std::to_string(ip_[0]) + "." + std::to_string(ip_[1]) + "." + std::to_string(ip_[2]) + "." + std::to_string(ip_[3]));
                    std::string host_addr(clienthost), host_name, host_name_local;

                    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NOFQDN);
                    host_name = clienthost;

#ifdef _LINUX
                    ((char*)&(((sockaddr_in *)&clientaddr)->sin_addr.s_addr))[0] = 127;
                    ((char*)&(((sockaddr_in *)&clientaddr)->sin_addr.s_addr))[1] = 0;
                    ((char*)&(((sockaddr_in *)&clientaddr)->sin_addr.s_addr))[2] = 0;
                    ((char*)&(((sockaddr_in *)&clientaddr)->sin_addr.s_addr))[3] = 1;
#else
                    ((sockaddr_in *)&clientaddr)->sin_addr.S_un.S_un_b.s_b1 = 127;
                    ((sockaddr_in *)&clientaddr)->sin_addr.S_un.S_un_b.s_b2 = 0;
                    ((sockaddr_in *)&clientaddr)->sin_addr.S_un.S_un_b.s_b3 = 0;
                    ((sockaddr_in *)&clientaddr)->sin_addr.S_un.S_un_b.s_b4 = 1;
#endif

                    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NOFQDN);
                    host_name_local = clienthost;

                    if ((host_addr.find(ip_address) == std::string::npos) && (host_name.find(host_name_local) == std::string::npos))
                    {
                        UDT::close(client_sock_);

                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        goto re_accept;
                    }
                    else
                    {
                        ip_str_ = ip_address;
                        connected_ = true;
                    }
                }
            }

            void initiate_connection()
            {
                while (!connected_)
                {
                    addrinfo hints;
                    
                    if (disconnecting_)
                        THROWM(fplog::exceptions::Connect_Failed, "Failed to accept connection.");
                    
                    memset(&hints, 0, sizeof(struct addrinfo));

                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;

                    high_uid_ = false;

                    struct addrinfo *peer;

                    client_sock_ = UDT::socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

                    // Windows UDP issue
                    // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
                    #ifdef WIN32
                        UDT::setsockopt(client_sock_, 0, UDT_MSS, new int(1052), sizeof(int));
                    #endif

                    std::string peer_port = (high_uid_ ? std::to_string(uid_.low) : std::to_string(uid_.high));
                    std::string ip_address(std::to_string(ip_[0]) + "." + std::to_string(ip_[1]) + "." + std::to_string(ip_[2]) + "." + std::to_string(ip_[3]));
                    
                    if (0 != getaddrinfo(ip_address.c_str(), peer_port.c_str(), &hints, &peer))
                        THROWM(fplog::exceptions::Connect_Failed, (std::string("Cannot connect to ") + ip_str_ + ":" + peer_port).c_str());

                    // connect to the server, implict bind
                    if (UDT::ERROR == UDT::connect(client_sock_, peer->ai_addr, static_cast<int>(peer->ai_addrlen)))
                    {
                        freeaddrinfo(peer);
                        THROWM(fplog::exceptions::Connect_Failed, UDT::getlasterror().getErrorMessage());
                    }

                    ip_str_ = ip_address;
                    connected_ = true;

                    freeaddrinfo(peer);
                }
            }
    };

    void UDT_Transport::connect(const Params& params)
    {
        impl_->connect(params);
    }

    void UDT_Transport::disconnect()
    {
        impl_->disconnect();
    }

    size_t UDT_Transport::read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
    {
        return impl_->read(buf, buf_size, timeout);
    }

    size_t UDT_Transport::write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
    {
        return impl_->write(buf, buf_size, timeout);
    }

    UDT_Transport::UDT_Transport()
    {
        impl_ = new UDT_Transport_Impl();
    }

    UDT_Transport::~UDT_Transport()
    {
        delete impl_;
    }
};
