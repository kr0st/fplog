#include "Transport_Factory.h"
#include <spipc/UDT_Transport.h>
#include <spipc/socket_transport.h>
#include <utils.h>
#include "Mongo_Storage.h"
#include "Mongo_Stub.h"

namespace fpcollect {


class Console_Output: public fplog::Transport_Interface
{
    public:

            virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                return 0;
            }

            virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
            {
                if (!buf)
                    return 0;

                char* str = new char[buf_size + 1];
                memset(str, 0, buf_size + 1);
                memcpy(str, buf, buf_size);

                std::cout << str << std::endl << std::endl;

                delete str;
                return buf_size;
            }
};

fplog::Transport_Interface* Transport_Factory::create(const fplog::Transport_Interface::Params& params)
{
    for (auto param : params)
    {
        if (generic_util::find_str_no_case(param.first, "type"))
        {
            if (generic_util::find_str_no_case(param.second, "ip"))
            {
                for (auto p2 : params)
                    if (generic_util::find_str_no_case(p2.first, "transport"))
                    {
                        if (generic_util::find_str_no_case(p2.second, "udp"))
                        {
                            return new spipc::Socket_Transport();
                        }
                    }

                return new spipc::UDT_Transport();
            }

            if (generic_util::find_str_no_case(param.second, "mongo_stub"))
            {
                return new fpcollect::Mongo_Stub();
            }

            if (generic_util::find_str_no_case(param.second, "mongo"))
            {
                return new fpcollect::Mongo_Storage();
            }

            if (generic_util::find_str_no_case(param.second, "console"))
            {
                return new fpcollect::Console_Output();
            }
        }
    }

    return 0;
}

};
