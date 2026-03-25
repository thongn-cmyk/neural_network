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

    class PolymorphicDestructor
    {
        public:

            virtual ~PolymorphicDestructor() noexcept = default;
    };

    template <class T, class CharMemoryManager>
    using unique_ptr = smart_pointer::unique_ptr_implementation::unique_ptr<T, CharMemoryManager>;

    template <class CharMemoryManager>
    struct ControlBlock
    {
        // T obj; //I forgot !!!
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

            ~UniquePtrDestructor() noexcept
            {
                this->resource = nullptr;
            }

            auto get() && noexcept -> unique_ptr<Args...>&&
            {
                return static_cast<unique_ptr<Args...>&&>(this->resource);
            }
    };

    template <class Child, class Parent>
    struct is_shared_ptr_polymorphic_convertible_child: std::conjunction<std::is_base_of<Parent, Child>, std::negation<Child, Parent>>{};

    template <class Child, class Parent>
    static inline constexpr bool is_shared_ptr_polymorphic_convertible_child_v = is_unique_ptr_convertible_child<Child, Parent>::value;

    template <class Child, class Parent, class = void>
    struct is_shared_ptr_staticastable_child: std::false_type{};

    template <class Child, class Parent>
    struct is_shared_ptr_staticastable_child<Child, Parent, std::void_t<decltype(static_cast<Parent *>(std::declval<Child *>()))>>: std::true_type{};

    template <class Child, class Parent>
    static inline constexpr bool is_shared_ptr_staticastable_child_v = is_shared_ptr_staticastable_child<Child, Parent>::value;

    template <class T>
    struct is_unique_ptr: std::false_type{};

    template <class ...Args>
    struct is_unique_ptr<smart_pointer::unique_ptr_implementation::unique_ptr<Args...>>: std::true_type{};

    template <class T, class CharMemoryManager>
    class shared_ptr
    {
        private:

            template <class U, class U1>
            friend class shared_ptr;

            using self                  = shared_ptr;

            T * obj;
            ControlBlock<CharMemoryManager> * ctrl_blk;
            CharMemoryManager deallocator;

            struct unique_ptr_init_tag{};

            template <class ...Args, class ResourceManagerLike>
            static constexpr auto make_control_block(UniquePtrDestructor<Args...>&& destructor,
                                                     ResourceManagerLike&& resource_manager_like,
                                                     size_t initial_count = 0u) -> ControlBlock<CharMemoryManager> *
            {
                size_t memblk_1_sz      = resource_manager_like.template allocation_size<ControlBlock<CharMemoryManager>>(1u);
                size_t memblk_2_sz      = resource_manager_like.template allocation_size<UniquePtrDestructor<Args...>>(1u);
                size_t total_memblk_sz  = memblk_1_sz + memblk_2_sz;
                char * mem              = resource_manager_like.template allocate<char>(total_memblk_sz); //
                char * nxt_mem          = std::next(mem, memblk_1_sz); //

                ControlBlock<CharMemoryManager> * tmp_ctrl_blk = new (mem) ControlBlock<CharMemoryManager>
                {
                    .counter    = std::atomic<size_t>(initial_count),
                    .destructor = unique_ptr<UniquePtrDestructor<Args...>, CharMemoryManager>(new (nxt_mem) UniquePtrDestructor<Args...>(std::move(destructor)), std::forward<ResourceManagerLike>(resource_manager_like))
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
                    this->deallocator   = {};

                    return;
                }

                auto resource_ptr       = other.get();
                auto resource_manager   = other.get_deleter();

                UniquePtrDestructor destructor(std::move(other));

                try
                {
                    this->ctrl_blk = this->make_control_block(std::move(destructor), resource_manager, 1u);
                }
                catch (...)
                {
                    other = std::move(destructor).get();
                    throw;
                }

                this->obj           = resource_ptr;
                this->deallocator   = std::move(resource_manager);
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

            constexpr shared_ptr(): obj(nullptr),
                                    ctrl_blk(nullptr),
                                    deallocator(){}

            constexpr shared_ptr(std::nullptr_t): shared_ptr(){}

            template <class T1, std::enable_if_t<std::conjunction_v<std::is_same<T, T1>>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, std::enable_if_t<std::conjunction_v<is_shared_ptr_polymorphic_convertible_child<T1, T>,
                                                                    std::negation<std::is_same<T, T1>>>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, std::enable_if_t<std::conjunction_v<is_shared_ptr_staticastable_child<T1, T>,
                                                                    std::negation<is_shared_ptr_polymorphic_convertible_child<T, T1>>,
                                                                    std::negation<is_shared_ptr_polymorphic_convertible_child<T1, T>>,
                                                                    std::negation<std::is_same<T, T1>>>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::is_same_v<T, T1>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg,
                                 CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::conjunction_v<is_shared_ptr_polymorphic_convertible_child<T1, T>,
                                                                                                 std::negation<std::is_same<T, T1>>>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg,
                                 CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::conjunction_v<is_shared_ptr_staticastable_child<T1, T>,
                                                                                                 std::negation<is_shared_ptr_polymorphic_convertible_child<T, T1>>,
                                                                                                 std::negation<is_shared_ptr_polymorphic_convertible_child<T1, T>>,
                                                                                                 std::negation<std::is_same<T, T1>>>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg,
                                 CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, std::enable_if_t<is_unique_ptr_v<std::decay_t<T1>>, bool> = true>
            constexpr shared_ptr(T1&& value): shared_ptr(unique_ptr_init_tag{}, std::forward<T1>(value)){}

            template <class T1, class OtherCharMemoryManager, std::enable_if_t<std::disjunction_v<std::negation<std::is_same<T1, T>>,
                                                                                                  std::negation<std::is_same<CharMemoryManager, OtherCharMemoryManager>>>, bool> = true>
            constexpr shared_ptr(shared_ptr<T1, OtherCharMemoryManager> other): obj(std::exchange(other.obj, nullptr)),
                                                                                ctrl_blk(std::exchange(other.ctrl_blk, nullptr)),
                                                                                deallocator(std::move(other.deallocator)){}{}

            constexpr shared_ptr(const self& other): obj(other.obj)
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

                    this->ctr_blk->counter.fetch_add(1u, std::memory_order_relaxed);
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

            constexpr auto operator *() const noexcept -> T&
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
                this->deallocator.deallocate(memblk);
            }

            __attribute__((noinline)) constexpr void release_control_block(ControlBlock<CharMemoryManager> * ctrl_blk_arg) noexcept
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
                    std::atomic_thread_fence(std::mmeory_order_acquire);
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
                this->deallocator   = {};
            }
    };

    template <class T, class CharMemoryManager>
    class shared_ptr<T[], CharMemoryManager>: private shared_ptr<T, CharMemoryManager>
    {
        private:

            using self  = shared_ptr<T[], CharMemoryManager>;
            using base  = shared_ptr<T, CharMemoryManager>;

        public:

            constexpr shared_ptr(): base(){}

            constexpr shared_ptr(std::nullptr_t): shared_ptr(){}

            template <class T1, std::enable_if_t<std::is_same_v<T1, T>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj): base(obj){}

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::is_same_v<T1, T>, bool> = true>
            constexpr shared_ptr(T1 * obj,
                                 CharMemoryManagerLike&& deallocator_arg): base(obj, std::forward<CharMemoryManagerLike>(deallocator_arg)){}

            template <class T1, std::enable_if_t<std::conjunction_v<is_bounded_array_unique_ptr<std::decay_t<T1>>,
                                                                    std::is_same<T, typename std::decay_t<T1>::element_type>>, bool> = true>
            constexpr shared_ptr(T1&& value): base(std::forward<T1>(value)){}

            template <class OtherCharMemoryManager, std::enable_if_t<std::negation_v<std::is_same<CharMemoryManager, OtherCharMemoryManager>>, bool> = true>
            constexpr shared_ptr(std::shared_ptr<T[], OtherCharMemoryManager> other): base(static_cast<shared_ptr<T, OtherCharMemoryManager>&&>(other)){}

            constexpr shared_ptr(const self& other): base(static_cast<base&>(other)){}

            constexpr shared_ptr(self&& other) noexcept: base(static_cast<base&&>(other)){}

            constexpr auto operator =(const self& other) -> self&
            {
                return static_cast<self&>(static_cast<base&>(*this) = other);
            }

            constexpr auto operator =(self&& other) noexcept -> self&
            {
                return static_cast<self&>(static_cast<self&>(*this) = static_cast<base&&>(other));
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) == other;
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) != other;
            }

            constexpr auto operator >(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) > other;
            }

            constexpr auto operator >=(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) >= other;
            }

            constexpr auto operator <(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) < other;
            }

            constexpr auto operator <=(const self& other) const noexcept -> bool
            {
                return static_cast<const self&>(*this) <= other;
            }

            constexpr void swap(self& other) noexcept
            {
                base::swap(other);
            }

            constexpr auto get() const noexcept -> T *
            {
                return base::get();
            }

            constexpr auto operator[](size_t idx) const noexcept -> T&
            {
                return base::get()[idx];
            }

            constexpr auto get_count() const noexcept -> size_t
            {
                return base::get_count();
            }

            constexpr operator bool() const noexcept
            {
                return static_cast<bool>(static_cast<const self&>(*this));
            }
    };
}

#endif