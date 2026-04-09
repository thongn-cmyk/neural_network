#ifndef __FIRE_BANDWIDTH_CONTROL_FIREABLE_INTERFACE_H__
#define __FIRE_BANDWIDTH_CONTROL_FIREABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace fire_bandwidth_control::interface
{
    class FireableInterface
    {
        public:

            virtual ~FireableInterface() noexcept = default;

            virtual auto fire_one(common_exception::CancellationTokenInterface& cancellation_token) -> bool = 0;
    };
}

#endif