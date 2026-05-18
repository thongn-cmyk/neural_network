#ifndef __MAIN_SERVICE_H__
#define __MAIN_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include "main_broker.h"
#include "main_service_id.h"
#include "thread_service.h"
#include <stl_extension/stdx.h>

namespace main_service
{
    struct Signature{};

    using SingletonObject = stdx::singleton_container<std::unique_ptr<main_service::main_broker::BrokerageController>, Signature>;

    auto get_instance() -> main_service::main_broker::BrokerageController * 
    {
        if (SingletonObject::get() == nullptr)
        {
            std::abort();
        }

        return SingletonObject::get().get();
    }

    void _dispose_thread(std::thread * thr) noexcept
    {
        using namespace main_service::thread_service;

        ThreadDisposableArgument arg
        {
            .thr = thr
        };

        get_instance()->ask(std::make_shared<main_service::thread_service::ThreadDisposable>(std::move(arg)));
    }

    class LocalThreadDisposer: public virtual main_service::thread_service::ThreadDisposerInterface
    {
        public:

            void dispose_thread(std::thread * thr) noexcept
            {
                _dispose_thread(thr);
            }
    };

    void init()
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = std::make_unique<main_service::main_broker::BrokerageController>();
        SingletonObject::get()->add_resolver(main_service::THREAD_BROKERAGE_IDENTIFIER, std::make_unique<main_service::thread_service::ThreadBroker<>>(std::make_shared<LocalThreadDisposer>()));
        SingletonObject::get()->add_resolver(main_service::THREAD_DISPOSABLE_IDENTIFIER, std::make_unique<main_service::thread_service::ThreadDisposer>());
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = nullptr;
    }

    auto broke_thread(const std::shared_ptr<main_service::thread_service::TaskInterface>& task) -> std::shared_ptr<std::thread>
    {
        using namespace main_service::thread_service;

        if (task == nullptr)
        {
            throw std::invalid_argument("invalid task, null task");
        }

        std::shared_ptr<std::thread> rs{};

        ThreadResolvableArgument arg
        {
            .dst    = &rs,
            .task   = task
        };

        get_instance()->ask(std::make_shared<main_service::thread_service::ThreadResolvable>(std::move(arg)));

        return rs;
    }

    void main_subscribe() 
    {
        get_instance()->main_subscribe();
    }
}

#endif