#ifndef __CONCURRENT_QUEUE_BOUNDED_QUEUE_H__
#define __CONCURRENT_QUEUE_BOUNDED_QUEUE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <mutex_extension/fair_mutex.h>
#include <utility>

namespace concurrent_queue::bounded_queue
{
    template <class T>
    class BoundedQueue
    {
        private:

            struct WaitingArgument
            {
                std::optional<T> * dst;
                std::binary_semaphore * smp;
            };

            struct PushingArgument
            {
                T item;
                std::binary_semaphore * smp;
            };

            std::deque<T> workorder_container;
            size_t workorder_container_cap;
            std::deque<WaitingArgument> waiting_arg_vec;
            std::deque<PushingArgument> pushing_arg_vec;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            bool is_poisoned;

        public:

            static_assert(std::is_nothrow_move_constructible_v<T>);
            static_assert(std::is_nothrow_move_assignable_v<T>);

            BoundedQueue(size_t workorder_container_cap): workorder_container(),
                                                          workorder_container_cap(workorder_container_cap),
                                                          waiting_arg_vec(),
                                                          pushing_arg_vec(),
                                                          mtx(fair_mutex::make_unique_fair_atomic_flag()),
                                                          is_poisoned(false){}

            void push(T&& arg)
            {
                std::binary_semaphore waiting_smp(0);

                bool is_wait_required = [&]
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        throw std::invalid_argument("container poisoned");
                    }

                    if (!this->waiting_arg_vec.empty())
                    {
                        auto arg2   = std::move(this->waiting_arg_vec.front());
                        this->waiting_arg_vec.pop_front();
                        *arg2.dst   = std::move(arg);

                        arg2.smp->release();

                        return false;
                    }

                    if (this->workorder_container.size() == this->workorder_container_cap)
                    {
                        auto push_arg = PushingArgument
                        {
                            .item   = std::move(arg),
                            .smp    = &waiting_smp
                        };

                        try
                        {
                            this->pushing_arg_vec.push_back(std::move(push_arg));
                        }
                        catch (...)
                        {
                            arg = std::move(push_arg.item);
                            throw;
                        }

                        return true;
                    }

                    this->workorder_container.push_back(std::move(arg));
                    return false;
                }();

                if (is_wait_required)
                {
                    waiting_smp.acquire();
                }
            }

            auto pop() noexcept -> std::optional<T>
            {
                std::optional<T> wait_item;
                std::binary_semaphore waiting_smp(0);

                bool is_wait_required = [&]
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->is_poisoned)
                    {
                        wait_item = std::nullopt;

                        return false;
                    }

                    if (!this->pushing_arg_vec.empty())
                    {
                        auto arg    = std::move(this->pushing_arg_vec.front());
                        this->pushing_arg_vec.pop_front();
                        arg.smp->release();
                        wait_item   = std::move(arg.item);

                        return false;
                    }

                    if (!this->workorder_container.empty())
                    {
                        auto arg    = std::move(this->workorder_container.front());
                        this->workorder_container.pop_front();
                        wait_item   = std::move(arg);

                        return false;
                    }

                    auto waiting_arg = WaitingArgument
                    {
                        .dst    = &wait_item,
                        .smp    = &waiting_smp
                    };

                    this->waiting_arg_vec.push_back(std::move(waiting_arg));

                    return true;
                }();

                if (is_wait_required)
                {
                    waiting_smp.acquire();
                }

                return wait_item;
            }

            void poison() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                for (const auto& waiting_arg: this->waiting_arg_vec)
                {
                    *waiting_arg.dst = std::nullopt;
                    waiting_arg.smp->release();
                }

                for (const auto& pushing_arg: this->pushing_arg_vec)
                {
                    pushing_arg.smp->release();
                }

                this->is_poisoned = true;
            }
    };   
}

#endif