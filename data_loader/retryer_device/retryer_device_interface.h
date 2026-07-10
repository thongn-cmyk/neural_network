#ifndef __DATA_LOADER_RETRYER_DEVICE_RETRYER_DEVICE_INTERFACE_H__
#define __DATA_LOADER_RETRYER_DEVICE_RETRYER_DEVICE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <common_exception/cancellation_token.h>
#include "runnable_device_interface.h"

namespace data_loader::retryer_device
{
    class RetryerMachineInterface
    {
        public:

            virtual ~RetryerMachineInterface() noexcept = default;

            virtual void run(RunnableInterface& runnable,
                             common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };
}

#endif