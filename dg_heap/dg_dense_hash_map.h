
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2021 Martin Ankerl <http://martin.ankerl.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __DG_DENSE_HASH_MAP_H__
#define __DG_DENSE_HASH_MAP_H__

#include <stdint.h>
#include <stdlib.h>
#include <ratio>
#include <memory>
#include <bit>
#include <type_traits>
#include <vector>
#include <utility>
#include <memory>
#include <stdexcept>
#include <limits>
#include <functional>
#include <algorithm>

namespace dg_heap::dense_hash_map{

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;

    template <class T, class U>
    struct DGForwardLikeHelper{
        using type = std::remove_reference_t<U>;
    };

    template <class T, class U>
    struct DGForwardLikeHelper<T&, U>{
        using type = std::add_lvalue_reference_t<std::remove_reference_t<U>>;
    };

    template <class T, class U>
    using dg_forward_like_t = typename DGForwardLikeHelper<T, U>::type; 

    template <class T, class U>
    constexpr auto dg_forward_like(U&& value) noexcept -> dg_forward_like_t<T, U>&&{

        if constexpr(std::is_same_v<U, std::remove_reference_t<U>>){
            static_assert(FALSE_VAL<>);
        } else{
            return static_cast<dg_forward_like_t<T, U>&&>(value);
        }
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2(T val) noexcept -> size_t{

        return static_cast<size_t>(sizeof(T) * CHAR_BIT - 1u) - static_cast<size_t>(std::countl_zero(val));
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    static constexpr auto ceil2(T val) noexcept -> T{

        if (val < 2u) [[unlikely]]{
            return 1u;
        } else [[likely]]{
            T uplog_value = ulog2(static_cast<T>(val - 1u)) + 1u;
            return T{1u} << uplog_value;
        }
    }

    template <class T>
    static __attribute__((always_inline)) constexpr auto dg_restrict_swap_for_destroy(T * __restrict__ lhs, T * __restrict__ rhs) noexcept(noexcept(std::swap(std::declval<T&>(), std::declval<T&>()))){

        if constexpr(std::is_trivial_v<T>){
            *lhs = *rhs;
        } else{
            std::swap(*lhs, *rhs);
        }
    }

    template <class T, class = void>
    struct null_addr{};

    template <class T>
    struct null_addr<T, std::void_t<std::enable_if_t<std::is_unsigned_v<T>>>>{
        static inline constexpr T value = std::numeric_limits<T>::max();
    };

    template <class T>
    static inline constexpr T null_addr_v = null_addr<T>::value; 

    template <class T, class = void>
    struct get_virtual_addr{};

    template <class T>
    struct get_virtual_addr<T, std::void_t<std::enable_if_t<std::is_unsigned_v<T>>>>{
        using type = T;
    };

    template <class T>
    using get_virtual_addr_t = typename get_virtual_addr<T>::type;

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_1{
        key_t first;
        mapped_t second;
        virtual_addr_t nxt_addr;
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_2{
        key_t first;
        virtual_addr_t nxt_addr;
        mapped_t second;
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_3{
        mapped_t second;
        key_t first;
        virtual_addr_t nxt_addr;
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_4{
        mapped_t second;
        virtual_addr_t nxt_addr;
        key_t first;  
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_5{
        virtual_addr_t nxt_addr;
        key_t first;
        mapped_t second;
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct Node_6{
        virtual_addr_t nxt_addr;
        mapped_t second;
        key_t first;
    };
    
    template <class T>
    struct is_node1: std::false_type{};

    template <class ...Args>
    struct is_node1<Node_1<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node1_v = is_node1<T>::value;

    template <class T>
    struct is_node2: std::false_type{};

    template <class ...Args>
    struct is_node2<Node_2<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node2_v = is_node2<T>::value;

    template <class T>
    struct is_node3: std::false_type{};

    template <class ...Args>
    struct is_node3<Node_3<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node3_v = is_node3<T>::value;

    template <class T>
    struct is_node4: std::false_type{};

    template <class ...Args>
    struct is_node4<Node_4<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node4_v = is_node4<T>::value;

    template <class T>
    struct is_node5: std::false_type{};

    template <class ...Args>
    struct is_node5<Node_5<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node5_v = is_node5<T>::value;

    template <class T>
    struct is_node6: std::false_type{};

    template <class ...Args>
    struct is_node6<Node_6<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_node6_v = is_node6<T>::value;

    template <class ...Args>
    constexpr auto is_least(Args ...args) noexcept -> bool{

        std::array<size_t, sizeof...(Args)> sz_arr{args...};
        static_assert(sz_arr.size() != 0u);

        if (sz_arr.size() == 1u){
            return true;
        }

        size_t cmp_arg = sz_arr[0u];

        for (size_t i = 1u; i < sz_arr.size(); ++i){
            if (cmp_arg > sz_arr[i]){
                return false;
            }
        }

        return true;
    }

    constexpr auto vector_precond() -> bool{

        std::vector<size_t> vec{};
        return vec.capacity() == 0u;
    } 

    // template <class T>
    constexpr auto min_size_clamp(std::size_t arg, std::size_t lo, std::size_t hi) ->  size_t{

        if (arg > hi){
            throw std::length_error("bad size clamp");
        }

        return std::max(arg, lo);
    }

    template <class key_t, class mapped_t, class virtual_addr_t>
    using ReorderedNode = std::conditional_t<is_least(sizeof(Node_1<key_t, mapped_t, virtual_addr_t>), sizeof(Node_2<key_t, mapped_t, virtual_addr_t>), sizeof(Node_3<key_t, mapped_t, virtual_addr_t>), sizeof(Node_4<key_t, mapped_t, virtual_addr_t>), sizeof(Node_5<key_t, mapped_t, virtual_addr_t>), sizeof(Node_6<key_t, mapped_t, virtual_addr_t>)),
                                             Node_1<key_t, mapped_t, virtual_addr_t>,
                                             std::conditional_t<is_least(sizeof(Node_2<key_t, mapped_t, virtual_addr_t>), sizeof(Node_3<key_t, mapped_t, virtual_addr_t>), sizeof(Node_4<key_t, mapped_t, virtual_addr_t>), sizeof(Node_5<key_t, mapped_t, virtual_addr_t>), sizeof(Node_6<key_t, mapped_t, virtual_addr_t>)),
                                                                Node_2<key_t, mapped_t, virtual_addr_t>,
                                                                std::conditional_t<is_least(sizeof(Node_3<key_t, mapped_t, virtual_addr_t>), sizeof(Node_4<key_t, mapped_t, virtual_addr_t>), sizeof(Node_5<key_t, mapped_t, virtual_addr_t>), sizeof(Node_6<key_t, mapped_t, virtual_addr_t>)),
                                                                                   Node_3<key_t, mapped_t, virtual_addr_t>,
                                                                                   std::conditional_t<is_least(sizeof(Node_4<key_t, mapped_t, virtual_addr_t>), sizeof(Node_5<key_t, mapped_t, virtual_addr_t>), sizeof(Node_6<key_t, mapped_t, virtual_addr_t>)),
                                                                                                      Node_4<key_t, mapped_t, virtual_addr_t>,
                                                                                                      std::conditional_t<is_least(sizeof(Node_5<key_t, mapped_t, virtual_addr_t>), sizeof(Node_6<key_t, mapped_t, virtual_addr_t>)),
                                                                                                                         Node_5<key_t, mapped_t, virtual_addr_t>,
                                                                                                                         Node_6<key_t, mapped_t, virtual_addr_t>>>>>>;

    template <class node_t, class key_t, class mapped_t, class virtual_addr_t>
    static auto node_initialize(key_t&& key, mapped_t&& mapped, virtual_addr_t&& va_addr) -> node_t{

        if constexpr(is_node1_v<node_t>){
            return node_t{std::forward<key_t>(key), std::forward<mapped_t>(mapped), std::forward<virtual_addr_t>(va_addr)};
        } else if constexpr(is_node2_v<node_t>){
            return node_t{std::forward<key_t>(key), std::forward<virtual_addr_t>(va_addr), std::forward<mapped_t>(mapped)};
        } else if constexpr(is_node3_v<node_t>){
            return node_t{std::forward<mapped_t>(mapped), std::forward<key_t>(key), std::forward<virtual_addr_t>(va_addr)};
        } else if constexpr(is_node4_v<node_t>){
            return node_t{std::forward<mapped_t>(mapped), std::forward<virtual_addr_t>(va_addr), std::forward<key_t>(key)};
        } else if constexpr(is_node5_v<node_t>){
            return node_t{std::forward<virtual_addr_t>(va_addr), std::forward<key_t>(key), std::forward<mapped_t>(mapped)};
        } else if constexpr(is_node6_v<node_t>){
            return node_t{std::forward<virtual_addr_t>(va_addr), std::forward<mapped_t>(mapped), std::forward<key_t>(key)};
        } else{
            static_assert(FALSE_VAL<>);
        }
    }

    template <class Flag, class key_t, class mapped_t, class virtual_addr_t>
    struct NodeChooser{
        using type = Node_2<key_t, mapped_t, virtual_addr_t>;
    };

    template <class key_t, class mapped_t, class virtual_addr_t>
    struct NodeChooser<std::integral_constant<bool, true>, key_t, mapped_t, virtual_addr_t>{
        using type = std::conditional_t<sizeof(Node_2<key_t, mapped_t, virtual_addr_t>) <= sizeof(ReorderedNode<key_t, mapped_t, virtual_addr_t>),
                                        Node_2<key_t, mapped_t, virtual_addr_t>,
                                        std::conditional_t<sizeof(Node_5<key_t, mapped_t, virtual_addr_t>) <= sizeof(ReorderedNode<key_t, mapped_t, virtual_addr_t>),
                                                           Node_5<key_t, mapped_t, virtual_addr_t>,
                                                           ReorderedNode<key_t, mapped_t, virtual_addr_t>>>;
    };

    static_assert(vector_precond());

    template <class Flag, class key_t, class mapped_t, class virtual_addr_t>
    using Node = typename NodeChooser<Flag, key_t, mapped_t, virtual_addr_t>::type;

    //believe it or not, this unordered map is from Jesus himself, he's my savior of the low level memory kernel management of packets
    //I have since made an incredible progress to the next iteration of the map
    //though not very clear, the map performance can only be seen in large scale application not microbenchmarks

    //we focus on: memory footprint of the bucket_vec and virtual_storage_vec
    //memory fragmentation of allocation (will converge to 0 at a certain time)

    //all std::vector<>:: operation, heavily optimized by compilers
    //trivial type-erased tuples or pairs, this is after the popularity of auto and template polymorphisms

    //fastest clear() map
    //best possible map if only used with clear() and find() and insert()

    //performance is on-par with std::vector<>[] access (wow, if the dictionary is < 256 elements)
    //I dont really know the dark-magic behind the if pipelines but that's the number

    template <class Key, class Mapped, class VirtualAddrType = std::size_t, class HasStructureReordering = std::integral_constant<bool, true>, class Hasher = std::hash<Key>, class Pred = std::equal_to<Key>, class StdCompatibleAllocator = std::allocator<std::pair<const Key, Mapped>>, class LoadFactor = std::ratio<7, 8>>
    class unordered_node_map{

        private:

            using Allocator = typename std::allocator_traits<StdCompatibleAllocator>::template rebind_alloc<Node<HasStructureReordering, Key, Mapped, VirtualAddrType>>;

            std::vector<Node<HasStructureReordering, Key, Mapped, VirtualAddrType>, Allocator> virtual_storage_vec;
            std::vector<VirtualAddrType, typename std::allocator_traits<Allocator>::template rebind_alloc<VirtualAddrType>> bucket_vec;
            Hasher _hasher;
            Pred pred;
            Allocator allocator;

        public:

            using key_type                  = Key;
            using mapped_type               = Mapped;
            using value_type                = Node<HasStructureReordering, Key, Mapped, VirtualAddrType>;
            using hasher                    = Hasher;
            using key_equal                 = Pred;
            using allocator_type            = Allocator;
            using reference                 = value_type&;
            using const_reference           = const value_type&;
            using pointer                   = typename std::allocator_traits<Allocator>::pointer;
            using const_pointer             = typename std::allocator_traits<Allocator>::const_pointer;
            using iterator                  = typename decltype(virtual_storage_vec)::iterator;
            using const_iterator            = typename decltype(virtual_storage_vec)::const_iterator;
            using size_type                 = std::size_t;
            using difference_type           = std::intmax_t;
            using self                      = unordered_node_map;
            using load_factor_ratio         = typename LoadFactor::type;
            using virtual_addr_t            = get_virtual_addr_t<VirtualAddrType>;
            using node_t                    = Node<HasStructureReordering, Key, Mapped, VirtualAddrType>;

            static inline constexpr virtual_addr_t NULL_VIRTUAL_ADDR    = null_addr_v<virtual_addr_t>;
            static inline constexpr size_t POW2_GROWTH_FACTOR           = 1u;
            static inline constexpr uint64_t MIN_CAP                    = 8u;
            static inline constexpr uint64_t MAX_CAP                    = uint64_t{1} << 50;

            static_assert((std::numeric_limits<size_type>::max() >= MAX_CAP));

            static_assert(std::disjunction_v<std::is_same<typename std::ratio<1, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<2, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<3, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<4, 8>::type, load_factor_ratio>,
                                             std::is_same<typename std::ratio<5, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<6, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<7, 8>::type, load_factor_ratio>, 
                                             std::is_same<typename std::ratio<8, 8>::type, load_factor_ratio>>);

            constexpr explicit unordered_node_map(size_type bucket_count,
                                                  const Hasher& _hasher = Hasher(),
                                                  const Pred& pred = Pred(),
                                                  const Allocator& allocator = Allocator()): virtual_storage_vec(allocator),
                                                                                             bucket_vec(min_size_clamp(static_cast<std::size_t>(ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<std::size_t>(self::max_capacity())), self::NULL_VIRTUAL_ADDR, allocator),
                                                                                             _hasher(_hasher),
                                                                                             pred(pred),
                                                                                             allocator(allocator){

                this->virtual_storage_vec.reserve(self::right_capacity_to_size(this->capacity()));
            }

            constexpr unordered_node_map(size_type bucket_count,
                                         const Hasher& _hasher,
                                         const Allocator& allocator): unordered_node_map(bucket_count, _hasher, Pred(), allocator){}

            constexpr unordered_node_map(size_type bucket_count,
                                         const Allocator& allocator): unordered_node_map(bucket_count, Hasher(), allocator){}

            constexpr explicit unordered_node_map(const Allocator& allocator): unordered_node_map(self::min_capacity(), allocator){}

            constexpr unordered_node_map(): unordered_node_map(Allocator()){}

            template <class InputIt>
            constexpr unordered_node_map(InputIt first,
                                         InputIt last,
                                         size_type bucket_count,
                                         const Hasher& _hasher = Hasher(),
                                         const Pred& pred = Pred(),
                                         const Allocator& allocator = Allocator()): unordered_node_map(bucket_count, _hasher, pred, allocator){

                this->insert(first, last); //bad, leak
            }

            template <class InputIt>
            constexpr unordered_node_map(InputIt first,
                                         InputIt last,
                                         size_type bucket_count,
                                         const Allocator& allocator): unordered_node_map(first, last, bucket_count, Hasher(), Pred(), allocator){}

            constexpr unordered_node_map(std::initializer_list<std::pair<const Key, Mapped>> init_list,
                                         size_type bucket_count,
                                         const Hasher& _hasher,
                                         const Allocator& allocator): unordered_node_map(init_list.begin(), init_list.end(), bucket_count, _hasher, Pred(), allocator){}

            constexpr unordered_node_map(std::initializer_list<std::pair<const Key, Mapped>> init_list,
                                         size_type bucket_count,
                                         const Allocator& allocator): unordered_node_map(init_list.begin(), init_list.end(), bucket_count, Hasher(), allocator){}

            __attribute__((noinline)) constexpr void rehash(size_type tentative_new_cap){

                if (tentative_new_cap <= this->capacity()){
                    return;
                }

                size_t new_bucket_cap               = std::max(self::min_capacity(), static_cast<size_type>(ceil2(tentative_new_cap)));

                if (new_bucket_cap > self::max_capacity()){
                    throw std::length_error("bad unordered_node_map capacity");
                }

                size_t new_virtual_storage_vec_cap  = self::right_capacity_to_size(new_bucket_cap);
                auto new_bucket_vec                 = decltype(bucket_vec)(new_bucket_cap, self::NULL_VIRTUAL_ADDR, this->allocator);

                this->virtual_storage_vec.reserve(new_virtual_storage_vec_cap); 

                //static_assert(noexcept(this->_hasher(key))); TODOs: compile time validation

                for (size_t i = 0u; i < this->virtual_storage_vec.size(); ++i){
                    this->virtual_storage_vec[i].nxt_addr   = self::NULL_VIRTUAL_ADDR;
                    size_t hashed_value                     = this->_hasher(this->virtual_storage_vec[i].first);
                    size_t bucket_idx                       = hashed_value & (new_bucket_cap - 1u);
                    virtual_addr_t * insert_reference       = &new_bucket_vec[bucket_idx];

                    while (true){
                        if (*insert_reference == self::NULL_VIRTUAL_ADDR){
                            break;
                        }

                        insert_reference = &this->virtual_storage_vec[*insert_reference].nxt_addr;
                    }

                    *insert_reference = static_cast<virtual_addr_t>(i);
                }

                this->bucket_vec = std::move(new_bucket_vec);
            }

            constexpr void reserve(size_type new_sz){

                if (new_sz <= this->size()){
                    return;
                }

                if (new_sz > self::max_size()){
                    throw std::length_error("bad unordered_node_map size");
                }

                this->rehash(self::ceil_size_to_capacity(new_sz));
            }

            template <class KeyLike, class ...Args>
            constexpr auto try_emplace(KeyLike&& key, Args&& ...args) -> std::pair<iterator, bool>{

                return this->internal_insert(node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<Args>(args)...), NULL_VIRTUAL_ADDR));
            }

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> std::pair<iterator, bool>{

                return this->insert(std::pair<const Key, Mapped>(std::forward<Args>(args)...));
            }

            template <class ValueLike = std::pair<const Key, Mapped>>
            constexpr auto insert(ValueLike&& value) -> std::pair<iterator, bool>{

                return this->internal_insert(node_initialize<node_t>(key_type(dg_forward_like<ValueLike>(value.first)), mapped_type(dg_forward_like<ValueLike>(value.second)), NULL_VIRTUAL_ADDR));
            }

            template <class Iterator>
            constexpr void insert(Iterator first, Iterator last){

                //give the user a chance to not have leak by using proper std::move() + friends

                this->reserve(this->size() + std::distance(first, last));

                while (first != last){
                    this->insert(*first);
                    std::advance(first, 1u);
                }
            }

            constexpr void insert(std::initializer_list<std::pair<const Key, Mapped>> init_list){

                this->insert(init_list.begin(), init_list.end());
            }

            template <class KeyLike, class MappedLike>
            constexpr auto insert_or_assign(KeyLike&& key, MappedLike&& mapped) -> std::pair<iterator, bool>{

                return this->internal_insert_or_assign(node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<MappedLike>(mapped)), NULL_VIRTUAL_ADDR));
            }

            template <class KeyLike>
            constexpr auto operator[](KeyLike&& key) -> mapped_type&{

                return std::get<0>(this->try_emplace(std::forward<KeyLike>(key), mapped_type{}))->second;
            }

            constexpr void clear() noexcept(true){

                // static_assert(noexcept(this->virtual_storage_vec.clear())); TODOs: compile_time validation

                this->virtual_storage_vec.clear();
                std::fill(this->bucket_vec.begin(), this->bucket_vec.end(), self::NULL_VIRTUAL_ADDR);
            }

            constexpr void swap(self& other) noexcept(true){

                // static_assert(noexcept(std::swap(this->virtual_storage_vec, other.virtual_storage_vec))); TODOs: compile_time validation
                // static_assert(noexcept(std::swap(this->bucket_vec, other.bucket_vec))); TODOs: compile_time validation
                // static_assert(noexcept(std::swap(this->_hasher, other._hasher))); TODOs: compile_time validation
                // static_assert(noexcept(std::swap(this->pred, other.pred))); TODOs: compile_time validation
                // static_assert(noexcept(std::swap(this->allocator, other.allocator))); TODOs: compile_time validation

                std::swap(this->virtual_storage_vec, other.virtual_storage_vec);
                std::swap(this->bucket_vec, other.bucket_vec);
                std::swap(this->_hasher, other._hasher);
                std::swap(this->pred, other.pred);
                std::swap(this->allocator, other.allocator);
            }

            template <class EraseArg>
            constexpr auto erase(EraseArg&& erase_arg) noexcept(true){

                if constexpr(std::is_convertible_v<EraseArg&&, const_iterator>){
                    if constexpr(std::is_nothrow_convertible_v<EraseArg&&, const_iterator>){
                        return this->internal_erase_iter(std::forward<EraseArg>(erase_arg));
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                } else{
                    return static_cast<size_type>(this->internal_erase_key(std::forward<EraseArg>(erase_arg)));
                }
            }

            //__attribute__ ((const)) for considerations, this is very important because our map can replace table lookups with lower memory footprints

            template <class KeyLike>
            constexpr auto find(const KeyLike& key) const noexcept(true) -> const_iterator{

                return this->internal_find(key);
            }

            template <class KeyLike>
            constexpr auto find(const KeyLike& key) noexcept(true) -> iterator{

                return std::next(this->virtual_storage_vec.begin(), std::distance(this->virtual_storage_vec.cbegin(), this->internal_find(key)));
            }

            template <class KeyLike>
            constexpr auto contains(const KeyLike& key) const noexcept(true) -> bool{ 

                return this->find(key) != this->end();
            }

            template <class KeyLike>
            constexpr auto count(const KeyLike& key) const noexcept(true) -> size_t{

                return static_cast<size_t>(this->contains(key));
            }

            template <class KeyLike>
            constexpr auto at(const KeyLike& key) const -> const mapped_type&{

                auto ptr = this->find(key);

                if (ptr == this->end()){
                    throw std::out_of_range("unordered_node_map bad access");
                }

                return ptr->second;
            }

            template <class KeyLike>
            constexpr auto at(const KeyLike& key) -> mapped_type&{

                auto ptr = this->find(key);

                if (ptr == this->end()){
                    throw std::out_of_range("unordered_node_map bad access");
                }

                return ptr->second;
            }

            constexpr auto empty() const noexcept -> bool{

                return this->virtual_storage_vec.empty();
            }

            constexpr auto capacity() const noexcept -> size_type{

                return this->bucket_vec.size();
            }
            
            static constexpr auto allocation_size(size_t bucket_count) noexcept -> size_type{

                size_t up_ceil_bucket_count = std::max(ceil2(bucket_count), self::min_capacity());
                size_t vec_cap              = self::right_capacity_to_size(up_ceil_bucket_count);

                size_t bucket_allocation_sz = up_ceil_bucket_count * sizeof(typename decltype(bucket_vec)::value_type) + alignof(typename decltype(bucket_vec)::value_type);
                size_t node_allocation_sz   = vec_cap * sizeof(typename decltype(virtual_storage_vec)::value_type) + alignof(typename decltype(virtual_storage_vec)::value_type);
                size_t total_allocation_sz  = bucket_allocation_sz + node_allocation_sz;

                return total_allocation_sz;
            }

            static constexpr auto bucket_count_for_size_of(size_t element_sz) noexcept -> size_type{

                if (element_sz == 0u)
                {
                    return min_capacity();
                }

                size_t upround_element_sz   = (((element_sz - 1u) / load_factor_ratio::num) + 1u) * load_factor_ratio::num;
                size_t sufficient_capacity  = self::right_size_to_capacity(upround_element_sz);
                size_t tentative_capacity   = ceil2(sufficient_capacity);
                size_t actual_capacity      = std::max(tentative_capacity, self::min_capacity());

                return actual_capacity; 
            }

            static consteval auto min_capacity() -> size_type{

                return self::MIN_CAP;
            }

            static consteval auto max_capacity() -> size_type{

                return self::MAX_CAP;
            }

            constexpr auto size() const noexcept -> size_type{

                return this->virtual_storage_vec.size();
            }

            static consteval auto max_size() -> size_type{

                return self::right_capacity_to_size(self::max_capacity()); 
            }

            constexpr auto hash_function() const & noexcept -> const Hasher&{

                return this->_hasher;
            }

            constexpr auto hash_function() && noexcept -> Hasher&&{

                return static_cast<Hasher&&>(this->_hasher);
            }

            constexpr auto key_eq() const & noexcept -> const Pred&{

                return this->pred;
            }

            constexpr auto key_eq() && noexcept -> Pred&&{

                return static_cast<Pred&&>(this->pred);
            }
 
            constexpr auto begin() noexcept -> iterator{

                return this->virtual_storage_vec.begin();   
            }

            constexpr auto begin() const noexcept -> const_iterator{

                return this->virtual_storage_vec.begin();
            }

            constexpr auto cbegin() const noexcept -> const_iterator{

                return this->virtual_storage_vec.cbegin();
            }

            constexpr auto end() noexcept -> iterator{

                return this->virtual_storage_vec.end();
            }

            constexpr auto end() const noexcept -> const_iterator{

                return this->virtual_storage_vec.end();
            }

            constexpr auto cend() const noexcept -> const_iterator{

                return this->virtual_storage_vec.cend();
            }

            static consteval auto max_load_factor() noexcept -> double{

                return static_cast<double>(self::load_factor_ratio::num) / self::load_factor_ratio::den;
            }

            static constexpr auto capacity_to_size(size_t cap) -> size_t{

                return cap * self::max_load_factor();
            }

            static constexpr auto size_to_capacity(size_t sz) -> size_t{

                return sz / self::max_load_factor();
            }

        private:

            static constexpr auto right_capacity_to_size(size_t cap) noexcept -> size_t{

                return cap / static_cast<size_t>(load_factor_ratio::den) * static_cast<size_t>(load_factor_ratio::num);
            } 

            static constexpr auto right_size_to_capacity(size_t sz) noexcept -> size_t{

                return sz / static_cast<size_t>(load_factor_ratio::num) * static_cast<size_t>(load_factor_ratio::den);
            }

            static constexpr auto ceil_size_to_capacity(size_t sz) noexcept -> size_t{

                if (sz == 0u){
                    return 0u;
                }

                size_t mul_value        = sz * static_cast<size_t>(load_factor_ratio::den);
                size_t round_mul_value  = (((mul_value - 1u) / load_factor_ratio::num) + 1u) * load_factor_ratio::num;
                size_t new_cap          = round_mul_value / load_factor_ratio::num;

                return new_cap;
            }

            constexpr auto to_bucket_index(size_t hashed_value) const noexcept -> size_t{

                return hashed_value & static_cast<size_t>(this->bucket_vec.size() - 1u);
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) const noexcept(true) -> const virtual_addr_t *{

                //static_assert(noexcept(this->_hasher(key))); TODOs: compile time validation
                //static_assert(noexcept(this->pred(this->virtual_storage_vec[*current].first, key))) TODOs: compile time validation

                size_t hashed_value             = this->_hasher(key);
                size_t bucket_idx               = this->to_bucket_index(hashed_value);
                const virtual_addr_t * current  = &this->bucket_vec[bucket_idx];

                while (true){
                    if (*current == self::NULL_VIRTUAL_ADDR || this->pred(this->virtual_storage_vec[*current].first, key)){
                        return current;
                    }

                    current = &this->virtual_storage_vec[*current].nxt_addr;
                }
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) noexcept(true) -> virtual_addr_t *{

                return const_cast<virtual_addr_t *>(static_cast<const self *>(this)->internal_find_bucket_reference(key));
            }

            template <class KeyLike>
            constexpr auto internal_find(const KeyLike& key) const noexcept(true) -> const_iterator{

                const virtual_addr_t * addr = this->internal_find_bucket_reference(key);

                if (*addr == self::NULL_VIRTUAL_ADDR){
                    return this->virtual_storage_vec.cend();
                }

                return std::next(this->virtual_storage_vec.cbegin(), *addr);
            } 

            template <class ValueLike>
            constexpr auto internal_insert(ValueLike&& value) -> std::pair<iterator, bool>{

                if (this->virtual_storage_vec.size() == this->virtual_storage_vec.capacity()) [[unlikely]]{ //strong guarantee, might corrupt vector_capacity <-> bucket_vec_size ratio, signals an uphash
                    this->rehash(this->bucket_vec.size() << self::POW2_GROWTH_FACTOR);
                }

                virtual_addr_t * insert_reference   = this->internal_find_bucket_reference(value.first);

                if (*insert_reference == self::NULL_VIRTUAL_ADDR){
                    *insert_reference   = static_cast<virtual_addr_t>(this->virtual_storage_vec.size());
                    this->virtual_storage_vec.emplace_back(std::forward<ValueLike>(value));

                    return std::make_pair(std::next(this->virtual_storage_vec.begin(), *insert_reference), true);
                }

                return std::make_pair(std::next(this->virtual_storage_vec.begin(), *insert_reference), false);
            }

            template <class ValueLike>
            constexpr auto internal_insert_or_assign(ValueLike&& value) -> std::pair<iterator, bool>{

                auto [it, status] = this->internal_insert(std::forward<ValueLike>(value));

                if (!status)
                {
                    it->second = dg_forward_like<ValueLike>(value.second);
                }

                return {it, status};
            }

            template <class KeyLike>
            constexpr auto internal_erase_key(const KeyLike& key) noexcept(true) -> bool{

                // static_assert(noexcept(std::swap(std::declval<node_t&>, std::declval<node_t&>)));
                // static_assert(noexcept(this->virtual_storage_vec.pop_back()));

                virtual_addr_t * key_reference  = this->internal_find_bucket_reference(key);

                if (*key_reference == self::NULL_VIRTUAL_ADDR){
                    return false;
                }

                //alright, we have provided all the arguments we could to the compiler, it's up to the randomness of the wild to render things now

                virtual_addr_t * swapping_reference = this->internal_find_bucket_reference(this->virtual_storage_vec.back().first); 
    
                if (swapping_reference == key_reference) [[unlikely]]{
                    *key_reference = this->virtual_storage_vec[*key_reference].nxt_addr;
                } else [[likely]]{
                    if (swapping_reference == &this->virtual_storage_vec[*key_reference].nxt_addr) [[unlikely]]{
                        swapping_reference = key_reference;
                    }

                    *swapping_reference = std::exchange(*key_reference, this->virtual_storage_vec[*key_reference].nxt_addr); 
                    dg_restrict_swap_for_destroy(&this->virtual_storage_vec[*swapping_reference], &this->virtual_storage_vec.back());
                }

                this->virtual_storage_vec.pop_back();
                return true;
            }

            constexpr auto internal_erase_iter(const_iterator iter) noexcept(true) -> iterator{

                if (iter == this->cend())[[unlikely]]{
                    return this->end();
                } else [[likely]]{
                    size_t off = std::distance(this->virtual_storage_vec.cbegin(), iter); 
                    this->internal_erase_key(iter->first);
                    return std::next(this->virtual_storage_vec.begin(), off);
                }
            }
    };


    template <class ...Args>
    constexpr auto operator ==(const unordered_node_map<Args...>& lhs, const unordered_node_map<Args...>& rhs) noexcept(true) -> bool{

        if (lhs.size() != rhs.size()){
            return false;
        }

        for (const auto& kv_pair: lhs){
            auto rhs_ptr = rhs.find(kv_pair.first);

            if (rhs_ptr == rhs.end()){
                return false;
            }

            if (rhs_ptr->second != kv_pair.second){
                return false;
            }
        }

        return true;
    }

    template <class ...Args>
    constexpr auto operator !=(const unordered_node_map<Args...>& lhs, const unordered_node_map<Args...>& rhs) noexcept(true) -> bool{
        
        return !(lhs == rhs);
    }
}

namespace std{

    template <class ...Args>
    constexpr void swap(dg_heap::dense_hash_map::unordered_node_map<Args...>& lhs,
                        dg_heap::dense_hash_map::unordered_node_map<Args...>& rhs) noexcept(noexcept(std::declval<dg_heap::dense_hash_map::unordered_node_map<Args...>&>().swap(std::declval<dg_heap::dense_hash_map::unordered_node_map<Args...>&>()))){

        lhs.swap(rhs);
    }

    template <class ...Args, class Pred>
    constexpr void erase_if(dg_heap::dense_hash_map::unordered_node_map<Args...>& umap,
                            Pred pred){

        //a reverse erase_if is better cache_wise speaking

        auto it = umap.begin();

        while (it != umap.end()){
            if (pred(*it)){
                it = umap.erase(it);
            } else{
                std::advance(it, 1u);
            }
        }
    }
}

#endif