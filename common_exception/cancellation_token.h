#ifndef __CANCELLATION_TOKEN_H__
#define __CANCELLATION_TOKEN_H__

#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <memory>

namespace common_exception
{
    struct CancellationTokenInterface
    {
        virtual ~CancellationTokenInterface() noexcept = default;

        virtual auto is_canceled() noexcept -> bool = 0;
    };

    class CancellationToken: public virtual CancellationTokenInterface
    {
        private:

            std::atomic<bool> status;

        public:

            CancellationToken(): status(false){}

            auto is_canceled() noexcept -> bool
            {
                return this->status.load(std::memory_order_relaxed);
            }

            void cancel() noexcept
            {
                this->status.exchange(true, std::memory_order_relaxed);
            }
    };

    template <class Lambda>
    class LambdaCancellationToken: public virtual CancellationTokenInterface
    {
        private:

            Lambda lambda;
        
        public:

            // static_assert(std::is_nothrow_destructible_v<Lambda>);

            LambdaCancellationToken(Lambda lambda) noexcept(std::is_nothrow_move_constructible_v<Lambda>): lambda(std::move(lambda)){}

            auto is_canceled() noexcept -> bool
            {
                static_assert(noexcept(std::declval<Lambda&>()()));

                return this->lambda();
            }
    };

    class ObjectLifeCancellationToken: public virtual common_exception::CancellationTokenInterface
    {
        private:

            common_exception::CancellationTokenInterface * base;
            std::atomic<bool> is_out_of_scope;

        public:

            ObjectLifeCancellationToken(common_exception::CancellationTokenInterface& base): base(&base),
                                                                                             is_out_of_scope(false){}

            auto is_canceled() noexcept -> bool
            {
                if (this->is_out_of_scope.load(std::memory_order_relaxed))
                {
                    return true;
                }

                return this->base->is_canceled();
            }

            void out_scope() noexcept
            {
                this->is_out_of_scope.exchange(true, std::memory_order_relaxed);
            }
    };

    class ObjectLifeCancellationTokenStackHolder
    {
        private:

            std::shared_ptr<ObjectLifeCancellationToken> base;

            using self = ObjectLifeCancellationTokenStackHolder;

        public:

            ObjectLifeCancellationTokenStackHolder(common_exception::CancellationTokenInterface& base): base(std::make_shared<ObjectLifeCancellationToken>(base)){}

            ObjectLifeCancellationTokenStackHolder(const self&) = delete;
            ObjectLifeCancellationTokenStackHolder(self&&) = delete;
            auto operator =(const self&) -> self& = delete;
            auto operator =(self&&) -> self& = delete;

            ~ObjectLifeCancellationTokenStackHolder() noexcept
            {
                this->base->out_scope();
            }

            auto get() -> std::shared_ptr<common_exception::CancellationTokenInterface>
            {
                return this->base;
            }
    };
}

#endif