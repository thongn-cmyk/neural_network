#ifndef __DG_CONCURRENCY_BASE_H__
#define __DG_CONCURRENCY_BASE_H__

#include <stdint.h>
#include <stdlib.h>
#include "concurrency_base_onfly.h"
#include "concurrency_base_dedicated.h"
#include "worker_interface.h"
#include "concurrency_base_definition.h"

namespace concurrency_base
{
    using namespace concurrency_base::definition;

    enum daemon_option: daemon_kind_t
    {
        MAILBOX_UNIT_DAEMON     = 0x0000,
        MAILBOX_STREAM_DAEMON   = 0x0001,
        MAILBOX_CHANNEL_DAEMON  = 0x0002,
        REST_SERVER_DAEMON      = 0x0003,
        REST_CLIENT_DAEMON      = 0x0004,
        COROUTINE_DAEMON        = 0x0005,
        CRON_TICK_DAEMON        = 0x0006,
        CRON_WORK_DAEMON        = 0x0007,
        ASYNC_SEQPAR_DAEMON     = 0x0008,
        ASYNC_CUDA_DAEMON       = 0x0009,
        COMMON_ONFLY_POOL       = 0x0100,
        DEDICATED_ONFLY_POOL    = 0x0101
    };

    struct WorkerInformation
    {
        std::optional<int> cpu_id;
        daemon_kind_t daemon;
    };

    struct Config
    {
        std::vector<WorkerInformation> worker_vec;
    };

    using WorkerInterface               = concurrency_base::interface::WorkerInterface;
    using InterruptableWorkerInterface  = concurrency_base::interface::InterruptableWorkerInterface;

    static auto _is_dedicated_kind(daemon_kind_t kind) noexcept -> bool
    {
        return (kind >> 8) == 0;
    }

    static auto _is_onfly_kind(daemon_kind_t kind) noexcept -> bool
    {
        return (kind >> 8) == 1;
    }

    static auto _to_base_kind(daemon_kind_t kind) noexcept -> daemon_kind_t
    {
        return kind & std::numeric_limits<uint8_t>::max();
    }

    static auto _get_dedicated_config(const Config& config) -> concurrency_base::dedicated::Config
    {
        concurrency_base::dedicated::Config result{};

        for (const auto& worker_info: config.worker_vec)
        {
            if (_is_dedicated_kind(worker_info.daemon))
            {
                result.worker_vec.push_back(concurrency_base::dedicated::WorkerInformation
                {
                    .cpu_id = worker_info.cpu_id,
                    .daemon = _to_base_kind(worker_info.daemon)
                });
            }
        }

        return result;
    }

    static auto _get_onfly_config(const Config& config) -> concurrency_base::onfly::Config
    {
        concurrency_base::onfly::Config result{};

        for (const auto& worker_info: config.worker_vec)
        {
            if (_is_onfly_kind(worker_info.daemon))
            {
                result.worker_vec.push_back(concurrency_base::onfly::WorkerInformation
                {
                    .daemon = _to_base_kind(worker_info.daemon)
                });
            }
        }

        return result;
    }

    void init(Config config, bool main_inclusion = true)
    {
        concurrency_base::dedicated::init(_get_dedicated_config(config), main_inclusion);
        concurrency_base::onfly::init(_get_onfly_config(config));
    }

    void deinit() noexcept
    {
        concurrency_base::onfly::deinit();
        concurrency_base::dedicated::deinit();
    }

    auto get_thread_count() noexcept -> size_t
    {
        return concurrency_base::dedicated::get_thread_count() + concurrency_base::onfly::get_thread_count();
    }

    static __attribute__((noinline)) auto _is_registered_thread() noexcept -> bool
    {
        return concurrency_base::dedicated::is_registered_thread() || concurrency_base::onfly::is_registered_thread();
    }

    auto is_registered_thread() noexcept -> bool
    {
        thread_local std::optional<bool> result = std::nullopt;

        if (!result.has_value()) [[unlikely]]
        {
            result = _is_registered_thread();
            return result.value();
        }
        else [[likely]]
        {
            return result.value();
        }
    }

    static __attribute__((noinline)) auto _this_thread_idx() noexcept -> size_t
    {
        if (concurrency_base::dedicated::is_registered_thread())
        {
            return concurrency_base::dedicated::this_thread_idx();
        }

        return concurrency_base::dedicated::get_thread_count() + concurrency_base::onfly::this_thread_idx();
    }

    auto this_thread_idx() noexcept -> size_t
    {
        thread_local std::optional<size_t> result = std::nullopt;

        if (!result.has_value()) [[unlikely]]
        {
            result = _this_thread_idx();
            return result.value();
        }
        else [[likely]]
        {
            return result.value();
        }
    }

    static auto _daemon_saferegister_dedicated(daemon_kind_t base_kind, std::unique_ptr<WorkerInterface> worker) -> std::shared_ptr<void>
    {
        auto result = concurrency_base::dedicated::daemon_saferegister(base_kind, std::move(worker));

        if (!result.has_value())
        {
            common_exception::throw_exception(result.error());
        }

        return std::make_shared<decltype(result)>(std::move(result));
    }

    static auto _daemon_saferegister_onfly(daemon_kind_t base_kind, std::unique_ptr<InterruptableWorkerInterface> worker) -> std::shared_ptr<void>
    {
        auto result = concurrency_base::onfly::daemon_saferegister(base_kind, std::move(worker));

        return std::make_shared<decltype(result)>(std::move(result));
    }

    auto daemon_saferegister(daemon_kind_t kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<std::shared_ptr<void>, exception_t>
    {
        try
        {
            if (_is_dedicated_kind(kind))
            {
                return _daemon_saferegister_dedicated(_to_base_kind(kind), std::move(worker));
            }
            else
            {
                common_exception::throw_valid_exception(common_exception::INVALID_ARGUMENT);
            }
        }
        catch (...)
        {
            return std::unexpected(common_exception::wrap_std_exception(std::current_exception()));
        }
    }

    auto daemon_saferegister(daemon_kind_t kind, std::unique_ptr<InterruptableWorkerInterface> worker) noexcept -> std::expected<std::shared_ptr<void>, exception_t>
    {
        try
        {
            if (_is_onfly_kind(kind))
            {
                return _daemon_saferegister_onfly(_to_base_kind(kind), std::move(worker));
            }
            else
            {
                common_exception::throw_valid_exception(common_exception::INVALID_ARGUMENT);
            }
        }
        catch (...)
        {
            return std::unexpected(common_exception::wrap_std_exception(std::current_exception()));
        }
    }

    using daemon_raii_handle_t = std::shared_ptr<void>;
}

#endif