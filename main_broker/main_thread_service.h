#ifndef __MAIN_THREAD_SERVICE_H__
#define __MAIN_THREAD_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include "main_broker.h"

namespace main_thread_service
{
    struct TaskInterface
    {
        virtual ~TaskInterface() noexcept = default;
        virtual void run(common_exception::CancellationTokenInterface& cancellation_token) = 0;
    };

    struct ThreadResolvableArgument
    {
        std::shared_ptr<std::thread> * dst;
        std::unique_ptr<TaskInterface> task;
    };

    class ThreadResolvable: public virtual main_broker::Resolvable
    {
        private:

            ThreadResolvableArgument arg;

        public:

            ThreadResolvable(ThreadResolvableArgument arg) noexcept: arg(std::move(arg)){}

            auto get_argument() && noexcept -> ThreadResolvableArgument&&
            {
                return static_cast<ThreadResolvableArgument&&>(this->arg);
            }

            auto get_resolvable_id() noexcept -> uint8_t
            {
                return 0u;
            }
    };

    class ThreadBroker: public virtual main_broker::ResolverInterface
    {
        public:

            void resolve(std::shared_ptr<Resolvable> resolvable)
            {
                std::shared_ptr<ThreadResolvable> thread_resolvable = std::dynamic_pointer_cast<ThreadResolvable>(resolvable);

                if (thread_resolvable == nullptr)
                {
                    throw std::invalid_argument("bad resolvable, null");
                }

                std::shared_ptr<common_exception::CancellationToken> cancellation_token = std::make_shared<common_exception::CancellationToken>();
                ThreadResolvableArgument arg = std::move(*thread_resolvable).get_argument();

                auto lambda_task = [cancellation_token, task = std::move(arg.task)]() noexcept
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

                auto thread_destructor = [cancellation_token](std::thread * thr) noexcept
                {
                    cancellation_token->cancel();
                    thr->join();

                    delete thr;
                };
                
                *arg.dst = std::unique_ptr<std::thread, decltype(thread_destructor)>(new std::thread(std::move(lambda_task)), thread_destructor);
            }
    };
}

#endif