#ifndef __GLOBAL_REST_CONFIG_H__
#define __GLOBAL_REST_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>

namespace global_config::rest_config
{
    static inline constexpr uint32_t HIGH_AVAILABILITY_CHANNEL  = 0u;
    static inline constexpr uint32_t GENERAL_COMPUTE_CHANNEL    = 1u;
}

#endif