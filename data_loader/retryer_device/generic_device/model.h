#ifndef __DATA_LOADER_RETRYER_DEVICE_GENERIC_DEVICE_MODEL_H__
#define __DATA_LOADER_RETRYER_DEVICE_GENERIC_DEVICE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <variant>
#include <data_loader/retryer_device/normal_device/model.h>
#include <serializer/compact_serializer.h>
#include <stl_extension/stdx.h>
#include <string>

namespace data_loader::retryer_device::generic_device
{
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
}

#endif