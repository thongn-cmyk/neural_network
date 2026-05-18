#ifndef __DG_NETWORK_DATASTRUCTURE_H__
#define __DG_NETWORK_DATASTRUCTURE_H__

//define HEADER_CONTROL 1

#include <stdint.h>
#include <stdlib.h>
#include "network_exception.h"
#include <ratio>
#include <memory>
#include <bit>
#include <type_traits>
#include <vector>
#include <utility>
#include <memory>
#include <stdexcept>
#include <limits>
#include "stdx.h"
#include <atomic>
#include "network_trivial_serializer.h"

// #include "network_log.h"

//move assignment + move operation is probably one of the most heatly debated problem in C++
//usually move assignment + move operation would "guarantee" that the moved object to be in a "safe" state to be deallocated + "safe" state to do another move + etc.
//this is the correct language, because snapping it to a state where it'd be in a "defined default state" is wrong, for the reasons being that it would not be noexcept 

namespace dg_sock::network_datastructure::cyclic_queue{

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;
 
    class pow2_cyclic_queue_index_getter_device{

        private:

            size_t off;
            size_t cap;
        
        public:

            constexpr pow2_cyclic_queue_index_getter_device() = default;
            constexpr pow2_cyclic_queue_index_getter_device(size_t off, size_t cap) noexcept: off(off), cap(cap){}

            constexpr auto operator()(size_t virtual_idx) const noexcept -> size_t{

                return (this->off + virtual_idx) & (this->cap - 1u);
            }
    };

    //alright we'll be back,
    //we are back
    //let's see the optimizables, first is decomposition by inheritance
    //we dont really know why this is not optimized by the std compiler... 

    template <class BaseIterator>
    class pow2_cyclic_queue_iterator{

        private:

            template <class OtherBaseIterator>
            friend class pow2_cyclic_queue_iterator;

            BaseIterator iter_head;
            intmax_t virtual_idx;
            pow2_cyclic_queue_index_getter_device index_getter;

        public:

            using self              = pow2_cyclic_queue_iterator; 
            using difference_type   = std::ptrdiff_t;
            using value_type        = typename BaseIterator::value_type; 

            constexpr pow2_cyclic_queue_iterator(): iter_head(), 
                                                    virtual_idx(), 
                                                    index_getter(){}

            constexpr pow2_cyclic_queue_iterator(BaseIterator iter_head,
                                                 intmax_t virtual_idx,
                                                 pow2_cyclic_queue_index_getter_device index_getter) noexcept(std::is_nothrow_move_constructible_v<BaseIterator>): iter_head(std::move(iter_head)),
                                                                                                                                                                   virtual_idx(virtual_idx),
                                                                                                                                                                   index_getter(index_getter){} 

            template <class OtherBaseIterator, std::enable_if_t<!std::is_same_v<OtherBaseIterator, BaseIterator> && std::is_convertible_v<OtherBaseIterator, BaseIterator>, bool> = true>
            constexpr pow2_cyclic_queue_iterator(const pow2_cyclic_queue_iterator<OtherBaseIterator>& other) noexcept(std::is_nothrow_convertible_v<OtherBaseIterator, BaseIterator>): iter_head(other.iter_head),
                                                                                                                                                                                       virtual_idx(other.virtual_idx),
                                                                                                                                                                                       index_getter(other.index_getter){}

            constexpr auto operator ++() noexcept -> self&{

                this->virtual_idx += 1;
                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self{

                static_assert(std::is_nothrow_copy_constructible_v<self>);

                self rs = *this;
                this->virtual_idx += 1;
                return rs;
            }

            constexpr auto operator --() noexcept -> self&{

                this->virtual_idx -= 1;
                return *this;
            }

            constexpr auto operator --(int) noexcept -> self{

                static_assert(std::is_nothrow_copy_constructible_v<self>);

                self rs = *this;
                this->virtual_idx -= 1;
                return rs;
            }

            constexpr auto operator +(difference_type off) const noexcept -> self{

                return self(this->iter_head, this->virtual_idx + off, this->index_getter);
            }

            constexpr auto operator +=(difference_type off) noexcept -> self&{

                this->virtual_idx += off;
                return *this;
            }

            constexpr auto operator -(difference_type off) const noexcept -> self{

                return self(this->iter_head, this->virtual_idx - off, this->index_getter);
            }

            constexpr auto operator -(const self& other) const noexcept -> difference_type{

                return this->virtual_idx - other.virtual_idx;
            }
        
            constexpr auto operator -=(difference_type off) noexcept -> self&{

                this->virtual_idx -= off;
                return *this;
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool{

                return this->iter_head == other.iter_head && this->virtual_idx == other.virtual_idx;
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool{

                return this->iter_head != other.iter_head || this->virtual_idx != other.virtual_idx;
            }

            constexpr auto operator <=(const self& other) const noexcept -> bool{

                return this->iter_head < other.iter_head || (this->iter_head == other.iter_head && this->virtual_idx <= other.virtual_idx);
            }

            constexpr auto operator *() const noexcept -> decltype(auto){

                size_t actual_idx = this->index_getter(stdxx::wrap_safe_integer_cast(this->virtual_idx));
                return std::next(this->iter_head, actual_idx).operator *();
            }

            constexpr auto operator ->() const noexcept -> decltype(auto){

                size_t actual_idx = this->index_getter(stdxx::wrap_safe_integer_cast(this->virtual_idx));
                return std::next(this->iter_head, actual_idx).operator ->();
            }

            constexpr auto operator [](intmax_t offset) const noexcept -> decltype(auto){

                size_t actual_idx = this->index_getter(stdxx::wrap_safe_integer_cast(this->virtual_idx + offset));
                return std::next(this->iter_head, actual_idx).operator *();
            }
    };

    template <class T, class Allocator = std::allocator<T>>
    class simple_pow2_cyclic_queue{

        private:

            std::vector<T, Allocator> data_arr;
            size_t off;
            size_t sz;
            size_t cap;

            using self              = simple_pow2_cyclic_queue;

        public:

            using value_type        = T;
            using iterator          = pow2_cyclic_queue_iterator<typename std::vector<T, Allocator>::iterator>;
            using const_iterator    = pow2_cyclic_queue_iterator<typename std::vector<T, Allocator>::const_iterator>;
            using size_type         = std::size_t;

            static inline constexpr size_t DEFAULT_POW2_EXPONENT = 10u; 

            constexpr simple_pow2_cyclic_queue(): simple_pow2_cyclic_queue(DEFAULT_POW2_EXPONENT){}

            constexpr simple_pow2_cyclic_queue(size_t pow2_exponent): data_arr(size_t{1} << pow2_exponent),
                                                                      off(0u),
                                                                      sz(0u),
                                                                      cap(size_t{1} << pow2_exponent){}

            constexpr void swap(self& other) noexcept(true){

                std::swap(this->data_arr, other.data_arr);
                std::swap(this->off, other.off);
                std::swap(this->sz, other.sz);
                std::swap(this->cap, other.cap);
            }

            constexpr auto front() const noexcept -> const T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(0u);
                return this->data_arr[ptr];
            }

            constexpr auto front() noexcept -> T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(0u);
                return this->data_arr[ptr];
            }

            constexpr auto back() const noexcept -> const T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(this->sz - 1u);
                return this->data_arr[ptr];
            }

            constexpr auto back() noexcept -> T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(this->sz - 1u);
                return this->data_arr[ptr];
            }

            constexpr auto empty() const noexcept -> bool{

                return this->sz == 0u;                
            }

            constexpr auto begin() const noexcept -> const_iterator{

                return const_iterator(this->data_arr.begin(), 0u, this->get_index_getter_device());
            }

            constexpr auto end() const noexcept -> const_iterator{

                return const_iterator(this->data_arr.begin(), this->sz, this->get_index_getter_device());
            }

            constexpr auto begin() noexcept -> iterator{
                
                return iterator(this->data_arr.begin(), 0u, this->get_index_getter_device());
            }

            constexpr auto end() noexcept -> iterator{

                return iterator(this->data_arr.begin(), this->sz, this->get_index_getter_device());
            }

            constexpr auto cbegin() const noexcept -> const_iterator{

                return this->begin();
            }

            constexpr auto cend() const noexcept -> const_iterator{

                return this->end();
            }

            constexpr auto size() const noexcept -> size_t{

                return this->sz;
            }

            constexpr auto capacity() const noexcept -> size_t{

                //its good to add assumptions
                [[assume(stdxx::is_pow2(this->cap))]];
                return this->cap;
            }

            constexpr auto operator[](size_t idx) const noexcept -> const T&{

                return this->data_arr[this->to_index(idx)];
            }

            constexpr auto operator[](size_t idx) noexcept -> T&{

                return this->data_arr[this->to_index(idx)];
            }

            constexpr auto at(size_t idx) const noexcept -> const T&{

                return (*this)[idx];
            }

            constexpr auto at(size_t idx) noexcept -> T&{

                return (*this)[idx];
            }

            constexpr auto resize(size_t new_sz) noexcept -> exception_t{

                if (new_sz > this->cap){
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }

                if (new_sz >= this->sz){
                    this->sz = new_sz;
                    return dg_sock::network_exception::SUCCESS;
                }

                this->defaultize_range(new_sz, this->sz - new_sz);
                this->sz = new_sz;

                return dg_sock::network_exception::SUCCESS;
            }

            template <class ValueLike = value_type>
            constexpr auto push_back(ValueLike&& value) noexcept -> exception_t{

                if (this->sz == this->cap){
                    return dg_sock::network_exception::QUEUE_FULL;
                }

                size_t ptr = this->to_index(this->sz);

                if constexpr(std::is_nothrow_assignable_v<T&, ValueLike&&>){
                    this->data_arr[ptr] = std::forward<ValueLike>(value);
                    this->sz            += 1u;
                    return dg_sock::network_exception::SUCCESS; 
                } else{
                    try{
                        this->data_arr[ptr] = std::forward<ValueLike>(value);
                        this->sz            += 1u;
                        return dg_sock::network_exception::SUCCESS;
                    } catch (...){
                        return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                    }
                }
            }

            constexpr void pop_front() noexcept{

                static_assert(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_assignable_v<T&, T&&>);

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr          = this->to_index(0u);
                this->data_arr[ptr] = T{};
                this->off           += 1u;
                this->sz            -= 1u;
            }
 
            constexpr void pop_back() noexcept{

                static_assert(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_assignable_v<T&, T&&>);

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr          = this->to_index(this->sz - 1u);
                this->data_arr[ptr] = T{};
                this->sz            -= 1u;
            } 

            constexpr void erase_front_range(size_t sz) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->sz){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INVALID_ARGUMENT));
                        std::abort();
                    }
                }

                this->defaultize_range(0u, sz);
                this->off += sz; 
                this->sz  -= sz;
            }

            constexpr void erase_back_range(size_t sz) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->sz){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INVALID_ARGUMENT));
                        std::abort();
                    }
                }

                this->resize(this->sz - sz);
            }

            constexpr void clear() noexcept{

                this->resize(0u);
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool{

                return std::equal(this->begin(), this->end(), other.begin(), other.end());
            }

        private:

            constexpr auto to_index(size_t virtual_offset) const noexcept -> size_t{

                return (this->off + virtual_offset) & (this->cap - 1u);
            }

            constexpr auto get_index_getter_device() const noexcept -> pow2_cyclic_queue_index_getter_device{

                return pow2_cyclic_queue_index_getter_device(this->off, this->cap);
            }

            constexpr void defaultize_range(size_t virtual_idx, size_t sz) noexcept{

                static_assert(std::is_nothrow_default_constructible_v<T>);

                auto fill_task = [&, this]<class T1>(T1) noexcept{
                    size_t first                        = this->to_index(virtual_idx);
                    size_t last                         = first + sz;
                    size_t eoq_last                     = std::min(this->cap, last); 
                    const size_t wrapped_around_first   = 0u;
                    size_t wrapped_around_last          = last - eoq_last;

                    std::fill(std::next(this->data_arr.begin(), first), std::next(this->data_arr.begin(), eoq_last), T{});
                    std::fill(std::next(this->data_arr.begin(), wrapped_around_first), std::next(this->data_arr.begin(), wrapped_around_last), T{});
                };

                if constexpr(std::is_trivial_v<T>){
                    fill_task(int{});
                } else{
                    if constexpr(std::is_assignable_v<T&, const T&>){
                        if constexpr(std::is_nothrow_assignable_v<T&, const T&>){
                            fill_task(int{});
                        } else{
                            if constexpr(std::is_assignable_v<T&, T&&>){
                                if constexpr(std::is_nothrow_assignable_v<T&, T&&>){
                                    for (size_t i = 0u; i < sz; ++i){
                                        this->operator[](virtual_idx + i) = T{};
                                    }
                                } else{
                                    static_assert(FALSE_VAL<>);
                                }
                            } else{
                                static_assert(FALSE_VAL<>);
                            }
                        }
                    } else if constexpr(std::is_assignable_v<T&, T&&>){
                        if constexpr(std::is_nothrow_assignable_v<T&, T&&>){
                            for (size_t i = 0u; i < sz; ++i){
                                this->operator[](virtual_idx + i) = T{};
                            }      
                        } else{
                            static_assert(FALSE_VAL<>);
                        }                  
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                }
            }
    };
    
    template <class Iter, class T, class = void>
    struct is_const_iter: std::false_type{};

    template <class Iter, class T>
    struct is_const_iter<Iter, T, std::void_t<std::enable_if_t<std::is_same_v<decltype(*std::declval<Iter&>()), std::add_lvalue_reference_t<std::add_const_t<T>>>>>>: std::true_type{};

    template <class Iter, class T>
    static inline constexpr bool is_const_iter_v = is_const_iter<Iter, T>::value;
    
    template <class Iter, class T, class = void>
    struct is_normal_iter: std::false_type{};

    template <class Iter, class T>
    struct is_normal_iter<Iter, T, std::void_t<std::enable_if_t<std::is_same_v<decltype(*std::declval<Iter&>()), std::add_lvalue_reference_t<T>>>>>: std::true_type{};

    template <class Iter, class T>
    static inline constexpr bool is_normal_iter_v = is_normal_iter<Iter, T>::value;

    //this is not very fast, we hope that it is just a thin container for vector<>, we always use inheritance to avoid memory read issues
    template <class T, class ST, class BaseIterator>
    class aligned_storage_vector_iterator{

        private:

            template <class T1, class ST1, class BaseIterator1>
            friend class aligned_storage_vector_iterator;

            BaseIterator iter;

        public:

            using self              = aligned_storage_vector_iterator; 
            using difference_type   = std::ptrdiff_t;
            using value_type        = T; 

            // template <class T1 = BaseIterator, std::enable_if_t<std::is_nothrow_default_constructible_v<T1>, bool> = true>
            constexpr aligned_storage_vector_iterator(): iter(){}

            constexpr aligned_storage_vector_iterator(BaseIterator iter)noexcept(std::is_nothrow_move_constructible_v<BaseIterator>): iter(std::move(iter)){} 

            template <class OtherBaseIterator, std::enable_if_t<!std::is_same_v<OtherBaseIterator, BaseIterator> && std::is_convertible_v<OtherBaseIterator, BaseIterator>, bool> = true>
            constexpr aligned_storage_vector_iterator(const aligned_storage_vector_iterator<T, ST, OtherBaseIterator>& other) noexcept(std::is_nothrow_convertible_v<OtherBaseIterator, BaseIterator>): iter(other.iter){}

            constexpr auto operator ++() noexcept -> self&{

                std::advance(this->iter, 1u);
                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self{

                static_assert(std::is_nothrow_copy_constructible_v<self>);

                self rs = *this;
                std::advance(this->iter, 1u);
                return rs;
            }

            constexpr auto operator --() noexcept -> self&{

                std::advance(this->iter, -1);
                return *this;
            }

            constexpr auto operator --(int) noexcept -> self{

                static_assert(std::is_nothrow_copy_constructible_v<self>);

                self rs = *this;
                std::advance(this->iter, -1);
                return rs;
            }

            constexpr auto operator +(difference_type off) const noexcept -> self{

                return self(std::next(this->iter, off));
            }

            constexpr auto operator +=(difference_type off) noexcept -> self&{

                std::advance(this->iter, off);
                return *this;
            }

            constexpr auto operator -(difference_type off) const noexcept -> self{

                return self(std::prev(this->iter, off));
            }

            constexpr auto operator -(const self& other) const noexcept -> difference_type{

                return std::distance(other.iter, this->iter);
            }
        
            constexpr auto operator -=(difference_type off) noexcept -> self&{

                std::advance(this->iter, -off);
                return *this;
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool{

                return this->iter == other.iter;
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool{

                return this->iter != other.iter;
            }

            constexpr auto operator <=(const self& other) const noexcept -> bool{

                return this->iter <= other.iter;
            }

            constexpr auto operator[](intmax_t idx) const noexcept -> decltype(auto){

                if constexpr(is_const_iter_v<BaseIterator, ST>){
                    return *std::launder(reinterpret_cast<const T *>(&this->iter[idx]));
                } else if constexpr(is_normal_iter_v<BaseIterator, ST>){
                    return *std::launder(reinterpret_cast<T *>(&this->iter[idx]));
                } else{
                    static_assert(FALSE_VAL<>);
                }
            }

            constexpr auto operator *() const noexcept -> decltype(auto){

                if constexpr(is_const_iter_v<BaseIterator, ST>){
                    return *std::launder(reinterpret_cast<const T *>(&(*this->iter)));
                } else if constexpr(is_normal_iter_v<BaseIterator, ST>){
                    return *std::launder(reinterpret_cast<T *>(&(*this->iter)));
                } else{
                    static_assert(FALSE_VAL<>);
                }
            }

            constexpr auto operator ->() const noexcept -> decltype(auto){

                if constexpr(is_const_iter_v<BaseIterator, ST>){
                    return std::launder(reinterpret_cast<const T *>(&(*this->iter)));
                } else if constexpr(is_normal_iter_v<BaseIterator, ST>){
                    return std::launder(reinterpret_cast<T *>(&(*this->iter)));
                } else{
                    static_assert(FALSE_VAL<>);
                }            
            }
    };

    template <class T, class Allocator = std::allocator<T>>
    class nontrivial_pow2_cyclic_queue{

        private:

            //alright, we arent being greedy, we might step into the undefined territory

            using rebinded_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<std::aligned_storage_t<sizeof(T), alignof(T)>>;

            std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>, rebinded_allocator> data_arr;
            size_t off;
            size_t sz;
            size_t cap;

        public:

            static_assert(std::is_nothrow_destructible_v<T>);

            using self                                      = nontrivial_pow2_cyclic_queue;
            using value_type                                = T;

            using _internal_vector_semantic_iterator        = aligned_storage_vector_iterator<T, std::aligned_storage_t<sizeof(T), alignof(T)>, typename std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>, rebinded_allocator>::iterator>;
            using _internal_vector_semantic_const_iterator  = aligned_storage_vector_iterator<T, std::aligned_storage_t<sizeof(T), alignof(T)>, typename std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>, rebinded_allocator>::const_iterator>;

            using iterator                                  = pow2_cyclic_queue_iterator<_internal_vector_semantic_iterator>;
            using const_iterator                            = pow2_cyclic_queue_iterator<_internal_vector_semantic_const_iterator>;

            static inline constexpr size_t DEFAULT_POW2_EXPONENT = 10u;

            constexpr nontrivial_pow2_cyclic_queue(): nontrivial_pow2_cyclic_queue(DEFAULT_POW2_EXPONENT){}

            constexpr nontrivial_pow2_cyclic_queue(size_t pow2_exponent): data_arr(size_t{1} << pow2_exponent),
                                                                          off(0u),
                                                                          sz(0u),
                                                                          cap(size_t{1} << pow2_exponent){}
            

            constexpr nontrivial_pow2_cyclic_queue(const self& other): nontrivial_pow2_cyclic_queue(stdxx::ulog2(other.data_arr.size())){

                for (const auto& e: other){
                    this->push_back(e);
                }
            }

            constexpr nontrivial_pow2_cyclic_queue(self&& other) noexcept(true): data_arr(std::move(other.data_arr)),
                                                                                 off(std::exchange(other.off, 0u)),
                                                                                 sz(std::exchange(other.sz, 0u)),
                                                                                 cap(std::exchange(other.cap, 0u)){}

            constexpr ~nontrivial_pow2_cyclic_queue() noexcept{

                this->clear();
            }

            constexpr auto operator =(const self& other) -> self&{

                if (this == std::addressof(other)){
                    return *this;
                }

                auto proxy = self(stdxx::ulog2(other.data_arr.size()));

                for (const auto& e: other){
                    proxy.push_back(e);
                }

                return *this = std::move(proxy);
            }

            constexpr auto operator =(self&& other) noexcept(true) -> self&{

                if (this == std::addressof(other)){
                    return *this;
                }

                this->clear();

                this->data_arr  = std::move(other.data_arr);
                this->off       = other.off;
                this->sz        = other.sz;
                this->cap       = other.cap;
                other.off       = 0u;
                other.sz        = 0u;
                other.cap       = 0u;

                return *this;
            }

            constexpr void swap(self& other) noexcept(true){

                std::swap(this->data_arr, other.data_arr);
                std::swap(this->off, other.off);
                std::swap(this->sz, other.sz);
                std::swap(this->cap, other.cap);
            }

            //access value via pointers are undefined for this very reason
            //the array is not new[] constructible, every defined usage of these guys must be directly right after std::launder<>, a linger on such is UNDEFINED
            //even if a storage reference is also not implemented correctly

            constexpr auto front() const noexcept -> const T&{
                
                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(0u);
                return *std::launder(reinterpret_cast<const T*>(&this->data_arr[ptr]));
            }

            constexpr auto front() noexcept -> T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(0u);
                return *std::launder(reinterpret_cast<T *>(&this->data_arr[ptr]));
            }

            constexpr auto back() const noexcept -> const T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(this->sz - 1u);
                return *std::launder(reinterpret_cast<const T *>(&this->data_arr[ptr]));
            }

            constexpr auto back() noexcept -> T&{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(this->sz - 1u);
                return *std::launder(reinterpret_cast<T *>(&this->data_arr[ptr]));
            }

            constexpr auto empty() const noexcept -> bool{

                return this->sz == 0u;
            }

            constexpr auto begin() const noexcept -> const_iterator{

                return const_iterator(_internal_vector_semantic_const_iterator(this->data_arr.begin()), 0u, this->get_index_getter_device());
            }

            constexpr auto end() const noexcept -> const_iterator{

                return const_iterator(_internal_vector_semantic_const_iterator(this->data_arr.begin()), this->sz, this->get_index_getter_device());
            }

            constexpr auto begin() noexcept -> iterator{

                return iterator(_internal_vector_semantic_iterator(this->data_arr.begin()), 0u, this->get_index_getter_device());
            }

            constexpr auto end() noexcept -> iterator{

                return iterator(_internal_vector_semantic_iterator(this->data_arr.begin()), this->sz, this->get_index_getter_device());
            }

            constexpr auto cbegin() const noexcept -> const_iterator{

                return this->begin();
            }

            constexpr auto cend() const noexcept -> const_iterator{

                return this->end();
            }

            constexpr auto size() const noexcept -> size_t{

                return this->sz;
            }

            constexpr auto capacity() const noexcept -> size_t{

                [[assume(stdxx::is_pow2(this->cap))]];
                return this->cap;
            }

            constexpr auto operator[](size_t idx) const noexcept -> const T&{

                return *std::launder(reinterpret_cast<const T *>(&this->data_arr[this->to_index(idx)]));
            }

            constexpr auto operator[](size_t idx) noexcept -> T&{

                return *std::launder(reinterpret_cast<T *>(&this->data_arr[this->to_index(idx)]));
            }

            constexpr auto at(size_t idx) const noexcept -> const T&{

                return (*this)[idx];
            }

            constexpr auto at(size_t idx) noexcept -> T&{

                return (*this)[idx];
            }

            template <class T1 = T, std::enable_if_t<std::is_default_constructible_v<T1>, bool> = true>
            constexpr auto resize(size_t new_sz) noexcept -> exception_t{

                //this is harder than expected

                if (new_sz > this->cap){
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }
                
                if (new_sz == this->sz){
                    return dg_sock::network_exception::SUCCESS;
                }

                if (new_sz > this->sz){
                    exception_t err = this->dispatch_default_initialize(this->sz, new_sz - this->sz); //this is very hard to implement, i'll try

                    if (dg_sock::network_exception::is_failed(err)){
                        return err;
                    }

                    this->sz = new_sz;
                    return dg_sock::network_exception::SUCCESS;
                }

                this->dispatch_destroy(new_sz, this->sz - new_sz);
                this->sz = new_sz;

                return dg_sock::network_exception::SUCCESS;
            }

            template <class ValueLike = value_type>
            constexpr auto push_back(ValueLike&& value) noexcept -> exception_t{

                if (this->sz == this->cap){
                    return dg_sock::network_exception::QUEUE_FULL;
                }

                size_t ptr = this->to_index(this->sz);

                if constexpr(std::is_nothrow_constructible_v<T, ValueLike&&>){
                    new (&this->data_arr[ptr]) T(std::forward<ValueLike>(value));
                    this->sz += 1u;
                    return dg_sock::network_exception::SUCCESS;
                } else{
                    try{
                        new (&this->data_arr[ptr]) T(std::forward<ValueLike>(value));
                        this->sz += 1u;
                        return dg_sock::network_exception::SUCCESS;
                    } catch (...){
                        return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                    }
                }
            }

            constexpr void pop_front() noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(0u);
                std::destroy_at(std::launder(reinterpret_cast<T *>(&this->data_arr[ptr])));
                this->off   += 1;
                this->sz    -= 1;
            }

            constexpr void pop_back() noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->sz == 0u){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t ptr = this->to_index(this->sz - 1u);
                std::destroy_at(std::launder(reinterpret_cast<T *>(&this->data_arr[ptr])));
                this->sz    -= 1;
            }

            constexpr void erase_front_range(size_t sz) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    this->pop_front();
                }
            }

            constexpr void erase_back_range(size_t sz) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    this->pop_back();
                }
            }

            constexpr void clear() noexcept{

                this->erase_back_range(this->size());
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool{

                return std::equal(this->begin(), this->end(), other.begin(), other.end());
            }
        
        private:

            constexpr auto to_index(size_t virtual_off) const noexcept -> size_t{

                return (this->off + virtual_off) & (this->cap - 1u);
            }

            constexpr auto get_index_getter_device() const noexcept -> pow2_cyclic_queue_index_getter_device{

                return pow2_cyclic_queue_index_getter_device(this->off, this->cap);
            }

            constexpr void dispatch_destroy(size_t virtual_off, size_t sz) noexcept{

                //optimizables

                for (size_t i = 0u; i < sz; ++i){
                    size_t abs_off = this->to_index(virtual_off + i);
                    std::destroy_at(std::launder(reinterpret_cast<T *>(&this->data_arr[abs_off])));
                }
            }

            template <class T1 = T, std::enable_if_t<std::is_default_constructible_v<T1>, bool> = true>
            constexpr auto dispatch_default_initialize(size_t virtual_off, size_t sz) noexcept -> exception_t{

                //optimizables

                if constexpr(std::is_nothrow_default_constructible_v<T>){
                    for (size_t i = 0u; i < sz; ++i){
                        size_t abs_off = this->to_index(virtual_off + i); 
                        new (&this->data_arr[abs_off]) T(); //this is the error that I will forever remember, I never know that new () fundamental is NOT new, this is very new, this is the new thing that I will never forget in the new future
                    }

                    return dg_sock::network_exception::SUCCESS;
                } else{
                    try{
                        size_t success_sz   = 0u;
                        auto backoff_task   = [&, this]() noexcept{
                            this->dispatch_destroy(virtual_off, success_sz);
                        };
                        auto backoff_grd    = stdxx::resource_guard(backoff_task); 

                        for (size_t i = 0u; i < sz; ++i){
                            size_t abs_off = this->to_index(virtual_off + i);
                            new (&this->data_arr[abs_off]) T();
                            success_sz += 1u;
                        }

                        backoff_grd.release();
                        return dg_sock::network_exception::SUCCESS;
                    } catch (...){
                        return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                    }
                }
            }
    };

    template <class T, class Allocator, class = void>
    struct pow2_cyclic_queue_chooser{
        using type = nontrivial_pow2_cyclic_queue<T, Allocator>;
        // using type = simple_pow2_cyclic_queue<T, Allocator>;
    };

    template <class T, class Allocator>
    struct pow2_cyclic_queue_chooser<T, Allocator, std::void_t<std::enable_if_t<std::is_trivial_v<T>>>>{
        using type = simple_pow2_cyclic_queue<T, Allocator>;
    };

    template <class T, class Allocator = std::allocator<T>>
    using pow2_cyclic_queue = typename pow2_cyclic_queue_chooser<T, Allocator>::type;
}

namespace dg_sock::network_datastructure::unordered_map_variants{

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

        //https://en.cppreference.com/w/cpp/utility/forward

        if constexpr(std::is_same_v<U, std::remove_reference_t<U>>){
            static_assert(FALSE_VAL<>); //this is not defined, for our usage, I dont know how people define their usage of perfect forwarding, it alters the semantic of forward, this is the most confusing technical decision in our career, forward scope of usage only supposes to forward the arguments, not their class members
                                        //if the containees are to be forwarded as their container, it is forward_like<T, U>
                                        //I know the std took another step of making the invoking container to have && and & for static_cast<object&&>().whatever()
                                        //this is precisely why it is very confusing
        } else{
            return static_cast<dg_forward_like_t<T, U>&&>(value);
        }
    }

    //I just feel like size_t out of nowhere makes no sense
    //and we should stay in the unsigned territory to avoid thinking about signness and friends
    //this should be good

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2(T val) noexcept -> size_t{

        return static_cast<size_t>(sizeof(T) * CHAR_BIT - 1u) - static_cast<size_t>(std::countl_zero(val));
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    static constexpr auto ceil2(T val) noexcept -> T{

        if (val < 2u) [[unlikely]]{
            return 1u;
        } else [[likely]]{
            T uplog_value = unordered_map_variants::ulog2(static_cast<T>(val - 1u)) + 1u;
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

    //there is an existing evidence that the ordering of these guys + the padding + half word load + full word load will heavily affect the performance by a factor of 2x - 3x
    //it's very implementation + platform specific of how to use this map
    //we are not going down the rabbit hole for now

    //the only optimization we could further make is actually compile-time deterministic class member layout by using sfinae
    //we dont have anything to do, so let's choose the least byte of these guys
    //alright, yall can argue that the alignment + half word + full word whatever load
    //the most important thing that we care about is the memory footprint, the worst case memory footprint

    //assume that our container footprint is 64KB, we expect to fit the entire container in the cache to retrieve all the records
    //how precisely do we do this?
    //by builiding a radix tree, or delvrsrv of hash_table
    //as long as 64KB hash_table maps to a 16KB worth of random key findings, we are in a good place   

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
                                                                                             bucket_vec(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<std::size_t>(self::max_capacity())), self::NULL_VIRTUAL_ADDR, allocator),
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

                size_t new_bucket_cap               = std::max(self::min_capacity(), static_cast<size_type>(unordered_map_variants::ceil2(tentative_new_cap)));

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

                return this->internal_insert(unordered_map_variants::node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<Args>(args)...), NULL_VIRTUAL_ADDR));
            }

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> std::pair<iterator, bool>{

                return this->insert(std::pair<const Key, Mapped>(std::forward<Args>(args)...));
            }

            template <class ValueLike = std::pair<const Key, Mapped>>
            constexpr auto insert(ValueLike&& value) -> std::pair<iterator, bool>{

                return this->internal_insert(unordered_map_variants::node_initialize<node_t>(key_type(dg_forward_like<ValueLike>(value.first)), mapped_type(dg_forward_like<ValueLike>(value.second)), NULL_VIRTUAL_ADDR));
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

                return this->internal_insert_or_assign(unordered_map_variants::node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<MappedLike>(mapped)), NULL_VIRTUAL_ADDR));
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

                size_t up_ceil_bucket_count = std::max(unordered_map_variants::ceil2(bucket_count), self::min_capacity());
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
                size_t tentative_capacity   = unordered_map_variants::ceil2(sufficient_capacity);
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

    constexpr auto mul_round(size_t x, size_t multiplier) noexcept -> size_t
    {
        if (multiplier == 0u)
        {
            // throw std::invalid_argument("bad multiplier, 0");
            std::abort();
        }

        if (x == 0u)
        {
            return 0u;
        }

        size_t previous_x       = x - 1u;
        size_t previous_slot    = previous_x / multiplier;
        size_t next_slot        = previous_slot + 1u;

        return next_slot;
    }

    template <class Key, class Mapped, class Hasher = std::hash<Key>, class VirtualAddrType = std::size_t, class HasStructureReordering = std::integral_constant<bool, true>, class Pred = std::equal_to<Key>, class Allocator = std::allocator<Node<HasStructureReordering, Key, Mapped, VirtualAddrType>>, class LoadFactor = std::ratio<4, 8>>
    class cyclic_unordered_node_map{

        private:

            dg_sock::network_datastructure::cyclic_queue::pow2_cyclic_queue<Node<HasStructureReordering, Key, Mapped, VirtualAddrType>, typename std::allocator_traits<Allocator>::template rebind_alloc<Node<HasStructureReordering, Key, Mapped, VirtualAddrType>>> virtual_storage_vec;
            std::vector<VirtualAddrType, typename std::allocator_traits<Allocator>::template rebind_alloc<VirtualAddrType>> bucket_vec;
            size_t virtual_addr_offset;
            Hasher _hasher;
            Pred pred;
            Allocator allocator;

        public:

            using key_type                      = Key;
            using mapped_type                   = Mapped;
            using value_type                    = Node<HasStructureReordering, Key, Mapped, VirtualAddrType>;
            using hasher                        = Hasher;
            using key_equal                     = Pred;
            using allocator_type                = Allocator;
            using reference                     = value_type&;
            using const_reference               = const value_type&;
            using pointer                       = typename std::allocator_traits<Allocator>::pointer;
            using const_pointer                 = typename std::allocator_traits<Allocator>::const_pointer;
            using iterator                      = decltype(virtual_storage_vec)::iterator;
            using const_iterator                = decltype(virtual_storage_vec)::const_iterator;
            using size_type                     = std::size_t;
            using difference_type               = std::intmax_t;
            using self                          = cyclic_unordered_node_map;
            using load_factor_ratio             = typename LoadFactor::type;
            using virtual_addr_t                = get_virtual_addr_t<VirtualAddrType>;
            using node_t                        = Node<HasStructureReordering, Key, Mapped, VirtualAddrType>;

            static inline constexpr virtual_addr_t NULL_VIRTUAL_ADDR    = null_addr_v<virtual_addr_t>;
            static inline constexpr uint64_t MIN_CAP                    = 8u;
            static inline constexpr uint64_t MAX_CAP                    = size_t{1} << 50;

            static_assert(std::numeric_limits<size_type>::max() >= MAX_CAP);

            static_assert(std::disjunction_v<std::is_same<typename std::ratio<4, 8>::type, load_factor_ratio>,
                                             std::is_same<typename std::ratio<8, 8>::type, load_factor_ratio>>);

            constexpr explicit cyclic_unordered_node_map(size_type bucket_count,
                                                         const Hasher& _hasher = Hasher(),
                                                         const Pred& pred = Pred(),
                                                         const Allocator& allocator = Allocator()): virtual_storage_vec(unordered_map_variants::ulog2(self::right_capacity_to_size(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<size_t>(self::max_capacity()))))),
                                                                                                    bucket_vec(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<size_t>(self::max_capacity())), self::NULL_VIRTUAL_ADDR, allocator),
                                                                                                    virtual_addr_offset(0u),
                                                                                                    _hasher(_hasher),
                                                                                                    pred(pred),
                                                                                                    allocator(allocator){}
 
            constexpr cyclic_unordered_node_map(size_type bucket_count,
                                                const Hasher& _hasher,
                                                const Allocator& allocator): cyclic_unordered_node_map(bucket_count, _hasher, Pred(), allocator){}

            constexpr cyclic_unordered_node_map(size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_map(bucket_count, Hasher(), allocator){}

            constexpr explicit cyclic_unordered_node_map(const Allocator& allocator): cyclic_unordered_node_map(self::min_capacity(), allocator){}

            constexpr cyclic_unordered_node_map(): cyclic_unordered_node_map(Allocator()){}

            template <class InputIt>
            constexpr cyclic_unordered_node_map(InputIt first,
                                                InputIt last,
                                                size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_map(bucket_count, _hasher, pred, allocator){
                            
                this->insert(first, last);
            }

            constexpr cyclic_unordered_node_map(std::initializer_list<std::pair<const Key, Mapped>> init_list,
                                                size_type bucket_count,
                                                const Hasher& _hasher,
                                                const Allocator& allocator): cyclic_unordered_node_map(init_list.begin(), init_list.end(), bucket_count, _hasher, Pred(), allocator){}
            
            constexpr cyclic_unordered_node_map(std::initializer_list<std::pair<const Key, Mapped>> init_list,
                                                size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_map(init_list.begin(), init_list.end(), bucket_count, Hasher(), allocator){}
            
            template <class KeyLike, class ...Args>
            constexpr auto try_emplace(KeyLike&& key, Args&& ...args) -> std::pair<iterator, bool>{

                return this->internal_insert(unordered_map_variants::node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<Args>(args)...), NULL_VIRTUAL_ADDR));
            }

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> std::pair<iterator, bool>{

                return this->insert(std::pair<const Key, Mapped>(std::forward<Args>(args)...));
            }

            template <class ValueLike = std::pair<const Key, Mapped>>
            constexpr auto insert(ValueLike&& value) -> std::pair<iterator, bool>{

                return this->internal_insert(unordered_map_variants::node_initialize<node_t>(key_type(dg_forward_like<ValueLike>(value.first)), mapped_type(dg_forward_like<ValueLike>(value.second)), NULL_VIRTUAL_ADDR));
            }

            template <class Iterator>
            constexpr void insert(Iterator first, Iterator last){

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

                return this->internal_insert_or_assign(unordered_map_variants::node_initialize<node_t>(key_type(std::forward<KeyLike>(key)), mapped_type(std::forward<MappedLike>(mapped)), NULL_VIRTUAL_ADDR));
            }

            template <class KeyLike>
            constexpr auto operator[](KeyLike&& key) -> mapped_type&{

                return std::get<0>(this->try_emplace(std::forward<KeyLike>(key), mapped_type{}))->second;
            }

            constexpr void clear() noexcept(true){

                //static_assert();

                this->virtual_storage_vec.clear();
                std::fill(this->bucket_vec.begin(), this->bucket_vec.end(), NULL_VIRTUAL_ADDR);
                this->virtual_addr_offset = 0u;
            }

            constexpr void swap(self& other) noexcept(true){

                //static_assert
                //static_assert
                //...

                std::swap(this->virtual_storage_vec, other.virtual_storage_vec);
                std::swap(this->bucket_vec, other.bucket_vec);
                std::swap(this->virtual_addr_offset, other.virtual_addr_offset);
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
                    throw std::out_of_range("cyclic_unordered_node_map bad access");
                }

                return ptr->second;
            }

            template <class KeyLike>
            constexpr auto at(const KeyLike& key) -> mapped_type&{

                auto ptr = this->find(key);

                if (ptr == this->end()){
                    throw std::out_of_range("cyclic_unordered_node_map bad access");
                }

                return ptr->second;
            }

            constexpr auto empty() const noexcept -> bool{

                return this->virtual_storage_vec.empty();
            }

            constexpr auto capacity() const noexcept -> size_type{

                return this->bucket_vec.size();
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

            static constexpr auto ceil_capacity_to_size(size_t cap) -> size_t{

                return right_capacity_to_size(mul_round(cap, load_factor_ratio::den));
            }

            static constexpr auto ceil_size_to_capacity(size_t sz) -> size_t{

                return right_size_to_capacity(mul_round(sz, load_factor_ratio::num));
            } 
            
        private:
            
            static constexpr auto right_capacity_to_size(size_t cap) noexcept -> size_t{

                return cap / static_cast<size_t>(load_factor_ratio::den) * static_cast<size_t>(load_factor_ratio::num);
            } 

            static constexpr auto right_size_to_capacity(size_t sz) noexcept -> size_t{

                return sz / static_cast<size_t>(load_factor_ratio::num) * static_cast<size_t>(load_factor_ratio::den);
            }

            constexpr auto to_bucket_index(size_t hashed_value) const noexcept -> size_t{

                return hashed_value & static_cast<size_t>(this->bucket_vec.size() - 1u);
            }

            constexpr auto to_storage_addr(virtual_addr_t org_virtual_addr) const noexcept -> virtual_addr_t{

                static_assert(std::is_unsigned_v<virtual_addr_t>);

                return (this->virtual_addr_offset + static_cast<size_t>(org_virtual_addr)) & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);
            }

            constexpr auto to_virtual_addr(virtual_addr_t storage_addr) const noexcept -> virtual_addr_t{

                static_assert(std::is_unsigned_v<virtual_addr_t>);

                virtual_addr_t storage_offset       = this->virtual_addr_offset & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);
                size_t promoted_storage_addr        = static_cast<size_t>(storage_addr) + this->virtual_storage_vec.capacity();
                size_t difference                   = promoted_storage_addr - static_cast<size_t>(storage_offset);
                virtual_addr_t demoted_difference   = difference & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);

                return demoted_difference;
            } 

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) const noexcept(true) -> const virtual_addr_t *{

                //static_assert(noexcept(this->_hasher(key)));
                //static_assert(noexcept(this->pred(this->virtual_storage_vec[*current].key, key)));

                size_t hashed_value             = this->_hasher(key);
                size_t bucket_idx               = this->to_bucket_index(hashed_value);
                const virtual_addr_t * current  = &this->bucket_vec[bucket_idx];

                while (true){
                    if (*current == self::NULL_VIRTUAL_ADDR || this->pred(this->virtual_storage_vec[this->to_storage_addr(*current)].first, key)){
                        return current;
                    }

                    current = &this->virtual_storage_vec[this->to_storage_addr(*current)].nxt_addr;
                }
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) noexcept(true) -> virtual_addr_t *{

                return const_cast<virtual_addr_t *>(static_cast<const self *>(this)->internal_find_bucket_reference(key));
            }

            template <class KeyLike>
            constexpr auto internal_find(const KeyLike& key) const noexcept(true) -> const_iterator{

                const virtual_addr_t * bucket_reference = this->internal_find_bucket_reference(key);

                if (*bucket_reference == self::NULL_VIRTUAL_ADDR){
                    return this->virtual_storage_vec.cend();
                }

                return std::next(this->virtual_storage_vec.cbegin(), this->to_storage_addr(*bucket_reference));
            }

            constexpr void internal_erase_front() noexcept{

                //static_assert(noexcept(this->virtual_storage_vec.pop_front()));

                virtual_addr_t * bucket_reference   = this->internal_find_bucket_reference(this->virtual_storage_vec.front().first);
                *bucket_reference                   = this->virtual_storage_vec.front().nxt_addr;
                this->virtual_storage_vec.pop_front();
                this->virtual_addr_offset           += this->virtual_storage_vec.capacity() - 1u;
                this->virtual_addr_offset           &= this->virtual_storage_vec.capacity() - 1u;
            }

            template <class ValueLike>
            constexpr auto internal_insert(ValueLike&& value) -> std::pair<iterator, bool>{
    
                if (this->virtual_storage_vec.size() == this->virtual_storage_vec.capacity()){
                    this->internal_erase_front();
                }

                virtual_addr_t * insert_reference   = this->internal_find_bucket_reference(value.first);

                if (*insert_reference == self::NULL_VIRTUAL_ADDR){
                    *insert_reference   = this->to_virtual_addr(this->virtual_storage_vec.size());
                    this->virtual_storage_vec.push_back(std::forward<ValueLike>(value));
                    auto rs             = std::make_pair(std::next(this->virtual_storage_vec.begin(), this->virtual_storage_vec.size() - 1u), true);

                    return rs;
                }

                return std::make_pair(std::next(this->virtual_storage_vec.begin(), this->to_storage_addr(*insert_reference)), false);
            }

            template <class ValueLike>
            constexpr auto internal_insert_or_assign(ValueLike&& value) -> std::pair<iterator, bool>{

                auto [ptr, status] = this->internal_insert(std::forward<ValueLike>(value));

                if (!status){
                    ptr->second = dg_forward_like<ValueLike>(value.second);
                }

                return std::make_pair(ptr, true);
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

                virtual_addr_t * swapping_reference = this->internal_find_bucket_reference(this->virtual_storage_vec.front().first); 
    
                if (swapping_reference == key_reference) [[unlikely]]{
                    *key_reference = this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr;
                } else [[likely]]{
                    if (swapping_reference == &this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr) [[unlikely]]{
                        swapping_reference = key_reference;
                    }

                    *swapping_reference = std::exchange(*key_reference, this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr); 
                    dg_restrict_swap_for_destroy(&this->virtual_storage_vec[this->to_storage_addr(*swapping_reference)], &this->virtual_storage_vec.front());
                }

                this->virtual_storage_vec.pop_front();

                this->virtual_addr_offset   += this->virtual_storage_vec.capacity() - 1u;
                this->virtual_addr_offset   &= this->virtual_storage_vec.capacity() - 1u;

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
    constexpr auto operator ==(const cyclic_unordered_node_map<Args...>& lhs, const cyclic_unordered_node_map<Args...>& rhs) noexcept(true) -> bool{

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
    constexpr auto operator !=(const cyclic_unordered_node_map<Args...>& lhs, const cyclic_unordered_node_map<Args...>& rhs) noexcept(true) -> bool{

        return !(lhs == rhs);
    }

    template <class KeyType, class VirtualAddrType>
    struct UnorderedSetNode{
        KeyType key;
        VirtualAddrType nxt_addr;        
    };

    template <class BidirIterator>
    class unordered_set_node_external_iterator{
        
        private:

            BidirIterator base_node_iterator;

            template <class OtherBidirIterator>
            friend class unordered_set_node_external_iterator;

        public:

            using self              = unordered_set_node_external_iterator;
            using difference_type   = std::ptrdiff_t;
            using value_type        = std::remove_reference_t<decltype(std::declval<BidirIterator&>()->key)>;

            constexpr unordered_set_node_external_iterator(): base_node_iterator(){}

            constexpr unordered_set_node_external_iterator(BidirIterator base_node_iterator) noexcept(std::is_nothrow_move_constructible_v<BidirIterator>): base_node_iterator(std::move(base_node_iterator)){}

            template <class OtherBidirIterator, std::enable_if_t<!std::is_same_v<OtherBidirIterator, BidirIterator> && std::is_convertible_v<OtherBidirIterator, BidirIterator>, bool> = true>
            constexpr unordered_set_node_external_iterator(const unordered_set_node_external_iterator<OtherBidirIterator>& other) noexcept(std::is_nothrow_convertible_v<OtherBidirIterator, BidirIterator>): base_node_iterator(other.base_node_iterator){} 

            constexpr auto operator ++() noexcept -> self&{

                ++this->base_node_iterator;
                return *this;
            }

            constexpr auto operator ++(int) noexcept -> self{

                return self(this->base_node_iterator++);
            }

            constexpr auto operator --() noexcept -> self&{

                --this->base_node_iterator;
                return *this;
            }

            constexpr auto operator --(int) noexcept -> self{

                return self(this->base_node_iterator--);
            }

            constexpr auto operator ==(const self& other) const noexcept -> bool{

                return this->base_node_iterator == other.base_node_iterator;
            }

            constexpr auto operator !=(const self& other) const noexcept -> bool{

                return this->base_node_iterator != other.base_node_iterator;
            }

            constexpr auto operator *() const noexcept -> decltype(auto){

                return (*this->base_node_iterator).key;
            }

            constexpr auto unfancy() const noexcept -> const BidirIterator&{

                return this->base_node_iterator;
            }
    };

    template <class KeyType, class VirtualAddrType = std::size_t, class Hasher = std::hash<KeyType>, class Pred = std::equal_to<KeyType>, class Allocator = std::allocator<KeyType>, class LoadFactor = std::ratio<7, 8>>
    class unordered_node_set{

        private:

            std::vector<VirtualAddrType, typename std::allocator_traits<Allocator>::template rebind_alloc<VirtualAddrType>> bucket_vec;
            std::vector<UnorderedSetNode<KeyType, VirtualAddrType>, typename std::allocator_traits<Allocator>::template rebind_alloc<UnorderedSetNode<KeyType, VirtualAddrType>>> virtual_storage_vec; 
            Hasher _hasher;
            Pred pred;
            Allocator allocator;

            using nofancy_iterator          = typename decltype(virtual_storage_vec)::iterator;
            using nofancy_const_iterator    = typename decltype(virtual_storage_vec)::const_iterator;
            using node_t                    = UnorderedSetNode<KeyType, VirtualAddrType>;

        public:

            using key_type                  = KeyType;
            using value_type                = KeyType;
            using hasher                    = Hasher;
            using key_equal                 = Pred;
            using allocator_type            = Allocator;
            using reference                 = value_type&;
            using const_reference           = const value_type&;
            using pointer                   = typename std::allocator_traits<Allocator>::pointer;
            using const_pointer             = typename std::allocator_traits<Allocator>::const_pointer;
            using iterator                  = unordered_set_node_external_iterator<nofancy_iterator>;
            using const_iterator            = unordered_set_node_external_iterator<nofancy_const_iterator>;
            using size_type                 = std::size_t;
            using difference_type           = std::ptrdiff_t;
            using self                      = unordered_node_set;
            using load_factor_ratio         = typename LoadFactor::type;
            using virtual_addr_t            = VirtualAddrType;

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

            constexpr explicit unordered_node_set(size_type bucket_count,
                                                  const Hasher& _hasher       = Hasher(),
                                                  const Pred& pred            = Pred(),
                                                  const Allocator& allocator  = Allocator()): virtual_storage_vec(allocator),
                                                                                              bucket_vec(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<std::size_t>(self::max_capacity())), self::NULL_VIRTUAL_ADDR),
                                                                                             _hasher(_hasher),
                                                                                              pred(pred),
                                                                                              allocator(allocator){

                this->virtual_storage_vec.reserve(self::right_capacity_to_size(this->capacity()));
            }

            constexpr unordered_node_set(size_type bucket_count,
                                         const Hasher& _hasher,
                                         const Allocator& allocator): unordered_node_set(bucket_count, _hasher, Pred(), allocator){}

            constexpr unordered_node_set(size_type bucket_count,
                                         const Allocator& allocator): unordered_node_set(bucket_count, Hasher(), allocator){}

            constexpr explicit unordered_node_set(const Allocator& allocator): unordered_node_set(self::min_capacity(), allocator){}

            constexpr unordered_node_set(): unordered_node_set(Allocator()){}

            template <class InputIt>
            constexpr unordered_node_set(InputIt first,
                                         InputIt last,
                                         size_type bucket_count,
                                         const Hasher& _hasher      = Hasher(),
                                         const Pred& pred           = Pred(),
                                         const Allocator& allocator = Allocator()): unordered_node_set(bucket_count, _hasher, pred, allocator){

                this->insert(first, last); //bad, leak
            }

            template <class InputIt>
            constexpr unordered_node_set(InputIt first,
                                         InputIt last,
                                         size_type bucket_count,
                                         const Allocator& allocator): unordered_node_set(first, last, bucket_count, Hasher(), Pred(), allocator){}

            template <class InputIt>
            constexpr unordered_node_set(InputIt first,
                                         InputIt last,
                                         size_type bucket_count,
                                         const Hasher& _hasher,
                                         const Allocator& allocator): unordered_node_set(first, last, bucket_count, _hasher, Pred(), allocator){}

            constexpr unordered_node_set(std::initializer_list<value_type> init_list,
                                         size_type bucket_count,
                                         const Hasher& _hasher      = Hasher(),
                                         const Pred& pred           = Pred(),
                                         const Allocator& allocator = Allocator()): unordered_node_set(init_list.begin(), init_list.end(), bucket_count, _hasher, pred, allocator){}

            constexpr unordered_node_set(std::initializer_list<value_type> init_list,
                                         size_type bucket_count,
                                         const Hasher& _hasher,
                                         const Allocator& allocator): unordered_node_set(init_list.begin(), init_list.end(), bucket_count, _hasher, allocator){}
            
            constexpr unordered_node_set(std::initializer_list<value_type> init_list,
                                         size_type bucket_count,
                                         const Allocator& allocator): unordered_node_set(init_list, bucket_count, Hasher(), allocator){}

            __attribute__((noinline)) constexpr void rehash(size_type tentative_new_cap){

                //the motivation to write this function is to:
                //(1): keep the CRUD map property
                //(2): keep the ratio of unordered_map_bucket <-> storage_vector ratio by using induction (assume that the ratio is kept, we need to prove using induction that an invoke of this function will keep such property)

                if (tentative_new_cap <= this->capacity()){
                    return;
                }

                size_t new_bucket_cap               = std::max(self::min_capacity(), static_cast<size_type>(unordered_map_variants::ceil2(tentative_new_cap))); //std::max() is not necessary due to induction

                if (new_bucket_cap > self::max_capacity()){
                    throw std::length_error("bad unordered_node_set capacity");
                }

                //we are upsizing, because we assumed the ratio, we can make sure that the reserve order is thru in the sense of updating the current cap -> the new cap

                size_t new_virtual_storage_vec_cap  = self::right_capacity_to_size(new_bucket_cap);
                auto new_bucket_vec                 = decltype(bucket_vec)(new_bucket_cap, self::NULL_VIRTUAL_ADDR, this->allocator);

                this->virtual_storage_vec.reserve(new_virtual_storage_vec_cap);

                //imagine a picture of contiguous bucket chainings, assume that all the bucket chaining "starting" from the new_bucket_vec is correct up to the current point
                //we'd have to use induction to keep the property 
                //we are to insert a new bucket, we'd first have to void the nxt_addr, and find the "new bucket location" in our newly created "contiguous bucket chaining" and chain the bucket to that

                for (size_t i = 0u; i < this->virtual_storage_vec.size(); ++i){
                    this->virtual_storage_vec[i].nxt_addr   = self::NULL_VIRTUAL_ADDR;
                    size_t hashed_value                     = this->_hasher(this->virtual_storage_vec[i].key);
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
                    throw std::length_error("bad unordered_node_set size");
                }

                this->rehash(self::ceil_size_to_capacity(new_sz)); //this will ceil up to max_size(), so we are in "the numerical defined" range
            }

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> std::pair<const_iterator, bool>{

                auto [nofancy_it, status] = this->nofancy_emplace(std::forward<Args>(args)...);
                return std::make_pair(const_iterator(nofancy_it), status);
            } 

            template <class KeyLike>
            constexpr auto insert(KeyLike&& key) -> std::pair<const_iterator, bool>{

                auto [nofancy_it, status] = this->nofancy_insert(std::forward<KeyLike>(key));
                return std::make_pair(const_iterator(nofancy_it), status);
            }

            template <class InputIt>
            constexpr void insert(InputIt first, InputIt last){

                this->reserve(this->size() + std::distance(first, last)); //we are giving our fellows a chance to not have leaks during nothrow move iterator (which is very very bad, most software failed at this point)

                while (first != last){
                    this->insert(*first);
                    std::advance(first, 1u);
                }
            }

            constexpr void insert(std::initializer_list<KeyType> init_list){

                this->insert(init_list.begin(), init_list.end());
            }

            constexpr void clear() noexcept(true){

                //static_assert(noexcept(this->virtual_storage_vec.clear()));

                this->virtual_storage_vec.clear();
                std::fill(this->bucket_vec.begin(), this->bucket_vec.end(), self::NULL_VIRTUAL_ADDR);
            }

            constexpr void swap(self& other) noexcept(true){

                //static_assert(noexcept(std::swap(this->virtual_storage_vec, other.virtual_storage_vec)));
                //static_assert(noexcept(std::swap(this->bucket_vec, other.bucket_vec)));
                //static_assert(noexcept(std::swap(this->_hasher, other._hasher)));
                //static_assert(noexcept(std::swap(this->pred, other.pred)));
                //static_assert(noexcept(std::swap(this->allocator, other.allocator)));

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
                        return this->nofancy_erase(const_iterator(std::forward<EraseArg>(erase_arg)).unfancy());
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                } else{
                    return this->nofancy_erase(std::forward<EraseArg>(erase_arg));
                }
            }

            template <class KeyLike>
            constexpr auto find(const KeyLike& key) const noexcept(true) -> const_iterator{
                
                return const_iterator(this->nofancy_find(key));
            }

            template <class KeyLike>
            constexpr auto contains(const KeyLike& key) const noexcept(true) -> bool{

                return this->find(key) != this->end();
            }

            template <class KeyLike>
            constexpr auto count(const KeyLike& key) const noexcept(true) -> size_t{

                return static_cast<size_t>(this->contains(key));
            }

            constexpr auto empty() const noexcept -> bool{

                return this->virtual_storage_vec.empty();
            }

            constexpr auto capacity() const noexcept -> size_type{

                return this->bucket_vec.size();
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

            constexpr auto begin() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_begin());
            }

            constexpr auto cbegin() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cbegin());
            }

            constexpr auto end() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_end());
            }

            constexpr auto cend() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cend());
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

                //static_assert(noexcept(this->_hasher(key)));
                //static_assert(noexcept(this->pred(this->virtual_storage_vec[*current].key, key)));

                size_t hashed_value             = this->_hasher(key);
                size_t bucket_idx               = this->to_bucket_index(hashed_value);
                const virtual_addr_t * current  = &this->bucket_vec[bucket_idx];

                while (true){
                    if (*current == self::NULL_VIRTUAL_ADDR || this->pred(this->virtual_storage_vec[*current].key, key)){ //pred order?
                        return current;
                    }

                    current = &this->virtual_storage_vec[*current].nxt_addr;
                }
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) noexcept(true) -> virtual_addr_t *{

                //this is guaranteed to be "defined according to std"
                return const_cast<virtual_addr_t *>(static_cast<const self *>(this)->internal_find_bucket_reference(key));
            }

            template <class KeyLike>
            constexpr auto internal_find(const KeyLike& key) const noexcept(true) -> nofancy_const_iterator{

                const virtual_addr_t * bucket_reference = this->internal_find_bucket_reference(key);

                if (*bucket_reference == self::NULL_VIRTUAL_ADDR){
                    return this->nofancy_end();
                }

                return std::next(this->virtual_storage_vec.begin(), *bucket_reference);
            }

            template <class BucketLike>
            constexpr auto internal_insert(BucketLike&& bucket) -> std::pair<nofancy_const_iterator, bool>{

                if (this->virtual_storage_vec.size() == this->virtual_storage_vec.capacity()){ //the only regret we have is that we cant keep 1024 items strict if it is reserved for 1024 items, without loss of generality
                    this->rehash(this->capacity() << self::POW2_GROWTH_FACTOR);
                }

                virtual_addr_t * insert_reference   = this->internal_find_bucket_reference(bucket.key);

                if (*insert_reference == self::NULL_VIRTUAL_ADDR){
                    *insert_reference   = static_cast<virtual_addr_t>(this->virtual_storage_vec.size());
                    this->virtual_storage_vec.push_back(std::forward<BucketLike>(bucket));
                    auto rs             = std::make_pair(std::prev(this->virtual_storage_vec.cend()), true); //this might be "ambiguous," we would rather do a previous statement

                    return rs;
                }

                return std::make_pair(std::next(this->virtual_storage_vec.cbegin(), *insert_reference), false);
            }

            template <class KeyLike>
            constexpr auto internal_erase_key(const KeyLike& key) noexcept(true) -> bool{

                virtual_addr_t * key_reference = this->internal_find_bucket_reference(key);

                if (*key_reference == self::NULL_VIRTUAL_ADDR){
                    return false;
                }

                //we found a bucket, we are guaranteed to have at least 1 bucket => a back().key is guaranteed to be a valid statement

                virtual_addr_t * swapping_reference = this->internal_find_bucket_reference(this->virtual_storage_vec.back().key);

                if (swapping_reference == key_reference){
                    *key_reference = this->virtual_storage_vec[*key_reference].nxt_addr;
                } else{
                    if (swapping_reference == &this->virtual_storage_vec[*key_reference].nxt_addr) [[unlikely]]{
                        swapping_reference = key_reference;
                    }

                    *swapping_reference = std::exchange(*key_reference, this->virtual_storage_vec[*key_reference].nxt_addr);
                    dg_restrict_swap_for_destroy(&this->virtual_storage_vec[*swapping_reference], &this->virtual_storage_vec.back());
                }

                this->virtual_storage_vec.pop_back();
                return true;
            }

            constexpr auto internal_erase_iter(nofancy_const_iterator iter) noexcept(true) -> nofancy_const_iterator{

                if (iter == this->nofancy_cend()){
                    return this->nofancy_cend();
                } else [[likely]]{
                    size_t off = std::distance(this->virtual_storage_vec.cbegin(), iter);
                    this->internal_erase_key(iter->key);
                    return std::next(this->virtual_storage_vec.cbegin(), off);
                }
            }

            template <class ...Args>
            constexpr auto nofancy_emplace(Args&& ...args) -> std::pair<nofancy_const_iterator, bool>{

                return this->internal_insert(UnorderedSetNode<KeyType, VirtualAddrType>{.key        = KeyType(std::forward<Args>(args)...),
                                                                                        .nxt_addr   = self::NULL_VIRTUAL_ADDR});
            }

            template <class KeyLike>
            constexpr auto nofancy_insert(KeyLike&& key) -> std::pair<nofancy_const_iterator, bool>{

                return this->internal_insert(UnorderedSetNode<KeyType, VirtualAddrType>{.key        = std::forward<KeyLike>(key),
                                                                                        .nxt_addr   = self::NULL_VIRTUAL_ADDR});
            }

            template <class EraseArg>
            constexpr auto nofancy_erase(EraseArg&& erase_arg) noexcept(true){

                if constexpr(std::is_convertible_v<EraseArg&&, nofancy_const_iterator>){
                    if constexpr(std::is_nothrow_convertible_v<EraseArg&&, nofancy_const_iterator>){
                        return this->internal_erase_iter(std::forward<EraseArg>(erase_arg));
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                } else{
                    return static_cast<size_type>(this->internal_erase_key(std::forward<EraseArg>(erase_arg)));
                }
            }

            template <class KeyLike>
            constexpr auto nofancy_find(const KeyLike& key) const noexcept(true) -> nofancy_const_iterator{

                return this->internal_find(key);
            }

            constexpr auto nofancy_begin() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.begin();
            }

            constexpr auto nofancy_cbegin() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.cbegin();
            }

            constexpr auto nofancy_end() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.end();
            }

            constexpr auto nofancy_cend() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.cend();
            }
    };

    template <class ...Args>
    constexpr auto operator ==(const unordered_node_set<Args...>& lhs, const unordered_node_set<Args...>& rhs) noexcept(true) -> bool{

        if (lhs.size() != rhs.size()){
            return false;
        }

        for (const auto& key: lhs){
            if (!rhs.contains(key)){
                return false;
            }
        }

        return true;
    }

    template <class ...Args>
    constexpr auto operator !=(const unordered_node_set<Args...>& lhs, const unordered_node_set<Args...>& rhs) noexcept(true) -> bool{

        return !(lhs == rhs);
    }

    template <class KeyType, class VirtualAddrType = std::size_t, class Hasher = std::hash<KeyType>, class Pred = std::equal_to<KeyType>, class Allocator = std::allocator<KeyType>, class LoadFactor = std::ratio<4, 8>>
    class cyclic_unordered_node_set{

        private:

            dg_sock::network_datastructure::cyclic_queue::pow2_cyclic_queue<UnorderedSetNode<KeyType, VirtualAddrType>, typename std::allocator_traits<Allocator>::template rebind_alloc<UnorderedSetNode<KeyType, VirtualAddrType>>> virtual_storage_vec;
            std::vector<VirtualAddrType, typename std::allocator_traits<Allocator>::template rebind_alloc<VirtualAddrType>> bucket_vec;
            size_t virtual_addr_offset;
            Hasher _hasher;
            Pred pred;
            Allocator allocator;

            using node_t                    = UnorderedSetNode<KeyType, VirtualAddrType>;

        public:

            using nofancy_iterator          = typename decltype(virtual_storage_vec)::iterator;
            using nofancy_const_iterator    = typename decltype(virtual_storage_vec)::const_iterator;

            using key_type                  = KeyType;
            using value_type                = KeyType;
            using hasher                    = Hasher;
            using key_equal                 = Pred;
            using allocator_type            = Allocator;
            using reference                 = value_type&;
            using const_reference           = const value_type&;
            using pointer                   = typename std::allocator_traits<Allocator>::pointer; //
            using const_pointer             = typename std::allocator_traits<Allocator>::const_pointer; //
            using iterator                  = unordered_set_node_external_iterator<nofancy_iterator>;
            using const_iterator            = unordered_set_node_external_iterator<nofancy_const_iterator>;
            using size_type                 = std::size_t;
            using difference_type           = std::ptrdiff_t;
            using self                      = cyclic_unordered_node_set;
            using load_factor_ratio         = typename LoadFactor::type;
            using virtual_addr_t            = VirtualAddrType;

            static inline constexpr virtual_addr_t NULL_VIRTUAL_ADDR        = null_addr_v<virtual_addr_t>;
            static inline constexpr uint64_t MIN_CAP                        = 8u;
            static inline constexpr uint64_t MAX_CAP                        = uint64_t{1} << 50;

            static_assert((std::numeric_limits<size_type>::max() >= MAX_CAP));

            static_assert(std::disjunction_v<std::is_same<typename std::ratio<4, 8>::type, load_factor_ratio>,
                                             std::is_same<typename std::ratio<8, 8>::type, load_factor_ratio>>);

            constexpr explicit cyclic_unordered_node_set(size_type bucket_count,
                                                         const Hasher& _hasher      = Hasher(),
                                                         const Pred& pred           = Pred(),
                                                         const Allocator& allocator = Allocator()): virtual_storage_vec(unordered_map_variants::ulog2(self::right_capacity_to_size(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<size_t>(self::max_capacity()))))),
                                                                                                    bucket_vec(unordered_map_variants::min_size_clamp(static_cast<std::size_t>(unordered_map_variants::ceil2(bucket_count)), static_cast<std::size_t>(self::min_capacity()), static_cast<size_t>(self::max_capacity())), self::NULL_VIRTUAL_ADDR, allocator),
                                                                                                    virtual_addr_offset(0u),
                                                                                                    _hasher(_hasher),
                                                                                                    allocator(allocator),
                                                                                                    pred(pred){}

            constexpr cyclic_unordered_node_set(size_type bucket_count,
                                                const Hasher& _hasher,
                                                const Allocator& allocator): cyclic_unordered_node_set(bucket_count, _hasher, Pred(), allocator){}

            constexpr cyclic_unordered_node_set(size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_set(bucket_count, Hasher(), allocator){}

            constexpr cyclic_unordered_node_set(const Allocator& allocator): cyclic_unordered_node_set(self::min_capacity(), allocator){}

            constexpr cyclic_unordered_node_set(): cyclic_unordered_node_set(Allocator()){}

            template <class InputIt>
            constexpr cyclic_unordered_node_set(InputIt first,
                                                InputIt last,
                                                size_type bucket_count,
                                                const Hasher& _hasher       = Hasher(),
                                                const Pred& pred            = Pred(),
                                                const Allocator& allocator  = Allocator()): cyclic_unordered_node_set(bucket_count, _hasher, pred, allocator){

                this->insert(first, last);
            }

            template <class InputIt>
            constexpr cyclic_unordered_node_set(InputIt first,
                                                InputIt last,
                                                size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_set(first, last, bucket_count, Hasher(), Pred(), allocator){}

            template <class InputIt>
            constexpr cyclic_unordered_node_set(InputIt first,
                                                InputIt last,
                                                size_type bucket_count,
                                                const Hasher& _hasher,
                                                const Allocator& allocator): cyclic_unordered_node_set(first, last, bucket_count, _hasher, Pred(), allocator){}

            constexpr cyclic_unordered_node_set(std::initializer_list<value_type> init_list,
                                                size_type bucket_count,
                                                const Hasher& _hasher       = Hasher(),
                                                const Pred& pred            = Pred(),
                                                const Allocator& allocator  = Allocator()): cyclic_unordered_node_set(init_list.begin(), init_list.end(), bucket_count, _hasher, pred, allocator){}

            constexpr cyclic_unordered_node_set(std::initializer_list<value_type> init_list,
                                                size_type bucket_count,
                                                const Hasher& _hasher,
                                                const Allocator& allocator): cyclic_unordered_node_set(init_list.begin(), init_list.end(), bucket_count, _hasher, allocator){}

            constexpr cyclic_unordered_node_set(std::initializer_list<value_type> init_list,
                                                size_type bucket_count,
                                                const Allocator& allocator): cyclic_unordered_node_set(init_list, bucket_count, Hasher(), allocator){}

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> std::pair<const_iterator, bool>{

                auto [nofancy_it, status] = this->nofancy_emplace(std::forward<Args>(args)...);
                return std::make_pair(const_iterator(nofancy_it), status);
            }

            template <class KeyLike>
            constexpr auto insert(KeyLike&& key) -> std::pair<const_iterator, bool>{

                auto [nofancy_it, status] = this->nofancy_insert(std::forward<KeyLike>(key));
                return std::make_pair(const_iterator(nofancy_it), status);
            }

            template <class InputIt>
            constexpr void insert(InputIt first, InputIt last){

                while (first != last){
                    this->insert(*first);
                    std::advance(first, 1u);
                }
            }

            constexpr void insert(std::initializer_list<KeyType> init_list){

                this->insert(init_list.begin(), init_list.end());
            }

            constexpr void clear() noexcept(true){

                //static_assert(noexcept(this->virtual_storage_vec.clear()));

                this->virtual_storage_vec.clear();
                std::fill(this->bucket_vec.begin(), this->bucket_vec.end(), self::NULL_VIRTUAL_ADDR);
                this->virtual_addr_offset = 0u;
            }

            constexpr void swap(self& other) noexcept(true){

                //static_assert();
                //static_assert();
                //static_assert();
                //static_assert();
                //static_assert();
                //static_assert();

                std::swap(this->virtual_storage_vec, other.virtual_storage_vec);
                std::swap(this->bucket_vec, other.bucket_vec);
                std::swap(this->virtual_addr_offset, other.virtual_addr_offset);
                std::swap(this->_hasher, other._hasher);
                std::swap(this->pred, other.pred);
                std::swap(this->allocator, other.allocator);
            }

            template <class EraseArg>
            constexpr auto erase(EraseArg&& erase_arg) noexcept(true){

                if constexpr(std::is_convertible_v<EraseArg&&, const_iterator>){
                    if constexpr(std::is_nothrow_convertible_v<EraseArg&&, const_iterator>){
                        return this->nofancy_erase(const_iterator(std::forward<EraseArg>(erase_arg)).unfancy());
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                } else{
                    return this->nofancy_erase(std::forward<EraseArg>(erase_arg));
                }
            }

            template <class KeyLike>
            constexpr auto find(const KeyLike& key) const noexcept(true) -> const_iterator{

                return const_iterator(this->nofancy_find(key));
            }

            template <class KeyLike>
            constexpr auto contains(const KeyLike& key) const noexcept(true) -> bool{

                return this->find(key) != this->cend();
            }

            template <class KeyLike>
            constexpr auto count(const KeyLike& key) const noexcept(true) -> size_t{

                return static_cast<size_t>(this->contains(key));
            }

            constexpr auto empty() const noexcept -> bool{

                return this->virtual_storage_vec.empty();
            }

            constexpr auto capacity() const noexcept -> size_type{

                return this->bucket_vec.size();
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

            constexpr auto begin() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cbegin());
            }

            constexpr auto cbegin() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cbegin());
            }

            constexpr auto end() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cend());
            }

            constexpr auto cend() const noexcept -> const_iterator{

                return const_iterator(this->nofancy_cend());
            }

            static consteval auto max_load_factor() -> double{

                return static_cast<double>(self::load_factor_ratio::num) / self::load_factor_ratio::den;
            }

            static constexpr auto capacity_to_size(size_t cap) -> size_t{

                return cap * self::max_load_factor();
            }

            static constexpr auto size_to_capacity(size_t sz) -> size_t{

                return sz / self::max_load_factor();
            }

            static constexpr auto ceil_capacity_to_size(size_t cap) -> size_t{

                return right_capacity_to_size(mul_round(cap, load_factor_ratio::den));
            }

            static constexpr auto ceil_size_to_capacity(size_t sz){

                return right_size_to_capacity(mul_round(sz, load_factor_ratio::num));
            }

        private:

            static constexpr auto right_capacity_to_size(size_t cap) noexcept -> size_t{

                return cap / static_cast<size_t>(load_factor_ratio::den) * static_cast<size_t>(load_factor_ratio::num);
            }

            static constexpr auto right_size_to_capacity(size_t sz) noexcept -> size_t{

                return sz / static_cast<size_t>(load_factor_ratio::num) * static_cast<size_t>(load_factor_ratio::den);
            }

            constexpr auto to_bucket_index(size_t hashed_value) const noexcept -> size_t{

                return hashed_value & static_cast<size_t>(this->bucket_vec.size() - 1u);
            }

            constexpr auto to_storage_addr(virtual_addr_t org_virtual_addr) const noexcept -> virtual_addr_t{

                static_assert(std::is_unsigned_v<virtual_addr_t>);

                return (this->virtual_addr_offset + static_cast<size_t>(org_virtual_addr)) & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);
            }

            constexpr auto to_virtual_addr(virtual_addr_t storage_addr) const noexcept -> virtual_addr_t{

                static_assert(std::is_unsigned_v<virtual_addr_t>);

                virtual_addr_t storage_offset       = this->virtual_addr_offset & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);
                size_t promoted_storage_addr        = static_cast<size_t>(storage_addr) + this->virtual_storage_vec.capacity();
                size_t difference                   = promoted_storage_addr - static_cast<size_t>(storage_offset);
                virtual_addr_t demoted_difference   = difference & static_cast<size_t>(this->virtual_storage_vec.capacity() - 1u);

                return demoted_difference;
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) const noexcept(true) -> const virtual_addr_t *{

                //static_assert(this->_hasher(key));
                //static_assert(this->pred(this->virtual_addr_vec[*current].key, key));

                size_t hashed_value             = this->_hasher(key);
                size_t bucket_idx               = this->to_bucket_index(hashed_value);
                const virtual_addr_t * current  = &this->bucket_vec[bucket_idx];

                while (true){
                    if (*current == self::NULL_VIRTUAL_ADDR || this->pred(this->virtual_storage_vec[this->to_storage_addr(*current)].key, key)){
                        return current;
                    }

                    current = &this->virtual_storage_vec[this->to_storage_addr(*current)].nxt_addr;
                }
            }

            template <class KeyLike>
            constexpr auto internal_find_bucket_reference(const KeyLike& key) noexcept(true) -> virtual_addr_t *{

                return const_cast<virtual_addr_t *>(static_cast<const self *>(this)->internal_find_bucket_reference(key));
            }

            template <class KeyLike>
            constexpr auto internal_find(const KeyLike& key) const noexcept(true) -> nofancy_const_iterator{

                const virtual_addr_t * bucket_reference = this->internal_find_bucket_reference(key);

                if (*bucket_reference == self::NULL_VIRTUAL_ADDR){
                    return this->virtual_storage_vec.cend();
                }

                return std::next(this->virtual_storage_vec.cbegin(), this->to_storage_addr(*bucket_reference));
            }

            constexpr void internal_erase_front() noexcept{

                //static_assert(noexcept(this->virtual_storage_vec.pop_front()));

                virtual_addr_t * bucket_reference       = this->internal_find_bucket_reference(this->virtual_storage_vec.front().key);
                *bucket_reference                       = this->virtual_storage_vec.front().nxt_addr;
                this->virtual_storage_vec.pop_front();
                this->virtual_addr_offset               += this->virtual_storage_vec.capacity() - 1u;
                this->virtual_addr_offset               &= this->virtual_storage_vec.capacity() - 1u;
            }

            template <class BucketLike>
            constexpr auto internal_insert(BucketLike&& bucket) -> std::pair<nofancy_const_iterator, bool>{

                if (this->virtual_storage_vec.size() == this->virtual_storage_vec.capacity()){ //the only regret we have is that we cant keep 1024 items strict if it is reserved for 1024 items, without loss of generality
                    this->internal_erase_front();
                }

                virtual_addr_t * insert_reference   = this->internal_find_bucket_reference(bucket.key);

                if (*insert_reference == self::NULL_VIRTUAL_ADDR){
                    *insert_reference   = this->to_virtual_addr(this->virtual_storage_vec.size());
                    this->virtual_storage_vec.push_back(std::forward<BucketLike>(bucket));
                    auto rs             = std::make_pair(std::prev(this->virtual_storage_vec.cend()), true);

                    return rs;
                }

                return std::make_pair(std::next(this->virtual_storage_vec.cbegin(), this->to_storage_addr(*insert_reference)), false);
            }

            template <class ...Args>
            constexpr auto nofancy_emplace(Args&& ...args) -> std::pair<nofancy_const_iterator, bool>{

                return this->internal_insert(UnorderedSetNode<KeyType, VirtualAddrType>{.key      = KeyType(std::forward<Args>(args)...),
                                                                                        .nxt_addr = self::NULL_VIRTUAL_ADDR});
            }

            template <class KeyLike>
            constexpr auto nofancy_insert(KeyLike&& key) -> std::pair<nofancy_const_iterator, bool>{

                return this->internal_insert(UnorderedSetNode<KeyType, VirtualAddrType>{.key        = std::forward<KeyLike>(key),
                                                                                        .nxt_addr   = self::NULL_VIRTUAL_ADDR});
            }

            template <class KeyLike>
            constexpr auto internal_erase_key(const KeyLike& key) noexcept(true) -> bool{

                virtual_addr_t * key_reference = this->internal_find_bucket_reference(key);

                if (*key_reference == self::NULL_VIRTUAL_ADDR){
                    return false;
                }

                //we found a bucket, we are guaranteed to have at least 1 bucket => a back().key is guaranteed to be a valid statement

                virtual_addr_t * swapping_reference = this->internal_find_bucket_reference(this->virtual_storage_vec.front().key);

                if (swapping_reference == key_reference){
                    *key_reference = this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr;
                } else{
                    if (swapping_reference == &this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr) [[unlikely]]{
                        swapping_reference = key_reference;
                    }

                    *swapping_reference = std::exchange(*key_reference, this->virtual_storage_vec[this->to_storage_addr(*key_reference)].nxt_addr);
                    dg_restrict_swap_for_destroy(&this->virtual_storage_vec[this->to_storage_addr(*swapping_reference)], &this->virtual_storage_vec.front());
                }

                this->virtual_storage_vec.pop_front();

                this->virtual_addr_offset   += this->virtual_storage_vec.capacity() - 1u;
                this->virtual_addr_offset   &= this->virtual_storage_vec.capacity() - 1u;

                return true;
            }

            constexpr auto internal_erase_iter(nofancy_const_iterator iter) noexcept(true) -> nofancy_const_iterator{

                if (iter == this->nofancy_cend()){
                    return this->nofancy_cend();
                } else [[likely]]{
                    size_t off = std::distance(this->virtual_storage_vec.cbegin(), iter);
                    this->internal_erase_key(iter->key);
                    return std::next(this->virtual_storage_vec.cbegin(), off);
                }
            }

            template <class EraseArg>
            constexpr auto nofancy_erase(EraseArg&& erase_arg) noexcept(true){

                if constexpr(std::is_convertible_v<EraseArg&&, nofancy_const_iterator>){
                    if constexpr(std::is_nothrow_convertible_v<EraseArg&&, nofancy_const_iterator>){
                        return this->internal_erase_iter(std::forward<EraseArg>(erase_arg));
                    } else{
                        static_assert(FALSE_VAL<>);
                    }
                } else{
                    return static_cast<size_type>(this->internal_erase_key(std::forward<EraseArg>(erase_arg)));
                }
            }

            template <class KeyLike>
            constexpr auto nofancy_find(const KeyLike& key) const noexcept(true) -> nofancy_const_iterator{

                return this->internal_find(key);
            }

            constexpr auto nofancy_begin() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.begin();
            }

            constexpr auto nofancy_cbegin() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.cbegin();
            }

            constexpr auto nofancy_end() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.end();
            }

            constexpr auto nofancy_cend() const noexcept -> nofancy_const_iterator{

                return this->virtual_storage_vec.cend();
            }
    };

    template <class ...Args>
    constexpr auto operator ==(const cyclic_unordered_node_set<Args...>& lhs, const cyclic_unordered_node_set<Args...>& rhs) noexcept(true) -> bool{

        if (lhs.size() != rhs.size()){
            return false;
        }

        for (const auto& key: lhs){
            if (!rhs.contains(key)){
                return false;
            }
        }

        return true;
    }

    template <class ...Args>
    constexpr auto operator !=(const cyclic_unordered_node_set<Args...>& lhs, const cyclic_unordered_node_set<Args...>& rhs) noexcept(true) -> bool{

        return !(lhs == rhs);
    }
}

namespace std{

    template <class ...Args>
    constexpr void swap(dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Args...>& lhs,
                        dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Args...>& rhs) noexcept(noexcept(std::declval<dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Args...>&>().swap(std::declval<dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Args...>&>()))){

        lhs.swap(rhs);
    }

    template <class ...Args, class Pred>
    constexpr void erase_if(dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<Args...>& umap,
                            Pred pred){

        auto it = umap.begin();

        while (it != umap.end()){
            if (pred(*it)){
                it = umap.erase(it);
            } else{
                std::advance(it, 1u);
            }
        }
    }

    template <class ...Args>
    constexpr void swap(dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_map<Args...>& lhs,
                        dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_map<Args...>& rhs) noexcept(noexcept(std::declval<dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_map<Args...>&>().swap(std::declval<dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_map<Args...>&>()))){

        lhs.swap(rhs);
    }

    template <class ...Args>
    constexpr void swap(dg_sock::network_datastructure::unordered_map_variants::unordered_node_set<Args...>& lhs,
                        dg_sock::network_datastructure::unordered_map_variants::unordered_node_set<Args...>& rhs) noexcept(noexcept(std::declval<dg_sock::network_datastructure::unordered_map_variants::unordered_node_set<Args...>&>().swap(std::declval<dg_sock::network_datastructure::unordered_map_variants::unordered_node_set<Args...>&>()))){

        lhs.swap(rhs);
    }

    template <class ...Args, class Pred>
    constexpr void erase_if(dg_sock::network_datastructure::unordered_map_variants::unordered_node_set<Args...>& uset,
                            Pred pred){

        auto it = uset.begin();

        while (it != uset.end()){
            if (pred(*it)){
                it = uset.erase(it);
            } else{
                std::advance(it, 1u);
            }
        }
    }

    template <class ...Args>
    constexpr void swap(dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_set<Args...>& lhs,
                        dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_set<Args...>& rhs) noexcept(noexcept(std::declval<dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_set<Args...>&>().swap(std::declval<dg_sock::network_datastructure::unordered_map_variants::cyclic_unordered_node_set<Args...>&>()))){

        lhs.swap(rhs);
    }
}

namespace dg_sock::network_datastructure::atomic_loader{

    //we will use this a lot
    //because we can prove that relaxed ordering (in the compiler's eyes) is not less than the ordering of normal non-concurrent variables
    //thus the code that is correct for normal variables must be correct for relaxed variables
    //we dont use std::memory_order_seq_cst tx in and out of the load() and unload() for this very reason
    //what we are missing is a relaxed mutual exclusion lock(), unlock()
    //we'll be back

    template <class T>
    class reflectible_relaxed_loader{

        private:

            using word_t = std::array<char, 8u>; 

            static inline constexpr size_t REFLECTIBLE_SZ               = dg_sock::network_trivial_serializer::size(T{});
            static inline constexpr size_t ADJUSTED_REFLECTIBLE_SZ      = std::max(size_t{1}, REFLECTIBLE_SZ);
            static inline constexpr size_t CONTAINER_EVEN_SZ            = ADJUSTED_REFLECTIBLE_SZ / sizeof(word_t);
            static inline constexpr size_t CONTAINER_ODD_SZ             = size_t{ADJUSTED_REFLECTIBLE_SZ % sizeof(word_t) != 0u};
            static inline constexpr size_t CONTAINER_SZ                 = CONTAINER_EVEN_SZ + CONTAINER_ODD_SZ;
            static inline constexpr size_t SERIALIZED_RAW_CONTAINER_SZ  = dg_sock::network_trivial_serializer::size(std::array<word_t, CONTAINER_SZ>{});

            std::array<std::atomic<word_t>, CONTAINER_SZ> atomic_container;

        public:

            constexpr auto load() noexcept -> T{

                std::array<char, SERIALIZED_RAW_CONTAINER_SZ> serialized_raw_container{};
                std::array<word_t, CONTAINER_SZ> raw_container{};
                T rs{};

                for (size_t i = 0u; i < CONTAINER_SZ; ++i){
                    raw_container[i] = this->atomic_container[i].load(std::memory_order_relaxed);
                }

                dg_sock::network_trivial_serializer::serialize_into(serialized_raw_container.data(), raw_container);
                dg_sock::network_trivial_serializer::deserialize_into(rs, serialized_raw_container.data());

                return rs;
            }

            constexpr void dump(T value) noexcept{

                std::array<char, SERIALIZED_RAW_CONTAINER_SZ> serialized_raw_container{};
                std::array<word_t, CONTAINER_SZ> raw_container{};

                dg_sock::network_trivial_serializer::serialize_into(serialized_raw_container.data(), value);
                dg_sock::network_trivial_serializer::deserialize_into(raw_container, serialized_raw_container.data());

                for (size_t i = 0u; i < CONTAINER_SZ; ++i){
                    this->atomic_container[i].exchange(raw_container[i], std::memory_order_relaxed);
                }
            }
    };

}

#endif
