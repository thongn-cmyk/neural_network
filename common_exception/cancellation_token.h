#ifndef __CANCELLATION_TOKEN_H__
#define __CANCELLATION_TOKEN_H__

#include <stdint.h>
#include <stdlib.h>
#include <atomic>

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
}

#endif