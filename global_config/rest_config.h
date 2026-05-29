#ifndef __GLOBAL_REST_CONFIG_H__
#define __GLOBAL_REST_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>

namespace global_config::rest_config
{
    static inline constexpr uint32_t HIGH_AVAILABILITY_CHANNEL                      = 0u;
    static inline constexpr uint32_t GENERAL_COMPUTE_CHANNEL                        = 1u;

    static inline const std::chrono::seconds DEFAULT_REQUEST_CLIENT_TIMEOUT_DURATION = std::chrono::seconds(40);
    static inline const std::chrono::seconds DEFAULT_REQUEST_SERVER_TIMEOUT_DURATION = std::chrono::seconds(40);

    //we'd have to leverage this global config space to define all of the conventional configurations
}

#endif