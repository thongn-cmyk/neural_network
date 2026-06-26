#ifndef __DATA_LOADER_RETRYER_DEVICE_RUNNABLE_DEVICE_INTERFACE_H__
#define __DATA_LOADER_RETRYER_DEVICE_RUNNABLE_DEVICE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <common_exception/cancellation_token.h>

namespace data_loader::retryer_device
{
    class RunnableInterface
    {
        public:

            virtual ~RunnableInterface() noexcept = default;

            virtual void run(common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };
}

#endif
