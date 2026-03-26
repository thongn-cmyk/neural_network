#ifndef __DG_SHARED_PTR_H__
#define __DG_SHARED_PTR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stl_extension/stdx.h>
#include "unique_ptr.h"
#include <atomic>

namespace smart_pointer::shared_ptr_implementation
{
    //this should suffice for our use cases, because we are very very very memory-ordering sensitive

    template <class T, class = void>
    struct has_star_operator: std::false_type{};

    template <class T>
    struct has_star_operator<T, std::void_t<decltype(*std::declval<T&>())>>: std::true_type{};

    template <class T>
    static inline constexpr bool has_star_operator_v = has_star_operator<T>::value;

    class PolymorphicDestructor
    {
        public:

            virtual ~PolymorphicDestructor() noexcept = default;
    };

    template <class ...Args>
    using unique_ptr = smart_pointer::unique_ptr_implementation::unique_ptr<Args...>;

    template <class CharMemoryManager>
    struct ControlBlock
    {
        std::atomic<size_t> counter;
        unique_ptr<PolymorphicDestructor, CharMemoryManager> destructor;
    };

    template <class ...Args>
    class UniquePtrDestructor: public virtual PolymorphicDestructor
    {
        private:

            unique_ptr<Args...> resource;

        public:

            UniquePtrDestructor(unique_ptr<Args...>&& resource): resource{std::move(resource)}{}

            auto get() && noexcept -> unique_ptr<Args...>&&
            {
                return static_cast<unique_ptr<Args...>&&>(this->resource);
            }
    };

    template <class T>
    struct is_unique_ptr: std::false_type{};

    template <class ...Args>
    struct is_unique_ptr<smart_pointer::unique_ptr_implementation::unique_ptr<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

    template <class T, class T1>
    struct shared_ptr_allocation
    {
        T ctrl_blk;
        T1 polymorphic_destructor;
    };

    template <class T, class T1, class T2>
    struct shared_ptr_allocation_2
    {
        T ctrl_blk;
        T1 polymorphic_destructor;
        T2 obj;
    };

    struct NoActionCharMemoryDeallocator
    {
        constexpr void deallocate_one(void * memblk) noexcept
        {
            (void) memblk;
        }
    };

    struct one_block_init_tag{};

    template <class T, class CharMemoryManager>
    class shared_ptr
    {
        private:

            template <class U, class U1>
            friend class shared_ptr;

            using self                  = shared_ptr;

            T * obj;
            ControlBlock<NoActionCharMemoryDeallocator> * ctrl_blk;
            CharMemoryManager deallocator;

            struct unique_ptr_init_tag{};

            template <class ...Args, class ResourceManagerLike>
            static constexpr auto make_control_block(unique_ptr<Args...>&& destructor,
                                                     ResourceManagerLike&& resource_manager_like,
                                                     size_t initial_count) -> ControlBlock<NoActionCharMemoryDeallocator> *
            {
                using allocation_t      = shared_ptr_allocation<ControlBlock<NoActionCharMemoryDeallocator>, UniquePtrDestructor<Args...>>;

                void * mem              = resource_manager_like.template allocate_one<allocation_t>();
                void * ctrl_blk_mem     = static_cast<void *>(mem);
                void * destructor_mem   = static_cast<void *>(&reinterpret_cast<allocation_t *>(mem)->polymorphic_destructor);

                auto destructor_arg     = unique_ptr<UniquePtrDestructor<Args...>, NoActionCharMemoryDeallocator>(new (destructor_mem) UniquePtrDestructor<Args...>(std::move(destructor)), NoActionCharMemoryDeallocator{});

                ControlBlock<NoActionCharMemoryDeallocator> * tmp_ctrl_blk = new (ctrl_blk_mem) ControlBlock<NoActionCharMemoryDeallocator>
                {
                    .counter    = std::atomic<size_t>(initial_count),
                    .destructor = std::move(destructor_arg)
                };

                return tmp_ctrl_blk;
            }

            template <class T1, class OtherCharMemoryManager>
            constexpr void unique_to_shared_initialize(unique_ptr<T1, OtherCharMemoryManager>&& other)
            {
                if (other == nullptr)
                {
                    this->obj           = nullptr;
                    this->ctrl_blk      = nullptr;

                    return;
                }

                auto resource_ptr       = other.get();
                auto resource_manager   = other.get_deleter();
                this->ctrl_blk          = this->make_control_block(std::move(other), resource_manager.value(), 1);
                this->obj               = resource_ptr;
                this->deallocator       = std::move(resource_manager.value());
            }

            template <class T1, class CharMemoryManagerLike = CharMemoryManager>
            constexpr void raw_unique_initialize(T1 * obj_arg,
                                                 const CharMemoryManagerLike& mem_manager = CharMemoryManagerLike{})
            {
                unique_ptr<T, CharMemoryManager> unique_resource(obj_arg, mem_manager);

                try
                {
                    this->unique_to_shared_initialize(std::move(unique_resource));
                }
                catch (...)
                {
                    unique_resource.release();
                    throw;
                }
            }

            template <class T1, class OtherCharMemoryManager>
            constexpr shared_ptr(unique_ptr_init_tag, unique_ptr<T1, OtherCharMemoryManager>&& other)
            {
                this->unique_to_shared_initialize(std::move(other));
            }

        public:

            using pointer       = T *;
            using element_type  = T;

            constexpr shared_ptr(): obj(nullptr),
                                    ctrl_blk(nullptr),
                                    deallocator(){}

            constexpr shared_ptr(std::nullptr_t): shared_ptr(){}

            template <class T1>
            constexpr explicit shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, class CharMemoryManagerLike>
            constexpr shared_ptr(T1 * obj_arg,
                                 CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg,
                                            std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, std::enable_if_t<is_unique_ptr_v<std::decay_t<T1>>, bool> = true>
            constexpr shared_ptr(T1&& value): shared_ptr(unique_ptr_init_tag{}, std::forward<T1>(value)){}

            template <class T1, class OtherCharMemoryManager, std::enable_if_t<std::disjunction_v<std::negation<std::is_same<T1, T>>,
                                                                                                  std::negation<std::is_same<CharMemoryManager, OtherCharMemoryManager>>>, bool> = true>
            constexpr shared_ptr(shared_ptr<T1, OtherCharMemoryManager> other): obj(std::exchange(other.obj, nullptr)),
                                                                                ctrl_blk(std::exchange(other.ctrl_blk, nullptr)),
                                                                                deallocator(std::move(other.deallocator)){}

            constexpr shared_ptr(const self& other): obj(other.obj),
                                                     ctrl_blk(other.ctrl_blk),
                                                     deallocator(other.deallocator)
            {
                if (this->ctrl_blk != nullptr)
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (this->obj == nullptr)
                        {
                            std::abort();
                        }
                    }

                    this->ctrl_blk->counter.fetch_add(1u, std::memory_order_relaxed);
                }
            }

            constexpr shared_ptr(self&& other) noexcept: obj(std::exchange(other.obj, nullptr)),
                                                         ctrl_blk(std::exchange(other.ctrl_blk, nullptr)),
                                                         deallocator(std::move(other.deallocator)){}

            template <class ResourceManagerLike, class ...Args>
            constexpr shared_ptr(one_block_init_tag, ResourceManagerLike&& resource_manager_like, Args&& ...args)
            {
                using allocation_t      = shared_ptr_allocation_2<ControlBlock<NoActionCharMemoryDeallocator>, UniquePtrDestructor<T, NoActionCharMemoryDeallocator>, T>;

                void * mem              = resource_manager_like.template allocate_one<allocation_t>();
                void * ctrl_blk_mem     = static_cast<void *>(mem);
                void * destructor_mem   = static_cast<void *>(&reinterpret_cast<allocation_t *>(mem)->polymorphic_destructor);
                void * obj_mem          = static_cast<void *>(&reinterpret_cast<allocation_t *>(mem)->obj);

                T * obj;

                try
                {
                    obj = new (obj_mem) T(std::forward<Args>(args)...);
                }
                catch (...)
                {
                    resource_manager_like.deallocate_one(mem);
                    throw;
                }

                auto destructor         = unique_ptr<T, NoActionCharMemoryDeallocator>(obj);
                auto destructor_arg     = unique_ptr<PolymorphicDestructor, NoActionCharMemoryDeallocator>(new (destructor_mem) UniquePtrDestructor<T, NoActionCharMemoryDeallocator>(std::move(destructor)), NoActionCharMemoryDeallocator{});

                ControlBlock<NoActionCharMemoryDeallocator> * tmp_ctrl_blk = new (ctrl_blk_mem) ControlBlock<NoActionCharMemoryDeallocator>
                {
                    .counter    = std::atomic<size_t>(1u),
                    .destructor = std::move(destructor_arg)
                };

                this->ctrl_blk          = tmp_ctrl_blk;
                this->obj               = obj;
                this->deallocator       = std::forward<ResourceManagerLike>(resource_manager_like);
            }

            ~shared_ptr() noexcept
            {
                this->clean_resource();
            }

            constexpr auto operator =(const self& other) -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->clean_resource();

                this->obj           = other.obj;
                this->ctrl_blk      = other.ctrl_blk;
                this->deallocator   = other.deallocator;

                if (this->ctrl_blk != nullptr)
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (this->obj == nullptr)
                        {
                            std::abort();
                        }
                    }

                    this->ctrl_blk->counter.fetch_add(1u, std::memory_order_relaxed);
                }

                return *this;
            }

            constexpr auto operator =(self&& other) noexcept -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->clean_resource();

                this->obj           = std::exchange(other.obj, nullptr);
                this->ctrl_blk      = std::exchange(other.ctrl_blk, nullptr);
                this->deallocator   = std::move(other.deallocator);

                return *this;
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool
            {
                return this->get() == other.get();
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool
            {
                return this->get() != other.get();
            }

            constexpr auto operator >(const self& other) const noexcept -> bool
            {
                return this->get() > other.get();
            }

            constexpr auto operator >=(const self& other) const noexcept -> bool
            {
                return this->get() >= other.get();
            }

            constexpr auto operator <(const self& other) const noexcept -> bool
            {
                return this->get() < other.get();
            }

            constexpr auto operator <=(const self& other) const noexcept -> bool
            {
                return this->get() <= other.get();
            }

            constexpr void swap(self& other) noexcept
            {
                std::swap(this->obj, other.obj);
                std::swap(this->ctrl_blk, other.ctrl_blk);
                std::swap(this->deallocator, other.deallocator);
            }

            constexpr auto get() const noexcept -> T *
            {
                return this->obj;
            }

            constexpr auto operator ->() const noexcept -> T *
            {
                return this->get();
            }

            template <class Tmp = self, std::enable_if_t<has_star_operator_v<Tmp>, bool> = true>
            constexpr auto operator *() const noexcept -> typename Tmp::element_type&
            {
                return *this->get();
            }

            constexpr auto get_count() const noexcept -> size_t
            {
                if (this->ctrl_blk == nullptr)
                {
                    return 0u;
                }

                return this->ctrl_blk->counter.load(std::memory_order_relaxed);
            }

            constexpr operator bool() const noexcept
            {
                return this->get() != nullptr;
            }

        private:

            __attribute__((noinline, noipa)) constexpr void deallocate_memory(void * memblk) noexcept
            {
                this->deallocator.deallocate_one(memblk);
            }

            __attribute__((noinline)) constexpr void release_control_block(ControlBlock<NoActionCharMemoryDeallocator> * ctrl_blk_arg) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (ctrl_blk_arg == nullptr)
                    {
                        std::abort();
                    }

                    if (ctrl_blk_arg->counter.load(std::memory_order_relaxed) != 0u)
                    {
                        std::abort();
                    }
                }

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                else
                {
                    std::atomic_thread_fence(std::memory_order_acquire);
                }

                std::destroy_at(ctrl_blk_arg); //sync pointing memory
                this->deallocate_memory(ctrl_blk_arg);
            }

            constexpr void clean_resource() noexcept
            {
                if (this->ctrl_blk == nullptr)
                {
                    return;
                }

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (this->obj == nullptr)
                    {
                        std::abort();
                    }
                }

                size_t previous_count = this->ctrl_blk->counter.fetch_sub(1u, std::memory_order_relaxed);

                if (previous_count == 1u) [[unlikely]]
                {
                    this->release_control_block(this->ctrl_blk);
                }

                this->ctrl_blk      = nullptr;
                this->obj           = nullptr;
            }
    };

    template <class T, class CharMemoryManagerLike, class ...Args, std::enable_if_t<std::negation_v<std::is_array<T>>, bool> = true>
    constexpr auto allocate_shared(CharMemoryManagerLike&& mem_manager, Args&& ...args)
    {
        using shared_ptr_t = shared_ptr<T, std::decay_t<CharMemoryManagerLike>>;

        return shared_ptr_t(one_block_init_tag{},
                            std::forward<CharMemoryManagerLike>(mem_manager),
                            std::forward<Args>(args)...);
    }

    template <class T>
    struct is_bounded_array_unique_ptr: std::false_type{};

    template <class T, class T1>
    struct is_bounded_array_unique_ptr<unique_ptr<T[], T1>>: std::true_type{};

//     template <class T, class CharMemoryManager>
//     class shared_ptr<T[], CharMemoryManager>: private shared_ptr<T, CharMemoryManager>
//     {
//         private:

//             template <class U, class U1>
//             friend class shared_ptr;

//             using self  = shared_ptr<T[], CharMemoryManager>;
//             using base  = shared_ptr<T, CharMemoryManager>;

//         public:

//             using pointer       = T *;
//             using element_type  = T;

//             constexpr shared_ptr(): base(){}

//             constexpr shared_ptr(std::nullptr_t): shared_ptr(){}

//             template <class T1, std::enable_if_t<std::is_same_v<std::decay_t<T1>, std::decay_t<T>>, bool> = true>
//             constexpr explicit shared_ptr(T1 * obj): base(obj){}

//             template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::is_same_v<std::decay_t<T1>, std::decay_t<T>>, bool> = true>
//             constexpr shared_ptr(T1 * obj,
//                                  CharMemoryManagerLike&& deallocator_arg): base(obj, std::forward<CharMemoryManagerLike>(deallocator_arg)){}

//             template <class T1, std::enable_if_t<std::conjunction_v<is_bounded_array_unique_ptr<std::decay_t<T1>>,
//                                                                     std::is_same<std::decay_t<T>, std::decay_t<typename std::decay_t<T1>::element_type>>>, bool> = true>
//             constexpr shared_ptr(T1&& value): base(std::forward<T1>(value)){}

//             template <class OtherCharMemoryManager, std::enable_if_t<std::negation_v<std::is_same<CharMemoryManager, OtherCharMemoryManager>>, bool> = true>
//             constexpr shared_ptr(shared_ptr<T[], OtherCharMemoryManager> other): base(static_cast<shared_ptr<T, OtherCharMemoryManager>&&>(other)){}

//             constexpr shared_ptr(const self& other): base(static_cast<base&>(other)){}

//             constexpr shared_ptr(self&& other) noexcept: base(static_cast<base&&>(other)){}

//             constexpr auto operator =(const self& other) -> self&
//             {
//                 return static_cast<self&>(static_cast<base&>(*this) = other);
//             }

//             constexpr auto operator =(self&& other) noexcept -> self&
//             {
//                 return static_cast<self&>(static_cast<base&>(*this) = static_cast<base&&>(other));
//             }

//             constexpr auto operator ==(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) == other;
//             }

//             constexpr auto operator !=(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) != other;
//             }

//             constexpr auto operator >(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) > other;
//             }

//             constexpr auto operator >=(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) >= other;
//             }

//             constexpr auto operator <(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) < other;
//             }

//             constexpr auto operator <=(const self& other) const noexcept -> bool
//             {
//                 return static_cast<const base&>(*this) <= other;
//             }

//             constexpr void swap(self& other) noexcept
//             {
//                 base::swap(other);
//             }

//             constexpr auto get() const noexcept -> T *
//             {
//                 return base::get();
//             }

//             constexpr auto operator[](size_t idx) const noexcept -> T&
//             {
//                 return base::get()[idx];
//             }

//             constexpr auto get_count() const noexcept -> size_t
//             {
//                 return base::get_count();
//             }

//             constexpr operator bool() const noexcept
//             {
//                 return static_cast<bool>(static_cast<const self&>(*this));
//             }
//     };

//     template <class T, class CharMemoryManagerLike, std::enable_if_t<std::is_array_v<T>, bool> = true>
//     constexpr auto allocate_shared(CharMemoryManagerLike&& mem_manager, size_t sz)
//     {
//         static_assert(std::is_unbounded_array_v<T>);

//         using shared_ptr_t = shared_ptr<T, std::decay_t<CharMemoryManagerLike>>;

//         return shared_ptr_t(one_block_init_tag{},
//                             std::forward<CharMemoryManagerLike>(mem_manager),
//                             size_t sz);
//     }
// }

#endif