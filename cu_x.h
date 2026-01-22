//HEADER_CONTROL 0

#ifndef __CU_X_H__
#define __CU_X_H__

#define DEVICE_IDENTIFIER inline

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <thread>
#include <memory>
#include <cuda_runtime.h>

namespace cu_x
{
    DEVICE_IDENTIFIER void panic_cuda_trap()
    {
        assert(false);
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    static constexpr auto is_pow2(T value) noexcept -> bool
    {
        if (value == 0u)
        {
            return false;
        }

        T value_one = value - 1u; //godzilla
        return (value & value_one) == 0u;
    }

    template <uintptr_t ALIGNMENT_SZ>
    static constexpr auto align(uintptr_t ptr, const std::integral_constant<uintptr_t, ALIGNMENT_SZ>) noexcept -> uintptr_t
    {
        static_assert(is_pow2(ALIGNMENT_SZ));

        uintptr_t fwd_sz    = ALIGNMENT_SZ - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }

    static constexpr auto align(uintptr_t ptr, uintptr_t alignment_sz) noexcept -> uintptr_t
    {
        assert(is_pow2(alignment_sz));

        uintptr_t fwd_sz    = alignment_sz - 1u;
        uintptr_t bit_mask  = ~fwd_sz;
        uintptr_t fwd_ptr   = ptr + fwd_sz;

        return fwd_ptr & bit_mask;
    }   

    template <class T>
    __attribute__((noinline)) DEVICE_IDENTIFIER auto object_malloc(size_t object_count) -> char *
    {
        if (object_count == 0u)
        {
            return nullptr;
        }

        static_assert(sizeof(T) != 0u);
        static_assert(alignof(T) != 0u);

        constexpr size_t FWD_SZ     = alignof(T) - 1u;
        static_assert(FWD_SZ <= std::numeric_limits<uint16_t>::max());

        size_t sz                   = object_count * sizeof(T);
        size_t total_sz             = sz + sizeof(uint16_t);
        size_t aligned_total_sz     = total_sz + FWD_SZ;

        void * void_buf;
        cudaError_t  err            = cudaMalloc(static_cast<void **>(&void_buf), aligned_total_sz);

        if (err != cudaSuccess)
        {
            panic_cuda_trap();
        }

        if (void_buf == nullptr)
        {
            panic_cuda_trap();
        }

        char * buf                  = static_cast<char *>(void_buf);

        char * fwd_buf              = std::next(buf, sizeof(uint16_t));
        char * aligned_fwd_buf      = reinterpret_cast<char *>(align(reinterpret_cast<uintptr_t>(fwd_buf), std::integral_constant<uintptr_t, alignof(T)>{}));

        char * prev_aligned_fwd_buf = std::prev(aligned_fwd_buf, sizeof(uint16_t));
        uint16_t dist               = std::distance(fwd_buf, aligned_fwd_buf);

        std::memcpy(prev_aligned_fwd_buf, &dist, sizeof(uint16_t));

        return aligned_fwd_buf;
    }

    __attribute__((noinline)) DEVICE_IDENTIFIER void object_free(void * buf) noexcept
    {
        if (buf == nullptr)
        {
            return;
        }

        char * char_buf = static_cast<char *>(buf);
        char * prev_buf = std::prev(char_buf, sizeof(uint16_t));

        uint16_t dist;
        std::memcpy(&dist, prev_buf, sizeof(uint16_t));

        char * fwd_buf  = std::prev(char_buf, dist);
        char * org_buf  = std::prev(fwd_buf, sizeof(uint16_t));

        cudaFree(org_buf);
    }

    template <class T>
    class CudaSTLAllocator
    {
        private:

            template <class U>
            friend class CudaSTLAllocator;

        public:

            using value_type = T;
            using pointer = T*;
            using const_pointer = const T*;
            using reference = T&;
            using const_reference = const T&;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;

            template <class U>
            struct rebind
            {
                using other = CudaSTLAllocator<U>;
            };

            constexpr CudaSTLAllocator() noexcept = default;

            DEVICE_IDENTIFIER auto allocate(size_t n) -> T *
            {
                return object_malloc<T>(n);
            }
            
            DEVICE_IDENTIFIER void deallocate(T * ptr, size_t sz)
            {
                object_free(ptr);
            }

            template <class ...Args>
            DEVICE_IDENTIFIER void construct(T * ptr, Args&& ...args)
            {
                new (ptr) T(std::forward<Args>(args)...);
            }

            DEVICE_IDENTIFIER void destroy(T * ptr)
            {
                std::destroy_at(ptr);
            }
    };

    template <class T, class U>
    constexpr auto operator ==(const CudaSTLAllocator<T>& lhs, const CudaSTLAllocator<U>& rhs) noexcept -> bool
    {
        return true;
    }

    template <class T, class U>
    constexpr auto operator !=(const CudaSTLAllocator<T>& lhs, const CudaSTLAllocator<U>& rhs) noexcept -> bool
    {
        return false;
    }

    template <class T>
    using cu_vector = std::vector<T, CudaSTLAllocator>;

    template <class Key, class Value, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = CudaSTLAllocator<std::pair<const Key, Value>>>
    using cu_unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

    auto make_cuda_buffer_from_size(size_t sz) -> std::shared_ptr<char[]>
    {
        if (sz == 0u)
        {
            return nullptr;
        }

        void * cuda_buf = nullptr;
        cudaError_t err = cudaMalloc(static_cast<void **>(&cuda_buf), sz);

        if (err != cudaSuccess)
        {
            if (is_mem_exhaustion_error(err))
            {
                throw std::bad_alloc();
            }

            throw std::runtime_error(cudaGetErrorString(err));
        }

        if (cuda_buf == nullptr)
        {
            throw std::runtime_error("cuda corruption");
        }

        auto destructor = [](char * buf) noexcept
        {
            cudaFree(static_cast<void *>(buf));
        };

        return std::unique_ptr<char[], decltype(destructor)>(static_cast<char *>(cuda_buf), destructor);
    }

    auto make_cuda_buffer_from_host_view(std::string_view host_view) -> std::shared_ptr<char[]>
    {
        std::shared_ptr<char[]> rs  = make_cuda_buffer_from_size(host_view.size());

        if (host_view.size() == 0u)
        {
            return rs;
        }

        cudaError_t err             = cudaMemcpy(rs.get(), host_view.data(), host_view.size(), cudaMemcpyHostToDevice);

        if (err != cudaSuccess)
        {
            throw std::runtime_error(cudaGetErrorString(err));
        }

        return rs;
    }

    auto cuda_to_host_buf(const std::shared_ptr<char[]>& cuda_buf, size_t cuda_buf_sz) -> std::shared_ptr<char[]>
    {
        if (cuda_buf == nullptr)
        {
            if (cuda_buf_sz == 0u)
            {
                return nullptr;
            }
            else
            {
                throw std::invalid_argument("corrupted buffer");
            }
        }

        std::shared_ptr<char[]> rs  = std::make_shared<char[]>(cuda_buf_sz);
        cudaError_t err             = cudaMemcpy(rs.get(), cuda_buf.get(), cuda_buf_sz, cudaMemcpyDeviceToHost);

        if (err != cudaSuccess)
        {
            throw std::runtime_error(cudaGetErrorString(err));
        }

        return rs;
    }

    class CorruptionReportControllerInterface
    {
        public:

            virtual ~CorruptionReportControllerInterface() noexcept = default;
            virtual void set_flag() = 0;
            virtual auto get_and_clear() noexcept -> bool = 0;
    };

    class CorruptionReportCenterInterface
    {
        public:

            virtual ~CorruptionReportCenterInterface() noexcept = default;
            virtual void report() = 0;
    };

    class CorruptionReportController: public virtual CorruptionReportControllerInterface
    {
        private:

            bool is_set;
            std::optional<std::chrono::time_point<std::chrono::steady_clock>> last_clear_time;
            std::chrono::nanoseconds grace_period;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            CorruptionReportController(std::chrono::nanoseconds grace_period): is_set(false),
                                                                               last_clear_time(std::nullopt),
                                                                               grace_period(grace_period),
                                                                               mtx(fair_mutex::make_unique_fair_atomic_flag())
            {
                if (this->grace_period < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad CorruptionReportController's grace period");
                }
            }

            void set_flag()
            {
                using namespace std::chrono;

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->last_clear_time.has_value())
                {
                    time_point<steady_clock> now    = steady_clock::now();
                    nanoseconds dur                 = duration_cast<nanoseconds>(now - this->last_clear_time.value());

                    if (dur > this->grace_period)
                    {
                        this->is_set = true;
                    }
                }
                else
                {
                    this->is_set = true;
                }
            }

            auto get_and_clear() noexcept -> bool
            {
                using namespace std::chrono;

                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                bool old = std::exchange(this->is_set, false);

                if (old)
                {
                    this->last_clear_time = steady_clock::now();
                }
            }
    };

    class CorruptionWorker
    {
        private:

            std::shared_ptr<CorruptionReportControllerInterface> report_controller;
            std::chrono::nanoseconds sleep_dur;
            std::atomic<bool> poison_pill;
            std::atomic<bool> was_ran;

        public:

            CorruptionWorker(std::shared_ptr<CorruptionReportControllerInterface> report_controller,
                             std::chrono::nanoseconds sleep_dur): report_controller(std::move(report_controller)),
                                                                  sleep_dur(sleep_dur),
                                                                  poison_pill(false),
                                                                  was_ran(false){

                if (this->report_controller == nullptr)
                {
                    throw std::invalid_argument("null CorruptionWorker constructor's report controller");
                }

                if (this->sleep_dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad CorruptionWorker's sleep duration");
                }
            }

            void run()
            {
                bool status = this->was_ran.exchange(true, std::memory_order_relaxed);

                if (status)
                {
                    throw std::runtime_error("second CorruptionWorker's run");
                }

                while (true)
                {
                    bool was_poisoned = this->poison_pill.load(std::memory_order_relaxed);

                    if (was_poisoned)
                    {
                        break;
                    }

                    bool flag = this->report_controller->get_and_clear();

                    if (flag == true)
                    {
                        cudaDeviceReset();
                    }

                    std::this_thread::sleep_for(this->sleep_dur);
                }
            }

            void stop() noexcept
            {
                this->poison_pill.exchange(true, std::memory_order_relaxed);
            }
    };

    struct CorruptionWorkerHandle
    {
        std::shared_ptr<CorruptionWorker> worker;
        std::shared_ptr<std::thread> thr;
    };

    static auto make_corruption_worker(std::shared_ptr<CorruptionReportControllerInterface> report_controller,
                                       std::chrono::nanoseconds sleep_dur) -> std::shared_ptr<void>
    {
        const std::chrono::nanoseconds MIN_SLEEP_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(0));
        const std::chrono::nanoseconds MAX_SLEEP_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

        if (report_controller == nullptr)
        {
            throw std::invalid_argument("null report controller");
        }

        if (std::clamp(sleep_dur, MIN_SLEEP_DUR, MAX_SLEEP_DUR) != sleep_dur)
        {
            throw std::invalid_argument("bad sleep duration");
        }

        auto destructor = [](void * arg) noexcept
        {
            CorruptionWorkerHandle * worker_handle = static_cast<CorruptionWorkerHandle *>(arg);
            worker_handle->worker->stop();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            worker_handle->thr->join();

            delete worker_handle;
        };

        std::shared_ptr<CorruptionWorker> corruption_worker = std::make_shared<CorruptionWorker>(report_controller, sleep_dur);
        auto thr_runner = [=]() noexcept
        {
            corruption_worker->run();
        };
        std::shared_ptr<std::thread> thr                    = std::make_shared<std::thread>(thr_runner);
        CorruptionWorkerHandle * corruption_worker_handle;

        try
        {
            corruption_worker_handle = new CorruptionWorkerHandle
            (
                CorruptionWorkerHandle
                {
                    .worker = corruption_worker,
                    .thr    = thr
                }
            );
        }
        catch (...)
        {
            corruption_worker->stop();
            std::atomic_signal_fence(std::memory_order_seq_cst);
            thr->join();
            throw;
        }

        return std::unique_ptr<void, decltype(destructor)>
        (
            static_cast<void *>(corruption_worker_handle),
            destructor
        );
    }

    class CorruptionReportCenter: public virtual CorruptionReportCenterInterface
    {
        private:

            std::shared_ptr<CorruptionReportControllerInterface> controller;
            std::shared_ptr<void> worker;

        public:

            CorruptionReportCenter(std::shared_ptr<CorruptionReportControllerInterface> controller,
                                   std::shared_ptr<void> worker): controller(std::move(controller)),
                                                                  worker(std::move(worker)){}

            void report()
            {
                this->controller->set_flag();
            }
    };

    class CorruptionReportCenterFactory
    {
        private:

            static auto get_corruption_report_controller(std::chrono::nanoseconds grace_period) -> std::unique_ptrCorruptionReportControllerInterface>
            {
                const std::chrono::nanoseconds MIN_GRACE_PERIOD = std::chrono::nanoseconds(0);
                const std::chrono::nanoseconds MAX_GRACE_PERIOD = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

                if (std::clamp(grace_period, MIN_GRACE_PERIOD, MAX_GRACE_PERIOD) != grace_period)
                {
                    throw std::invalid_argument("bad CorruptionReportController factory construction's grace period");
                }

                return std::make_unique<CorruptionReportController>(grace_period);
            }

        public:

            static auto get_corruption_report_center(std::chrono::nanoseconds grace_period,
                                                     std::chrono::nanoseconds worker_check_interval) -> std::unique_ptr<CorruptionReportCenterInterface>
            {
                std::shared_ptr<CorruptionReportControllerInterface> controller = get_corruption_report_controller(grace_period);

                return std::make_unique<CorruptionReportCenterInterface>(controller,
                                                                         make_corruption_worker(controller, worker_check_interval));
            }
    };

    struct Signature{};

    using corruption_instance = stdx::singleton_container<std::unique_ptr<CorruptionReportCenterInterface>, Signature>;

    void init_report_center(std::chrono::nanoseconds grace_period,
                            std::chrono::nanoseconds worker_check_interval)
    {
        corruption_instance::get() = CorruptionReportCenterFactory::get_corruption_report_center(grace_period, worker_check_interval);
    }

    // static volatile int lazy_report_center_initializer = []
    // {
    //     const std::chrono::nanoseconds DEFAULT_GRACE_PERIOD             = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(30));
    //     const std::chrono::nanoseconds DEFAULT_WORKER_CHECK_INTERVAL    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));

    //     init_report_center(DEFAULT_GRACE_PERIOD, DEFAULT_WORKER_CHECK_INTERVAL);

    //     return 1;
    // }();

    void deinit_report_center()
    {
        corruption_instance::get() = nullptr;
    }

    void report_corruption()
    {
        corruption_instance::get()->report();
    }
}

#endif