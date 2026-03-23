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

namespace smart_pointer::unique_ptr_implementation
{
    template <class Child, class Parent>
    struct is_unique_ptr_convertible_child: std::conjunction<std::is_base_of<Parent, Child>, std::negation<std::is_base_of<Child, Parent>>>{};

    template <class Child, class Parent>
    static inline constexpr bool is_unique_ptr_convertible_child_v = is_unique_ptr_convertible_child<Child, Parent>::value;

    template <class T, class CharMemoryDeallocator>
    class unique_ptr
    {
        private:

            template <class U, class U1>
            friend class unique_ptr;

            using self                  = unique_ptr;

            T * obj;
            polymorphic_offset_t org_byte_offset;
            CharMemoryDeallocator deallocator;

        public:

            using polymorphic_offset_t  = int16_t;

            template <class Tmp = CharMemoryDeallocator, std::enable_if_t<std::is_default_constructible_v<Tmp>, bool> = true>
            constexpr unique_ptr(): obj(nullptr),
                                    org_byte_offset(0),
                                    deallocator(){}

            template <class Tmp = self, std::enable_if_t<std::is_default_constructible_v<Tmp>, bool> = true>
            constexpr unique_ptr(std::nullptr): unique_ptr(){}

            template <class T1, class Tmp = CharMemoryDeallocator, std::enable_if_t<std::conjunction_v<std::is_default_constructible<Tmp>,
                                                                                                       std::is_same<T1, T>>, bool> = true>
            constexpr unique_ptr(T1 * obj_arg): obj(obj_arg),
                                                org_byte_offset(0),
                                                deallocator(){}


            template <class T1, class Tmp = CharMemoryDeallocator, std::enable_if_t<std::conjunction_v<std::is_default_constructible<Tmp>,
                                                                                                       is_unique_ptr_convertible_child<T1, T>>, bool> = true>
            constexpr unique_ptr(T1 * obj_arg): obj(obj_arg),
                                                org_byte_offset(calculate_offset<T>(obj_arg)),
                                                deallocator(){}

            template <class T1, class CharMemoryDeallocatorLike, std::enable_if_t<std::is_same_v<T, T1>, bool> = true>
            constexpr explicit unique_ptr(T1 * obj_arg,
                                          CharMemoryDeallocatorLike&& deallocator_arg): obj(obj_arg),
                                                                                        org_byte_offset(0),
                                                                                        deallocator(std::forward<CharMemoryDeallocatorLike>(deallocator_arg)){}

            template <class T1, class CharMemoryDeallocatorLike, std::enable_if_t<is_unique_ptr_convertible_child<T1, T>, bool> = true>
            constexpr explicit unique_ptr(T1 * obj_arg,
                                          CharMemoryDeallocatorLike&& deallocator_arg): obj(obj_arg),
                                                                                        org_byte_offset(calculate_offset<T>(obj_arg)),
                                                                                        deallocator(std::forward<CharMemoryDeallocatorLike>(deallocator_arg)){}

            template <class T1, std::enable_if_t<std::is_unique_ptr_convertible_child<T1, T>, bool> = true>
            constexpr unique_ptr(unique_ptr<T1, CharMemoryDeallocator>&& other) noexcept
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

            constexpr auto get() const noexcept -> T *
            {
                return this->obj;
            }

            constexpr auto get_deleter() const noexcept -> const CharMemoryDeallocator&
            {
                return this->deallocator;
            }

            constexpr auto get_deleter() && noexcept -> CharMemoryDeallocator&&
            {
                return static_cast<CharMemoryDeallocator&&>(this->deallocator);
            }

        private:

            constexpr auto get_polymorphic_byte_offset() const noexcept -> polymorphic_offset_t
            {
                return this->org_byte_offset;
            }

            template <class ToType, class FrType>
            static constexpr auto calculate_offset(FrType * obj) noexcept -> polymorphic_offset_t
            {
                ToType * casted_obj = obj;
                intmax_t offset     = std::distance(static_cast<const char *>(obj), static_cast<const char *>(casted_obj));

                return stdx::safe_integer_cast<polymorphic_offset_t>(offset);
            }

            constexpr void clean_resource() noexcept
            {
                if (this->obj == nullptr)
                {
                    return;
                }

                std::destroy_at(this->obj);

                char * memblk = std::prev(reinterpret_cast<char *>(this->obj), this->org_byte_offset);
                this->deallocator(memblk);
                this->obj = nullptr;
            }
    };

    template <class T, class CharMemoryDeallocator>
    class unique_ptr<T[], CharMemoryDeallocator>
    {
        private:

            template <class U, class U1>
            friend class unique_ptr;

            using self                  = unique_ptr;

            T * obj;
            CharMemoryDeallocator deallocator;
        
        public:

            template <class Tmp = CharMemoryDeallocator, std::enable_if_t<std::is_default_constructible_v<Tmp>, bool> = true>
            constexpr unique_ptr(): obj(nullptr),
                                    deallocator(){}

            template <class Tmp = self, std::enable_if_t<std::is_default_constructible_v<Tmp>, bool> = true>
            constexpr unique_ptr(std::nullptr): unique_ptr(){}

            template <class T1, class Tmp = CharMemoryDeallocator, std::enable_if_t<std::conjunction_v<std::is_default_constructible<Tmp>,
                                                                                                       std::is_same<T, T1>>, bool> = true>
            constexpr unique_ptr(T1 * obj_arg): obj(obj_arg),
                                                deallocator(){}

            template <class T1, class CharMemoryDeallocatorLike, std::enable_if_t<std::is_same_v<T, T1>, bool> = true>
            constexpr unique_ptr(T1 * obj_arg,
                                 CharMemoryDeallocatorLike&& deallocator_arg): obj(obj_arg),
                                                                               deallocator(std::forward<CharMemoryDeallocatorLike>(deallocator_arg)){}

            constexpr unique_ptr(const self&) = delete;

            constexpr unique_ptr(self&& other) noexcept: obj(std::exchange(other.obj, nullptr)),
                                                         deallocator(std::move(other.deallocator)){}

            self& operator =(const self& other) = delete;

            auto operator =(self&& other) noexcept -> self&
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

            constexpr auto get() const noexcept -> T *
            {
                return this->obj;
            }

            constexpr auto operator[](size_t idx) const noexcept -> T&
            {
                return this->obj[idx];
            }

            constexpr auto get_deleter() const noexcept -> const CharMemoryDeallocator&
            {
                return this->deallocator;
            }

            constexpr auto get_deleter() && noexcept -> CharMemoryDeallocator&&
            {
                return static_cast<CharMemoryDeallocator&&>(this->deallocator);
            }

        private:
        
            constexpr void clean_resource() noexcept
            {
                if (this->obj == nullptr)
                {
                    return;
                }

                size_t sz = this->deallocator.size(this->obj);
                std::destroy(this->obj, std::next(this->obj, sz));

                this->deallocator(static_cast<char *>(this->obj));
                this->obj = nullptr;
            }
    };
}

#endif