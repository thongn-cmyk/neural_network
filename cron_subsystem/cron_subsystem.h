#ifndef __CRON_SUBSYSTEM_CRON_SUBSYSTEM_H__
#define __CRON_SUBSYSTEM_CRON_SUBSYSTEM_H__

#include "implementation/cron_subsystem_implementation.h"
#include <global_config/cron_subsystem_config.h>
#include <stl_extension/stdx.h>

//how about this, I will implement the extern and the splits of the functions
//but we'd do unity build for now for clean syntax
//just like a C# project
//because I literally don't want to involve inline, extern, static, complex compilation that hinders our debugging process

namespace cron_subsystem
{
    using namespace cron_subsystem::implementation;

    struct Signature{};

    using PeriodicCronJobSingletonContainer = stdx::singleton_container<std::unique_ptr<PeriodicCronLauncherInterface>, Signature>;

    extern void init()
    {
        using namespace global_config::cron_subsystem_config;

        stdx::memtransaction_guard tx_grd;

        PeriodicCronJobSingletonContainer::get() = PeriodicLauncherBuilder{}.set_cron_scan_wavelength(PERIODIC_CRON_SCAN_WAVELENGTH)
                                                                            .set_concurrent_worker_size(PERIODIC_CRON_WORKER_SZ)
                                                                            .set_waker_kind(PERIODIC_CRON_IS_SLEEPLESS_WAKER)
                                                                            .build();
    }

    extern void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        PeriodicCronJobSingletonContainer::get() = nullptr;
    }

    extern auto register_periodic_cronjob(const std::shared_ptr<UpdatableInterface>& updatable,
                                          std::chrono::nanoseconds dur) -> std::shared_ptr<void>
    {
        if (PeriodicCronJobSingletonContainer::get() == nullptr)
        {
            std::abort();
        }

        return PeriodicCronJobSingletonContainer::get()->launch(updatable, dur);
    }
}

#endif