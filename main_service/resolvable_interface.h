#ifndef __MAIN_SERVICE_RESOLVABLE_INTERFACE_H__
#define __MAIN_SERVICE_RESOLVABLE_INTERFACE_H__

#include <stdint.h>

namespace main_service::main_broker
{
    class Resolvable
    {
        public:

            virtual ~Resolvable() noexcept = default;

            virtual auto get_resolvable_id() noexcept -> uint8_t = 0;
    };
}

#endif