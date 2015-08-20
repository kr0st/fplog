#pragma once
#include <fplog_transport.h>

namespace fplogd {

class Transport_Factory: public fplog::Transport_Factory
{
    public:

        fplog::Transport_Interface* create(const fplog::Transport_Interface::Params& params);
};

};
