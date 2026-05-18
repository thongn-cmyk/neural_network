#ifndef __MAIN_THREAD_SERVICE_H__
#define __MAIN_THREAD_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include "resolvable_interface.h"
#include "resolver_interface.h"
#include <memory>
#include <common_exception/common_exception.h>
#include <common_exception/cancellation_token.h>
#include <logging_subsystem/logging_subsystem.h>
#include "main_service_id.h"

namespace main_service::thread_service
{
    struct TaskInterface
    {
        virtual ~TaskInterface() noexcept = default;

        virtual void run(common_exception::CancellationTokenInterface& cancellation_token) noexcept = 0;
    };

    struct ThreadResolvableArgument
    {
        std::shared_ptr<std::thread> * dst;
        std::shared_ptr<TaskInterface> task;
    };

    class ThreadResolvable: public virtual main_service::main_broker::Resolvable
    {
        private:

            ThreadResolvableArgument arg;

        public:

            ThreadResolvable(ThreadResolvableArgument arg): arg(std::move(arg)){}

            auto get_argument() noexcept -> ThreadResolvableArgument&
            {
                return this->arg;
            }

            auto get_resolvable_id() noexcept -> thread_service_id_t
            {
                return main_service::THREAD_BROKERAGE_IDENTIFIER;
            }
    };

    struct ThreadDisposableArgument
    {
        std::thread * thr;
    };

    class ThreadDisposable: public virtual main_service::main_broker::Resolvable
    {
        private:

            ThreadDisposableArgument arg;
        
        public:

            ThreadDisposable(ThreadDisposableArgument arg): arg(std::move(arg)){}

            auto get_argument() noexcept -> ThreadDisposableArgument&
            {
                return this->arg;
            }

            auto get_resolvable_id() noexcept -> thread_service_id_t
            {
                return main_service::THREAD_DISPOSABLE_IDENTIFIER;
            }
    };

    class ThreadDisposerInterface
    {
        public:

            virtual ~ThreadDisposerInterface() noexcept = default;

            virtual void dispose_thread(std::thread * thr) noexcept = 0;
    };

    //it's circular... I would understand but we don't care about that for now

    template <class Allocator = std::allocator<std::thread>>
    class ThreadBroker: public virtual main_service::main_broker::ResolverInterface
    {
        private:

            std::shared_ptr<ThreadDisposerInterface> thread_disposer;
            std::shared_ptr<Allocator> allocator;

        public:

            ThreadBroker(std::shared_ptr<ThreadDisposerInterface> thread_disposer_arg,
                         const Allocator& allocator_arg = Allocator{})
            {
                if (thread_disposer_arg == nullptr)
                {
                    throw std::invalid_argument("bad thread disposer argument, null");
                }

                this->thread_disposer   = thread_disposer_arg;
                this->allocator         = std::make_shared<Allocator>(allocator_arg);
            }

            void resolve(std::shared_ptr<main_service::main_broker::Resolvable> resolvable)
            {
                std::shared_ptr<ThreadResolvable> thread_resolvable = std::dynamic_pointer_cast<ThreadResolvable>(resolvable);

                if (thread_resolvable == nullptr)
                {
                    throw std::invalid_argument("bad resolvable, null");
                }

                std::shared_ptr<common_exception::CancellationToken> cancellation_token = std::make_shared<common_exception::CancellationToken>();
                ThreadResolvableArgument& arg = thread_resolvable->get_argument();

                auto lambda_task = [cancellation_token, task = arg.task]() noexcept
                {
                    try
                    {
                        task->run(*cancellation_token);
                    }
                    catch (...)
                    {
                        logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("main_brokerage")
                                                                                       .topic("thread_brokerage")
                                                                                       .message(std::current_exception())
                                                                                       .error());
                    }
                };

                auto thread_destructor = [cancellation_token,
                                          deallocator   = this->allocator,
                                          disposer      = this->thread_disposer](std::thread * thr) noexcept
                {
                    cancellation_token->cancel();
                    disposer->dispose_thread(thr);
                    deallocator->deallocate(thr, 1u);
                };

                std::thread * mem  = this->allocator->allocate(1u);

                try
                {
                    *arg.dst = std::unique_ptr<std::thread, decltype(thread_destructor)>(new (mem) std::thread(std::move(lambda_task)), thread_destructor);
                }
                catch (...)
                {
                    this->allocator->deallocate(mem, 1u);
                    throw;
                }
            }
    };

    class ThreadDisposer: public virtual main_service::main_broker::ResolverInterface
    {
        public:

            void resolve(std::shared_ptr<main_service::main_broker::Resolvable> resolvable)
            {
                std::shared_ptr<ThreadDisposable> thread_disposable = std::dynamic_pointer_cast<ThreadDisposable>(resolvable);

                if (thread_disposable == nullptr)
                {
                    throw std::invalid_argument("bad resolvable, null");
                }

                ThreadDisposableArgument& arg = thread_disposable->get_argument();

                if (arg.thr == nullptr)
                {
                    return;
                }

                try
                {
                    if (arg.thr->joinable())
                    {
                        arg.thr->join();
                    }
                }
                catch (...)
                {
                    std::abort();
                }

                std::destroy_at(arg.thr);
            }
    };
}

#endif