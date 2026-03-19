
#ifndef __DG_CONCURRENCY_BASE_H__
#define __DG_CONCURRENCY_BASE_H__

//define HEADER_CONTROL 3

#include <stddef.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <memory>
#include "worker_controller.h"
#include <bit>
#include <stl_extension/datastructure.h>
#include <stl_extension/stdx.h>
#include <optional>

namespace concurrency_base
{
    using daemon_kind_t     = concurrency_base::worker_controller::daemon_kind_t;

    enum daemon_option: daemon_kind_t
    {
        MAILBOX_UNIT_DAEMON     = 0u,
        MAILBOX_STREAM_DAEMON   = 1u,
        MAILBOX_CHANNEL_DAEMON  = 2u,
        REST_SERVER_DAEMON      = 3u,
        REST_CLIENT_DAEMON      = 4u,
        COROUTINE_DAEMON        = 5u,
        CRON_TICK_DAEMON        = 6u,
        CRON_WORK_DAEMON        = 7u,
        ASYNC_SEQPAR_DAEMON     = 8u,
        ASYNC_CUDA              = 9u
    };

    using WorkerInterface   = concurrency_base::worker_controller::WorkerInterface; 

    struct WorkerInformation
    {
        std::optional<int> cpu_id;
        daemon_kind_t daemon;
    };

    struct Config
    {
        std::vector<WorkerInformation> worker_vec;
    };

    struct Signature{}; 

    struct ConcurrencyResource
    {
        std::unique_ptr<concurrency_base::worker_controller::DaemonControllerInterface> daemon_controller; 
        datastructure::unordered_map_variants::unordered_node_map<std::thread::id, size_t> thrid_to_idx_map;
    };

    using SingletonObject = stdx::singleton_container<ConcurrencyResource, Signature>;

    extern void init(Config config, bool main_inclusion = true)
    {
        using namespace concurrency_base::worker_controller;

        stdx::memtransaction_guard tx_grd;

        std::vector<std::pair<std::unique_ptr<DaemonRunnerInterface>, daemon_kind_t>> runner_kind_vec{};
        datastructure::unordered_map_variants::unordered_node_map<std::thread::id, size_t>  thrid_to_idx_map{};

        for (size_t i = 0u; i < config.worker_vec.size(); ++i)
        {
            std::unique_ptr<DaemonDedicatedRunnerInterface> runner;

            if (config.worker_vec[i].cpu_id.has_value())
            {
                runner = DaemonRunnerFactory::spawn_std_daemon_affined_runner({config.worker_vec[i].cpu_id.value()});
            }
            else
            {
                runner = DaemonRunnerFactory::spawn_std_daemon_runner();
            }

            thrid_to_idx_map[runner->id()] = i;
            runner_kind_vec.push_back(std::make_pair(std::move(runner), config.worker_vec[i].daemon));
        }

        if (main_inclusion)
        {
            thrid_to_idx_map[std::this_thread::get_id()] = config.worker_vec.size();
        }

        SingletonObject::get() =
        {
            .daemon_controller  = ControllerFactory::spawn_daemon_controller(std::move(runner_kind_vec)),
            .thrid_to_idx_map   = std::move(thrid_to_idx_map)
        };
    }

    extern void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = {};
    }

    extern auto get_thread_count() noexcept -> size_t
    {
        return SingletonObject::get().thrid_to_idx_map.size();
    }

    extern auto is_registered_thread() noexcept -> bool
    {
        return SingletonObject::get().thrid_to_idx_map.contains(std::this_thread::get_id());
    }

    extern auto this_thread_idx() noexcept -> size_t
    {    
        auto ptr = SingletonObject::get().thrid_to_idx_map.find(std::this_thread::get_id());

        if constexpr(DEBUG_MODE_FLAG)
        {
            if (ptr == SingletonObject::get().thrid_to_idx_map.end())
            {
                std::abort();
            }
        }

        return ptr->second;
    }

    extern auto __attribute__((noipa)) daemon_register(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<size_t, exception_t>
    {
        auto ptr = SingletonObject::get().thrid_to_idx_map.find(std::this_thread::get_id());

        return SingletonObject::get().daemon_controller->_register(daemon_kind, std::move(worker));
    }

    extern void daemon_deregister(size_t id) noexcept
    {
        auto ptr = SingletonObject::get().thrid_to_idx_map.find(std::this_thread::get_id());
    
        if (ptr == SingletonObject::get().thrid_to_idx_map.end())
        {
            std::abort();
        }

        SingletonObject::get().daemon_controller->deregister(id);
    }

    using daemon_deregister_t = void (*)(size_t *) noexcept; 

    extern auto daemon_saferegister(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<std::unique_ptr<size_t, daemon_deregister_t>, exception_t>
    {
        std::expected<size_t, exception_t> handle = daemon_register(daemon_kind, std::move(worker));

        if (!handle.has_value())
        {
            return std::unexpected(handle.error());
        }

        constexpr auto resource_destructor = [](size_t * daemon_id) noexcept
        {
            daemon_deregister(*daemon_id);
            delete daemon_id;
        };

        try
        {
            return std::unique_ptr<size_t, daemon_deregister_t>(new size_t{handle.value()}, resource_destructor);
        }
        catch (...)
        {
            std::abort();   
        }
    }

    using daemon_raii_handle_t = std::unique_ptr<size_t, daemon_deregister_t>;
};

#endif