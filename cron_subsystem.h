#ifndef __CRON_SUBSYSTEM_H__
#define __CRON_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>

namespace cron_subsystem
{
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

    void register_interval_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                   std::chrono::nanoseconds dur) -> std::shared_ptr<void>
    {

    }

    void register_fire_and_forget_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                          std::chrono::time_point<std::chrono::system_clock> timepoint)
    {

    }
}

#endif