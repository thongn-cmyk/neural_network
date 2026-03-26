//despite my sincerest efforts in optimizing unique_ptr, it's just seemed that unique_ptr has built-in assembly optimizations that I cannot replicate, just like vector<>
//but we just have to do this because of abcdxyz
//firstly for the shared_ptr<> explicit implementation of memory orderings (we write the std such that it's only memory_ordering_acquire when count == 0, with no release because the release is now "in-sync" with or "responsible-by" the stable pointer)
//secondly for the unique_ptr<> polymorphic offset capturing, we'll try to be careful

#ifndef __DG_UNIQUE_PTR_H__
#define __DG_UNIQUE_PTR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stl_extension/stdx.h>
#include <type_traits>
#include <optional>

namespace smart_pointer::unique_ptr_implementation
{
    //OK

    template <class T, class = void>
    struct has_star_operator: std::false_type{};

    template <class T>
    struct has_star_operator<T, std::void_t<decltype(*std::declval<T&>())>>: std::true_type{};

    template <class T>
    static inline constexpr bool has_star_operator_v = has_star_operator<T>::value;

    template <class T, class CharMemoryDeallocator>
    class unique_ptr
    {
        private:

            template <class U, class U1>
            friend class unique_ptr;

            using self  = unique_ptr;
            using polymorphic_offset_t = int16_t;

            T * obj;
            polymorphic_offset_t org_byte_offset;
            std::optional<CharMemoryDeallocator> deallocator;

        public:

            using pointer       = T *;
            using element_type  = T;
            using deleter_type  = CharMemoryDeallocator;

            constexpr unique_ptr(): obj(nullptr),
                                    org_byte_offset(0),
                                    deallocator(std::nullopt){}

            constexpr unique_ptr(std::nullptr_t): unique_ptr(){}

            template <class T1, class Tmp = CharMemoryDeallocator, std::enable_if_t<std::is_default_constructible_v<Tmp>, bool> = true>
            constexpr explicit unique_ptr(T1 * obj_arg): obj(obj_arg),
                                                         org_byte_offset(calculate_offset<T>(obj_arg)),
                                                         deallocator(CharMemoryDeallocator{}){}

            template <class T1, class CharMemoryDeallocatorLike>
            constexpr unique_ptr(T1 * obj_arg,
                                 CharMemoryDeallocatorLike&& deallocator_arg): obj(obj_arg),
                                                                               org_byte_offset(calculate_offset<T>(obj_arg)),
                                                                               deallocator(std::forward<CharMemoryDeallocatorLike>(deallocator_arg)){}

            template <class T1, class OtherCharMemoryDeallocator, std::enable_if_t<std::disjunction_v<std::negation<std::is_same<T, T1>>,
                                                                                                      std::negation<std::is_same<CharMemoryDeallocator, OtherCharMemoryDeallocator>>>, bool> = true>
            constexpr unique_ptr(unique_ptr<T1, OtherCharMemoryDeallocator>&& other) noexcept
            {
                this->obj               = other.obj;
                this->org_byte_offset   = stdx::safe_integer_cast<polymorphic_offset_t>(static_cast<intmax_t>(other.org_byte_offset) + (other.obj != nullptr ? calculate_offset<T>(other.obj)
                                                                                                                                                             : polymorphic_offset_t{0}));
                this->deallocator       = std::move(other.deallocator);

                other.obj               = nullptr;
            }

            unique_ptr(const self&) = delete;

            constexpr unique_ptr(self&& other) noexcept: obj(std::exchange(other.obj, nullptr)),
                                                         org_byte_offset(other.org_byte_offset),
                                                         deallocator(std::move(other.deallocator)){}

            ~unique_ptr() noexcept
            {
                this->clean_resource();
            }

            self& operator =(const self&) = delete;

            constexpr auto operator =(self&& other) noexcept -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->clean_resource();

                this->obj               = std::exchange(other.obj, nullptr);
                this->org_byte_offset   = other.org_byte_offset;
                this->deallocator       = std::move(other.deallocator);

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

            constexpr void release() noexcept
            {
                this->obj = nullptr;
            }

            constexpr void swap(self& other) noexcept
            {
                std::swap(this->obj, other.obj);
                std::swap(this->org_byte_offset, other.org_byte_offset);;
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

            constexpr auto get_deleter() const noexcept -> const std::optional<CharMemoryDeallocator>&
            {
                return this->deallocator;
            }

            constexpr auto get_deleter() && noexcept -> std::optional<CharMemoryDeallocator>&&
            {
                return static_cast<std::optional<CharMemoryDeallocator>&&>(this->deallocator);
            }

            constexpr operator bool() const noexcept
            {
                return this->get() != nullptr;
            }

        private:

            template <class ToType, class FrType>
            static constexpr auto calculate_offset(FrType * obj) noexcept -> polymorphic_offset_t
            {
                ToType * casted_obj = obj;
                intmax_t offset     = std::distance(reinterpret_cast<const char *>(obj), reinterpret_cast<const char *>(casted_obj));

                return stdx::safe_integer_cast<polymorphic_offset_t>(offset);
            }

            __attribute__((noinline, noipa)) constexpr void dellocate_memory(void * memblk) noexcept
            {
                this->deallocator->deallocate_one(memblk);
            }

            constexpr void clean_resource() noexcept
            {
                if (this->obj == nullptr)
                {
                    return;
                }

                std::destroy_at(this->obj);

                char * memblk = std::prev(reinterpret_cast<char *>(this->obj), this->org_byte_offset);
                this->dellocate_memory(memblk);
                this->obj = nullptr;
            }
    };

    template <class T, class CharMemoryDeallocator>
    class unique_ptr<T[], CharMemoryDeallocator>
    {
        private:

            template <class U, class U1>
            friend class unique_ptr;

            using self  = unique_ptr;

            T * obj;
            std::optional<CharMemoryDeallocator> deallocator;

        public:

            using pointer       = T *;
            using element_type  = T;
            using deleter_type  = CharMemoryDeallocator;

            constexpr unique_ptr(): obj(nullptr),
                                    deallocator(std::nullopt){}

            constexpr unique_ptr(std::nullptr_t): unique_ptr(){}

            template <class T1, class Tmp = CharMemoryDeallocator, std::enable_if_t<std::conjunction_v<std::is_default_constructible<Tmp>,
                                                                                                       std::is_same<std::decay_t<T>, std::decay_t<T1>>>, bool> = true>
            constexpr explicit unique_ptr(T1 * obj_arg): obj(obj_arg),
                                                         deallocator(CharMemoryDeallocator{}){}

            template <class T1, class CharMemoryDeallocatorLike, std::enable_if_t<std::is_same_v<std::decay_t<T>, std::decay_t<T1>>, bool> = true>
            constexpr unique_ptr(T1 * obj_arg,
                                 CharMemoryDeallocatorLike&& deallocator_arg): obj(obj_arg),
                                                                               deallocator(std::forward<CharMemoryDeallocatorLike>(deallocator_arg)){}

            template <class OtherCharMemoryDeallocator, std::enable_if_t<std::negation_v<std::is_same<CharMemoryDeallocator, OtherCharMemoryDeallocator>>, bool> = true>
            constexpr unique_ptr(unique_ptr<T[], OtherCharMemoryDeallocator>&& other): obj(std::exchange(other.obj, nullptr),
                                                                                       deallocator(std::move(other.deallocator))){}

            constexpr unique_ptr(const self&) = delete;

            constexpr unique_ptr(self&& other) noexcept: obj(std::exchange(other.obj, nullptr)),
                                                         deallocator(std::move(other.deallocator)){}

            self& operator =(const self& other) = delete;

            constexpr auto operator =(self&& other) noexcept -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->clean_resource();

                this->obj           = std::exchange(other.obj, nullptr);
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

            constexpr void release() noexcept
            {
                this->obj = nullptr;
            }

            constexpr void swap(self& other) noexcept
            {
                std::swap(this->obj, other.obj);
                std::swap(this->deallocator, other.deallocator);
            }

            constexpr auto get() const noexcept -> T *
            {
                return this->obj;
            }

            constexpr auto operator[](size_t idx) const noexcept -> T&
            {
                return this->obj[idx];
            }

            constexpr auto get_deleter() const noexcept -> const std::optional<CharMemoryDeallocator>&
            {
                return this->deallocator;
            }

            constexpr auto get_deleter() && noexcept -> std::optional<CharMemoryDeallocator>&&
            {
                return static_cast<std::optional<CharMemoryDeallocator>&&>(this->deallocator);
            }

            constexpr operator bool() const noexcept
            {
                return this->get() != nullptr;
            }

        private:

            __attribute__((noinline, noipa)) constexpr auto get_allocation_size(const void * mem) const noexcept -> size_t
            {
                return this->deallocator->size(mem) / sizeof(T);
            }

            __attribute__((noinline, noipa)) constexpr void deallocate_memory(void * mem) noexcept
            {
                this->deallocator->deallocate(mem);
            }

            constexpr void clean_resource() noexcept
            {
                if (this->obj == nullptr)
                {
                    return;
                }

                size_t sz = this->get_allocation_size(this->obj);
                std::destroy(this->obj, std::next(this->obj, sz));

                this->deallocate_memory(this->obj);
                this->obj = nullptr;
            }
    };

    template <class T, class CharMemoryManager, class ...Args, std::enable_if_t<std::negation_v<std::is_array<T>>, bool> = true>
    constexpr auto allocate_unique(CharMemoryManager&& mem_manager, Args&& ...args) -> unique_ptr<T, std::decay_t<CharMemoryManager>>
    {
        void * memblk = mem_manager.template allocate_one<T>();
        T * obj;

        try
        {
            obj = new (memblk) T(std::forward<Args>(args)...);
        }
        catch (...)
        {
            mem_manager.deallocate(memblk);
            throw;
        }

        try
        {
            return unique_ptr<T, std::decay_t<CharMemoryManager>>(obj, std::forward<CharMemoryManager>(mem_manager));
        }
        catch (...)
        {
            std::abort();
        }
    }

    template <class T, class CharMemoryManager, std::enable_if_t<std::is_unbounded_array_v<T>, bool> = true>
    constexpr auto allocate_unique(CharMemoryManager&& mem_manager, size_t sz) -> unique_ptr<T, std::decay_t<CharMemoryManager>>
    {
        using base_type = std::remove_extent_t<T>;

        void * memblk = mem_manager.template allocate<base_type>(sz);
        base_type * obj;

        try
        {
            obj = new (memblk) base_type[sz];
        }
        catch (...)
        {
            mem_manager.deallocate(memblk);
            throw;
        }

        try
        {
            return unique_ptr<T, std::decay_t<CharMemoryManager>>(obj, std::forward<CharMemoryManager>(mem_manager));
        }
        catch (...)
        {
            std::abort();
        }
    }
}

#endif