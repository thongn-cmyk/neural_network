#ifndef __CONCURRENCY_UTILITY_CONCURRENCY_UTILITY_H__
#define __CONCURRENCY_UTILITY_CONCURRENCY_UTILITY_H__

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_detachable_task/detachable_task_handle_interface.h>
#include <internal_rest/network_rest_frame.h>

namespace concurrency_utility
{
    template <class T>
    using TaskPromise   = concurrency_detachable_task::DetachableTaskHandleInterface<T>;

    template <class T>
    using RestPromise   = dg_sock::network_rest_frame::client::Promise<T>; //model detach

    //__internal_usage__
    template <class T>
    class RestAsTaskPromise: public virtual TaskPromise<T>
    {
        private:

            std::shared_ptr<RestPromise<T>> base;
        
        public:

            RestAsTaskPromise(std::shared_ptr<RestPromise<T>> base_arg): base(std::move(base_arg)){}

            auto is_completed() noexcept -> bool
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->is_completed();
            }

            void interrupt() noexcept
            {
                (void) this->base;
            }

            auto wait() -> T
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->wait();
            }

            void detach() noexcept
            {
                this->base = nullptr;
            }
    };

    //__internal_usage__
    template <class T, class Resolutor>
    class CastedTaskPromise: public virtual TaskPromise<decltype(std::declval<Resolutor&>()(std::declval<const T&>()))>
    {
        private:
            
            std::shared_ptr<TaskPromise<T>> base;
            Resolutor resolutor;

        public:

            using casted_type   = decltype(std::declval<Resolutor&>()(std::declval<const T&>()));

            CastedTaskPromise(std::shared_ptr<TaskPromise<T>> base_arg,
                              Resolutor resolutor_arg): base(std::move(base_arg)),
                                                        resolutor(std::move(resolutor_arg)){}

            auto is_completed() noexcept -> bool
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->is_completed();
            }

            void interrupt() noexcept
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                this->base->interrupt();
            }

            auto wait() -> casted_type
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                auto result = this->base->wait();

                return this->resolutor(std::as_const(result));
            }

            void detach() noexcept
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                this->base->detach();
            }
    };

    template <class T>
    auto rest_to_task_promise(const std::shared_ptr<RestPromise<T>>& promise) -> std::unique_ptr<TaskPromise<T>>
    {
        if (promise == nullptr)
        {
            throw std::invalid_argument("bad promise, null");
        }

        return std::make_unique<RestAsTaskPromise<T>>(promise);
    }

    template <class T, class Resolutor>
    auto task_promise_cast(const std::shared_ptr<TaskPromise<T>>& promise,
                           Resolutor&& resolutor) -> std::unique_ptr<TaskPromise<decltype(resolutor(std::declval<const T&>()))>>
    {
        if (promise == nullptr)
        {
            throw std::invalid_argument("bad promise, null");
        }

        return std::make_unique<CastedTaskPromise<T, std::decay_t<Resolutor>>>(promise,
                                                                               std::forward<Resolutor>(resolutor));
    }

    //__internal_usage__
    template <class T, class Resolutor>
    class CastedRestPromise: public virtual RestPromise<decltype(std::declval<Resolutor&>()(std::declval<const T&>()))>
    {
        private:

            std::shared_ptr<RestPromise<T>> base;
            Resolutor resolutor;

        public:

            using casted_type   = decltype(std::declval<Resolutor&>()(std::declval<const T&>()));

            CastedRestPromise(std::shared_ptr<RestPromise<T>> base_arg,
                              Resolutor resolutor_arg): base(std::move(base_arg)),
                                                        resolutor(std::move(resolutor_arg)){}

            auto is_completed() noexcept -> bool
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                return this->base->is_completed();
            }

            auto wait() -> casted_type
            {
                if (this->base == nullptr)
                {
                    std::abort();
                }

                auto result = this->base->wait();

                return this->resolutor(std::as_const(result));
            }
    };

    //we are taking shared_ptr<> because not everything is ownership clear in this situation

    template <class T, class Resolutor>
    auto rest_promise_cast(const std::shared_ptr<RestPromise<T>>& promise,
                           Resolutor&& resolutor) -> std::unique_ptr<RestPromise<decltype(resolutor(std::declval<const T&>()))>>
    {
        if (promise == nullptr)
        {
            throw std::invalid_argument("bad promise, null");
        }

        return std::make_unique<CastedRestPromise<T, std::decay_t<Resolutor>>>(promise,
                                                                               std::forward<Resolutor>(resolutor));
    }

    template <class T>
    auto to_shared_promise(std::unique_ptr<RestPromise<T>>&& promise) -> std::shared_ptr<RestPromise<T>>
    {
        return promise;
    }

    template <class T>
    auto to_shared_promise(std::unique_ptr<TaskPromise<T>>&& promise) -> std::shared_ptr<TaskPromise<T>>
    {
        return promise;
    }
}

#endif