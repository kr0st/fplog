#pragma once
#include <fplog_transport.h>

namespace fpcollect {

class Transport_Factory: public fplog::Transport_Factory
{
    public:

        fplog::Transport_Interface* create(const fplog::Transport_Interface::Params& params);
};

};
