#ifndef __DG_CONCURRENCY_BASE_ONFLY_H__
#define __DG_CONCURRENCY_BASE_ONFLY_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include "onfly_worker_controller.h"
#include "concurrency_base_definition.h"
#include <common_exception/common_exception.h>
#include <stl_extension/stdx.h>

namespace concurrency_base::onfly
{
    using namespace concurrency_base::definition;

    enum daemon_option: daemon_kind_t
    {
        COMMON_ONFLY_POOL       = 0,
        DEDICATED_ONFLY_POOL    = 1
    };

    static inline constexpr size_t DAEMON_OPTION_ENUMERATION_RANGE = 2u;

    struct WorkerInformation
    {
        daemon_kind_t daemon;
    };

    struct Config
    {
        std::vector<WorkerInformation> worker_vec;
    };

    struct Signature{};

    struct ConcurrencyResource
    {
        std::unique_ptr<concurrency_base::onfly_worker_controller::DaemonControllerInterface> daemon_controller;
        std::vector<daemon_kind_t> offset_table;
        size_t total_thr_count;
    };

    using SingletonObject   = stdx::singleton_container<ConcurrencyResource, Signature>;
    using WorkerInterface   = concurrency_base::interface::WorkerInterface;

    static void _check_daemon_kind(daemon_kind_t daemon_kind)
    {
        if (daemon_kind >= DAEMON_OPTION_ENUMERATION_RANGE)
        {
            common_exception::throw_exception(common_exception::INVALID_ARGUMENT);
        }
    }

    static auto _get_daemon_thread_offset(daemon_kind_t daemon_kind) noexcept  -> size_t
    {
        if constexpr(DEBUG_MODE_FLAG)
        {
            if (daemon_kind >= SingletonObject::get().offset_table.size())
            {
                std::abort();            
            }
        }

        return SingletonObject::get().offset_table[daemon_kind];
    }

    void init(const Config& config)
    {
        stdx::memtransaction_guard tx_grd;

        std::unordered_map<daemon_kind_t, size_t> daemon_id_counter_map{};

        for (const auto& worker_info: config.worker_vec)
        {
            _check_daemon_kind(worker_info.daemon);        
            daemon_id_counter_map[worker_info.daemon]++;
        }

        std::unique_ptr<concurrency_base::onfly_worker_controller::DaemonControllerInterface> daemon_controller = std::make_unique<concurrency_base::onfly_worker_controller::FiniteDaemonController>(daemon_id_counter_map);
        std::vector<daemon_kind_t> offset_table{};
        size_t offset = 0u;

        for (size_t i = 0u; i < DAEMON_OPTION_ENUMERATION_RANGE; ++i)
        {
            offset_table.push_back(offset);
            offset += daemon_controller->daemon_size(i);
        }

        SingletonObject::get() = ConcurrencyResource
        {
            .daemon_controller  = std::move(daemon_controller),
            .offset_table       = std::move(offset_table),
            .total_thr_count    = offset
        };
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = {};
    }

    auto get_thread_count() noexcept -> size_t
    {
        return SingletonObject::get().total_thr_count;
    }

    static __attribute__((noinline)) auto _is_registered_thread() noexcept -> bool
    {
        if constexpr(DEBUG_MODE_FLAG)
        {
            if (SingletonObject::get().daemon_controller == nullptr)
            {
                std::abort();
            }
        }

        return SingletonObject::get().daemon_controller->daemon_info(std::this_thread::get_id()).has_value();
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
        if constexpr(DEBUG_MODE_FLAG)
        {
            if (!_is_registered_thread())
            {
                std::abort();
            }
        }

        auto daemon_info            = SingletonObject::get().daemon_controller->daemon_info(std::this_thread::get_id());

        if constexpr(DEBUG_MODE_FLAG)
        {
            if (!daemon_info.has_value())
            {
                std::abort();
            }
        }

        daemon_kind_t daemon_kind   = daemon_info->daemon_kind;
        size_t offset               = _get_daemon_thread_offset(daemon_kind);
        size_t relative_offset      = daemon_info->daemon_idx;

        return offset + relative_offset;
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

    auto daemon_register(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) -> size_t
    {
        if constexpr(DEBUG_MODE_FLAG)
        {
            if (SingletonObject::get().daemon_controller == nullptr)
            {
                std::abort();
            }
        }

        return SingletonObject::get().daemon_controller->_register(daemon_kind, std::move(worker));
    }

    void daemon_deregister(size_t id) noexcept
    {
        if constexpr(DEBUG_MODE_FLAG)
        {
            if (SingletonObject::get().daemon_controller == nullptr)
            {
                std::abort();
            }
        }

        return SingletonObject::get().daemon_controller->deregister(id);
    }

    using daemon_deregister_t = void (*) (size_t *) noexcept;

    auto daemon_saferegister(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) -> std::unique_ptr<size_t, daemon_deregister_t>
    {
        size_t resource_id = daemon_register(daemon_kind, std::move(worker));

        constexpr auto resource_destructor = [](size_t * daemon_id) noexcept
        {
            daemon_deregister(*daemon_id);
            delete daemon_id;
        };

        try
        {
            return std::unique_ptr<size_t, daemon_deregister_t>(new size_t{resource_id}, resource_destructor);
        }
        catch (...)
        {
            std::abort();
        }
    }
}

#endif