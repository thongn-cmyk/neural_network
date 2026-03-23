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
    struct is_shared_ptr_staticastable_child<Child, Parent, std::void_t<decltype(static_cast<Parent&>(std::declval<Child&>()))>>: std::true_type{};

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
                                                     ResourceManagerLike& resource_manager_like,
                                                     size_t initial_count = 0u) -> ControlBlock<CharMemoryManager> *
            {
                char * mem = resource_manager_like.template allocate<ControlBlock<CharMemoryManager>>(1u);

                ControlBlock<CharMemoryManager> * tmp_ctrl_blk = new (mem) ControlBlock<CharMemoryManager>
                {
                    .counter    = std::atomic<size_t>(initial_count),
                    .destructor = std::move(destructor)
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

            constexpr shared_ptr(std::nullptr): shared_ptr(){}

            template <class T1, std::enable_if_t<std::conjunction_v<std::is_same<T, T1>>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, std::enable_if_t<std::conjunction_v<is_shared_ptr_polymorphic_convertible_child<T1, T>,
                                                                    std::negate<std::is_same<T, T1>>>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, std::enable_if_t<std::conjunction_v<is_shared_ptr_staticastable_child<T1, T>,
                                                                    std::negate<is_shared_ptr_polymorphic_convertible_child<T1, T>>,
                                                                    std::negate<std::is_same<T, T1>>>, bool> = true>
            constexpr shared_ptr(T1 * obj_arg)
            {
                this->raw_unique_initialize(obj_arg);
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::is_same_v<T, T1>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg,
                                          CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::conjunction_v<is_shared_ptr_polymorphic_convertible_child<T1, T>,
                                                                                                 std::negate<std::is_same<T, T1>>>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg,
                                          CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, class CharMemoryManagerLike, std::enable_if_t<std::conjunction_v<is_shared_ptr_staticastable_child<T1, T>,
                                                                                                 std::negate<is_shared_ptr_polymorphic_convertible_child<T1, T>>,
                                                                                                 std::negate<std::is_same<T, T1>>>, bool> = true>
            constexpr explicit shared_ptr(T1 * obj_arg,
                                          CharMemoryManagerLike&& deallocator_arg)
            {
                this->raw_unique_initialize(obj_arg, std::forward<CharMemoryManagerLike>(deallocator_arg));
            }

            template <class T1, std::enable_if_t<is_unique_ptr_v<std::decay_t<T1>>, bool> = true>
            constexpr shared_ptr(T1&& value): shared_ptr(unique_ptr_init_tag{}, std::forward<T1>(value)){}

            template <class T1, class OtherCharMemoryManager, std::enable_if_t<std::disjunction_v<std::negate<std::is_same<T1, T>>,
                                                                                                      std::negate<std::is_same<CharMemoryManager, OtherCharMemoryManager>>>, bool> = true>
            constexpr shared_ptr(shared_ptr<T1, OtherCharMemoryManager> other)
            {
                this->obj           = std::exchange(other.obj, nullptr);
                this->ctrl_blk      = std::exchange(other.ctrl_blk, nullptr);
                this->deallocator   = std::move(other.deallocator);

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

            constexpr auto get() const noexcept -> T *
            {
                return this->obj;
            }

            constexpr auto get_count() const noexcept -> size_t
            {
                if (this->ctrl_blk == nullptr)
                {
                    return 0u;
                }

                return this->ctrl_blk->counter.load(std::memory_order_relaxed);
            }

        private:

            __attribute__((noinline)) constexpr void release_control_block() noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (this->obj == nullptr)
                    {
                        std::abort();
                    }

                    if (this->ctrl_blk == nullptr)
                    {
                        std::abort();
                    }

                    if (this->ctrl_blk->counter.load(std::memory_order_relaxed) != 0u)
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

                std::destroy_at(this->ctrl_blk); //sync pointing memory
                this->deallocator(static_cast<char *>(this->ctrl_blk)); //defer the deallocated semantic memory to the deallocation of pointer (semantic blk is now in-sync with the pointer)

                this->ctrl_blk  = nullptr;
                this->obj       = nullptr;
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
                    this->release_control_block();
                }
            }
    };

    template <class T, class CharMemoryManager>
    class shared_ptr<T[], CharMemoryManager>
    {

    };
}

#endif