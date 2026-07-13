#ifndef __DATA_LOADER_RETRYER_DEVICE_NORMAL_DEVICE_MODEL_H__
#define __DATA_LOADER_RETRYER_DEVICE_NORMAL_DEVICE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <serializer/compact_serializer.h>

namespace data_loader::retryer_device::normal_device
{
    struct RetryConfig
    {
        std::chrono::nanoseconds base_wait_time;
        std::chrono::nanoseconds max_wait_time;
        double exponential_base;
        uint64_t max_retry_count;
        std::optional<std::vector<std::string>> retryable_exception_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(base_wait_time,
                      max_wait_time,
                      exponential_base,
                      max_retry_count,
                      retryable_exception_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(base_wait_time,
                      max_wait_time,
                      exponential_base,
                      max_retry_count,
                      retryable_exception_vec);
        }
    };

    struct ExternalRetryConfig
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

    auto to_external_retry_config(const RetryConfig& config) -> ExternalRetryConfig
    {
        return ExternalRetryConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_retry_config(const ExternalRetryConfig& config) -> RetryConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<RetryConfig>(config.config_bytestream);
    }
}

#endif