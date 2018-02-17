#include "Mongo_Storage.h"
#include <common/fplog_exceptions.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

#include <mongo/client/dbclient.h>
#include <utils.h>

namespace fpcollect {

class Mongo_Init
{
    public:

        Mongo_Init()
        {
            static mongo::client::Options options;
            options.setCallShutdownAtExit();
            mongo::client::initialize(options);
        }
};

Mongo_Storage::~Mongo_Storage()
{
    disconnect();
    delete connection_;
}

void Mongo_Storage::connect(const Params& params)
{
    try
    {
        static std::auto_ptr<Mongo_Init> init(new Mongo_Init());
        std::string ip, port, collection, auth_db, user, password;

        for (auto item : params)
        {
            if(generic_util::find_str_no_case(item.first, "ip"))
                ip = item.second;
            
            if(generic_util::find_str_no_case(item.first, "port"))
                port = item.second;

            if(generic_util::find_str_no_case(item.first, "collection"))
                collection = item.second;
            
            if(generic_util::find_str_no_case(item.first, "auth_db"))
                auth_db = item.second;
            
            if(generic_util::find_str_no_case(item.first, "user"))
                user = item.second;
            
            if(generic_util::find_str_no_case(item.first, "password"))
                password = item.second;
        }

        if (ip.empty())
            THROWM(fplog::exceptions::Connect_Failed, "IP not provided.");

        if (port.empty())
            port = "27017"; //default

        if (collection.empty())
            collection = "fplog.logs"; //default

        collection_ = collection;

        if (!connection_)
            connection_ = new mongo::DBClientConnection();

        connection_->connect(ip + ":" + port);
        if (!auth_db.empty() && !password.empty() && !user.empty())
        {
            std::string err;
            connection_->auth(auth_db, user, password, err);
        }
    }
    catch (mongo::DBException& e)
    {
        THROWM(fplog::exceptions::Connect_Failed, e.what());
    }
}

void Mongo_Storage::disconnect()
{
}

size_t Mongo_Storage::write(const void* buf, size_t buf_size, size_t timeout)
{
    if (!buf || (buf_size == 0))
        return 0;

    if (((const char*)buf)[buf_size - 1] != 0)
        THROWM(fplog::exceptions::Write_Failed, "Only null-terminated strings are supported by the storage.");

    try
    {
        mongo::BSONObj bson(mongo::fromjson((const char*)buf));
        connection_->insert(collection_, bson);
    }
    catch (mongo::DBException& e)
    {
        THROWM(fplog::exceptions::Write_Failed, e.what());
    }

    return buf_size;
}

};
