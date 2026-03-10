//HEADER_CONTROL 0

#ifndef __FAIR_MUTEX_H__
#define __FAIR_MUTEX_H__

#include <stdint.h>
#include <stdlib.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <emmintrin.h>

namespace fair_mutex
{
    static inline constexpr size_t EXPBACKOFF_MUTEX_SPINLOCK_SIZE                               = size_t{1} << 4;
    static inline constexpr size_t EXPBACKOFF_FAIR_AF_YIELD_SPINLOCK_SIZE                       = size_t{1} << 10;
    static inline constexpr std::chrono::nanoseconds EXPBACKOFF_DEFAULT_SPIN_PERIOD             = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(10));
    static inline constexpr std::chrono::nanoseconds EXPBACKOFF_MUTEX_SPIN_PERIOD               = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(10));
    static inline constexpr std::chrono::nanoseconds EXPBACKOFF_FAIR_AF_YIELD_SPIN_PERIOD       = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(10));
    static inline const std::thread::id NULL_THREAD_ID                                          = std::bit_cast<std::thread::id>(14422289045101533236ULL);

    struct fair_atomic_flag
    {
        std::atomic_flag atomic_flag;
        std::atomic<std::thread::id> yield_thr_id;
        std::atomic<size_t> busy_waiter_sz;
    };

    auto make_fair_atomic_flag(bool value = false) noexcept -> fair_atomic_flag
    {
        return fair_atomic_flag
        {
            .atomic_flag        = std::atomic_flag(value),
            .yield_thr_id       = std::atomic<std::thread::id>(NULL_THREAD_ID),
            .busy_waiter_sz     = std::atomic<size_t>(0u)
        };
    }

    auto inplace_make_fair_atomic_flag(fair_atomic_flag& atomic_flag,
                                       bool value = false) noexcept
    {
        if (value)
        {
            atomic_flag.atomic_flag.test_and_set();
        }
        else
        {
            atomic_flag.atomic_flag.clear();
        }

        atomic_flag.yield_thr_id.exchange(NULL_THREAD_ID);
        atomic_flag.busy_waiter_sz.exchange(0u);
    }

    auto make_unique_fair_atomic_flag(bool value = false) -> std::unique_ptr<fair_atomic_flag>
    {
        auto rs = std::make_unique<fair_atomic_flag>();
        inplace_make_fair_atomic_flag(*rs, value);

        return rs;
    }

    template <class Lambda>
    inline bool eventloop_expbackoff_spin(Lambda&& lambda, 
                                          size_t spin_sz,
                                          std::chrono::nanoseconds period) noexcept(noexcept(lambda()))
    {
        const size_t BASE                   = 2u;
        const size_t MAX_SEQUENTIAL_PAUSE   = 64u;
        size_t current_sequential_pause     = 1u;

        for (size_t i = 0u; i < spin_sz; ++i)
        {
            if (lambda())
            {
                return true;
            }

            for (size_t i = 0u; i < current_sequential_pause; ++i)
            {
                _mm_pause();
            }

            current_sequential_pause = std::min(MAX_SEQUENTIAL_PAUSE, current_sequential_pause * BASE);
        }

        return false;
    }

    inline __attribute__((noinline)) void fair_atomic_flag_memsafe_lock_body(fair_atomic_flag * mtx)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);

        auto yield_job = [&]() noexcept
        {
            return mtx->yield_thr_id.load(std::memory_order_relaxed) != std::this_thread::get_id();
        };

        eventloop_expbackoff_spin(yield_job, EXPBACKOFF_FAIR_AF_YIELD_SPINLOCK_SIZE, EXPBACKOFF_FAIR_AF_YIELD_SPIN_PERIOD);

        auto job = [&]() noexcept
        {
            return mtx->atomic_flag.test_and_set(std::memory_order_relaxed) == false;
        };

        size_t busy_waiter_sz = mtx->busy_waiter_sz.load(std::memory_order_relaxed);

        if (busy_waiter_sz == 0u)
        {
            if (!job())
            {
                while (true)
                {
                    if (eventloop_expbackoff_spin(job, EXPBACKOFF_MUTEX_SPINLOCK_SIZE, EXPBACKOFF_MUTEX_SPIN_PERIOD))
                    {
                        break;
                    }

                    mtx->busy_waiter_sz.fetch_add(1u, std::memory_order_relaxed);
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    mtx->atomic_flag.wait(true, std::memory_order_relaxed);
                    mtx->busy_waiter_sz.fetch_sub(1u, std::memory_order_relaxed);
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                }
            }
        }
        else
        {
            while (true)
            {
                mtx->busy_waiter_sz.fetch_add(1u, std::memory_order_relaxed);
                std::atomic_signal_fence(std::memory_order_seq_cst);
                mtx->atomic_flag.wait(true, std::memory_order_relaxed);
                mtx->busy_waiter_sz.fetch_sub(1u, std::memory_order_relaxed);
                std::atomic_signal_fence(std::memory_order_seq_cst);

                if (eventloop_expbackoff_spin(job, EXPBACKOFF_MUTEX_SPINLOCK_SIZE, EXPBACKOFF_MUTEX_SPIN_PERIOD))
                {
                    break;
                }
            }
        }

        mtx->yield_thr_id.exchange(NULL_THREAD_ID, std::memory_order_relaxed); //
    }

    inline __attribute__((always_inline)) void fair_atomic_flag_memsafe_lock(fair_atomic_flag * mtx)
    {
        fair_atomic_flag_memsafe_lock_body(mtx);

        if constexpr(STRONG_MEMORY_ORDERING_FLAG)
        {
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
        else
        {
            std::atomic_thread_fence(std::memory_order_acquire);
        }
    }

    inline __attribute__((noinline)) void fair_atomic_flag_memsafe_unlock_body(fair_atomic_flag * mtx)
    {
        size_t busy_waiter_sz = mtx->busy_waiter_sz.load(std::memory_order_relaxed);
        std::atomic_signal_fence(std::memory_order_seq_cst);

        if (busy_waiter_sz > 0u)
        {
            mtx->yield_thr_id.exchange(std::this_thread::get_id(), std::memory_order_relaxed);
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }

        mtx->atomic_flag.clear(std::memory_order_relaxed);
        std::atomic_signal_fence(std::memory_order_seq_cst);
        mtx->atomic_flag.notify_one();
    }

    inline __attribute__((always_inline)) void fair_atomic_flag_memsafe_unlock(fair_atomic_flag * mtx)
    {
        if constexpr(STRONG_MEMORY_ORDERING_FLAG)
        {
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
        else
        {
            std::atomic_thread_fence(std::memory_order_release);
        }

        fair_atomic_flag_memsafe_unlock_body(mtx);
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }

    template <class T>
    class xlock_guard_base{};

    template <>
    class xlock_guard_base<fair_atomic_flag>
    {
        private:

            fair_atomic_flag * volatile mtx;

        public:

            using self = xlock_guard_base;

            inline __attribute__((always_inline)) xlock_guard_base(fair_atomic_flag& mtx) noexcept: mtx(&mtx)
            {
                fair_atomic_flag_memsafe_lock(this->mtx);
            }

            xlock_guard_base(const self&) = delete;
            xlock_guard_base(self&&) = delete;

            inline __attribute__((always_inline)) ~xlock_guard_base() noexcept
            {
                fair_atomic_flag_memsafe_unlock(this->mtx);
            }

            self& operator =(const self&) = delete;
            self& operator =(self&&) = delete;            
    };

    template <>
    class xlock_guard_base<std::mutex>
    {
        private:

            std::mutex * volatile mtx;

        public:

            using self = xlock_guard_base;

            inline __attribute__((always_inline)) xlock_guard_base(std::mutex& mtx) noexcept: mtx(&mtx)
            {
                this->mtx->lock();
            }

            xlock_guard_base(const self&) = delete;
            xlock_guard_base(self&&) = delete;

            inline __attribute__((always_inline)) ~xlock_guard_base() noexcept
            {
                this->mtx->unlock();
            }

            self& operator =(const self&) = delete;
            self& operator =(self&&) = delete;
    };

    template <class Lock>
    struct xlock_guard_chooser{};

    template <>
    struct xlock_guard_chooser<std::atomic_flag>
    {
        using type = xlock_guard_base<std::atomic_flag>;
    };

    template <>
    struct xlock_guard_chooser<fair_atomic_flag>
    {
        using type = xlock_guard_base<fair_atomic_flag>;
    };

    template <>
    struct xlock_guard_chooser<std::mutex>
    {
        using type = std::lock_guard<std::mutex>;
    };

    template <class Lock>
    using xlock_guard = typename xlock_guard_chooser<Lock>::type;
}

#endif