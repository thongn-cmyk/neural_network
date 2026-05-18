#ifndef __DG_NETWORK_CONCURRENCY_X_H__
#define __DG_NETWORK_CONCURRENCY_X_H__

//define HEADER_CONTROL 5

#include <memory>
#include <tuple>
#include <atomic>
#include <utility>
#include <type_traits>
#include "network_type_traits_x.h"
#include "network_randomizer.h"
#include <chrono>

namespace dg_sock::network_concurrency_infretry_x{

    struct Executable{
        virtual ~Executable() noexcept = default;
        virtual auto run() noexcept -> bool = 0;
    };

    struct ExecutorInterface{
        virtual ~ExecutorInterface() noexcept = default;
        virtual void exec(Executable&) noexcept = 0;
    };

    struct ExecutorDestructorInterface{
        virtual ~ExecutorDestructorInterface() noexcept = default;
    };

    template <size_t DICE_SZ>
    class StdExecutor: public virtual ExecutorInterface{

        private:

            std::shared_ptr<std::atomic<bool>> poison_pill;
            std::chrono::nanoseconds wait_dur; 

        public:

            StdExecutor(std::shared_ptr<std::atomic<bool>> poison_pill,
                        std::chrono::nanoseconds wait_dur): poison_pill(std::move(poison_pill)),
                                                            wait_dur(std::move(wait_dur)){}

            void exec(Executable& executor) noexcept{

                while (true){
                    if (this->load_poison_pill()){
                        return;
                    }

                    if (executor.run()){
                        return;
                    }

                    std::this_thread::sleep_for(this->wait_dur);
                }
            }
        
        private:

            auto load_poison_pill() const noexcept -> bool{

                size_t dice_idx = dg_sock::network_randomizer::randomize_xrange(std::integral_constant<size_t, DICE_SZ>{});

                if (dice_idx == 0u){
                    return this->poison_pill->load(std::memory_order_relaxed);
                }

                return false;
            }
    };

    class StdExecutorDestructor: public virtual ExecutorDestructorInterface{

        private:

            std::shared_ptr<std::atomic<bool>> eventloop_poison_pill;
        
        public:

            StdExecutorDestructor(std::shared_ptr<std::atomic<bool>> eventloop_poison_pill) noexcept: eventloop_poison_pill(std::move(eventloop_poison_pill)){}

            ~StdExecutorDestructor() noexcept{

                this->eventloop_poison_pill->exchange(true, std::memory_order_relaxed);
            }
    };

    template <class Lambda>
    class ExecutableWrapper: public virtual Executable{

        private:

            Lambda lambda;
        
        public:

            static_assert(dg_sock::network_type_traits_x::is_base_type_v<Lambda>);
            static_assert(std::is_nothrow_destructible_v<Lambda>);
            static_assert(std::is_nothrow_move_constructible_v<Lambda>);
            static_assert(std::is_nothrow_invocable_v<Lambda>);
            static_assert(std::is_same_v<bool, decltype(std::declval<Lambda>()())>);

            ExecutableWrapper(Lambda lambda) noexcept: lambda(std::move(lambda)){}

            auto run() noexcept -> bool{

                return this->lambda();
            }
    };

    auto get_infretry_machine(std::chrono::nanoseconds wait_dur) -> std::pair<std::unique_ptr<ExecutorInterface>, std::unique_ptr<ExecutorDestructorInterface>>{

        constexpr size_t DICE_SZ = size_t{1} << 8;
        
        std::shared_ptr<std::atomic<bool>> interceptor          = std::make_shared<std::atomic<bool>>(bool{false});
        std::unique_ptr<ExecutorInterface> executor             = std::make_unique<StdExecutor<DICE_SZ>>(interceptor, wait_dur);
        std::unique_ptr<ExecutorDestructorInterface> destructor = std::make_unique<StdExecutorDestructor>(interceptor);

        return std::make_pair(std::move(executor), std::move(destructor));   
    } 
} 

#endif