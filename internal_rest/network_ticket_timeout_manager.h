#ifndef __NETWORK_TICKET_TIMEOUT_MANAGER_H__
#define __NETWORK_TICKET_TIMEOUT_MANAGER_H__

#include <stdint.h>
#include <stdlib.h>
#include "network_allocation.h"
#include "network_std_container.h"
#include <chrono>
#include "network_exception.h"
#include "stdx.h"
#include "network_log.h"
#include "network_ack_expiry_queue.h"
#include <cron_subsystem/cron_subsystem.h>

namespace dg_sock::ticket_system
{
    template <class T, class StatelessIdExtractor, class ClockType = std::chrono::steady_clock>
    using temporal_ordered_item_map = dg_sock::network_datastructure::expiry_queue::temporal_ordered_item_map<T, StatelessIdExtractor, ClockType>;

    template <class ticket_id_t>
    struct ClockInArgument
    {
        ticket_id_t clocked_in_ticket;
        std::chrono::nanoseconds expiry_dur;
    };

    template <class ticket_id_t>
    struct TicketTimeoutManagerInterface
    {
        virtual ~TicketTimeoutManagerInterface() noexcept = default;

        virtual void clock_in(ClockInArgument<ticket_id_t> * clockin_arr, size_t sz, exception_t * exception_arr) noexcept = 0;
        virtual void get_expired_ticket(ticket_id_t * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept = 0;
        virtual void void_ticket(ticket_id_t * ticket_arr, size_t sz) noexcept = 0;
        virtual auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    template <class ticket_id_t>
    class TicketTimeoutManager
    {
        public:

            struct TicketIdExtractor
            {
                constexpr auto operator()(const ticket_id_t& arg) -> ticket_id_t
                {
                    return arg;
                }
            };

            struct PushWaitBucket
            {
                ClockInArgument<ticket_id_t> * clock_in_arr;
                size_t clock_in_arr_sz;
                std::binary_semaphore * smp;
            };

            struct PopWaitBucket
            {
                ticket_id_t * output_arr;
                size_t * output_arr_sz;
                size_t output_arr_cap;
                std::binary_semaphore * smp;
            };

        private:

            dg_sock::pow2_cyclic_queue<PushWaitBucket> push_wait_bucket_vec;
            dg_sock::pow2_cyclic_queue<PopWaitBucket> pop_wait_bucket_vec;
            temporal_ordered_item_map<ticket_id_t, TicketIdExtractor, std::chrono::steady_clock> expiry_bucket_queue;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            stdxx::hdi_container<std::chrono::nanoseconds> max_dur;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            using ticket_id_type = ticket_id_t;

            TicketTimeoutManager(dg_sock::pow2_cyclic_queue<PushWaitBucket> push_wait_bucket_vec,
                                 dg_sock::pow2_cyclic_queue<PopWaitBucket> pop_wait_bucket_vec,
                                 temporal_ordered_item_map<ticket_id_t, TicketIdExtractor, std::chrono::steady_clock> expiry_bucket_queue,
                                 std::unique_ptr<stdxx::fair_atomic_flag> mtx,
                                 stdxx::hdi_container<std::chrono::nanoseconds> max_dur,
                                 stdxx::hdi_container<size_t> max_consume_per_load) noexcept: push_wait_bucket_vec(std::move(push_wait_bucket_vec)),
                                                                                              pop_wait_bucket_vec(std::move(pop_wait_bucket_vec)),
                                                                                              expiry_bucket_queue(std::move(expiry_bucket_queue)),
                                                                                              mtx(std::move(mtx)),
                                                                                              max_dur(std::move(max_dur)),
                                                                                              max_consume_per_load(std::move(max_consume_per_load)){}

            void clock_in(ClockInArgument<ticket_id_t> * registering_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (!this->is_valid_clock_in_arguments(registering_arr, sz))
                {
                    std::fill(exception_arr, std::next(exception_arr, sz), dg_sock::network_exception::INVALID_ARGUMENT);
                    return;
                }

                if (sz == 0u)
                {
                    return;
                }

                std::fill(exception_arr, std::next(exception_arr, sz), dg_sock::network_exception::SUCCESS);
                std::binary_semaphore smp(0);

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    auto now                = std::chrono::steady_clock::now(); 
                    bool has_room           = this->expiry_bucket_queue.size() + sz <= this->expiry_bucket_queue.capacity();
                    bool is_fair            = this->push_wait_bucket_vec.empty();
                    bool can_direct_dropin  = has_room & is_fair;

                    if (can_direct_dropin)
                    {
                        for (size_t i = 0u; i < sz; ++i)
                        {
                            auto [ticket_id, current_dur] = std::make_pair(registering_arr[i].clocked_in_ticket, registering_arr[i].expiry_dur);
                            dg_sock::network_exception_handler::nothrow_log(this->expiry_bucket_queue.add_or_update(ticket_id, now + current_dur));
                        }

                        return;
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->push_wait_bucket_vec.push_back(PushWaitBucket
                    {
                        .clock_in_arr       = registering_arr,
                        .clock_in_arr_sz    = sz,
                        .smp                = &smp
                    }));
                }

                //TODOs: we'd need to fix the lags of push|pop here

                //this can wait forever, this is a dangerous operation, because we'd have to make sure that the waiting size is less than that of the operatable size
                //we'll improvise

                smp.acquire();

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    auto now    = std::chrono::steady_clock::now();

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        auto [ticket_id, current_dur] = std::make_pair(registering_arr[i].clocked_in_ticket, registering_arr[i].expiry_dur);
                        dg_sock::network_exception_handler::nothrow_log(this->expiry_bucket_queue.update(ticket_id, now + current_dur));
                    }
                }
            }

            void get_expired_ticket(ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept
            {
                std::binary_semaphore smp(0);

                bool need_wait = [&]() noexcept
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    ticket_arr_sz   = 0u;
                    auto now        = std::chrono::steady_clock::now();

                    while (true)
                    {
                        if (ticket_arr_sz == ticket_arr_cap)
                        {
                            return false;
                        }

                        if (!this->expiry_bucket_queue.has_expired_item(now))
                        {
                            if (ticket_arr_sz == 0u)
                            {
                                dg_sock::network_exception_handler::nothrow_log(this->pop_wait_bucket_vec.push_back(PopWaitBucket
                                {
                                    .output_arr     = ticket_arr,
                                    .output_arr_sz  = &ticket_arr_sz,
                                    .output_arr_cap = ticket_arr_cap,
                                    .smp            = &smp
                                }));

                                return true;
                            }

                            return false;
                        }

                        ticket_arr[ticket_arr_sz++] = this->expiry_bucket_queue.get_expired_item(now).value();
                    }
                }();

                if (need_wait)
                {
                    smp.acquire();
                }
            }

            void void_ticket(ticket_id_t * ticket_arr, size_t sz) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i)
                {
                    this->expiry_bucket_queue.erase(ticket_arr[i]);
                }

                this->internal_update();
            }

            void update() noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                this->internal_update();
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds
            {
                return this->max_dur.value;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            }

        private:

            auto is_valid_clock_in_arguments(ClockInArgument<ticket_id_t> * clock_in_arr, size_t clock_in_arr_sz) noexcept -> bool
            {
                std::chrono::nanoseconds max_dur = this->max_clockin_dur();

                for (size_t i = 0u; i < clock_in_arr_sz; ++i)
                {
                    if (clock_in_arr[i].expiry_dur > max_dur)
                    {
                        return false;
                    }
                }

                return true;
            }

            void internal_update() noexcept
            {
                auto now = std::chrono::steady_clock::now(); 

                while (true)
                {
                    if (this->expiry_bucket_queue.size() != this->expiry_bucket_queue.capacity())
                    {
                        bool can_progress   = !this->push_wait_bucket_vec.empty();

                        if (can_progress)
                        {
                            this->resolve_one_push_doable();
                            continue;
                        }
                    }

                    {
                        bool can_progress_1 = !this->pop_wait_bucket_vec.empty(); 
                        bool can_progress_2 = this->expiry_bucket_queue.has_expired_item(now);
                        bool can_progress   = can_progress_1 & can_progress_2;

                        if (can_progress)
                        {
                            this->resolve_one_pop_doable();
                            continue;
                        }
                    }

                    break;
                }
            }

            void resolve_one_push_doable() noexcept
            {
                if (this->expiry_bucket_queue.size() == this->expiry_bucket_queue.capacity())
                {
                    return;
                }

                if (this->push_wait_bucket_vec.empty())
                {
                    return;
                }

                PushWaitBucket& bucket = this->push_wait_bucket_vec.front(); 

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (bucket.clock_in_arr_sz == 0u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                dg_sock::network_exception_handler::nothrow_log(this->expiry_bucket_queue.add(bucket.clock_in_arr[0].clocked_in_ticket,
                                                                                              std::chrono::time_point<std::chrono::steady_clock>::max()));

                bucket.clock_in_arr     = std::next(bucket.clock_in_arr);
                bucket.clock_in_arr_sz  -= 1;

                if (bucket.clock_in_arr_sz == 0u)
                {
                    bucket.smp->release();
                    this->push_wait_bucket_vec.pop_front();

                    return;
                }
            }

            void resolve_one_pop_doable() noexcept
            {
                if (this->pop_wait_bucket_vec.empty())
                {
                    return;
                }

                auto now            = std::chrono::steady_clock::now(); 
                bool need_release   = false;

                [&]() noexcept
                {
                    while (true)
                    {
                        if (!this->expiry_bucket_queue.has_expired_item(now))
                        {
                            return;
                        }

                        PopWaitBucket& bucket                           = this->pop_wait_bucket_vec.front(); 

                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (*bucket.output_arr_sz == bucket.output_arr_cap)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        bucket.output_arr[(*bucket.output_arr_sz)++]    = this->expiry_bucket_queue.get_expired_item(now).value();
                        need_release                                    = true;

                        if (*bucket.output_arr_sz == bucket.output_arr_cap)
                        {
                            bucket.smp->release();
                            this->pop_wait_bucket_vec.pop_front();
                            need_release = false;

                            return;
                        }
                    }
                }();

                if (need_release)
                {
                    this->pop_wait_bucket_vec.front().smp->release();
                    this->pop_wait_bucket_vec.pop_front();
                } 
            }
    };

    template <class ticket_id_t>
    class SelfUpdateTicketTimeoutManager: public virtual TicketTimeoutManagerInterface<ticket_id_t>
    {
        private:

            std::shared_ptr<void> daemon;
            std::shared_ptr<TicketTimeoutManager<ticket_id_t>> base;

            static inline const std::chrono::nanoseconds UPDATE_DURATION = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10));

        public:

            using ticket_id_type = ticket_id_t;

            SelfUpdateTicketTimeoutManager(std::unique_ptr<TicketTimeoutManager<ticket_id_t>> base_arg)
            {
                if (base_arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->base      = std::move(base_arg);
                std::shared_ptr<cron_subsystem::UpdatableInterface> updatable = dg_sock::make_shared<InternalUpdater>(this->base);
                this->daemon    = cron_subsystem::register_periodic_cronjob(updatable, UPDATE_DURATION);
            }

            void clock_in(ClockInArgument<ticket_id_t> * clockin_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->base->clock_in(clockin_arr, sz, exception_arr);
            }

            void get_expired_ticket(ticket_id_t * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept
            {
                this->base->get_expired_ticket(output_arr, output_arr_sz, output_arr_cap);
            }

            void void_ticket(ticket_id_t * ticket_arr, size_t sz) noexcept
            {
                this->base->void_ticket(ticket_arr, sz);
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds
            {
                return this->base->max_clockin_dur();
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->base->max_consume_size();
            }

        private:

            class InternalUpdater: public virtual cron_subsystem::UpdatableInterface
            {
                private:

                    std::shared_ptr<TicketTimeoutManager<ticket_id_t>> base;
                
                public:

                    InternalUpdater(std::shared_ptr<TicketTimeoutManager<ticket_id_t>> base): base(std::move(base)){}

                    void update()
                    {
                        this->base->update();
                    }
            };
    };

    struct ComponentFactory
    {
        template <class ticket_id_t>
        static auto get_ticket_timeout_manager(size_t push_concurrency_queue_sz,
                                               size_t pop_concurrency_queue_sz,
                                               size_t concurrent_request_cap,
                                               std::chrono::nanoseconds max_wait_dur) -> std::unique_ptr<TicketTimeoutManagerInterface<ticket_id_t>>
        {
            const size_t MIN_CONCURRENCY_QUEUE_SZ   = 1u;
            const size_t MAX_CONCURRENCY_QUEUE_SZ   = size_t{1} << 30;
            const size_t MIN_CONCURRENT_REQUEST_CAP = 1u;
            const size_t MAX_CONCURRENT_REQUEST_CAP = size_t{1} << 30;

            if (std::clamp(push_concurrency_queue_sz, MIN_CONCURRENCY_QUEUE_SZ, MAX_CONCURRENCY_QUEUE_SZ) != push_concurrency_queue_sz)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (!stdxx::is_pow2(push_concurrency_queue_sz))
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(pop_concurrency_queue_sz, MIN_CONCURRENCY_QUEUE_SZ, MAX_CONCURRENCY_QUEUE_SZ) != pop_concurrency_queue_sz)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (!stdxx::is_pow2(pop_concurrency_queue_sz))
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(concurrent_request_cap, MIN_CONCURRENT_REQUEST_CAP, MAX_CONCURRENT_REQUEST_CAP) != concurrent_request_cap)
            {
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz     = concurrent_request_cap >> 4;
            size_t MIN_CONSUME_SZ           = 1u;
            size_t actual_consume_sz        = std::max(tentative_consume_sz, MIN_CONSUME_SZ);

            size_t max_wait_sz              = actual_consume_sz;
            size_t concurrent_request_cap_2 = concurrent_request_cap + max_wait_sz;

            return std::make_unique<SelfUpdateTicketTimeoutManager<ticket_id_t>>(std::make_unique<TicketTimeoutManager<ticket_id_t>>(dg_sock::pow2_cyclic_queue<typename TicketTimeoutManager<ticket_id_t>::PushWaitBucket>(stdxx::ulog2(push_concurrency_queue_sz)),
                                                                                                                                     dg_sock::pow2_cyclic_queue<typename TicketTimeoutManager<ticket_id_t>::PopWaitBucket>(stdxx::ulog2(pop_concurrency_queue_sz)),
                                                                                                                                     temporal_ordered_item_map<ticket_id_t, typename TicketTimeoutManager<ticket_id_t>::TicketIdExtractor, std::chrono::steady_clock>(concurrent_request_cap_2),
                                                                                                                                     stdxx::make_unique_fair_atomic_flag(),
                                                                                                                                     stdxx::hdi_container<std::chrono::nanoseconds>(max_wait_dur),
                                                                                                                                     stdxx::hdi_container<size_t>(actual_consume_sz)));
        }

    };
}

#endif