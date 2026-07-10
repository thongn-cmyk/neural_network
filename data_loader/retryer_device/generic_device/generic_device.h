#ifndef __DATA_LOADER_GENERIC_RETRYER_DEVICE_H__
#define __DATA_LOADER_GENERIC_RETRYER_DEVICE_H__

#include <stl_extension/stdx.h>
#include <data_loader/retryer_device/normal_device/normal_device.h>
#include <data_loader/retryer_device/infinite_device/infinite_device.h>
#include <data_loader/retryer_device/retryer_device_interface.h>
#include <data_loader/retryer_device/runnable_device_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <data_loader/exception_base.h>
#include "model.h"

namespace data_loader::retryer_device::generic_device
{
    using namespace data_loader::exception_base;

    class GenericRetryerMachine: public virtual RetryerMachineInterface
    {
        private:

            std::unique_ptr<RetryerMachineInterface> base;

        public:

            GenericRetryerMachine(const GenericRetryConfig& config)
            {
                if (std::holds_alternative<retryer_device::normal_device::ExternalRetryConfig>(config.config))
                {
                    this->base  = std::make_unique<retryer_device::normal_device::RetryerMachine>(std::get<retryer_device::normal_device::ExternalRetryConfig>(config.config));
                }
                else if (std::holds_alternative<retryer_device::infinite_device::ExternalInfiniteRetryConfig>(config.config))
                {
                    this->base  = std::make_unique<retryer_device::infinite_device::InfiniteRetryerMachine>(std::get<retryer_device::infinite_device::ExternalInfiniteRetryConfig>(config.config));
                }
                else
                {
                    throw invalid_argument_base("bad retry config, dispatch code not found");
                }
            }

            GenericRetryerMachine(const ExternalGenericRetryConfig& config): GenericRetryerMachine(to_internal_generic_retry_config(config)){}

            void run(RunnableInterface& runnable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                this->base->run(runnable, cancellation_token);
            }
    };
}

#endif