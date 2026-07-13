#ifndef __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_MODEL_H__
#define __DATA_LOADER_RETRYER_DEVICE_INFINITE_DEVICE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <serializer/compact_serializer.h>

namespace data_loader::retryer_device::infinite_device
{
    struct InfiniteRetryConfig
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct ExternalInfiniteRetryConfig
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

    auto to_external_infinite_retry_config(const InfiniteRetryConfig& config) -> ExternalInfiniteRetryConfig
    {
        return ExternalInfiniteRetryConfig
        {
            .config_bytestream  = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_infinite_retry_config(const ExternalInfiniteRetryConfig& config) -> InfiniteRetryConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<InfiniteRetryConfig>(config.config_bytestream);
    }
}

#endif