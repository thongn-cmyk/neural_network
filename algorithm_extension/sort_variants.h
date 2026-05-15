// MIT License

// Copyright (c) 2025 tommy2tonez

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __SORT_VARIANTS_H__
#define __SORT_VARIANTS_H__

#include <random>
#include <memory>
#include <functional>
#include <utility>
#include <algorithm>
#include "assert.h"
#include <type_traits>
#include <bit>
#include <climits>

namespace dg::sort_variants::quicksort{

    static inline constexpr size_t BLOCK_PIVOT_MAX_WALL_SZ          = 64u;
    static inline constexpr size_t MAX_RECURSION_DEPTH              = 64u; 
    static inline constexpr size_t COMPUTE_LEEWAY_MULTIPLIER        = 8u;

    using qs_unsigned_bitset_t = uint64_t;

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2(T val) noexcept -> T{

        return static_cast<T>(sizeof(T) * CHAR_BIT - 1u) - static_cast<T>(std::countl_zero(val));
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

    template <class _Ty>
    static inline void insertion_sort_1(_Ty * first, _Ty * last){

        size_t sz = std::distance(first, last);
        
        //0 1
    
        for (size_t i = 0u; i < sz; ++i){        
            for (size_t j = 0u; j < i; ++j){
                size_t rhs_idx  = i - j;
                size_t lhs_idx  = rhs_idx - 1u;
    
                if (first[rhs_idx] >= first[lhs_idx]){
                    break;
                }
    
                std::swap(first[rhs_idx], first[lhs_idx]);
            }
        }
    }

    template <class CallBack, class First, class Second, class ...Args>
    static void insertion_sort(const CallBack& callback, First first, Second second, Args ...args){
        
        if constexpr(sizeof...(Args) == 0){
            callback(std::min(first, second), std::max(first, second));
        } else{
            auto cb_lambda = [=]<class ...AArgs>(AArgs ...aargs){
                callback(std::min(first, second), aargs...);
            };

            insertion_sort(cb_lambda, std::max(first, second), args...);
        }
    } 

    template <class CallBack, class First, class ...Args>
    static void template_sort(const CallBack& callback, First first, Args ...args){

        if constexpr(sizeof...(Args) == 0){
            callback(first);
        } else{
            auto cb_lambda  = [=]<class ...AArgs>(AArgs ...aargs){
                insertion_sort(callback, first, aargs...);
            };

            template_sort(cb_lambda, args...);
        }
    }

    template <class _Ty, size_t SZ_Arg>
    static void template_sort_arr(_Ty * first, const std::integral_constant<size_t, SZ_Arg>&){

        auto sort_cb    = [=]<class ...Args>(Args ...args){
            
            auto fwd_tup        = std::make_tuple(args...);
            const auto idx_seq  = std::make_index_sequence<sizeof...(Args)>{};

            [=]<class Tup, size_t ...IDX>(Tup&& tup, const std::index_sequence<IDX...>&){
                ((first[IDX]  = std::get<IDX>(tup)), ...);
            }(fwd_tup, idx_seq);

        };

        const auto idx_seq    = std::make_index_sequence<SZ_Arg>{};

        [=]<size_t ...IDX>(const std::index_sequence<IDX...>&){
            template_sort(sort_cb, first[IDX]...);
        }(idx_seq);
    }

    //can we get an Amen?
    //this is incredibly hard to write

    template <class _Ty>
    static __attribute__((noinline)) void insertion_sort_2(_Ty * first, _Ty * last){

        size_t sz                           = std::distance(first, last);
        constexpr size_t SLIDING_WINDOW_SZ  = 5u;

        if (sz < SLIDING_WINDOW_SZ){
            insertion_sort_1(first, last);
            return;
        }

        template_sort_arr(first, std::integral_constant<size_t, SLIDING_WINDOW_SZ>{});

        for (size_t i = SLIDING_WINDOW_SZ; i < sz; ++i){
            intmax_t sorting_idx = i;

            while (true){
                if (sorting_idx == 0){
                    break;
                }

                if (first[sorting_idx] >= first[sorting_idx - 1]){
                    break;
                }

                sorting_idx = std::max(intmax_t{0}, static_cast<intmax_t>(sorting_idx - (SLIDING_WINDOW_SZ - 1)));
                template_sort_arr(std::next(first, sorting_idx), std::integral_constant<size_t, SLIDING_WINDOW_SZ>{});
            }
        }
    }

    static consteval auto boundless_insertion_sort_overflow_size() -> size_t{

        return 5u;
    }

    static consteval auto boundless_insertion_sort_max_sorting_size() -> size_t{

        return 7u;
    }

    template <class _Ty>
    static inline void boundless_insertion_sort(_Ty * first, _Ty * last){

        constexpr size_t SLIDING_WINDOW_SZ = 5u;
        size_t sz = std::distance(first, last);

        template_sort_arr(first, std::integral_constant<size_t, SLIDING_WINDOW_SZ>{});

        if (sz <= SLIDING_WINDOW_SZ){
            (void) sz;
        } else if (sz <= 7u){
            for (size_t i = SLIDING_WINDOW_SZ; i < 6u; ++i){
                intmax_t sorting_idx = i;

                while (true){
                    if (sorting_idx == 0 || first[sorting_idx] >= first[sorting_idx - 1]){
                        break;
                    }

                    sorting_idx = std::max(intmax_t{0}, static_cast<intmax_t>(sorting_idx - (SLIDING_WINDOW_SZ - 1)));
                    template_sort_arr(std::next(first, sorting_idx), std::integral_constant<size_t, SLIDING_WINDOW_SZ>{});
                }
            }

            if (sz == 7u){
                intmax_t sorting_idx = 6;
    
                while (true){
                    if (sorting_idx == 0 || first[sorting_idx] >= first[sorting_idx - 1]){
                        break;
                    }
    
                    sorting_idx = std::max(intmax_t{0}, static_cast<intmax_t>(sorting_idx - (SLIDING_WINDOW_SZ - 1)));
                    template_sort_arr(std::next(first, sorting_idx), std::integral_constant<size_t, SLIDING_WINDOW_SZ>{});
                }
            }
        } else{
            std::unreachable();
        }
    }

    template <class _Ty>
    static inline __attribute__((always_inline)) void dg_restrict_swap(_Ty * __restrict__ lhs, _Ty * __restrict__ rhs){

        std::swap(*lhs, *rhs);
    }

    template <class Ty>
    static auto extract_greatereq_from_left(Ty * first, const Ty& pivot, const size_t sz) -> qs_unsigned_bitset_t{

        qs_unsigned_bitset_t lhs_bitset = 0u; 

        for (size_t i = 0u; i < sz; ++i) [[likely]]{
            size_t reverse_idx  = (sz - 1) - i;
            lhs_bitset          |= static_cast<qs_unsigned_bitset_t>(first[i] >= pivot) << reverse_idx; //refill_sz, we'd want to get the relative position compared to the advance -refill_sz
        }

        return lhs_bitset;
    }

    template <class Ty>
    static auto extract_lesser_from_right(Ty * last, const Ty& pivot, const size_t sz) -> qs_unsigned_bitset_t{

        qs_unsigned_bitset_t rhs_bitset = 0u;

        for (size_t i = 0u; i < sz; ++i) [[likely]]{
            size_t reverse_idx  = sz - (i + 1); 
            rhs_bitset          |= static_cast<qs_unsigned_bitset_t>(last[-(static_cast<intmax_t>(i) + 1)] < pivot) << reverse_idx;
        }

        return rhs_bitset;
    }

    template <class T, size_t SZ>
    static consteval auto low_bits(const std::integral_constant<size_t, SZ>) -> T{

        static_assert(std::is_unsigned_v<T>);
        static_assert(SZ <= std::numeric_limits<T>::digits);

        if (SZ == std::numeric_limits<T>::digits){
            return std::numeric_limits<T>::max();
        } else{
            return (T{1} << SZ) - 1u;
        }
    } 

    template <class Ty, size_t SZ>
    static inline __attribute__((always_inline)) auto extract_greatereq_from_left(Ty * first, const Ty& pivot, const std::integral_constant<size_t, SZ>) -> qs_unsigned_bitset_t{

        qs_unsigned_bitset_t lhs_bitset = 0u; 

        [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
            (
                [&]{
                    (void) IDX;
                    constexpr size_t reverse_idx = (SZ - 1) - IDX;
                    lhs_bitset |= static_cast<qs_unsigned_bitset_t>(first[IDX] < pivot) << reverse_idx; //less is better, due to the population of the operation, no one ever does >
                }(), ...
            );
        }(std::make_index_sequence<SZ>{});

        return (~lhs_bitset) & low_bits<qs_unsigned_bitset_t>(std::integral_constant<size_t, SZ>{});
    }

    template <class Ty, size_t SZ>
    static inline __attribute__((always_inline)) auto extract_lesser_from_right(Ty * last, const Ty& pivot, const std::integral_constant<size_t, SZ>) -> qs_unsigned_bitset_t{

        qs_unsigned_bitset_t rhs_bitset = 0u;
        Ty * first                      = std::prev(last, SZ);

        [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
            (
                [&]{
                    (void) IDX;
                    rhs_bitset |= static_cast<qs_unsigned_bitset_t>(first[IDX] < pivot) << IDX; //refill_sz, we'd want to get the relative position compared to the advance -refill_sz        
                }(), ...
            );
        }(std::make_index_sequence<SZ>{});

        return rhs_bitset;
    }

    template <class _Ty>
    static inline auto pivot_partition(_Ty * first, _Ty * last, _Ty * pivot) -> _Ty *{

        std::swap(*std::prev(last), *pivot);

        const _Ty& pivot_value          = *std::prev(last);
        _Ty * ffirst                    = first;
        _Ty * llast                     = std::prev(last);

        qs_unsigned_bitset_t lhs_bitset = 0u;
        qs_unsigned_bitset_t rhs_bitset = 0u;

        while (true){
            while (lhs_bitset != 0u && rhs_bitset != 0u){
                size_t lhs_back_idx     = std::countr_zero(lhs_bitset) + 1u;
                size_t rhs_forward_idx  = std::countr_zero(rhs_bitset);

                lhs_bitset              &= lhs_bitset - 1u;
                rhs_bitset              &= rhs_bitset - 1u;

                dg_restrict_swap(std::prev(ffirst, lhs_back_idx), std::next(llast, rhs_forward_idx));
            }

            if (std::distance(ffirst, llast) < BLOCK_PIVOT_MAX_WALL_SZ){
                break;
            }

            if (lhs_bitset == 0u){
                lhs_bitset = extract_greatereq_from_left(ffirst, pivot_value, std::integral_constant<size_t, BLOCK_PIVOT_MAX_WALL_SZ>{});
                std::advance(ffirst, BLOCK_PIVOT_MAX_WALL_SZ);
            } else{
                rhs_bitset = extract_lesser_from_right(llast, pivot_value, std::integral_constant<size_t, BLOCK_PIVOT_MAX_WALL_SZ>{});
                std::advance(llast, -static_cast<intmax_t>(BLOCK_PIVOT_MAX_WALL_SZ));
            }
        }

        size_t after_sz = std::distance(ffirst, llast);

        //lhs_bitset == 0u or rhs_bitset == 0u
        if (lhs_bitset == 0u){
            lhs_bitset      = extract_greatereq_from_left(ffirst, pivot_value, after_sz);                
            std::advance(ffirst, after_sz);
        } else{
            rhs_bitset      = extract_lesser_from_right(llast, pivot_value, after_sz);
            std::advance(llast, -static_cast<intmax_t>(after_sz));
        }

        while (lhs_bitset != 0u && rhs_bitset != 0u){
            size_t lhs_back_idx     = std::countr_zero(lhs_bitset) + 1u;
            size_t rhs_forward_idx  = std::countr_zero(rhs_bitset);
            lhs_bitset              &= lhs_bitset - 1u;
            rhs_bitset              &= rhs_bitset - 1u;

            dg_restrict_swap(std::prev(ffirst, lhs_back_idx), std::next(llast, rhs_forward_idx));
        }

        //we'll do another optimization here
        //three cases
        //lhs_bitset == 0u (rhs_bitset != 0u)
        //lhs_bitset == 0u (lhs_bitset != 0u)
        //lhs_bitset == 0u && rhs_bitset == 0u (other)

        if (lhs_bitset != 0u){
            _Ty * back_ptr = std::prev(llast, 1u); //we are doing greater partition, since all the llast forward are greater + eq, and all the prev_ffirst backward are lesser, the only partition to be considered is what's left in the lhs_bitset  

            while (lhs_bitset != 0u){
                size_t back_idx = std::countr_zero(lhs_bitset);
                lhs_bitset      &= lhs_bitset - 1u;
                std::iter_swap(back_ptr, std::prev(ffirst, back_idx + 1u));
                std::advance(back_ptr, -1);
            }

            std::iter_swap(std::next(back_ptr), std::prev(last));
            return std::next(back_ptr);
        }

        if (rhs_bitset != 0u){
            _Ty * front_ptr = ffirst; //we are doing lesser partition, ...

            while (rhs_bitset != 0u){
                size_t front_idx    = std::countr_zero(rhs_bitset);
                rhs_bitset          &= rhs_bitset - 1u;
                std::iter_swap(front_ptr, std::next(llast, front_idx));
                std::advance(front_ptr, 1u);
            }

            std::iter_swap(front_ptr, std::prev(last));
            return front_ptr;
        }

        std::iter_swap(llast, std::prev(last)); //first == last, according to our logic, all the last including itself forward (up to the std::prev(last)) are greater or equal, if it is std::prev(last) then it is definitely equal, we can safely do an iter_swap
        return llast;
    }

    template <class _Ty>
    static auto base_quicksort(_Ty * first, _Ty * last, _Ty * incl_last_boundlessable,  
                               uint64_t flops, uint64_t max_flops, uint32_t stack_idx) -> uint64_t{

        size_t sz = std::distance(first, last);

        if (sz <= boundless_insertion_sort_max_sorting_size()){
            intmax_t boundless_distance = std::distance(last, incl_last_boundlessable); 

            if (boundless_distance >= 0) [[likely]]{
                boundless_insertion_sort(first, last);
            } else [[unlikely]]{
                insertion_sort_2(first, last);
            }

            return sz * sz;
        }

        if (flops > max_flops || stack_idx >= MAX_RECURSION_DEPTH) [[unlikely]]{
            std::sort(first, last);
            return sz * ulog2(ceil2(sz));
        } else [[likely]]{
            static_assert(boundless_insertion_sort_max_sorting_size() >= 4);

            constexpr size_t PIVOT_CAND_SZ  = 3u;
            size_t mid_idx                  = sz >> 1;
            uint64_t incurred_cost          = 0u;

            _Ty * mid_ptr                   = std::next(first, mid_idx);
            _Ty * previous_mid_ptr          = std::prev(mid_ptr);

            template_sort_arr(previous_mid_ptr, std::integral_constant<size_t, PIVOT_CAND_SZ>{});

            _Ty * pivot_ptr                 = pivot_partition(first, last, mid_ptr);    

            incurred_cost                   += sz;
            incurred_cost                   += base_quicksort(first, pivot_ptr, incl_last_boundlessable, flops + incurred_cost, max_flops, stack_idx + 1u);
            incurred_cost                   += base_quicksort(std::next(pivot_ptr), last, incl_last_boundlessable, flops + incurred_cost, max_flops, stack_idx + 1u);

            return incurred_cost;    
        }
    }

    template <class _Ty>
    __attribute__((noinline)) void quicksort(_Ty * first, _Ty * last) noexcept{
        
        static_assert(std::is_arithmetic_v<_Ty>);

        size_t sz = std::distance(first, last);

        if (sz < boundless_insertion_sort_overflow_size()){
            insertion_sort_1(first, last);
            return;
        }

        size_t compute_sz               = sz * ulog2(ceil2(sz)) * COMPUTE_LEEWAY_MULTIPLIER;
        _Ty * incl_last_boundlessable   = std::prev(last, boundless_insertion_sort_overflow_size());

        base_quicksort(first, last, incl_last_boundlessable, 0u, compute_sz, 0u);
    }
} 

#endif