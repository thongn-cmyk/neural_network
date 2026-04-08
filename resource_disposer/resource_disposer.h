#ifndef __RESOURCE_DISPOSER_H__
#define __RESOURCE_DISPOSER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <atomic>
#include <concurrency_base/concurrency_base.h>
#include <stl_extension/stdx.h>

namespace resource_disposer
{
    class DisposableInterface
    {
        public:

            virtual ~DisposableInterface() noexcept = default;
    };

    class DisposableContainerInterface
    {
        public:

            virtual ~DisposableContainerInterface() noexcept = default;

            virtual void push(const std::shared_ptr<DisposableInterface>& disposable) = 0;
            virtual auto pop() noexcept -> std::shared_ptr<DisposableInterface> = 0;
            virtual void poison() noexcept = 0;
    };

    class DisposerInterface
    {
        public:

            virtual ~DisposerInterface() noexcept = default;

            virtual void dispose(const std::shared_ptr<DisposableInterface>& disposable) noexcept = 0;
    };

    class DisposerWorker: public virtual concurrency_base::WorkerInterface
    {
        private:

            std::shared_ptr<DisposableContainerInterface> disposable_container;

        public:

            DisposerWorker(const std::shared_ptr<DisposableContainerInterface>& disposable_container): disposable_container(disposable_container){}

            auto run_one_epoch() noexcept -> bool
            {
                this->disposable_container->pop();
                return true;
            }
    };

    class Disposer: public virtual DisposerInterface
    {
        private:

            std::shared_ptr<DisposableContainerInterface> disposable_container;
            std::shared_ptr<void> daemon;
        
        public:

            Disposer()
            {

            }

            ~Disposer() noexcept
            {
                this->disposable_container->poison();
                this->daemon = nullptr;
            }

            void dispose(const std::shared_ptr<DisposableInterface>& disposable) noexcept
            {
                if (disposable == nullptr)
                {
                    return;
                }

                this->disposable_container->push(disposable);
            }
    };

    struct Signature{};

    using SingletonObject = stdx::singleton_container<std::unique_ptr<DisposerInterface>, Signature>;

    void init()
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = std::make_unique<Disposer>();
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = nullptr;
    }

    auto get_instance() noexcept -> DisposerInterface * 
    {
        if (SingletonObject::get() == nullptr)
        {
            std::abort();
        }

        return SingletonObject::get().get();
    }

    void dispose(const std::shared_ptr<DisposableInterface>& disposable) noexcept
    {
        get_instance()->dispose(disposable);
    }

    template <class T>
    class DisposableWrapper: public virtual DisposableInterface
    {
        private:

            T resource;
        
        public:

            DisposableWrapper(T resource): resource(std::move(resource)){}
    };
}

#endif