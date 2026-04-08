#ifndef __FIRE_BANDWIDTH_CONTROL_FIREABLE_FIRER_INTERFACE_H__
#define __FIRE_BANDWIDTH_CONTROL_FIREABLE_FIRER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include "fireable_interface.h"
#include <common_exception/cancellation_token.h>

namespace fire_bandwidth_control::interface
{
    class FireableFirerInterface
    {
        public:

            virtual ~FireableFirerInterface() noexcept = default;
            virtual void run(FireableInterface& fireable,
                             common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };
}

#endif