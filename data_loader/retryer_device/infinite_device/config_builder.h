#ifndef __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_CONFIG_BUILDER_H__
#define __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include "model.h"

namespace data_loader::retryer_device::infinite_device
{
    class InfiniteRetryerMachineConfigBuilder
    {
        public:

            auto build() -> data_loader::retryer_device::infinite_device::ExternalInfiniteRetryConfig
            {
                return this->get_external_infinite_retry_config();
            }
        
        private:

            auto get_internal_infinite_retry_config() -> data_loader::retryer_device::infinite_device::InfiniteRetryConfig
            {
                return {};
            }

            auto get_external_infinite_retry_config() -> data_loader::retryer_device::infinite_device::ExternalInfiniteRetryConfig
            {
                return data_loader::retryer_device::infinite_device::to_external_infinite_retry_config
                (
                    this->get_internal_infinite_retry_config()
                );
            }
    };
}

#endif