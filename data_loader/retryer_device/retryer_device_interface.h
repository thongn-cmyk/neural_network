#ifndef __DATA_LOADER_RETRYER_DEVICE_INTERFACE_H__
#define __DATA_LOADER_RETRYER_DEVICE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <common_exception/cancellation_token.h>

namespace data_loader::retryer_device
{
    class RunnableInterface
    {
        public:

            virtual ~RunnableInterface() noexcept = default;

            virtual void run(common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };

    class RetryerMachineInterface
    {
        public:

            virtual ~RetryerMachineInterface() noexcept = default;

            virtual void run(RunnableInterface&,
                             common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };
}

#endif