#include "UDT_Transport.h"

namespace fplog
{
    class UDT_Transport::UDT_Transport_Impl
    {
        public:

            UDT_Transport_Impl()
            {
            }

            ~UDT_Transport_Impl()
            {
            }
    };

    void UDT_Transport::connect(const Params& params)
    {
    }

    void UDT_Transport::disconnect()
    {
    }

    size_t UDT_Transport::read(void* buf, size_t buf_size, size_t timeout = infinite_wait)
    {
        return 0;
    }

    size_t UDT_Transport::write(const void* buf, size_t buf_size, size_t timeout = infinite_wait)
    {
        return 0;
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
