#ifndef __GLOBAL_CONFIG_COROUTINE_X_CONFIG_H__
#define __GLOBAL_CONFIG_COROUTINE_X_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>

namespace global_config::coroutine_x_config
{
    static inline constexpr bool HAS_NETWORK_COROUTINE_MACHINE  = true;
    static inline constexpr bool HAS_FILEIO_COROUTINE_MACHINE   = true;
    static inline constexpr bool HAS_COMPUTE_COROUTINE_MACHINE  = true;
}

#endif