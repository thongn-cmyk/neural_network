#ifndef __CRON_SUBSYSTEM_H__
#define __CRON_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>

namespace cron_subsystem
{
    static inline constexpr uint8_t CRON_KIND_DEDICATED     = 0u;
    static inline constexpr uint8_t CRON_KIND_SHARED        = 1u;

    static inline constexpr uint8_t CRON_CHECK_BUSY         = 0u;
    static inline constexpr uint8_t CRON_CHECK_SLEEPY       = 1u;

    class UpdatableInterface
    {
        public:

            virtual ~UpdatableInterface() = default;
            virtual void update() = 0;
    };


    void init()
    {

    }

    void deinit() noexcept
    {

    }

    void register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                   std::chrono::nanoseconds dur,
                                   uint8_t cron_kind    = CRON_KIND_SHARED,
                                   uint8_t cron_check   = CRON_CHECK_SLEEPY) -> std::shared_ptr<void>
    {

    }

    void register_fire_and_forget_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                          std::chrono::time_point<std::chrono::system_clock> timepoint)
    {

    }
}

#endif