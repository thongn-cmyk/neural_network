#ifndef __DG_NETWORK_CONCURRENCY_IMPL1_LINUX_H__
#define __DG_NETWORK_CONCURRENCY_IMPL1_LINUX_H__

//define HEADER_CONTROL 1

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
#include "network_exception.h"
#include "stdx.h"

namespace dg_sock::network_concurrency_impl1::daemon_option_ns{

    using daemon_kind_t = uint8_t; 

    enum daemon_option: daemon_kind_t{
        COMPUTING_DAEMON        = 0u,
        TRANSPORTATION_DAEMON   = 1u,
        IO_DAEMON               = 2u,
        HEARTBEAT_DAEMON        = 3u,
        REST_SERVER_DAEMON      = 4u,
        REST_CLIENT_DAEMON      = 5u
    };
} 

namespace dg_sock::network_concurrency_impl1{

    using namespace daemon_option_ns;

    struct StdDaemonRunnableInterface{
        virtual ~StdDaemonRunnableInterface() noexcept = default;
        virtual void infloop() noexcept = 0;
        virtual void signal_abort() noexcept = 0; 
    };

    struct WorkerInterface{
        virtual ~WorkerInterface() noexcept = default;
        virtual bool run_one_epoch() noexcept = 0; //precond - exitable-in-all-scenerios execution
    };

    struct ReschedulerInterface{
        virtual ~ReschedulerInterface() noexcept = default;
        virtual void reschedule() noexcept = 0;
    };

    struct DaemonRunnerInterface{
        virtual ~DaemonRunnerInterface() noexcept = default;
        virtual void set_worker(std::unique_ptr<WorkerInterface>) noexcept = 0;
    };

    struct DaemonDedicatedRunnerInterface: DaemonRunnerInterface{
        virtual ~DaemonDedicatedRunnerInterface() noexcept = default;
        virtual auto id() noexcept -> std::thread::id = 0;
    };

    struct DaemonControllerInterface{
        virtual ~DaemonControllerInterface() noexcept = default;
        virtual auto _register(daemon_kind_t, std::unique_ptr<WorkerInterface>) noexcept -> std::expected<size_t, exception_t> = 0;
        virtual void deregister(size_t) noexcept = 0;
    };

    class RestWorker: public virtual WorkerInterface{

        public:

            bool run_one_epoch() noexcept{

                return false;
            }
    };

    struct WorkerFactory{

        static auto spawn_rest() -> std::unique_ptr<WorkerInterface>{

            return std::make_unique<RestWorker>();
        }
    };

    class SleepyRescheduler: public virtual ReschedulerInterface{

        private:

            std::chrono::nanoseconds sleep_dur;
        
        public: 

            SleepyRescheduler(std::chrono::nanoseconds sleep_dur) noexcept: sleep_dur(std::move(sleep_dur)){}

            void reschedule() noexcept{

                std::this_thread::sleep_for(this->sleep_dur);
            }
    };

    class SleepyYieldRescheduler: public virtual ReschedulerInterface{

        private:

            std::chrono::nanoseconds sleep_dur;
        
        public:

            SleepyYieldRescheduler(std::chrono::nanoseconds sleep_dur) noexcept: sleep_dur(std::move(sleep_dur)){}

            void reschedule() noexcept{

                std::this_thread::sleep_for(this->sleep_dur);
                std::this_thread::yield();
            }
    };

    class StdDaemonRunner: public virtual DaemonRunnerInterface,
                           public virtual StdDaemonRunnableInterface{

        private:

            std::unique_ptr<std::atomic<bool>> poison_pill;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            std::unique_ptr<WorkerInterface> worker;
            std::unique_ptr<ReschedulerInterface> rescheduler; 
            size_t loopchk_sz; 

        public:

            StdDaemonRunner(std::unique_ptr<std::atomic<bool>> poison_pill,
                            std::unique_ptr<stdxx::fair_atomic_flag> mtx,
                            std::unique_ptr<WorkerInterface> worker,
                            std::unique_ptr<ReschedulerInterface> rescheduler,
                            size_t loopchk_sz) noexcept: poison_pill(std::move(poison_pill)),
                                                         mtx(std::move(mtx)),
                                                         worker(std::move(worker)),
                                                         rescheduler(std::move(rescheduler)),
                                                         loopchk_sz(loopchk_sz){}

            void set_worker(std::unique_ptr<WorkerInterface> worker) noexcept{

                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                this->worker = std::move(worker);
            }

            void infloop() noexcept{

                this->poison_pill->exchange(false, std::memory_order_relaxed);

                std::atomic_thread_fence(std::memory_order_seq_cst); //hard-sync
                bool reschedule_on_null = false;

                while (!this->poison_pill->load(std::memory_order_relaxed)){
                    if (reschedule_on_null == true){
                        this->rescheduler->reschedule();
                        reschedule_on_null = false;
                    }

                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (this->worker == nullptr){
                        reschedule_on_null = true;
                        continue;
                    }
                    
                    for (size_t i = 0u; i < this->loopchk_sz; ++i){
                        bool run_flag = this->worker->run_one_epoch();

                        if (!run_flag){
                            this->rescheduler->reschedule();
                            break;
                        }
                    }
                }
            }

            void signal_abort() noexcept{

                this->poison_pill->exchange(true, std::memory_order_relaxed);
            }
    };

    class StdRaiiDaemonRunner: public virtual DaemonDedicatedRunnerInterface{

        private:

            std::shared_ptr<DaemonRunnerInterface> daemon_runner;
            std::shared_ptr<std::thread> thread;

        public:

            StdRaiiDaemonRunner(std::shared_ptr<DaemonRunnerInterface> daemon_runner, 
                                std::shared_ptr<std::thread> thread) noexcept: daemon_runner(std::move(daemon_runner)),
                                                                               thread(std::move(thread)){}


            void set_worker(std::unique_ptr<WorkerInterface> worker) noexcept{

                this->daemon_runner->set_worker(std::move(worker));
            }

            auto id() noexcept -> std::thread::id{
 
                return this->thread->get_id();
            }
    };

    class DaemonController: public virtual DaemonControllerInterface{

        private:

            std::unordered_map<daemon_kind_t, std::vector<size_t>> daemon_id_map;
            std::unordered_map<size_t, std::unique_ptr<DaemonRunnerInterface>> id_runner_map;
            std::unique_ptr<std::mutex> mtx;

        public:

            DaemonController(std::unordered_map<daemon_kind_t, std::vector<size_t>> daemon_id_map,
                             std::unordered_map<size_t, std::unique_ptr<DaemonRunnerInterface>> id_runner_map,
                             std::unique_ptr<std::mutex> mtx) noexcept: daemon_id_map(std::move(daemon_id_map)),
                                                                        id_runner_map(std::move(id_runner_map)),
                                                                        mtx(std::move(mtx)){}

            auto _register(daemon_kind_t daemon_kind, std::unique_ptr<WorkerInterface> worker) noexcept -> std::expected<size_t, exception_t>{
                
                std::lock_guard<std::mutex> lck_grd(*this->mtx);
                auto map_ptr = this->daemon_id_map.find(daemon_kind);

                if (worker == nullptr){
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (map_ptr == this->daemon_id_map.end()){
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (map_ptr->second.size() == 0u){
                    return std::unexpected(dg_sock::network_exception::NO_DAEMON_RUNNER_AVAILABLE);
                }

                size_t id = map_ptr->second.back();
                map_ptr->second.pop_back(); 
                this->id_runner_map[id]->set_worker(std::move(worker));
                
                auto rs = this->encode(id, daemon_kind);

                return rs;
            }

            void deregister(size_t encoded) noexcept{

                std::lock_guard<std::mutex> lck_grd(*this->mtx);
                auto [id, daemon_kind]  = this->decode(encoded);

                this->daemon_id_map[daemon_kind].push_back(id);
                this->id_runner_map[id]->set_worker(nullptr);
            }
        
        private:

            auto encode(size_t id, daemon_kind_t daemon_kind) noexcept -> size_t{

                static_assert(std::is_unsigned_v<daemon_kind_t>);
                constexpr size_t LOW_BIT_SZ = sizeof(daemon_kind_t) * CHAR_BIT;

                return (id << LOW_BIT_SZ) | static_cast<size_t>(daemon_kind); 
            }

            auto decode(size_t encoded) noexcept -> std::pair<size_t, daemon_kind_t>{
                
                static_assert(std::is_unsigned_v<daemon_kind_t>);
                constexpr size_t LOW_BIT_SZ = sizeof(daemon_kind_t) * CHAR_BIT;
                size_t id                   = encoded >> LOW_BIT_SZ;
                daemon_kind_t daemon_kind   = stdxx::low_bit<LOW_BIT_SZ>(encoded);

                return {id, daemon_kind};
            }
    };

    struct ReschedulerFactory{

        static auto spawn_sleepy_yield_rescheduler(std::chrono::nanoseconds sleep_dur) -> std::unique_ptr<ReschedulerInterface>{

            return std::make_unique<SleepyYieldRescheduler>(sleep_dur);
        }

        static auto spawn_sleepy_rescheduler(std::chrono::nanoseconds sleep_dur) -> std::unique_ptr<ReschedulerInterface>{

            return std::make_unique<SleepyRescheduler>(sleep_dur);
        }
    };

    // static void dg_legacy_cpuset_free(cpu_set_t * cpu_set) noexcept{

    //     CPU_FREE(cpu_set);
    // } 

    // using dg_legacy_cpuset_free_t = void (*)(cpu_set_t *) noexcept; 

    // struct NonLegacyPosixCpuSet{
    //     std::unique_ptr<cpu_set_t, dg_legacy_cpuset_free_t> legacy_cpusetup;
    //     size_t alloc_sz;
    // };

    // struct NonLegacyPosixCPUSetController{

    //     static auto make_cpuset(size_t cpu_sz) -> std::unique_ptr<NonLegacyPosixCpuSet>{

    //         std::unique_ptr<cpu_set_t, dg_legacy_cpuset_free_t> legacy_cpusetup = {CPU_ALLOC(cpu_sz), dg_legacy_cpuset_free};

    //         if (!legacy_cpusetup){
    //             dg_sock::network_exception::throw_exception(dg_sock::network_exception::OUT_OF_MEMORY);
    //         }
            
    //         size_t alloc_sz = CPU_ALLOC_SIZE(cpu_sz);
    //         CPU_ZERO_S(alloc_sz, legacy_cpusetup.get()); 

    //         return std::make_unique<NonLegacyPosixCpuSet>(NonLegacyPosixCpuSet{std::move(legacy_cpusetup), alloc_sz});
    //     }

    //     static void add_cpu_to_cpuset(NonLegacyPosixCpuSet * dst, int cpu_id){

    //         CPU_SET_S(cpu_id, dst->alloc_sz, dst->legacy_cpusetup.get());
    //     }
    // };

    // struct StdThreadFactory{

    //     template <class T>
    //     static void internal_pthread_setaffinity_np(T&& thr_handle, NonLegacyPosixCpuSet * cpusetp){

    //         int err = pthread_setaffinity_np(std::forward<T>(thr_handle), cpusetp->alloc_sz, cpusetp->legacy_cpusetup.get());
            
    //         if (err != 0){
    //             if (err == EFAULT){
    //                 dg_sock::network_exception::throw_exception(dg_sock::network_exception::PTHREAD_EFAULT);
    //             }
    //             if (err == EINVAL){
    //                 dg_sock::network_exception::throw_exception(dg_sock::network_exception::PTHREAD_EINVAL);
    //             }
    //             if (err == ESRCH){
    //                 dg_sock::network_exception::throw_exception(dg_sock::network_exception::PTHREAD_ESRCH);
    //             }

    //             dg_sock::network_exception::throw_exception(dg_sock::network_exception::UNIDENTIFIED_EXCEPTION);
    //         }
    //     }

    //     static auto spawn_thread(std::shared_ptr<StdDaemonRunnableInterface> runnable, std::vector<int> cpu_vec) -> std::shared_ptr<std::thread>{

    //         if (runnable == nullptr){
    //             dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
    //         }

    //         if (cpu_vec.empty()){
    //             dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
    //         }

    //         auto executable = [=]() noexcept{
    //             runnable->infloop();
    //         };

    //         auto destructor = [=](std::thread * thr_ins) noexcept{
    //             runnable->signal_abort();
    //             thr_ins->join();
    //             delete thr_ins;
    //         };

    //         auto thr_instance   = std::unique_ptr<std::thread, decltype(destructor)>(new std::thread(std::move(executable)), std::move(destructor));
    //         auto cpu_set        = NonLegacyPosixCPUSetController::make_cpuset(cpu_vec.size()); 

    //         for (int cpu_id: cpu_vec){
    //             NonLegacyPosixCPUSetController::add_cpu_to_cpuset(cpu_set.get(), cpu_id);
    //         }
            
    //         internal_pthread_setaffinity_np(thr_instance->native_handle(), cpu_set.get());
    //         return thr_instance;
    //     }

    //     static auto spawn_thread(std::shared_ptr<StdDaemonRunnableInterface> runnable) -> std::shared_ptr<std::thread>{

    //         if (runnable == nullptr){
    //             dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
    //         }

    //         auto executable = [=]() noexcept{
    //             runnable->infloop();
    //         };

    //         auto destructor = [=](std::thread * thr_ins) noexcept{
    //             runnable->signal_abort();
    //             thr_ins->join();
    //             delete thr_ins;
    //         };

    //         return std::unique_ptr<std::thread, decltype(destructor)>(new std::thread(std::move(executable)), std::move(destructor));
    //     }
    // };

    // struct DaemonRunnerFactory{

    //     static auto spawn_std_daemon_affine_runner(std::vector<int> cpu_set) -> std::unique_ptr<DaemonDedicatedRunnerInterface>{

    //         using namespace std::chrono_literals;

    //         const size_t LOOPCHK_SZ = 32u;

    //         auto rescheduler    = ReschedulerFactory::spawn_sleepy_rescheduler(10ms);
    //         auto mtx            = std::make_unique<stdxx::fair_atomic_flag>();
    //         stdxx::inplace_make_fair_atomic_flag(*mtx);
    //         auto poison_pill    = std::make_unique<std::atomic<bool>>();
    //         auto daemon_runner  = std::make_shared<StdDaemonRunner>(std::move(poison_pill), std::move(mtx), nullptr, std::move(rescheduler), LOOPCHK_SZ);
    //         auto thr_instance   = StdThreadFactory::spawn_thread(daemon_runner, cpu_set); 
    //         auto raii_runner    = std::make_unique<StdRaiiDaemonRunner>(daemon_runner, std::move(thr_instance)); 

    //         return raii_runner;
    //     } 

    //     static auto spawn_std_daemon_runner() -> std::unique_ptr<DaemonDedicatedRunnerInterface>{

    //         using namespace std::chrono_literals;

    //         const size_t LOOPCHK_SZ = 32u;

    //         auto rescheduler    = ReschedulerFactory::spawn_sleepy_rescheduler(10ms);
    //         auto mtx            = std::make_unique<stdxx::fair_atomic_flag>();
    //         stdxx::inplace_make_fair_atomic_flag(*mtx);
    //         auto poison_pill    = std::make_unique<std::atomic<bool>>();
    //         auto daemon_runner  = std::make_shared<StdDaemonRunner>(std::move(poison_pill), std::move(mtx), nullptr, std::move(rescheduler), LOOPCHK_SZ);
    //         auto thr_instance   = StdThreadFactory::spawn_thread(daemon_runner);
    //         auto raii_runner    = std::make_unique<StdRaiiDaemonRunner>(daemon_runner, std::move(thr_instance)); 

    //         return raii_runner;
    //     }
    // };

    // struct ControllerFactory{

    //     static auto spawn_daemon_controller(std::vector<std::pair<std::unique_ptr<DaemonRunnerInterface>, daemon_kind_t>> runner_kind_vec) -> std::unique_ptr<DaemonControllerInterface>{

    //         std::unordered_map<daemon_kind_t, std::vector<size_t>> kind_id_map{};
    //         std::unordered_map<size_t, std::unique_ptr<DaemonRunnerInterface>> id_runner_map{};
    //         size_t id_sz{}; 

    //         for (auto& vec_pair: runner_kind_vec){                
    //             auto runner         = std::move(std::get<0>(vec_pair));
    //             daemon_kind_t kind  = std::get<1>(vec_pair);
    //             size_t id           = id_sz;

    //             if (runner == nullptr){
    //                 dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
    //             }

    //             kind_id_map[kind].push_back(id);
    //             id_runner_map.emplace(std::make_pair(id, std::move(runner)));
    //             id_sz += 1;
    //         }
            
    //         auto mtx = std::make_unique<std::mutex>(); 
    //         return std::make_unique<DaemonController>(std::move(kind_id_map), std::move(id_runner_map), std::move(mtx));
    //     }
    // };
} 

#endif