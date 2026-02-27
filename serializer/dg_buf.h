#ifndef __DG_DGBUF__
#define __DG_DGBUF__

#include "trivial_serializer.h"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <vector>
#include <array>
#include <stdint.h>
#include <type_traits>
#include <utility>
#include <optional>
#include <functional>
#include <bit>
#include <algorithm>
#include "assert.h"
#include <string>
#include <string_view>
#include "assert.h"

namespace dg::dgbuf::types
{
    using vaddr_type    = uint64_t;
    using size_type     = uint64_t;
}

namespace dg::dgbuf::constants
{
    static inline constexpr size_t MAX_TEMPLATE_RECURSIVE_DEPTH = 5;
    static inline constexpr double CAP_TO_SIZE_RATIO = 2; 

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;
}

namespace dg::dgbuf::utility
{    
    struct modulo_key_to_idx
    {
        template <class Key>
        constexpr auto operator()(Key&& key, size_t cap) const noexcept(noexcept(key % cap)) -> size_t
        {
            return key % cap;
        }  
    };

    struct bitwise_and_key_to_idx
    {
        template <class Key>
        constexpr auto operator()(Key&& key, size_t cap) const noexcept(noexcept(key & (cap - 1u))) -> size_t
        {
            return key & (cap - 1u);
        }
    };

    static constexpr auto log2(size_t val) noexcept -> size_t
    {
        return static_cast<size_t>(sizeof(size_t) * CHAR_BIT - 1u) - static_cast<size_t>(std::countl_zero(val));
    }

    static constexpr auto pow2_ceil(size_t val) noexcept -> size_t
    {
        if (val < 2u) [[unlikely]]
        {
            return 1u;
        }
        else [[likely]]
        {
            size_t uplog_value = log2(static_cast<size_t>(val - 1u)) + 1u;
            return size_t{1u} << uplog_value;
        }
    }

    static constexpr auto is_pow2(size_t x) noexcept -> bool
    {
        return x != 0u && (x & (x - 1u)) == 0u;
    } 
}

namespace dg::dgbuf::iterator
{
    template <class T, class AutoSetter>
    class vector_view_iterator: public std::iterator<std::random_access_iterator_tag, T, std::ptrdiff_t, void *, T>
    {
        private:

            const char * buf;
            intmax_t offs;

            using self = vector_view_iterator<T, AutoSetter>;
            using base = std::iterator<std::random_access_iterator_tag, T, std::ptrdiff_t, void *, T>;

        public:

            using difference_type   = typename base::difference_type;
            using reference         = typename base::reference;

            constexpr vector_view_iterator() = default;

            constexpr vector_view_iterator(const char * buf,
                                           intmax_t offs) noexcept: buf(buf),
                                                                    offs(offs){}

            constexpr auto operator *() const noexcept -> reference
            {
                assert(this->buf != nullptr);

                T rs;
                trivial_serializer::deserialize_into(rs, std::next(this->buf, this->offs));

                return AutoSetter{}.auto_set(rs, this->buf);
            }

            constexpr auto operator[](difference_type idx) const noexcept -> reference
            {
                return *(*this + idx);
            }

            constexpr auto operator ==(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() == rhs.numerical_addr();
            }

            constexpr auto operator !=(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() != rhs.numerical_addr();
            }

            constexpr auto operator >(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() > rhs.numerical_addr();
            }

            constexpr auto operator <(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() < rhs.numerical_addr();
            }

            constexpr auto operator >=(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() >= rhs.numerical_addr();
            }

            constexpr auto operator <=(const self& rhs) const noexcept -> bool
            {
                return this->numerical_addr() <= rhs.numerical_addr();
            }

            constexpr auto operator ++() noexcept -> self&
            {
                this->offs += trivial_serializer::size(T{});

                return *this;
            } 

            constexpr auto operator ++(int) noexcept -> self
            {
                self pre = *this;
                ++(*this);

                return pre;
            }

            constexpr auto operator --() noexcept -> self&
            {
                this->offs -= static_cast<intmax_t>(trivial_serializer::size(T{}));

                return *this;
            }

            constexpr auto operator --(int) noexcept -> self
            {
                self pre = *this;
                --(*this);

                return pre;
            }

            constexpr auto operator +(difference_type idx) const noexcept -> self
            {
                return self{this->buf, this->offs + static_cast<intmax_t>(trivial_serializer::size(T{})) * idx};
            }

            constexpr auto operator +=(difference_type idx) noexcept -> self&
            {
                *this = *this + idx;

                return *this;
            }

            constexpr auto operator -(difference_type idx) const noexcept -> self
            {
                return self{this->buf, this->offs - static_cast<intmax_t>(trivial_serializer::size(T{})) * idx};
            }

            constexpr auto operator -=(difference_type idx) noexcept -> self&
            {
                *this = *this - idx;

                return *this;
            }

            constexpr auto operator -(const self& other) const noexcept -> difference_type
            {
                return static_cast<difference_type>(this->numerical_addr() - other.numerical_addr()) / static_cast<difference_type>(trivial_serializer::size(T{}));
            }
        
        private:

            constexpr auto numerical_addr() const noexcept -> intptr_t
            {
                return reinterpret_cast<intptr_t>(this->buf) + this->offs;
            } 
    };

    template <class T, class AutoSetter>
    constexpr auto operator +(typename vector_view_iterator<T, AutoSetter>::difference_type lhs, const vector_view_iterator<T, AutoSetter>& rhs) noexcept(noexcept(rhs + lhs)) -> decltype(rhs + lhs)
    {
        return rhs + lhs;
    }  

    template <class K, class V, class AutoSetter, class NextSeeker>
    class unordered_flat_map_view_iterator: public std::iterator<std::input_iterator_tag, std::pair<K, V>, std::ptrdiff_t, void *, std::pair<K, V>>
    {
        private:

            vector_view_iterator<std::optional<std::pair<K, V>>, AutoSetter> ptr;
            NextSeeker nxt_seeker; 

            using self      = unordered_flat_map_view_iterator<K, V, AutoSetter, NextSeeker>;
            using base      = std::iterator<std::input_iterator_tag, std::pair<K, V>, std::ptrdiff_t, void *, std::pair<K, V>>;

        public:

            using reference = typename base::reference;

            constexpr unordered_flat_map_view_iterator(vector_view_iterator<std::optional<std::pair<K, V>>, AutoSetter> ptr,
                                                       NextSeeker nxt_seeker) noexcept: ptr(ptr), nxt_seeker(nxt_seeker){}

            constexpr auto operator *() const noexcept -> reference
            {
                return (*this->ptr).value();
            }

            constexpr auto operator ==(const self& rhs) const noexcept -> bool
            {
                return this->ptr == rhs.ptr;
            }

            constexpr auto operator !=(const self& rhs) const noexcept -> bool
            {
                return this->ptr != rhs.ptr;
            }

            constexpr auto operator ++() noexcept -> self&
            {
                this->ptr = this->nxt_seeker(this->ptr);

                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self
            {
                self pre = *this;
                ++(*this);

                return pre;
            }
    };

    template <class K, class AutoSetter, class NextSeeker>
    class unordered_flat_set_view_iterator: public std::iterator<std::input_iterator_tag, K, std::ptrdiff_t, void *, K>
    {
        private:

            vector_view_iterator<std::optional<K>, AutoSetter> ptr;
            NextSeeker nxt_seeker;

            using self      = unordered_flat_set_view_iterator<K, AutoSetter, NextSeeker>;
            using base      = std::iterator<std::input_iterator_tag, K, std::ptrdiff_t, void *, K>;

        public:

            using reference = typename base::reference;

            constexpr unordered_flat_set_view_iterator(vector_view_iterator<std::optional<K>, AutoSetter> ptr,
                                                       NextSeeker nxt_seeker) noexcept: ptr(ptr), nxt_seeker(nxt_seeker){}

            constexpr auto operator *() const noexcept -> reference
            {
                return (*this->ptr).value();
            }

            constexpr auto operator ==(const self& rhs) const noexcept -> bool
            {
                return this->ptr == rhs.ptr;
            }

            constexpr auto operator !=(const self& rhs) const noexcept -> bool
            {    
                return this->ptr != rhs.ptr;
            }

            constexpr auto operator ++() noexcept -> self&
            {
                this->ptr = this->nxt_seeker(this->ptr);

                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self
            {
                self pre = *this;
                ++(*this);

                return pre;
            }
    };

    template <class K, class V, class AutoSetter>
    class map_view_iterator: public std::iterator<std::input_iterator_tag, std::pair<K, V>, std::ptrdiff_t, void *, std::pair<K, V>>
    {
        private:

            vector_view_iterator<std::pair<K, V>, AutoSetter> ptr;

            using self      = map_view_iterator<K, V, AutoSetter>;
            using base      = std::iterator<std::input_iterator_tag, std::pair<K, V>, std::ptrdiff_t, void *, std::pair<K, V>>;

        public:

            using reference = typename base::reference; 

            constexpr map_view_iterator(vector_view_iterator<std::pair<K, V>, AutoSetter> ptr) noexcept: ptr(ptr){}

            constexpr auto operator *() const noexcept -> reference
            {
                return *this->ptr;
            }

            constexpr auto operator ==(const self& rhs) const noexcept -> bool
            {
                return this->ptr == rhs.ptr;
            }

            constexpr auto operator !=(const self& rhs) const noexcept -> bool
            {
                return this->ptr != rhs.ptr;
            }

            constexpr auto operator ++() noexcept -> self&
            {
                ++this->ptr;

                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self
            {
                auto pre = *this;
                ++(*this);

                return pre;
            }   
    };

    template <class K, class AutoSetter>
    class set_view_iterator: public std::iterator<std::input_iterator_tag, K, std::ptrdiff_t, void *, K>
    {
        private:

            vector_view_iterator<K, AutoSetter> ptr;

            using self      = set_view_iterator<K, AutoSetter>;
            using base      = std::iterator<std::input_iterator_tag, K, std::ptrdiff_t, void *, K>;

        public:

            using reference = typename base::reference;

            constexpr set_view_iterator(vector_view_iterator<K, AutoSetter> ptr) noexcept: ptr(ptr){}

            constexpr auto operator *() const noexcept -> reference
            {
                return *this->ptr;
            }

            constexpr auto operator ==(const self& rhs) const noexcept -> bool
            {
                return this->ptr == rhs.ptr;
            }

            constexpr auto operator !=(const self& rhs) const noexcept -> bool
            {
                return this->ptr != rhs.ptr;
            }

            constexpr auto operator ++() noexcept -> self&
            {
                ++this->ptr;

                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self
            {
                auto pre = *this;
                ++(*this);

                return pre;
            }     
    };

    template <class T, class AutoSetter>
    static constexpr auto seek_next_available_bucket(vector_view_iterator<std::optional<T>, AutoSetter> first, vector_view_iterator<std::optional<T>, AutoSetter> last) noexcept
    {
        for (auto it = first; it != last; ++it)
        {
            if ((*it).has_value())
            {
                return it;
            }
        }

        return last;
    }

    template <class T, class AutoSetter>
    class propagator_device
    {
        private:

            vector_view_iterator<std::optional<T>, AutoSetter> last;

        public:

            constexpr propagator_device() = default;

            constexpr propagator_device(vector_view_iterator<std::optional<T>, AutoSetter> last) noexcept: last(last){}

            constexpr auto operator()(vector_view_iterator<std::optional<T>, AutoSetter> cur) noexcept -> vector_view_iterator<std::optional<T>, AutoSetter>
            {
                return seek_next_available_bucket(++cur, last);
            }
    };

    template <class T, class AutoSetter>
    static constexpr auto make_propagator_device(vector_view_iterator<std::optional<T>, AutoSetter> last) noexcept
    {
        propagator_device device(last);

        return device;
    }
}

namespace dg::dgbuf::datastructure
{
    template <class T, class AutoSetter>
    class vector_view
    {
        private:

            types::vaddr_type data;
            types::size_type sz;
            const char * buf;

            using self = vector_view;

        protected:

            constexpr auto buffer_head() const noexcept -> const char *
            {
                return this->buf;
            }

            constexpr auto buffer_data() const noexcept -> const char *
            {
                return std::next(this->buf, this->data);
            }

        public:

            using value_type        = T;
            using iterator_type     = iterator::vector_view_iterator<T, AutoSetter>;
            using difference_type   = typename iterator_type::difference_type;

            static_assert(true); //T types - 

            constexpr vector_view() = default;

            constexpr vector_view(types::vaddr_type data, 
                                  types::size_type sz, 
                                  const char * buf) noexcept: data(data), sz(sz), buf(buf){}

            constexpr void set_buf(const char * buf) noexcept
            {
                this->buf = buf;
            } 

            constexpr auto size() const noexcept -> types::size_type
            {
                return this->sz;
            }

            constexpr auto get(size_t idx) const noexcept -> T
            {
                assert(idx < this->sz);
                assert(this->buf != nullptr);
            
                const char * ptr = std::next(this->buf, this->data + idx * trivial_serializer::size(T{}));
                T rs;
                trivial_serializer::deserialize_into(rs, ptr);

                return AutoSetter{}.auto_set(rs, this->buf);
            }

            constexpr auto front() const noexcept -> T
            {
                return this->get(0u);
            }

            constexpr auto back() const noexcept -> T
            {
                assert(this->sz != 0u);
                return this->get(this->sz - 1u);
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator ==(const Other& other) const noexcept -> bool
            {
                if (other.size() != this->size())
                {
                    return false;
                }

                for (size_t i = 0u; i < this->size(); ++i)
                {
                    if (this->get(i) != other.get(i))
                    {
                        return false;
                    }
                }

                return true;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator !=(const Other& other) const noexcept -> bool
            {
                return !this->operator ==(other);
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >(const Other& other) const noexcept -> bool
            {
                size_t comparable_sz = std::min(this->size(), other.size());

                for (size_t i = 0u; i < comparable_sz; ++i)
                {
                    if (this->get(i) > other.get(i))
                    {
                        return true;
                    }

                    if (this->get(i) < other.get(i))
                    {
                        return false;
                    }
                }

                if (this->size() > other.size())
                {
                    return true;
                }

                if (this->size() < other.size())
                {
                    return false;
                }

                return false;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <=(const Other& other) const noexcept -> bool
            {
                return !this->operator >(other);                
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <(const Other& other) const noexcept -> bool
            {
                size_t comparable_sz = std::min(this->size(), other.size());

                for (size_t i = 0u; i < comparable_sz; ++i)
                {
                    if (this->get(i) < other.get(i))
                    {
                        return true;
                    }

                    if (this->get(i) > other.get(i))
                    {
                        return false;
                    }
                }

                if (this->size() < other.size())
                {
                    return true;
                }

                if (this->size() > other.size())
                {
                    return false;
                }

                return false;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >=(const Other& other) const noexcept -> bool
            {
                return !this->operator <(other);
            }

            constexpr auto operator[](size_t idx) const noexcept -> T
            {
                return this->get(idx);
            }

            constexpr auto begin() const noexcept -> iterator_type
            {
                iterator::vector_view_iterator<T, AutoSetter> it(this->buf, this->data);
                return it;
            }

            constexpr auto end() const noexcept -> iterator_type
            {
                iterator::vector_view_iterator<T, AutoSetter> it(this->buf, this->data + this->sz * trivial_serializer::size(T{}));
                return it;
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(this->data, this->sz);
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(this->data, this->sz);
            }
    };

    template <class AutoSetter>
    class str_view: private vector_view<char, AutoSetter>
    {
        private:

            using base              = vector_view<char, AutoSetter>;
            using self              = str_view;

        public:

            using value_type        = char;
            using iterator_type     = typename vector_view<char, AutoSetter>::iterator_type;
            using difference_type   = typename vector_view<char, AutoSetter>::difference_type;

            constexpr str_view() = default;

            constexpr str_view(types::vaddr_type data, 
                               types::size_type sz, 
                               const char * buf): base(data, sz, buf){}

            constexpr void set_buf(const char * buf) noexcept
            {
                base::set_buf(buf);
            }

            constexpr auto size() const noexcept -> types::size_type
            {
                return base::size();
            }

            constexpr auto get(size_t idx) const noexcept -> char
            {
                return base::get(idx);
            }

            constexpr auto front() const noexcept -> char
            {
                return base::front();
            }

            constexpr auto back() const noexcept -> char
            {
                return base::back();
            }

            constexpr auto operator[](size_t idx) const noexcept -> char
            {
                return base::operator[][idx];
            }

            constexpr operator std::string_view() const noexcept
            {
                return std::string_view(base::buffer_data(), base::size());
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) == static_cast<const base&>(other);
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) != static_cast<const base&>(other);
            }

            constexpr auto operator >(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) > static_cast<const base&>(other);
            }

            constexpr auto operator <=(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) <= static_cast<const base&>(other);
            }

            constexpr auto operator <(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) < static_cast<const base&>(other);
            }

            constexpr auto operator >=(const self& other) const noexcept -> bool
            {
                return static_cast<const base&>(*this) >= static_cast<const base&>(other);
            }

            constexpr auto operator ==(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) == other;
            }

            constexpr auto operator !=(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) != other;
            }

            constexpr auto operator >(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) > other;
            }

            constexpr auto operator <=(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) <= other;
            }

            constexpr auto operator <(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) < other;
            }

            constexpr auto operator >=(std::string_view other) const noexcept -> bool
            {
                return static_cast<std::string_view>(*this) >= other;
            }

            constexpr auto begin() const noexcept -> iterator_type
            {
                return base::begin();
            }

            constexpr auto end() const noexcept -> iterator_type
            {
                return base::end();
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(static_cast<const base&>(*this));
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(static_cast<base&>(*this));
            }
    };

    template <class Key, class Value, class AutoSetter, class Hasher = std::hash<Key>, class KeyEq = std::equal_to<Key>, class KeyToIdx = utility::modulo_key_to_idx>
    class unordered_flat_map_view
    {
        private:

            vector_view<std::optional<std::pair<Key, Value>>, AutoSetter> buckets;
            types::size_type sz;

            using self = unordered_flat_map_view;

        public:

            using value_type        = std::pair<Key, Value>;
            using difference_type   = typename vector_view<std::optional<std::pair<Key, Value>>, AutoSetter>::difference_type;
            using iterator_type     = iterator::unordered_flat_map_view_iterator<Key, Value, AutoSetter, iterator::propagator_device<std::pair<Key, Value>, AutoSetter>>;

            static_assert(std::is_trivially_constructible_v<Hasher>);
            static_assert(std::is_trivially_constructible_v<KeyEq>);
            static_assert(std::is_trivially_constructible_v<KeyToIdx>);

            constexpr unordered_flat_map_view() = default;

            constexpr unordered_flat_map_view(vector_view<std::optional<std::pair<Key, Value>>, AutoSetter> buckets,
                                              types::size_type sz) noexcept: buckets(buckets), sz(sz){}

            constexpr void set_buf(const char * buf) noexcept
            {
                this->buckets.set_buf(buf);
            }

            constexpr auto size() const noexcept -> types::size_type
            {
                return this->sz;
            }

            constexpr auto begin() const noexcept -> iterator_type
            {
                iterator::unordered_flat_map_view_iterator it(iterator::seek_next_available_bucket(this->buckets.begin(), this->buckets.end()), iterator::make_propagator_device(this->buckets.end()));
                return it;
            }

            constexpr auto end() const noexcept -> iterator_type
            {
                iterator::unordered_flat_map_view_iterator it(this->buckets.end(), iterator::make_propagator_device(this->buckets.end()));
                return it;
            }

            template <class KeyLike>
            constexpr auto find(KeyLike&& key) const noexcept -> iterator_type
            {
                auto hashed = Hasher{}(key);

                for (size_t i = 0; i < this->buckets.size(); ++i)
                {
                    size_t slot = KeyToIdx{}(hashed + i, this->buckets.size());
                    auto bucket = this->buckets.get(slot);

                    if (!bucket)
                    {
                        return this->end();
                    }

                    if (KeyEq{}(key, bucket->first))
                    {
                        iterator::unordered_flat_map_view_iterator it(std::next(this->buckets.begin(), slot), iterator::make_propagator_device(this->buckets.end()));
                        return it;
                    }
                }

                return this->end();
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator ==(const Other& other) const noexcept -> bool
            {
                if (this->size() != other.size())
                {
                    return false;
                }

                for (const auto [key, value]: *this)
                {
                    auto map_ptr = other.find(key);

                    if (map_ptr == other.end())
                    {
                        return false;
                    }

                    if ((*map_ptr).second != value)
                    {
                        return false;
                    }
                }

                return true;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator !=(const Other& other) const noexcept -> bool
            {
                return !this->operator ==(other);
            }

            template <class ...Args>
            constexpr auto operator[](Args&& ...args) noexcept -> value_type
            {
                return (*this->find(std::forward<Args>(args)...)).second;
            } 

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(this->buckets, this->sz);
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(this->buckets, this->sz);
            }
    };

    template <class Key, class AutoSetter, class Hasher = std::hash<Key>, class KeyEq = std::equal_to<Key>, class KeyToIdx = utility::modulo_key_to_idx>
    class unordered_flat_set_view
    {
        private:

            vector_view<std::optional<Key>, AutoSetter> buckets;
            types::size_type sz;

            using self = unordered_flat_set_view;

        public:

            using value_type        = Key;
            using difference_type   = typename vector_view<std::optional<Key>, AutoSetter>::difference_type;
            using iterator_type     = iterator::unordered_flat_set_view_iterator<Key, AutoSetter, iterator::propagator_device<Key, AutoSetter>>;

            static_assert(std::is_trivially_constructible_v<Hasher>);
            static_assert(std::is_trivially_constructible_v<KeyEq>);
            static_assert(std::is_trivially_constructible_v<KeyToIdx>);

            constexpr unordered_flat_set_view() = default;

            constexpr unordered_flat_set_view(vector_view<std::optional<Key>, AutoSetter> buckets,
                                              types::size_type sz) noexcept: buckets(buckets), sz(sz){}
            
            constexpr void set_buf(const char * buf) noexcept
            {
                this->buckets.set_buf(buf);
            } 

            constexpr auto size() const noexcept -> types::size_type
            {
                return this->sz;
            }

            constexpr auto begin() const noexcept -> iterator_type
            {    
                iterator::unordered_flat_set_view_iterator it(iterator::seek_next_available_bucket(this->buckets.begin(), this->buckets.end()), iterator::make_propagator_device(this->buckets.end()));
                return it;
            }

            constexpr auto end() const noexcept -> iterator_type
            {    
                iterator::unordered_flat_set_view_iterator it(this->buckets.end(), iterator::make_propagator_device(this->buckets.end()));
                return it;
            }

            template <class KeyLike>
            constexpr auto find(KeyLike&& key) const noexcept -> iterator_type
            {
                auto hashed = Hasher{}(key);

                for (size_t i = 0; i < this->sz; ++i)
                {
                    size_t slot = KeyToIdx{}(hashed + i, this->buckets.size());
                    auto bucket = this->buckets.get(slot);

                    if (!bucket)
                    {
                        return this->end();
                    }

                    if (KeyEq{}(key, bucket.value()))
                    {
                        iterator::unordered_flat_set_view_iterator it(std::next(this->buckets.begin(), slot), iterator::make_propagator_device(this->buckets.end()));
                        return it;
                    }
                }

                return this->end();
            }

            template <class KeyLike>
            constexpr auto contains(KeyLike&& key) const noexcept -> bool
            {
                return this->find(std::forward<KeyLike>(key)) != this->end();
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator ==(const Other& other) const noexcept -> bool
            {
                if (this->size() != other.size())
                {
                    return false;
                }

                for (const auto key: *this)
                {
                    auto map_ptr = other.find(key);

                    if (map_ptr == other.end())
                    {
                        return false;
                    }
                }

                return true;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator !=(const Other& other) const noexcept -> bool
            {
                return !this->operator ==(other);
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(this->buckets, this->sz);
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(this->buckets, this->sz);
            }
    };

    template <class Key, class Value, class AutoSetter, class Comparer = std::less<Key>>
    class map_view
    {
        private:

            vector_view<std::pair<Key, Value>, AutoSetter> buckets;
            types::size_type sz;

            using self = map_view;

        public:

            using value_type        = std::pair<Key, Value>;
            using difference_type   = typename vector_view<std::pair<Key, Value>, AutoSetter>::difference_type;
            using iterator_type     = iterator::map_view_iterator<Key, Value, AutoSetter>;

            static_assert(std::is_trivially_constructible_v<Comparer>);

            constexpr map_view() = default;

            constexpr map_view(vector_view<std::pair<Key, Value>, AutoSetter> buckets, 
                               types::size_type sz) noexcept: buckets(buckets), sz(sz){}

            constexpr void set_buf(const char * buf) noexcept
            {
                this->buckets.set_buf(buf);
            }

            constexpr auto size() const noexcept -> types::size_type
            {
                return this->sz;
            }

            constexpr auto begin() const noexcept -> iterator_type
            {    
                iterator::map_view_iterator it(this->buckets.begin()); 
                return it;
            }

            constexpr auto end() const noexcept -> iterator_type
            {
                iterator::map_view_iterator it(this->buckets.end());
                return it;
            }

            template <class KeyLike>
            constexpr auto find(KeyLike&& key) const noexcept -> iterator_type
            {    
                auto cmp = [](std::pair<Key, Value> lhs, const auto& tgt)
                {
                    return Comparer{}(lhs.first, tgt);
                };
                auto ptr = std::lower_bound(this->buckets.begin(), this->buckets.end(), key, cmp);

                if (ptr == this->buckets.end())
                {
                    return this->end();
                }

                std::pair<Key, Value> found = *ptr; 

                if (!Comparer{}(found.first, key) && !Comparer{}(key, found.first))
                {
                    iterator::map_view_iterator it(ptr);
                    return it;
                }

                return this->end();
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator ==(const Other& other) const noexcept -> bool
            {
                return this->buckets == other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator !=(const Other& other) const noexcept -> bool
            {
                return this->buckets != other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >(const Other& other) const noexcept -> bool
            {
                return this->buckets > other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <=(const Other& other) const noexcept -> bool
            {
                return this->buckets <= other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <(const Other& other) const noexcept -> bool
            {
                return this->buckets < other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >=(const Other& other) const noexcept -> bool
            {
                return this->buckets >= other.buckets;
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(this->buckets, this->sz);
            } 

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(this->buckets, this->sz);
            }
    };

    template <class Key, class AutoSetter, class Comparer = std::less<Key>>
    class set_view
    {
        private:

            vector_view<Key, AutoSetter> buckets;
            types::size_type sz;

            using self = set_view;

        public:

            using value_type        = Key;
            using difference_type   = typename vector_view<Key, AutoSetter>::difference_type;
            using iterator_type     = iterator::set_view_iterator<Key, AutoSetter>;

            static_assert(std::is_trivially_constructible_v<Comparer>);

            constexpr set_view() = default;

            constexpr set_view(vector_view<Key, AutoSetter> buckets,
                               types::size_type sz) noexcept: buckets(buckets), sz(sz){}

            constexpr void set_buf(const char * buf) noexcept
            {
                this->buckets.set_buf(buf);
            }

            constexpr auto size() const noexcept -> types::size_type
            {
                return this->sz;
            }

            constexpr auto begin() const noexcept -> iterator_type
            {    
                iterator::set_view_iterator it(this->buckets.begin());
                return it;
            }

            constexpr auto end() const noexcept -> iterator_type
            {    
                iterator::set_view_iterator it(this->buckets.end());
                return it;
            }

            template <class KeyLike>
            constexpr auto find(KeyLike&& key) const noexcept -> iterator_type
            {
                auto ptr = std::lower_bound(this->buckets.begin(), this->buckets.end(), key, Comparer{});

                if (ptr == this->buckets.end())
                {
                    return this->end();
                }

                if (!Comparer{}(*ptr, key) && !Comparer{}(key, *ptr))
                {
                    iterator::set_view_iterator it(ptr);
                    return it;
                }

                return this->end();
            }

            template <class KeyLike>
            constexpr auto contains(KeyLike&& key) const noexcept -> bool
            {
                return this->find(std::forward<KeyLike>(key)) != this->end();
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator ==(const Other& other) const noexcept -> bool
            {
                return this->buckets == other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator !=(const Other& other) const noexcept -> bool
            {
                return this->buckets != other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >(const Other& other) const noexcept -> bool
            {
                return this->buckets > other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <=(const Other& other) const noexcept -> bool
            {
                return this->buckets <= other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator <(const Other& other) const noexcept -> bool
            {
                return this->buckets < other.buckets;
            }

            template <class Other = self, std::enable_if_t<std::is_same_v<Other, self>, bool> = true>
            constexpr auto operator >=(const Other& other) const noexcept -> bool
            {
                return this->buckets >= other.buckets;
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) const noexcept
            {
                reflector(this->buckets, this->sz);
            }

            template <class Reflector>
            constexpr void dg_reflect(const Reflector& reflector) noexcept
            {
                reflector(this->buckets, this->sz);
            }
    };
}

namespace dg::dgbuf::types_space
{
    template <class T, class = void>
    struct is_std_fixed_size_container: std::false_type{};
    
    template <class T>
    struct is_std_fixed_size_container<T, std::void_t<decltype(std::tuple_size<T>::value)>>: std::true_type{};

    template <class T>
    struct is_std_optional: std::false_type{};

    template <class ...Args>
    struct is_std_optional<std::optional<Args...>>: std::true_type{}; 

    template <class T>
    struct is_std_basic_string: std::false_type{};

    template <class ...Args>
    struct is_std_basic_string<std::basic_string<char, std::char_traits<char>, Args...>>: std::true_type{};

    template <class T>
    struct is_std_vector: std::false_type{};

    template <class ...Args>
    struct is_std_vector<std::vector<Args...>>: std::true_type{};

    template <class T>
    struct is_std_unordered_map: std::false_type{};

    template <class ...Args>
    struct is_std_unordered_map<std::unordered_map<Args...>>: std::true_type{}; 

    template <class T>
    struct is_std_unordered_set: std::false_type{};

    template <class ...Args>
    struct is_std_unordered_set<std::unordered_set<Args...>>: std::true_type{};

    template <class T>
    struct is_std_map: std::false_type{};

    template <class ...Args>
    struct is_std_map<std::map<Args...>>: std::true_type{}; 

    template <class T>
    struct is_std_set: std::false_type{};

    template <class ...Args>
    struct is_std_set<std::set<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_vector_view: std::false_type{};

    template <class ...Args>
    struct is_dg_vector_view<datastructure::vector_view<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_str_view: std::false_type{};

    template <class ...Args>
    struct is_dg_str_view<datastructure::str_view<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_unordered_flat_map_view: std::false_type{};

    template <class ...Args>
    struct is_dg_unordered_flat_map_view<datastructure::unordered_flat_map_view<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_unordered_flat_set_view: std::false_type{};

    template <class ...Args>
    struct is_dg_unordered_flat_set_view<datastructure::unordered_flat_set_view<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_map_view: std::false_type{};

    template <class ...Args>
    struct is_dg_map_view<datastructure::map_view<Args...>>: std::true_type{};

    template <class T>
    struct is_dg_set_view: std::false_type{};

    template <class ...Args>
    struct is_dg_set_view<datastructure::set_view<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_std_fixed_size_container_v      = is_std_fixed_size_container<T>::value;

    template <class T>
    static inline constexpr bool is_std_optional_v                  = is_std_optional<T>::value;

    template <class T>
    static inline constexpr bool is_std_basic_string_v              = is_std_basic_string<T>::value;

    template <class T>
    static inline constexpr bool is_std_vector_v                    = is_std_vector<T>::value;

    template <class T>
    static inline constexpr bool is_std_unordered_map_v             = is_std_unordered_map<T>::value;

    template <class T>
    static inline constexpr bool is_std_unordered_set_v             = is_std_unordered_set<T>::value;

    template <class T>
    static inline constexpr bool is_std_map_v                       = is_std_map<T>::value;

    template <class T>
    static inline constexpr bool is_std_set_v                       = is_std_set<T>::value;

    template <class T>
    static inline constexpr bool is_dg_vector_view_v                = is_dg_vector_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_str_view_v                   = is_dg_str_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_unordered_flat_map_view_v    = is_dg_unordered_flat_map_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_unordered_flat_set_view_v    = is_dg_unordered_flat_set_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_map_view_v                   = is_dg_map_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_set_view_v                   = is_dg_set_view<T>::value;

    template <class T>
    static inline constexpr bool is_dg_container_view_v             = is_dg_vector_view_v<T> || is_dg_str_view_v<T>
                                                                        || is_dg_unordered_flat_map_view_v<T> || is_dg_unordered_flat_set_view_v<T> 
                                                                        || is_dg_map_view_v<T> || is_dg_set_view_v<T>;
}

namespace dg::dgbuf::hasher
{
    template <class DefaultHasher>
    struct str_view_hasher
    {
        using default_hasher_t = DefaultHasher;

        constexpr auto operator()(std::string_view value) -> size_t
        {
            return std::hash<std::string_view>{}(value);
        }
    };

    template <class T, class Default>
    struct custom_hasher
    {
        using type = Default;
    };

    template <class ...Args, class Default>
    struct custom_hasher<std::basic_string<Args...>, Default>
    {
        using type = str_view_hasher<Default>;
    };

    template <class T, class Default>
    using custom_hasher_t = typename custom_hasher<T, Default>::type;

    template <class DefaultHasher>
    struct reverse_hasher
    {
        using type = DefaultHasher;
    };

    template <class DefaultHasher>
    struct reverse_hasher<str_view_hasher<DefaultHasher>>
    {
        using type = DefaultHasher;
    };

    template <class T>
    using reverse_hasher_t = typename reverse_hasher<T>::type;

    template <class DefaultEqualTo>
    struct str_view_equal_to
    {
        using default_hasher_t = DefaultEqualTo;

        constexpr auto operator()(std::string_view lhs, std::string_view rhs) -> bool
        {
            return lhs == rhs;
        }
    };

    template <class T, class Default>
    struct custom_equal_to
    {
        using type = Default;
    };

    template <class ...Args, class Default>
    struct custom_equal_to<std::basic_string<Args...>, Default>
    {
        using type = str_view_equal_to<Default>;
    };

    template <class T, class Default>
    using custom_equal_to_t = typename custom_equal_to<T, Default>::type;

    template <class DefaultEqualTo>
    struct reverse_equal_to
    {
        using type = DefaultEqualTo;
    };

    template <class DefaultEqualTo>
    struct reverse_equal_to<str_view_equal_to<DefaultEqualTo>>
    {
        using type = DefaultEqualTo;
    };

    template <class T>
    using reverse_equal_to_t = typename reverse_equal_to<T>::type;
}

namespace dg::dgbuf::autosetter
{
    struct AutoSetter
    {
        template <class T, std::enable_if_t<types_space::is_dg_container_view_v<std::decay_t<T>>, bool> = true>
        constexpr auto auto_set(T&& value, const char * buffer_head) -> std::decay_t<T>
        {
            auto tmp = value;
            tmp.set_buf(buffer_head);

            return tmp;
        }

        template <class T, std::enable_if_t<types_space::is_std_optional_v<std::decay_t<T>>, bool> = true>
        constexpr auto auto_set(T&& value, const char * buffer_head) -> std::decay_t<T>
        {
            if (!value.has_value())
            {
                return std::nullopt;
            }

            return auto_set(value.value(), buffer_head);
        }

        template <class T, std::enable_if_t<types_space::is_std_fixed_size_container_v<std::decay_t<T>>, bool> = true>
        constexpr auto auto_set(T&& value, const char * buffer_head) -> std::decay_t<T>
        {
            const auto idx_seq = std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>();
            auto rs = std::decay_t<T>();

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                ((std::get<IDX>(rs) = auto_set(std::get<IDX>(value), buffer_head)), ...);
            }(idx_seq);

            return rs;
        }

        template <class T, std::enable_if_t<std::conjunction_v<std::negation<std::bool_constant<types_space::is_dg_container_view_v<std::decay_t<T>>>>,
                                                               std::negation<types_space::is_std_optional<std::decay_t<T>>>,
                                                               std::negation<types_space::is_std_fixed_size_container<std::decay_t<T>>>>, bool> = true>
        constexpr auto auto_set(T&& value, const char * buffer_head) -> std::decay_t<T>
        {
            return value;
        }
    };
}

namespace dg::dgbuf::stl_to_dgbuf
{
    template <size_t I = 0>
    struct type_converter
    {
        using successor = type_converter<I + 1>;

        template <class Key, class Value, class Hasher, class KeyEq, class ...Args>
        static auto convert(std::unordered_map<Key, Value, Hasher, KeyEq, Args...>) -> datastructure::unordered_flat_map_view<decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>())), autosetter::AutoSetter, hasher::custom_hasher_t<Key, Hasher>, hasher::custom_equal_to_t<Key, KeyEq>, utility::bitwise_and_key_to_idx>;

        template <class Key, class Hasher, class KeyEq, class ...Args>
        static auto convert(std::unordered_set<Key, Hasher, KeyEq, Args...>) -> datastructure::unordered_flat_set_view<decltype(successor::convert(std::declval<Key>())), autosetter::AutoSetter, hasher::custom_hasher_t<Key, Hasher>, hasher::custom_equal_to_t<Key, KeyEq>, utility::bitwise_and_key_to_idx>;

        template <class Key, class Value, class Comparer, class ...Args>
        static auto convert(std::map<Key, Value, Comparer, Args...>) -> datastructure::map_view<decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>())), autosetter::AutoSetter, Comparer>;

        template <class Key, class Comparer, class ...Args>
        static auto convert(std::set<Key, Comparer, Args...>) -> datastructure::set_view<decltype(successor::convert(std::declval<Key>())), autosetter::AutoSetter, Comparer>;

        template <class T, class ...Args>
        static auto convert(std::vector<T, Args...>) -> datastructure::vector_view<decltype(successor::convert(std::declval<T>())), autosetter::AutoSetter>;

        template <class ...Args>
        static auto convert(std::basic_string<char, std::char_traits<char>, Args...>) -> datastructure::str_view<autosetter::AutoSetter>;

        template <class T, size_t N>
        static auto convert(std::array<T, N>) -> std::array<decltype(successor::convert(std::declval<T>())), N>;

        template <class First, class Second>
        static auto convert(std::pair<First, Second>) -> std::pair<decltype(successor::convert(std::declval<First>())), decltype(successor::convert(std::declval<Second>()))>;

        template <class ...Args>
        static auto convert(std::tuple<Args...>) -> std::tuple<decltype(successor::convert(std::declval<Args>()))...>;

        template <class T>
        static auto convert(std::optional<T>) -> std::optional<decltype(successor::convert(std::declval<T>()))>;

        template <class T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        static auto convert(T) -> T; 
    };

    template <>
    struct type_converter<constants::MAX_TEMPLATE_RECURSIVE_DEPTH>
    {
        template <class T>
        static auto convert(T) -> T;
    };

    struct flattener
    {
        template <class Key, class Value, class Hasher, class ...Args, std::enable_if_t<std::is_trivially_constructible_v<Hasher>, bool> = true>
        static auto flatten(const std::unordered_map<Key, Value, Hasher, Args...>& map) -> std::vector<std::optional<std::pair<Key, Value>>>
        {
            auto cap        = utility::pow2_ceil(map.size() * constants::CAP_TO_SIZE_RATIO);
            auto buckets    = std::vector<std::optional<std::pair<Key, Value>>>(cap, std::nullopt);

            for (const auto& kv: map)
            {
                auto hashed = Hasher{}(kv.first);
                bool flag   = false;

                for (size_t i = 0; i < cap; ++i)
                {
                    size_t slot = utility::bitwise_and_key_to_idx{}(hashed + i, cap);

                    if (!buckets[slot])
                    {
                        buckets[slot] = kv;
                        flag = true;
 
                        break;
                    }
                }

                if (!flag)
                {
                    std::abort();
                }
            }

            return buckets;
        } 

        template <class Key, class Hasher, class ...Args, std::enable_if_t<std::is_trivially_constructible_v<Hasher>, bool> = true>
        static auto flatten(const std::unordered_set<Key, Hasher, Args...>& set) -> std::vector<std::optional<Key>>
        {
            auto cap        = utility::pow2_ceil(set.size() * constants::CAP_TO_SIZE_RATIO);
            auto buckets    = std::vector<std::optional<Key>>(cap, std::nullopt);

            for (const auto& k: set)
            {
                auto hashed = Hasher{}(k);
                bool flag   = false;

                for (size_t i = 0; i < cap; ++i)
                {
                    size_t slot = utility::bitwise_and_key_to_idx{}(hashed + i, cap);

                    if (!buckets[slot])
                    {
                        buckets[slot] = k;
                        flag = true;

                        break;
                    }
                }

                if (!flag)
                {
                    std::abort();
                }
            }

            return buckets;
        }

        template <class Key, class Value, class ...Args>
        static auto flatten(const std::map<Key, Value, Args...>& map) -> std::vector<std::pair<Key, Value>>
        {    
            auto buckets = std::vector<std::pair<Key ,Value>>();
            buckets.reserve(map.size());
            std::copy(map.begin(), map.end(), std::back_inserter(buckets));

            return buckets;
        }

        template <class Key, class ...Args>
        static auto flatten(const std::set<Key, Args...>& set) -> std::vector<Key>
        {
            auto buckets = std::vector<Key>();
            buckets.reserve(set.size());
            std::copy(set.begin(), set.end(), std::back_inserter(buckets));

            return buckets;
        }
    };

    struct serializer
    {
        template <class Streamable>
        void resize_streamable(Streamable& streamable, size_t expected_sz)
        {
            if (streamable.size() < expected_sz)
            {
                size_t nxt_capacity = utility::pow2_ceil(streamable.size());
                streamable.reserve(nxt_capacity);
            }

            streamable.resize(expected_sz);
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_vector_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            using converted_value_type = decltype(type_converter<>::convert(std::declval<typename T::value_type>()));

            types::vaddr_type vaddr = streamable.size();

            size_t offset           = streamable.size();
            size_t elemental_sz     = trivial_serializer::size(converted_value_type{});
            size_t total_sz         = elemental_sz * obj.size();
            size_t new_container_sz = streamable.size() + total_sz;
            size_t i                = 0u;

            this->resize_streamable(streamable, new_container_sz);

            for (const auto& e: obj)
            {
                auto serialized_e = serialize(e, streamable);
                trivial_serializer::serialize_into(std::next(streamable.data(), offset + elemental_sz * i), serialized_e);
                i += 1u;
            }

            return {vaddr, obj.size(), std::add_pointer_t<char>()};
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_basic_string_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            types::vaddr_type vaddr = streamable.size();

            size_t offset           = streamable.size();
            size_t elemental_sz     = 1u;
            size_t total_sz         = elemental_sz * obj.size();
            size_t new_container_sz = streamable.size() + total_sz;
            size_t i                = 0u;

            this->resize_streamable(streamable, new_container_sz);
            std::copy(obj.begin(), obj.end(), std::next(streamable.begin(), offset));

            return {vaddr, obj.size(), std::add_pointer_t<char>()};
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_unordered_map_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            auto flattened  = flattener::flatten(obj);
            auto serialized = serialize(flattened, streamable);

            return {serialized, obj.size()};
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_map_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            auto flattened  = flattener::flatten(obj);
            auto serialized = serialize(flattened, streamable);

            return {serialized, obj.size()};
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_unordered_set_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            auto flattened  = flattener::flatten(obj);
            auto serialized = serialize(flattened, streamable);

            return {serialized, obj.size()}; 
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_set_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            auto flattened  = flattener::flatten(obj);
            auto serialized = serialize(flattened, streamable);

            return {serialized, obj.size()};
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_optional_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            if (!obj.has_value())
            {
                return std::nullopt;
            }

            return serialize(obj.value(), streamable);
        }

        template <class T, class Streamable, std::enable_if_t<types_space::is_std_fixed_size_container_v<T>,  bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            auto rs         = decltype(type_converter<>::convert(obj))();
            auto idx_seq    = std::make_index_sequence<std::tuple_size_v<T>>();

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                ((std::get<IDX>(rs) = serialize(std::get<IDX>(obj), streamable)), ...);
            }(idx_seq);

            return rs;
        }

        template <class T, class Streamable, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        auto serialize(const T& obj, Streamable& streamable) -> decltype(type_converter<>::convert(obj))
        {
            return obj;
        }
    };
}

namespace dg::dgbuf::std_iterator
{
    template <class Container>
    class inserter: public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
        private:

            Container * container;

        public:

            using self = inserter<Container>; 

            inserter(Container& container) noexcept: container(&container){}

            template <class T>
            auto operator =(T&& e) noexcept(noexcept(container->insert(std::forward<T>(e)))) -> self&
            {
                container->insert(std::forward<T>(e));
                return *this;
            }

            auto operator *() noexcept -> self&
            {
                return *this;
            }

            auto operator++() noexcept -> self&
            {    
                return *this;
            }

            auto operator++(int) noexcept -> self&
            {
                return *this;
            }
    };

    template <class Container>
    static auto get_std_inserter(Container& container)
    {
        if constexpr(types_space::is_std_vector_v<Container> | types_space::is_std_basic_string_v<Container>)
        {
            return std::back_inserter(container);
        }
        else if constexpr(types_space::is_std_unordered_set_v<Container> | types_space::is_std_unordered_map_v<Container> | types_space::is_std_map_v<Container> | types_space::is_std_set_v<Container>)
        {
            inserter isrter(container);
            return isrter;
        }
        else
        {
            static_assert(constants::FALSE_VAL<>);
        }
    }
}

namespace dg::dgbuf::dgbuf_to_stl
{
    template <class Allocator, size_t I = 0>
    struct type_converter
    {
        using successor = type_converter<Allocator, I + 1>;

        template <class T>
        static auto convert(datastructure::vector_view<T, autosetter::AutoSetter>) -> std::vector<decltype(successor::convert(std::declval<T>())), typename std::allocator_traits<Allocator>::template rebind_alloc<decltype(successor::convert(std::declval<T>()))>>;

        static auto convert(datastructure::str_view<autosetter::AutoSetter>) -> std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<Allocator>::template rebind_alloc<char>>;

        template <class Key, class Value, class Hasher, class KeyEq, class ...Args>
        static auto convert(datastructure::unordered_flat_map_view<Key, Value, autosetter::AutoSetter, Hasher, KeyEq, Args...>) -> std::unordered_map<decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>())), hasher::reverse_hasher_t<Hasher>, hasher::reverse_equal_to_t<KeyEq>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>()))>>>;

        template <class Key, class Hasher, class KeyEq, class ...Args>
        static auto convert(datastructure::unordered_flat_set_view<Key, autosetter::AutoSetter, Hasher, KeyEq, Args...>) -> std::unordered_set<decltype(successor::convert(std::declval<Key>())), hasher::reverse_hasher_t<Hasher>, hasher::reverse_equal_to_t<KeyEq>, typename std::allocator_traits<Allocator>::template rebind_alloc<decltype(successor::convert(std::declval<Key>()))>>;

        template <class Key, class Value, class Comparer>
        static auto convert(datastructure::map_view<Key, Value, autosetter::AutoSetter, Comparer>) -> std::map<decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>())), Comparer, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const decltype(successor::convert(std::declval<Key>())), decltype(successor::convert(std::declval<Value>()))>>>;

        template <class Key, class Comparer>
        static auto convert(datastructure::set_view<Key, autosetter::AutoSetter, Comparer>) -> std::set<decltype(successor::convert(std::declval<Key>())), Comparer, typename std::allocator_traits<Allocator>::template rebind_alloc<decltype(successor::convert(std::declval<Key>()))>>;

        template <class T>
        static auto convert(std::optional<T>) -> std::optional<decltype(successor::convert(std::declval<T>()))>;

        template <class T, size_t N>
        static auto convert(std::array<T, N>) -> std::array<decltype(successor::convert(std::declval<T>())), N>;

        template <class First, class Second>
        static auto convert(std::pair<First, Second>) -> std::pair<decltype(successor::convert(std::declval<First>())), decltype(successor::convert(std::declval<Second>()))>;

        template <class ...Args>
        static auto convert(std::tuple<Args...>) -> std::tuple<decltype(successor::convert(std::declval<Args>()))...>;

        template <class T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        static auto convert(T) -> T;
    };

    template <class Allocator>
    struct type_converter<Allocator, constants::MAX_TEMPLATE_RECURSIVE_DEPTH>
    {
        template <class T>
        static auto convert(T) -> T;
    };

    template <class Allocator = std::allocator<void>>
    struct deserializer
    {
        using converter = type_converter<Allocator, 0>; 

        template <class T, std::enable_if_t<types_space::is_dg_container_view_v<T>, bool> = true>
        auto deserialize(const T& obj) -> decltype(converter::convert(obj))
        {
            auto rs             = decltype(converter::convert(obj))();
            auto transformer    = [&](const auto& e){return this->deserialize(e);};

            std::transform(obj.begin(), obj.end(), std_iterator::get_std_inserter(rs), transformer);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_std_optional_v<T>, bool> = true>
        auto deserialize(const T& obj) -> decltype(converter::convert(obj))
        {
            if (!obj)
            {
                return std::nullopt;
            }

            return deserialize(obj.value());
        }

        template <class T, std::enable_if_t<types_space::is_std_fixed_size_container_v<T>, bool> = true>
        auto deserialize(const T& obj) -> decltype(converter::convert(obj))
        {
            const auto idx_seq = std::make_index_sequence<std::tuple_size_v<T>>();
            auto rs = decltype(converter::convert(obj))();

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                ((std::get<IDX>(rs) = deserialize(std::get<IDX>(obj))), ...);
            }(idx_seq);

            return rs;
        }

        template <class T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        auto deserialize(const T& obj) -> decltype(converter::convert(obj))
        {
            return obj;
        }
    };
}

#endif