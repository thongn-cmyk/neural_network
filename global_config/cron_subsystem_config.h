#ifndef __GLOBAL_CONFIG_CRON_SUBSYSTEM_CONFIG_H__
#define __GLOBAL_CONFIG_CRON_SUBSYSTEM_CONFIG_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>

namespace global_config::cron_subsystem_config
{
    static inline const std::chrono::nanoseconds PERIODIC_CRON_SCAN_WAVELENGTH  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(100));
    static inline constexpr size_t PERIODIC_CRON_WORKER_SZ                      = 1;
    static inline constexpr bool PERIODIC_CRON_IS_SLEEPLESS_WAKER               = false;
}

#endif