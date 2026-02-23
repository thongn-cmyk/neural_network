
#ifndef __NETWORK_CONCURRENCY_H__
#define __NETWORK_CONCURRENCY_H__

//define HEADER_CONTROL 3

#include <stddef.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <memory>
#include "network_concurrency_impl1_linux.h"
#include <bit>
#include "network_raii_x.h"
#include "network_datastructure.h"

namespace dg_sock::network_concurrency{

    static inline constexpr size_t THREAD_COUNT = 32u;
    static inline constexpr size_t MAX_THREAD_COUNT = 10000u;

    using namespace dg_sock::network_concurrency_impl1::daemon_option_ns;
    
    using WorkerInterface = dg_sock::network_concurrency_impl1::WorkerInterface; 
    // using Config = dg_sock::network_concurrency_impl1::planner::Config; 

    struct signature_dg_network_concurrency{}; 

    struct ConcurrencyResource{
        std::unique_ptr<dg_sock::network_concurrency_impl1::DaemonControllerInterface> daemon_controller; 
        dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<std::thread::id, size_t> thrid_to_idx_map;
    };

    inline ConcurrencyResource * volatile concurrency_resource;

    extern void init(Config config)
    {
        stdxx::memtransaction_guard tx_grd;

        // // auto [daemon_controller, thr_id_vec] = dg_sock::network_concurrency_impl1::planner::spawn(config);
        // auto thrid_to_idx_map = dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<std::thread::id, size_t>(); 
        
        // for (size_t i = 0u; i < thr_id_vec.size(); ++i)
        // {
        //     thrid_to_idx_map[thr_id_vec[i]] = i;
        // }

        // concurrency_resource = new ConcurrencyResource(ConcurrencyResource{
        //     .daemon_controller  = std::move(daemon_controller),
        //     .thrid_to_idx_map   = std::move(thrid_to_idx_map)
        // });
    }

    extern void deinit() noexcept
    {
        delete concurrency_resource;
    }

    extern auto get_thread_count() noexcept -> size_t
    {
        return concurrency_resource->thrid_to_idx_map.size();
    }

    extern auto this_thread_idx() noexcept -> size_t
    {    
        auto ptr = concurrency_resource->thrid_to_idx_map.find(std::this_thread::get_id());

        if constexpr(DEBUG_MODE_FLAG)
        {
            if (ptr == concurrency_resource->thrid_to_idx_map.end())
            {
                std::abort();
            }
        }

        return ptr->second;
    }

    extern auto __attribute__((noipa)) daemon_register(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<size_t, exception_t>
    {
        auto ptr = concurrency_resource->thrid_to_idx_map.find(std::this_thread::get_id());

        if (ptr != concurrency_resource->thrid_to_idx_map.end())
        {
            return std::unexpected(dg_sock::network_exception::PTHREAD_CAUSA_SUI);
        }

        return concurrency_resource->daemon_controller->_register(daemon_kind, std::move(worker));
    }

    extern void daemon_deregister(size_t id) noexcept
    {
        auto ptr = concurrency_resource->thrid_to_idx_map.find(std::this_thread::get_id());
    
        if (ptr != concurrency_resource->thrid_to_idx_map.end())
        {
            std::abort();
        }

        concurrency_resource->daemon_controller->deregister(id);
    }

    using daemon_deregister_t = void (*)(size_t) noexcept; 

    extern auto daemon_saferegister(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<dg_sock::unique_resource<size_t, daemon_deregister_t>, exception_t>
    {
        std::expected<size_t, exception_t> handle = daemon_register(daemon_kind, std::move(worker));

        if (!handle.has_value())
        {
            return std::unexpected(handle.error());
        }

        return dg_sock::unique_resource<size_t, daemon_deregister_t>(handle.value(), daemon_deregister);
    }

    using daemon_raii_handle_t = dg_sock::unique_resource<size_t, daemon_deregister_t>;
};

#endif