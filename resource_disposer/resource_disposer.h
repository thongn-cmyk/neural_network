#ifndef __RESOURCE_DISPOSER_H__
#define __RESOURCE_DISPOSER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <atomic>
#include <concurrency_base/concurrency_base.h>
#include <stl_extension/stdx.h>
#include <concurrent_queue/bounded_queue.h>

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

    class DisposableContainer: public virtual DisposableContainerInterface
    {
        private:

            concurrent_queue::bounded_queue::BoundedQueue<std::shared_ptr<DisposableInterface>> base;
        
        public:

            DisposableContainer(size_t container_sz): base(container_sz){}

            void push(const std::shared_ptr<DisposableInterface>& disposable)
            {
                if (disposable == nullptr)
                {
                    return;
                }

                auto tmp = disposable;
                this->base.push(std::move(tmp));
            }

            auto pop() noexcept -> std::shared_ptr<DisposableInterface>
            {
                std::optional<std::shared_ptr<DisposableInterface>> result = this->base.pop();

                if (!result.has_value())
                {
                    return nullptr;
                }

                return std::move(result.value());
            }

            void poison() noexcept
            {
                this->base.poison();
            }
    };

    class Disposer: public virtual DisposerInterface
    {
        private:

            std::shared_ptr<DisposableContainerInterface> disposable_container;
            std::shared_ptr<void> daemon;

            static inline constexpr size_t DISPOSABLE_CONTAINER_SZ = size_t{1} << 4;

        public:

            Disposer()
            {
                std::shared_ptr<DisposableContainerInterface> container     = std::make_shared<DisposableContainer>(DISPOSABLE_CONTAINER_SZ);
                std::unique_ptr<concurrency_base::WorkerInterface> worker   = std::make_unique<DisposerWorker>(container);
                std::expected<std::shared_ptr<void>, exception_t> _daemon   = concurrency_base::daemon_saferegister(concurrency_base::RESOURCE_DISPOSER_DAEMON, std::move(worker));

                if (!_daemon.has_value())
                {
                    common_exception::throw_exception(_daemon.error());
                }

                this->disposable_container  = std::move(container);
                this->daemon                = std::move(_daemon.value());
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