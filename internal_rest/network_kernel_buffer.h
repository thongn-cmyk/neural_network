#ifndef __DG_NETWORK_KERNEL_BUFFER_H__
#define __DG_NETWORK_KERNEL_BUFFER_H__

#include <stdint.h>
#include <stdlib.h>
#include <cstring>
#include <memory>
#include "stdx.h"
#include "network_exception.h"
#include "network_log.h"

namespace dg_sock::network_kernel_buffer
{
    class StdAllocator
    {
        public:

            static inline auto malloc(const size_t byte_sz) noexcept -> std::expected<void *, exception_t>
            {
                if (byte_sz == 0u)
                {
                    return std::add_pointer_t<void>(nullptr);
                }

                void * rs = std::malloc(byte_sz);

                if (rs == nullptr)
                {
                    return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                }

                return rs;
            }

            static inline void free(void * mem_ptr) noexcept
            {
                if (mem_ptr == nullptr)
                {
                    return;
                }

                std::free(mem_ptr);
            }

            static inline auto realloc(void * mem_ptr, size_t new_sz) -> std::expected<void *, exception_t> 
            {
                if (new_sz == 0u)
                {
                    StdAllocator::free(mem_ptr);
                    return std::add_pointer_t<void>(nullptr);
                }

                void * rs = std::realloc(mem_ptr, new_sz);

                if (rs == nullptr)
                {
                    return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);                    
                }

                return rs;
            }
    };

    template <class StatelessAllocator = StdAllocator>
    class kernel_string
    {
        private:

            char * buffer;
            size_t buffer_sz;
            size_t buffer_cap;

            static inline constexpr size_t MIN_ALLOCATION_SZ = 8u;

        public:

            using self = kernel_string;
            using value_type = char;

            kernel_string() noexcept: buffer(static_cast<char *>(dg_sock::network_exception::remove_expected(StatelessAllocator::malloc(0u)))),
                                      buffer_sz(0u),
                                      buffer_cap(0u){}

            kernel_string(size_t str_sz): kernel_string() 
            {
                try
                {
                    this->resize(str_sz);
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            kernel_string(const self& other): kernel_string()
            {
                try
                {
                    *this = other;
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            kernel_string(std::basic_string_view<char> other): kernel_string()
            {
                try
                {
                    this->assign_string_view(other);
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            kernel_string(self&& other) noexcept: kernel_string()
            {
                *this = static_cast<self&&>(other);
            }

            ~kernel_string() noexcept
            {
                this->free_resource();
            }

            auto operator =(const self& other) -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->assign_string_view(other);
                return *this;
            }

            auto operator =(self&& other) noexcept -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->free_resource();

                this->buffer        = std::exchange(other.buffer, static_cast<char *>(dg_sock::network_exception::remove_expected(StatelessAllocator::malloc(0u))));
                this->buffer_sz     = std::exchange(other.buffer_sz, 0u);
                this->buffer_cap    = std::exchange(other.buffer_cap, 0u);

                return *this;
            }

            auto size() const noexcept -> size_t
            {
                return this->buffer_sz;
            }

            auto at(size_t idx) noexcept -> char&
            {
                this->bound_check(idx);

                return this->buffer[idx];
            }

            auto at(size_t idx) const noexcept -> const char&
            {
                this->bound_check(idx);

                return this->buffer[idx];
            }

            auto operator[](size_t idx) noexcept -> char&
            {
                return this->at(idx);
            }

            auto operator[](size_t idx) const noexcept -> const char&
            {
                return this->at(idx);
            }

            auto front() const noexcept -> const char&
            {
                return this->at(0u);
            }

            auto front() noexcept -> char&
            {
                return this->at(0u);
            }

            auto back() const noexcept -> const char&
            {
                this->bound_check(0u);

                return this->at(this->buffer_sz - 1u);
            }

            auto back() noexcept -> char&
            {
                this->bound_check(0u);

                return this->at(this->buffer_sz - 1u);
            }

            auto begin() noexcept -> char *
            {
                return this->buffer;
            }

            auto begin() const noexcept -> const char *
            {
                return this->buffer;
            }

            auto end() noexcept -> char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto end() const noexcept -> const char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto cbegin() const noexcept -> const char *
            {
                return this->buffer;
            }

            auto cend() const noexcept -> const char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto empty() const noexcept -> bool
            {
                return this->buffer_sz == 0u;
            }
            
            auto data() const noexcept -> const char *
            {
                return this->buffer;                
            }

            auto data() noexcept -> char *
            {
                return this->buffer;
            }

            consteval auto max_size() -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }

            void reserve(size_t byte_sz)
            {
                if (byte_sz <= this->buffer_cap)
                {
                    return;
                }

                std::expected<void *, exception_t> new_buffer = StatelessAllocator::realloc(this->buffer, byte_sz);

                if (!new_buffer.has_value())
                {
                    dg_sock::network_exception::throw_exception(new_buffer.error());
                }

                this->buffer        = static_cast<char *>(new_buffer.value());
                this->buffer_cap    = byte_sz;
            }

            void resize(size_t byte_sz)
            {
                this->reserve(byte_sz);
                this->buffer_sz = byte_sz;
            }

            auto capacity() const noexcept -> size_t
            {
                return this->buffer_cap;
            }

            void clear() noexcept
            {
                this->buffer_sz = 0u;
            }

            void push_back(char c)
            {
                if (this->buffer_sz == this->buffer_cap)
                {
                    size_t cand_1   = this->buffer_cap * 2u;
                    size_t cand_2   = MIN_ALLOCATION_SZ;
                    size_t cand     = std::max(cand_1, cand_2);

                    this->reserve(cand);
                }

                this->buffer[this->buffer_sz++] = c;
            }

            void pop_back() noexcept
            {
                this->bound_check(0u);

                this->buffer_sz -= 1u;
            }

            void swap(self& other) noexcept
            {
                std::swap(this->buffer, other.buffer);
                std::swap(this->buffer_sz, other.buffer_sz);
                std::swap(this->buffer_cap, other.buffer_cap);
            }

            operator std::basic_string_view<char>() const noexcept
            {
                return std::basic_string_view<char>(this->buffer, this->buffer_sz);
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector) const
            {
                reflector(this->buffer_sz);

                for (size_t i = 0u; i < this->buffer_sz; ++i)
                {
                    reflector(this->buffer[i]);
                }
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector)
            {
                size_t sz;
                reflector(sz);

                this->reserve(sz);
                this->clear();

                for (size_t i = 0u; i < sz; ++i)
                {
                    char c;
                    reflector(c);
                    this->push_back(c);
                }
            }

        private:

            void free_resource() noexcept
            {
                StatelessAllocator::free(this->buffer);
            }

            void assign_string_view(std::basic_string_view<char> str_view)
            {
                if (this->buffer_cap >= str_view.size())
                {
                    std::copy(str_view.begin(), str_view.end(), this->buffer);
                    this->buffer_sz = str_view.size();

                    return;
                }

                std::expected<void *, exception_t> mem_blk = StatelessAllocator::malloc(str_view.size());

                if (!mem_blk.has_value())
                {
                    dg_sock::network_exception::throw_exception(mem_blk.error());
                }

                this->free_resource();

                std::copy(str_view.begin(), str_view.end(), static_cast<char *>(mem_blk.value()));

                this->buffer        = static_cast<char *>(mem_blk.value());
                this->buffer_sz     = str_view.size();
                this->buffer_cap    = str_view.size();
            }

            void bound_check(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->buffer_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }
                else
                {
                    (void) idx;
                }
            }
    };

    template <class StatefulAllocator = StdAllocator>
    class polymorphic_kernel_string
    {
        private:

            char * buffer;
            size_t buffer_sz;
            size_t buffer_cap;
            StatefulAllocator allocator;

            static inline constexpr size_t MIN_ALLOCATION_SZ = 8u;

        public:

            static_assert(std::is_nothrow_copy_constructible_v<StatefulAllocator>);
            static_assert(std::is_nothrow_move_constructible_v<StatefulAllocator>);
            static_assert(std::is_nothrow_swappable_v<StatefulAllocator>);

            using self = polymorphic_kernel_string;
            using value_type = char;

            polymorphic_kernel_string(const StatefulAllocator& allocator = StatefulAllocator()) noexcept: buffer(static_cast<char *>(dg_sock::network_exception::remove_expected(allocator.malloc(0u)))),
                                                                                                          buffer_sz(0u),
                                                                                                          buffer_cap(0u),
                                                                                                          allocator(allocator){}

            polymorphic_kernel_string(size_t str_sz,
                                      const StatefulAllocator& allocator = StatefulAllocator()): polymorphic_kernel_string(allocator) 
            {
                try
                {
                    this->resize(str_sz);
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            polymorphic_kernel_string(const self& other): polymorphic_kernel_string(other.allocator)
            {
                try
                {
                    *this = other;
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            polymorphic_kernel_string(std::basic_string_view<char> other,
                                      const StatefulAllocator& allocator = StatefulAllocator()): polymorphic_kernel_string(allocator)
            {
                try
                {
                    this->assign_string_view(other);
                }
                catch (...)
                {
                    this->free_resource();
                    throw;
                }
            }

            polymorphic_kernel_string(self&& other) noexcept: polymorphic_kernel_string(other.allocator)
            {
                *this = static_cast<self&&>(other);
            }

            ~polymorphic_kernel_string() noexcept
            {
                this->free_resource();
            }

            auto operator =(const self& other) -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->assign_string_view(other);
                return *this;
            }

            auto operator =(self&& other) noexcept -> self&
            {
                if (this == std::addressof(other))
                {
                    return *this;
                }

                this->free_resource();

                this->buffer        = std::exchange(other.buffer, static_cast<char *>(dg_sock::network_exception::remove_expected(other.allocator.malloc(0u))));
                this->buffer_sz     = std::exchange(other.buffer_sz, 0u);
                this->buffer_cap    = std::exchange(other.buffer_cap, 0u);
                this->allocator     = other.allocator; //

                return *this;
            }

            auto size() const noexcept -> size_t
            {
                return this->buffer_sz;
            }

            auto at(size_t idx) noexcept -> char&
            {
                this->bound_check(idx);

                return this->buffer[idx];
            }

            auto at(size_t idx) const noexcept -> const char&
            {
                this->bound_check(idx);

                return this->buffer[idx];
            }

            auto operator[](size_t idx) noexcept -> char&
            {
                return this->at(idx);
            }

            auto operator[](size_t idx) const noexcept -> const char&
            {
                return this->at(idx);
            }

            auto front() const noexcept -> const char&
            {
                return this->at(0u);
            }

            auto front() noexcept -> char&
            {
                return this->at(0u);
            }

            auto back() const noexcept -> const char&
            {
                this->bound_check(0u);

                return this->at(this->buffer_sz - 1u);
            }

            auto back() noexcept -> char&
            {
                this->bound_check(0u);

                return this->at(this->buffer_sz - 1u);
            }

            auto begin() noexcept -> char *
            {
                return this->buffer;
            }

            auto begin() const noexcept -> const char *
            {
                return this->buffer;
            }

            auto end() noexcept -> char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto end() const noexcept -> const char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto cbegin() const noexcept -> const char *
            {
                return this->buffer;
            }

            auto cend() const noexcept -> const char *
            {
                return std::next(this->buffer, this->buffer_sz);
            }

            auto empty() const noexcept -> bool
            {
                return this->buffer_sz == 0u;
            }
            
            auto data() const noexcept -> const char *
            {
                return this->buffer;                
            }

            auto data() noexcept -> char *
            {
                return this->buffer;
            }

            consteval auto max_size() -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }

            void reserve(size_t byte_sz)
            {
                if (byte_sz <= this->buffer_cap)
                {
                    return;
                }

                std::expected<void *, exception_t> new_buffer = this->allocator.realloc(this->buffer, byte_sz);

                if (!new_buffer.has_value())
                {
                    dg_sock::network_exception::throw_exception(new_buffer.error());
                }

                this->buffer        = static_cast<char *>(new_buffer.value());
                this->buffer_cap    = byte_sz;
            }

            void resize(size_t byte_sz)
            {
                this->reserve(byte_sz);
                this->buffer_sz = byte_sz;
            }

            auto capacity() const noexcept -> size_t
            {
                return this->buffer_cap;
            }

            void clear() noexcept
            {
                this->buffer_sz = 0u;
            }

            void push_back(char c)
            {
                if (this->buffer_sz == this->buffer_cap)
                {
                    size_t cand_1   = this->buffer_cap * 2u;
                    size_t cand_2   = MIN_ALLOCATION_SZ;
                    size_t cand     = std::max(cand_1, cand_2);

                    this->reserve(cand);
                }

                this->buffer[this->buffer_sz++] = c;
            }

            void pop_back() noexcept
            {
                this->bound_check(0u);

                this->buffer_sz -= 1u;
            }

            void swap(self& other) noexcept
            {
                std::swap(this->buffer, other.buffer);
                std::swap(this->buffer_sz, other.buffer_sz);
                std::swap(this->buffer_cap, other.buffer_cap);
                std::swap(this->allocator, other.allocator);
            }

            operator std::basic_string_view<char>() const noexcept
            {
                return std::basic_string_view<char>(this->buffer, this->buffer_sz);
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector) const
            {
                reflector(this->buffer_sz);

                for (size_t i = 0u; i < this->buffer_sz; ++i)
                {
                    reflector(this->buffer[i]);
                }
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector)
            {
                size_t sz;
                reflector(sz);

                this->reserve(sz);
                this->clear();

                for (size_t i = 0u; i < sz; ++i)
                {
                    char c;
                    reflector(c);
                    this->push_back(c);
                }
            }

        private:

            void free_resource() noexcept
            {
                this->allocator.free(this->buffer);
            }

            void assign_string_view(std::basic_string_view<char> str_view)
            {
                if (this->buffer_cap >= str_view.size())
                {
                    std::copy(str_view.begin(), str_view.end(), this->buffer);
                    this->buffer_sz = str_view.size();

                    return;
                }

                std::expected<void *, exception_t> mem_blk = this->allocator.malloc(str_view.size());

                if (!mem_blk.has_value())
                {
                    dg_sock::network_exception::throw_exception(mem_blk.error());
                }

                this->free_resource();

                std::copy(str_view.begin(), str_view.end(), static_cast<char *>(mem_blk.value()));

                this->buffer        = static_cast<char *>(mem_blk.value());
                this->buffer_sz     = str_view.size();
                this->buffer_cap    = str_view.size();
            }

            void bound_check(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->buffer_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }
                else
                {
                    (void) idx;
                }
            }
    };

    // template <class StatefulAllocator = StdAllocator>
}

#endif