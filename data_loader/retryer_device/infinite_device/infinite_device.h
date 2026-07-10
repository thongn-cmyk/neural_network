#ifndef __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_INFINITE_DEVICE_H__
#define __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_INFINITE_DEVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/retryer_device/retryer_device_interface.h>
#include <data_loader/retryer_device/runnable_device_interface.h>
#include <data_loader/exception_base.h>
#include <common_exception/common_exception.h>
#include <optional>
#include "model.h"
#include <exception>
#include <stdexcept>

namespace data_loader::retryer_device::infinite_device
{
    using namespace data_loader::exception_base;

    class InfiniteRetryerMachine: public virtual RetryerMachineInterface
    {
        public:

            InfiniteRetryerMachine() = default;
            InfiniteRetryerMachine(const InfiniteRetryConfig& retry_config): InfiniteRetryerMachine(){}
            InfiniteRetryerMachine(const ExternalInfiniteRetryConfig& retry_config): InfiniteRetryerMachine(to_internal_infinite_retry_config(retry_config)){}

            void run(RunnableInterface& runnable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                while (true)
                {
                    try
                    {
                        runnable.run(cancellation_token);
                    }
                    catch (common_exception::operation_canceled_error& err)
                    {
                        throw;
                    }
                    catch (std::exception& e)
                    {
                        continue;
                    }

                    return;
                }
            }
    };
}

#endif