#ifndef __NETWORK_REACTOR_H__
#define __NETWORK_REACTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "stdx.h"
#include "network_allocation.h"
#include <semaphore>
#include "network_std_container.h"
#include <memory>
#include <atomic>
#include <expected>
#include "network_exception.h"

namespace dg_sock::network_reactor
{
    class ComplexReactor
    {
        private:

            dg_sock::unordered_unstable_map<std::thread::id, std::shared_ptr<std::binary_semaphore>> mtx_queue;
            size_t mtx_queue_cap;
            stdxx::fair_atomic_flag mtx_mtx_queue;
            stdxx::inplace_hdi_container<std::atomic<intmax_t>> counter;
            stdxx::inplace_hdi_container<std::atomic<intmax_t>> wakeup_threshold;
            stdxx::inplace_hdi_container<std::atomic<size_t>> mtx_queue_sz;

        public:

            ComplexReactor(size_t mtx_queue_cap): mtx_queue(),
                                                  mtx_queue_cap(mtx_queue_cap), 
                                                  mtx_mtx_queue(),
                                                  counter(std::in_place_t{}, 0),
                                                  wakeup_threshold(std::in_place_t{}, 0),
                                                  mtx_queue_sz(std::in_place_t{}, 0u)
            {    
                stdxx::inplace_make_fair_atomic_flag(this->mtx_mtx_queue);
            }

            void increment(size_t sz) noexcept
            {
                intmax_t current    = this->counter.value.fetch_add(sz, std::memory_order_relaxed) + sz;
                intmax_t expected   = this->wakeup_threshold.value.load(std::memory_order_relaxed);

                if (current < expected)
                {
                    return;
                }

                size_t current_queue_sz = this->mtx_queue_sz.value.load(std::memory_order_relaxed);

                if (current_queue_sz == 0u)
                {
                    return;
                }

                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);

                this->mtx_queue_sz.value.exchange(0u, std::memory_order_relaxed);
                this->do_release(this->mtx_queue);
            }

            void decrement(size_t sz) noexcept
            {
                this->counter.value.fetch_sub(sz, std::memory_order_relaxed);
            }

            void reset() noexcept
            {
                this->counter.value.exchange(intmax_t{0}, std::memory_order_relaxed);
            } 

            auto set_wakeup_threshold(intmax_t arg) noexcept
            {
                this->wakeup_threshold.value.exchange(arg, std::memory_order_relaxed);
            } 

            void subscribe(std::chrono::nanoseconds waiting_time) noexcept
            {
                intmax_t current    = this->counter.value.load(std::memory_order_relaxed);
                intmax_t expected   = this->wakeup_threshold.value.load(std::memory_order_relaxed);

                if (current >= expected)
                {
                    return;
                }

                std::shared_ptr<std::binary_semaphore> waiting_smp = dg_sock::network_allocation::make_shared<std::binary_semaphore>(0);

                [&, this]() noexcept
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (this->mtx_queue.size() == this->mtx_queue_cap)
                        {
                            std::abort();
                        }
                    }

                    auto [map_ptr, status] = this->mtx_queue.insert_or_assign(std::this_thread::get_id(), waiting_smp);

                    if (status)
                    {
                        this->mtx_queue_sz.value.fetch_add(1u, std::memory_order_relaxed);
                    }

                    intmax_t new_current = this->counter.value.load(std::memory_order_relaxed); 

                    if (new_current >= expected)
                    {
                        this->mtx_queue_sz.value.exchange(0u, std::memory_order_relaxed);
                        this->do_release(this->mtx_queue);
                    }
                }();

                std::expected<bool, exception_t> err;

                try
                {
                    err = waiting_smp->try_acquire_for(waiting_time);
                }
                catch (...)
                {
                    err = std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                }

                if (err.has_value() && err.value() == false)
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);
                    this->mtx_queue.erase(std::this_thread::get_id());
                }
            }

        private:

            inline __attribute__((force_inline)) void do_release(dg_sock::unordered_unstable_map<std::thread::id, std::shared_ptr<std::binary_semaphore>>& smp_vec)
            {
                for (const auto& kv_pair: smp_vec)
                {
                    kv_pair.second->release();
                }

                smp_vec.clear();
            }
    };   
}

#endif