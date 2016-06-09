#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>

#include "UDT_Transport.h"

#include <udt.h>
#include <test_util.h>
#include <mutex>
#include <fplog_exceptions.h>
#include <thread>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>


namespace fplog
{
    class UDT_Transport::UDT_Transport_Impl
    {
        public:

            UDT_Transport_Impl():
            client_sock_(-1),
            serv_sock_(-1),
            localhost_(false)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
            }

            ~UDT_Transport_Impl()
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
            }

            void connect(const Params& params)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                std::string UIDstr;

                if (params.find("uid") == params.end())
                {
                    if (params.find("UID") == params.end())
                    {
                        THROW(exceptions::Incorrect_Parameter);
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
                    THROW(exceptions::Incorrect_Parameter);
    
                if ((uid.low > 65535) || (uid.low < 1))
                    THROW(exceptions::Incorrect_Parameter);

                if (check_socket(client_sock_))
                    UDT::close(client_sock_);

                if (check_socket(serv_sock_))
                    UDT::close(serv_sock_);

                init_socket(client_sock_);
                init_socket(serv_sock_);
            }

            void disconnect()
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);

                if (check_socket(client_sock_))
                    UDT::close(client_sock_);

                if (check_socket(serv_sock_))
                    UDT::close(serv_sock_);

                init_socket(client_sock_);
                init_socket(serv_sock_);

                connected_ = false;

                memset(ip_, 0, sizeof(char) * 4);
                ip_str_ = "0.0.0.0";
            }

            size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);

                accept_connection();

                UDT::UDSET read_set;
                
                timeval to;
                to.tv_sec = timeout / 1000;
                to.tv_usec = (timeout % 1000) * 1000;

                if (UDT::select(client_sock_ + 1, &read_set, 0, 0, &to) < 0)
                {
                    THROWM(fplog::exceptions::Read_Failed, "Cannot select socket for reading!");
                }

                if (read_set.size() == 0)
                    THROW(fplog::exceptions::Timeout);

                int sz = UDT::recv(client_sock_, (char*)buf, buf_size, 0);
                if (sz == UDT::ERROR)
                {
                    THROWM(fplog::exceptions::Read_Failed, UDT::getlasterror().getErrorMessage());
                }

                return sz;
            }

            size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);

                initiate_connection();

                UDT::UDSET write_set;
                
                timeval to;
                to.tv_sec = timeout / 1000;
                to.tv_usec = (timeout % 1000) * 1000;

                if (UDT::select(client_sock_ + 1, 0, &write_set, 0, &to) < 0)
                {
                    THROWM(fplog::exceptions::Read_Failed, "Cannot select socket for writing!");
                }

                if (write_set.size() == 0)
                    THROW(fplog::exceptions::Timeout);

                int sz = UDT::send(client_sock_, (char*)buf, buf_size, 0);
                if (sz == UDT::ERROR)
                {
                    THROWM(fplog::exceptions::Write_Failed, UDT::getlasterror().getErrorMessage());
                }

                return sz;
            }


        private:

            // Automatically start up and clean up UDT module.
            static UDTUpDown _udt_;
            std::recursive_mutex mutex_;
            
            UDTSOCKET serv_sock_, client_sock_;
            bool connected_;
            
            fplog::UID uid_;
            bool high_uid_;

            unsigned char ip_[4];
            std::string ip_str_;

            bool localhost_;

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
                    if (retries <= 0)
                        THROWM(exceptions::Connect_Failed, "Failed to accept connection.");

                    addrinfo hints;
                    addrinfo* res;

                    memset(&hints, 0, sizeof(struct addrinfo));

                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;

                    std::string port(std::to_string(uid_.high));
                    high_uid_ = false;

                    disconnect();
                
                    if (0 != getaddrinfo(NULL, port.c_str(), &hints, &res))
                    {
                        if (!localhost_)
                            THROWM(fplog::exceptions::Connect_Failed, "Port is in use, cannot connect.");

                        port = std::to_string(uid_.low);
                        if (0 != getaddrinfo(NULL, port.c_str(), &hints, &res))
                            THROWM(fplog::exceptions::Connect_Failed, "Port is in use, cannot connect.");
                    }
                    else
                        high_uid_ = true;

                    serv_sock_ = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

                    if (UDT::ERROR == UDT::bind(serv_sock_, res->ai_addr, res->ai_addrlen))
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

                        THROWM(fplog::exceptions::Connect_Failed, "Socket cannot accept connections.");
                    }

                    if (UDT::INVALID_SOCK == (client_sock_ = UDT::accept(serv_sock_, (sockaddr*)&clientaddr, &addrlen)))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        retries--;
                        continue;
                    }

                    char clienthost[NI_MAXHOST];
                    char clientservice[NI_MAXSERV];

                    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
                    std::string ip_address(std::to_string(ip_[0]) + "." + std::to_string(ip_[1]) + "." + std::to_string(ip_[2]) + "." + std::to_string(ip_[3]));
                    std::string host_addr(clienthost);

                    if (host_addr.find(ip_address) == std::string::npos)
                    {
                        UDT::close(client_sock_);

                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        retries--;
                        continue;
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
                    addrinfo* res;

                    memset(&hints, 0, sizeof(struct addrinfo));

                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;

                    std::string port(std::to_string(uid_.high));
                    high_uid_ = false;

                    disconnect();

                    if (0 != getaddrinfo(NULL, port.c_str(), &hints, &res))
                    {
                        if (!localhost_)
                            THROWM(fplog::exceptions::Connect_Failed, "Port is in use, cannot connect.");

                        port = std::to_string(uid_.low);
                        if (0 != getaddrinfo(NULL, port.c_str(), &hints, &res))
                            THROWM(fplog::exceptions::Connect_Failed, "Port is in use, cannot connect.");
                    }
                    else
                        high_uid_ = true;

                    freeaddrinfo(res);

                    struct addrinfo *local, *peer;

                    memset(&hints, 0, sizeof(struct addrinfo));

                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;

                    if (0 != getaddrinfo(NULL, port.c_str(), &hints, &local))
                        THROWM(fplog::exceptions::Connect_Failed, "Port is in use, cannot connect.");

                    client_sock_ = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

                    // Windows UDP issue
                    // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
                    #ifdef WIN32
                        UDT::setsockopt(client_sock_, 0, UDT_MSS, new int(1052), sizeof(int));
                    #endif

                    freeaddrinfo(local);

                    std::string peer_port = (high_uid_ ? std::to_string(uid_.low) : std::to_string(uid_.high));
                    if (0 != getaddrinfo(ip_str_.c_str(), peer_port.c_str(), &hints, &peer))
                        THROWM(fplog::exceptions::Connect_Failed, (std::string("Cannot connect to ") + ip_str_ + ":" + peer_port).c_str());

                    // connect to the server, implict bind
                    if (UDT::ERROR == UDT::connect(client_sock_, peer->ai_addr, peer->ai_addrlen))
                    {
                        freeaddrinfo(peer);
                        THROWM(fplog::exceptions::Connect_Failed, UDT::getlasterror().getErrorMessage());
                    }

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
