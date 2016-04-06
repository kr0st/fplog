#include "Transport_Factory.h"
#include <spipc/Socket_Transport.h>
#include <utils.h>
//#include "Mongo_Storage.h"
#include "Mongo_stub.h"

namespace fpcollect {

fplog::Transport_Interface* Transport_Factory::create(const fplog::Transport_Interface::Params& params)
{
    for (auto param : params)
    {
        if (generic_util::find_str_no_case(param.first, "type"))
        {
            if (generic_util::find_str_no_case(param.second, "ip"))
            {
                return new spipc::Socket_Transport();
            }

            if (generic_util::find_str_no_case(param.second, "mongo"))
            {
                return new fpcollect::Mongo_stub();
            }
        }
    }

    return 0;
}

};
