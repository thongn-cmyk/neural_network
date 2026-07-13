#ifndef __DATA_LOADER_RETRYER_DEVICE_GENERIC_DEVICE_CONFIG_BUILDER_H__
#define __DATA_LOADER_RETRYER_DEVICE_GENERIC_DEVICE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include <stl_extension/stdx.h>
#include <variant>
#include "model.h"
#include <data_loader/retryer_device/infinite_device/config_builder.h>
#include <data_loader/retryer_device/normal_device/config_builder.h>

namespace data_loader::retryer_device::generic_device
{
    class GenericRetryerMachineConfigBuilder
    {
        private:

            using Builder_0 = data_loader::retryer_device::infinite_device::InfiniteRetryerMachineConfigBuilder;
            using Builder_1 = data_loader::retryer_device::normal_device::RetryerMachineConfigBuilder;

            std::variant<Builder_0, Builder_1> builder;

        public:

            GenericRetryerMachineConfigBuilder(): builder(){}

            auto as_infinite_retry_machine() -> Builder_0&
            {
                if (!std::holds_alternative<Builder_0>(this->builder))
                {
                    this->builder   = Builder_0{};
                }

                return std::get<Builder_0>(this->builder);
            }

            auto as_exponential_retry_machine() -> Builder_1&
            {
                if (!std::holds_alternative<Builder_1>(this->builder))
                {
                    this->builder   = Builder_1{};
                }

                return std::get<Builder_1>(this->builder);
            }

            auto build() -> ExternalGenericRetryConfig
            {
                return this->get_external_generic_retry_config();
            }

        private:

            auto get_internal_generic_retry_config() -> data_loader::retryer_device::generic_device::GenericRetryConfig
            {
                if (std::holds_alternative<Builder_0>(this->builder))
                {
                    return
                    {
                        .config = std::get<Builder_0>(this->builder).build()
                    };
                }
                else if (std::holds_alternative<Builder_1>(this->builder))
                {
                    return
                    {
                        .config = std::get<Builder_1>(this->builder).build()
                    };
                }
                else
                {
                    throw std::invalid_argument("bad retry option, option enumeration out of range");
                }
            }

            auto get_external_generic_retry_config() -> data_loader::retryer_device::generic_device::ExternalGenericRetryConfig
            {
                return data_loader::retryer_device::generic_device::to_external_generic_retry_config
                (
                    this->get_internal_generic_retry_config()
                );
            }
    };
}

#endif