#ifndef __MAIN_SERVICE_ID_H__
#define __MAIN_SERVICE_ID_H__

#include <stdlib.h>
#include <stdint.h>

namespace main_service
{
    using thread_service_id_t = uint8_t;

    static inline constexpr thread_service_id_t THREAD_BROKERAGE_IDENTIFIER     = 0u;
    static inline constexpr thread_service_id_t THREAD_DISPOSABLE_IDENTIFIER    = 1u;
}

#endif