#ifndef __COROUTINE_SUBSYSTEM_TOPIC_CODE_H__
#define __COROUTINE_SUBSYSTEM_TOPIC_CODE_H__

#include <stdint.h>
#include <stdlib.h>

namespace coroutine_x
{
    static inline constexpr uint8_t NETWORK_COROUTINE   = 0u;
    static inline constexpr uint8_t FILEIO_COROUTINE    = 1u;
    static inline constexpr uint8_t COMPUTE_COROUTINE   = 2u;
}

#endif