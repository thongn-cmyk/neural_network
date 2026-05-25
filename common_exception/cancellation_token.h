#ifndef __CANCELLATION_TOKEN_H__
#define __CANCELLATION_TOKEN_H__

#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <memory>
#include <mutex_extension/fair_mutex.h>

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
            bool is_out_of_scope;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ObjectLifeCancellationToken(common_exception::CancellationTokenInterface& base): base(&base),
                                                                                             is_out_of_scope(false),
                                                                                             mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            auto is_canceled() noexcept -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->is_out_of_scope)
                {
                    return true;
                }

                return this->base->is_canceled();
            }

            void out_scope() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->is_out_of_scope = true;
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

    class UnifiedCancellationToken: public virtual common_exception::CancellationTokenInterface
    {
        private:

            std::vector<std::shared_ptr<common_exception::CancellationTokenInterface>> cancellation_token_vec;
        
        public:

            UnifiedCancellationToken(std::vector<std::shared_ptr<common_exception::CancellationTokenInterface>> cancellation_token_vec): cancellation_token_vec(std::move(cancellation_token_vec))
            {
                for (const auto& e: this->cancellation_token_vec)
                {
                    if (e == nullptr)
                    {
                        throw std::invalid_argument("bad cancellation token, null");
                    }
                }
            }

            auto is_canceled() noexcept -> bool
            {
                for (const auto& e: this->cancellation_token_vec)
                {
                    if (e->is_canceled())
                    {
                        return true;
                    }
                }

                return false;
            }
    };
}

#endif