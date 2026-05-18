#ifndef __DATA_LOADER_GENERIC_RETRYER_DEVICE_H__
#define __DATA_LOADER_GENERIC_RETRYER_DEVICE_H__

#include <stl_extension/stdx.h>
#include "normal_device.h"
#include "retryer_device_interface.h"
#include <stdint.h>
#include <stdlib.h>
#include <data_loader/exception_base.h>
#include <serializer/compact_serializer.h>

namespace data_loader::retryer_device::generic_device
{
    using namespace data_loader::exception_base;

    struct GenericRetryConfig
    {
        std::variant<stdx::reflectible_monostate,
                     retryer_device::normal_device::ExternalRetryConfig> config;
    
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config);
        }
    };

    struct ExternalGenericRetryConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_generic_retry_config(const GenericRetryConfig& config) -> ExternalGenericRetryConfig
    {
        return ExternalGenericRetryConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_retry_config(const ExternalGenericRetryConfig& config) -> GenericRetryConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericRetryConfig>(config.config_bytestream);
    }

    class GenericRetryerMachine: public virtual RetryerMachineInterface
    {
        private:

            std::unique_ptr<RetryerMachineInterface> base;

        public:

            GenericRetryerMachine(const GenericRetryConfig& config)
            {
                if (std::holds_alternative<retryer_device::normal_device::ExternalRetryConfig>(config.config))
                {
                    this->base = std::make_unique<retryer_device::normal_device::RetryerMachine>(std::get<retryer_device::normal_device::ExternalRetryConfig>(config.config));
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