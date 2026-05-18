#ifndef __DG_NETWORK_KERNEL_MAILBOX_IMPL1_ALLOCATION_H__
#define __DG_NETWORK_KERNEL_MAILBOX_IMPL1_ALLOCATION_H__

#include "network_kernel_allocator.h"

namespace dg_sock::network_kernel_allocator_singleton
{
    struct Config
    {
        size_t total_mempiece_count;
        size_t mempiece_sz;
        size_t affined_refill_sz;
        size_t affined_mem_vec_capacity;
        size_t affined_free_vec_capacity;
    };

    template <class Signature>
    struct AllocatorInstance
    {
        using singleton_object = stdxx::singleton<Signature, std::unique_ptr<dg_sock::network_kernel_allocator::AllocatorInterface<dg_sock::network_kernel_allocator::AffinedMapAllocator>>>; 
        using self = AllocatorInstance;

        static void init(Config config)
        {
            using namespace dg_sock::network_kernel_allocator;

            std::unique_ptr<BatchAllocatorInterface> base_allocator = network_kernel_allocator::ComponentFactory::make_batch_allocator(config.total_mempiece_count,
                                                                                                                                       config.mempiece_sz);

            singleton_object::get() = network_kernel_allocator::ComponentFactory::make_affined_map_allocator(std::move(base_allocator),
                                                                                                             config.affined_refill_sz,
                                                                                                             config.affined_mem_vec_capacity,
                                                                                                             config.affined_free_vec_capacity);
        }

        static void deinit() noexcept
        {
            singleton_object::get() = nullptr;
        }

        static auto malloc(size_t byte_sz) noexcept -> std::expected<void *, exception_t>
        {
            if constexpr(DEBUG_MODE_FLAG)
            {
                if (singleton_object::get() == nullptr)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }            
            }

            if (byte_sz == 0u)
            {
                return std::add_pointer_t<void>(nullptr);
            }

            if (byte_sz > singleton_object::get()->malloc_size())
            {
                return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            return singleton_object::get()->malloc();
        }

        static auto realloc(void * old_ptr, size_t new_sz) noexcept -> std::expected<void *, exception_t>
        {
            if constexpr(DEBUG_MODE_FLAG)
            {
                if (singleton_object::get() == nullptr)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }

            if (old_ptr == nullptr)
            {
                return self::malloc(new_sz);
            }

            if (new_sz > singleton_object::get()->malloc_size())
            {
                return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            return old_ptr;
        }

        static void free(void * mem_ptr) noexcept
        {
            if constexpr(DEBUG_MODE_FLAG)
            {
                if (singleton_object::get() == nullptr)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }

            if (mem_ptr == nullptr)
            {
                return;
            }

            singleton_object::get()->free(mem_ptr);
        }
    };

    template <class Signature>
    struct AllocatorSingletonInstance
    {
        using singleton_object = stdxx::singleton<Signature, std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>>;

        static void init(Config config)
        {
            using namespace dg_sock::network_kernel_allocator;

            std::unique_ptr<BatchAllocatorInterface> base_allocator = network_kernel_allocator::ComponentFactory::make_batch_allocator(config.total_mempiece_count,
                                                                                                                                       config.mempiece_sz);

            singleton_object::get() = network_kernel_allocator::ComponentFactory::make_affined_map_allocator(std::move(base_allocator),
                                                                                                             config.affined_refill_sz,
                                                                                                             config.affined_mem_vec_capacity,
                                                                                                             config.affined_free_vec_capacity);
        }

        static void deinit()
        {
            singleton_object::get() = nullptr;
        }

        static auto get() noexcept -> const std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>&
        {
            return singleton_object::get();
        }
    };

    class UnsafeSingletonStorage
    {
        private:

            dg_sock::network_kernel_allocator::BaseAllocatorInterface * allocator;
        
        public:

            UnsafeSingletonStorage(const std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>& allocator): allocator(allocator.get())
            {
                if (this->allocator == nullptr)
                {
                    throw std::invalid_argument("bad allocator, null allocator");
                }
            }

            auto get() const noexcept -> dg_sock::network_kernel_allocator::BaseAllocatorInterface *
            {
                return this->allocator;
            }
    };

    class SafeSingletonStorage
    {
        private:

            std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface> allocator;
        
        public:

            SafeSingletonStorage(const std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>& allocator): allocator(allocator)
            {
                if (this->allocator == nullptr)
                {
                    throw std::invalid_argument("bad allocator, null allocator");
                }
            }

            auto get() const noexcept -> const std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>&
            {
                return this->allocator;
            }
    };

    template <bool IS_SAFE_SINGLETON>
    using SingletonStorage = std::conditional_t<IS_SAFE_SINGLETON,
                                                SafeSingletonStorage,
                                                UnsafeSingletonStorage>;

    template <class AllocatorSingletonFactory, bool IS_SAFE_SINGLETON = true>
    class SingletonPolymorphicAllocator: private SingletonStorage<IS_SAFE_SINGLETON>
    {  
        private:

            using Base = SingletonStorage<IS_SAFE_SINGLETON>;

        public:

            SingletonPolymorphicAllocator(): Base(AllocatorSingletonFactory::get()){}

            SingletonPolymorphicAllocator(const std::shared_ptr<dg_sock::network_kernel_allocator::BaseAllocatorInterface>& allocator): Base(allocator){}

            auto malloc(size_t byte_sz) const noexcept -> std::expected<void *, exception_t>
            {
                if (byte_sz == 0u)
                {
                    return std::add_pointer_t<void>(nullptr);
                }

                if (byte_sz > Base::get()->malloc_size())
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return Base::get()->malloc();
            }

            auto realloc(void * old_ptr, size_t new_sz) const noexcept -> std::expected<void *, exception_t>
            {
                if (old_ptr == nullptr)
                {
                    return this->malloc(new_sz);
                }

                if (new_sz > Base::get()->malloc_size())
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return old_ptr;
            }

            void free(void * mem_ptr) const noexcept
            {
                if (mem_ptr == nullptr)
                {
                    return;
                }

                Base::get()->free(mem_ptr);
            }
    };

    template <typename T, class StatelessAllocator>
    struct StdWrappedAllocator
    {
        using value_type = T;

        constexpr StdWrappedAllocator() = default;

        template <typename ...Args>
        constexpr StdWrappedAllocator(const StdWrappedAllocator<Args...>&) {}

        auto allocate(std::size_t n) const -> T *
        {
            std::expected<void *, exception_t> raw_mem = StatelessAllocator{}.malloc(n * sizeof(T)); 

            if (!raw_mem.has_value())
            {
                dg_sock::network_exception::throw_exception(raw_mem.error());
            }

            if constexpr(DEBUG_MODE_FLAG)
            {
                if (reinterpret_cast<uintptr_t>(raw_mem.value()) % alignof(std::max_align_t) != 0u)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }

            return static_cast<T *>(raw_mem.value());
        }

        void deallocate(T* p, std::size_t n) const 
        {
            StatelessAllocator{}.free(p);
        }

        template <typename U, typename... Args>
        void construct(U* p, Args&&... args) const
        {
            new (p) U(std::forward<Args>(args)...);
        }

        template <typename U>
        void destroy(U* p) const
        {
            std::destroy_at(p);
        }
    };

    template <typename ...Args, typename ...Args1>
    bool operator==(const StdWrappedAllocator<Args...>&, const StdWrappedAllocator<Args1...>&)
    {
        return true;
    }

    template <typename ...Args, typename ...Args1>
    bool operator!=(const StdWrappedAllocator<Args...>&, const StdWrappedAllocator<Args1...>&)
    {
        return false;
    }
}

#endif