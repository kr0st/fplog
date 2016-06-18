#include "Transport_Factory.h"
#include <spipc/Socket_Transport.h>
#include <spipc/UDT_Transport.h>
#include <utils.h>

namespace fplogd {

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
        }
    }

    return 0;
}

};
