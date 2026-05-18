#ifndef __ALGORITHM_EXTENSION_SHORT_HEAP_H__
#define __ALGORITHM_EXTENSION_SHORT_HEAP_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <algorithm>
#include <utility>
#include <iterator>
#include <memory>
#include <type_traits>
#include <numeric>
#include <cstdlib>

namespace algorithm_extension
{
    static inline constexpr size_t DEFAULT_OUT_DEGREE_SZ = 8u;

    struct GreaterEqualCmp
    {
        template <class Lhs, class Rhs>
        constexpr auto operator()(Lhs&& lhs, Rhs&& rhs) const noexcept(noexcept(lhs >= rhs)) -> bool
        {
            return lhs >= rhs;
        }
    };

    struct LessEqualCmp
    {
        template <class Lhs, class Rhs>
        constexpr auto operator()(Lhs&& lhs, Rhs&& rhs) const noexcept(noexcept(lhs >= rhs)) -> bool
        {
            return lhs <= rhs;
        }
    };

    template <class RandomIt, class Comparator, size_t OUT_DEGREE_SZ>
    static auto _is_heap(RandomIt heap_arr,
                         size_t idx, size_t heap_arr_sz,
                         Comparator&& is_better_cmp,
                         const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz)
    {
        static_assert(OUT_DEGREE_SZ != 0u);

        if (idx >= heap_arr_sz) [[unlikely]]
        {
            std::abort();
        }

        for (size_t i = 0u; i < OUT_DEGREE_SZ; ++i)
        {
            size_t child_idx = idx * OUT_DEGREE_SZ + (i + 1);

            if (child_idx >= heap_arr_sz)
            {
                break;
            }

            if (!is_better_cmp(heap_arr[idx], heap_arr[child_idx]))
            {
                return false;
            }
        }

        for (size_t i = 0u; i < OUT_DEGREE_SZ; ++i)
        {
            size_t child_idx = idx * OUT_DEGREE_SZ + (i + 1);

            if (child_idx >= heap_arr_sz)
            {
                break;
            }

            if (!_is_heap(heap_arr, child_idx, heap_arr_sz, is_better_cmp, out_degree_sz))
            {
                return false;
            }
        }

        return true;
    }

    template <class RandomIt, class Comparator = LessEqualCmp, size_t OUT_DEGREE_SZ = DEFAULT_OUT_DEGREE_SZ>
    auto is_heap(RandomIt first, RandomIt last, Comparator&& is_better_cmp = Comparator{},
                 const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz = std::integral_constant<size_t, OUT_DEGREE_SZ>{}) -> bool
    {
        intmax_t sz = std::distance(first, last);

        if (sz < 0) [[unlikely]]
        {
            std::abort();
        }

        if (sz == 0)
        {
            return true;
        }

        return _is_heap(first, 0u, sz, is_better_cmp, out_degree_sz);
    }

    template <class RandomIt, class Comparator, size_t OUT_DEGREE_SZ>
    static void _push_down_heap(RandomIt heap_arr,
                                size_t idx, size_t heap_arr_sz,
                                Comparator&& is_better_cmp,
                                const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz)
    {
        static_assert(OUT_DEGREE_SZ != 0u);

        if (idx >= heap_arr_sz) [[unlikely]]
        {
            std::abort();
        }

        size_t first_child_idx = idx * OUT_DEGREE_SZ + 1;

        if (first_child_idx >= heap_arr_sz)
        {
            return;
        }

        size_t cand = first_child_idx;

        for (size_t i = 1u; i < OUT_DEGREE_SZ; ++i)
        {
            size_t child_idx = idx * OUT_DEGREE_SZ + (i + 1);

            if (child_idx >= heap_arr_sz)
            {
                break;
            }

            if (is_better_cmp(heap_arr[child_idx], heap_arr[cand]))
            {
                cand = child_idx;
            }
        }

        if (is_better_cmp(heap_arr[idx], heap_arr[cand]))
        {
            return;
        }

        std::swap(heap_arr[idx], heap_arr[cand]);
        _push_down_heap(heap_arr, cand, heap_arr_sz, is_better_cmp, out_degree_sz);
    }

    template <class RandomIt, class Comparator, size_t OUT_DEGREE_SZ>
    static __attribute__((noinline)) void _push_down_heap_noinline(RandomIt heap_arr,
                                                                   size_t idx, size_t heap_arr_sz,
                                                                   Comparator&& is_better_cmp,
                                                                   const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz)
    {
        _push_down_heap(heap_arr,
                        idx, heap_arr_sz,
                        is_better_cmp,
                        out_degree_sz);
    }

    template <class RandomIt, class Comparator, size_t OUT_DEGREE_SZ>
    static void _fast_push_down_heap(RandomIt heap_arr,
                                     size_t idx, size_t heap_arr_sz,
                                     Comparator&& is_better_cmp,
                                     const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz)
    {
        static_assert(OUT_DEGREE_SZ != 0u);

        if (idx >= heap_arr_sz) [[unlikely]]
        {
            std::abort();
        }

        size_t last_child_idx   = idx * OUT_DEGREE_SZ + OUT_DEGREE_SZ;

        if (last_child_idx >= heap_arr_sz) [[unlikely]]
        {
            _push_down_heap_noinline(heap_arr, idx, heap_arr_sz, is_better_cmp, out_degree_sz);
            return;
        }
        else [[likely]]
        {
            size_t cand = idx * OUT_DEGREE_SZ + 1;

            for (size_t i = 1u; i < OUT_DEGREE_SZ; ++i)
            {
                size_t child_idx = idx * OUT_DEGREE_SZ + (i + 1);

                //I was thinking about sorting this with value extraction, but that's another topic to talk about, it all narrows to a simple 1 array sort and bit extraction and cmp if == root
                //keep in mind that we are providing all the things we could to the compiler to narrow the optimization heuristics
                //thing is branch is cheap but mem fetch is not on multi-core system. So we must increase outdegree here

                if (is_better_cmp(heap_arr[child_idx], heap_arr[cand]))
                {
                    cand = child_idx;
                }
            }

            if (is_better_cmp(heap_arr[idx], heap_arr[cand]))
            {
                return;
            }

            std::swap(heap_arr[idx], heap_arr[cand]);
            _fast_push_down_heap(heap_arr, cand, heap_arr_sz, is_better_cmp, out_degree_sz);
        }
    }

    template <class RandomIt, class Comparator, size_t OUT_DEGREE_SZ>
    static void _push_up_heap(RandomIt heap_arr,
                              size_t idx, size_t heap_arr_sz,
                              Comparator&& is_better_cmp,
                              const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz)
    {
        static_assert(OUT_DEGREE_SZ != 0u);

        if (idx >= heap_arr_sz) [[unlikely]]
        {
            std::abort();
        }

        if (idx == 0u)
        {
            return;
        }

        size_t parent_idx = (idx - 1u) / OUT_DEGREE_SZ;

        if (is_better_cmp(heap_arr[parent_idx], heap_arr[idx]))
        {
            return;
        }

        std::swap(heap_arr[parent_idx], heap_arr[idx]);
        _push_up_heap(heap_arr, parent_idx, heap_arr_sz, is_better_cmp, out_degree_sz);
    }

    template <class RandomIt, class Comparator = LessEqualCmp, size_t OUT_DEGREE_SZ = DEFAULT_OUT_DEGREE_SZ>
    void make_heap(RandomIt first, RandomIt last, Comparator&& is_better_cmp = Comparator{},
                   const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz = std::integral_constant<size_t, OUT_DEGREE_SZ>{})
    {
        static_assert(OUT_DEGREE_SZ != 0u);

        intmax_t sz = std::distance(first, last);
        
        if (sz < 0) [[unlikely]]
        {
            std::abort();
        }

        if (sz <= 1)
        {
            return;
        }

        size_t back_idx         = sz - 1;
        size_t reference_idx    = (back_idx - 1) / OUT_DEGREE_SZ;
        size_t root_sz          = reference_idx + 1u;

        for (size_t i = 0u; i < root_sz; ++i)
        {
            size_t root_idx = reference_idx - i;
            _push_down_heap(first, root_idx, sz, is_better_cmp, out_degree_sz);
        }
    }

    template <class RandomIt, class Comparator = LessEqualCmp, size_t OUT_DEGREE_SZ = DEFAULT_OUT_DEGREE_SZ>
    void pop_heap(RandomIt first, RandomIt last, Comparator&& is_better_cmp = Comparator{},
                  const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz = std::integral_constant<size_t, OUT_DEGREE_SZ>{})
    {
        intmax_t sz = std::distance(first, last);

        if (sz <= 0) [[unlikely]]
        {
            std::abort();
        }

        std::swap(first[0], first[sz - 1]);
        intmax_t new_sz = sz - 1;

        if (new_sz > 0)
        {
            _push_down_heap(first, 0u, new_sz, is_better_cmp, out_degree_sz);
        }
    }

    template <class RandomIt, class Comparator = LessEqualCmp, size_t OUT_DEGREE_SZ = DEFAULT_OUT_DEGREE_SZ>
    void push_heap(RandomIt first, RandomIt last, Comparator&& is_better_cmp = Comparator{},
                   const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz = std::integral_constant<size_t, OUT_DEGREE_SZ>{})
    {
        intmax_t sz = std::distance(first, last);

        if (sz <= 0) [[unlikely]]
        {
            std::abort();
        }

        _push_up_heap(first, sz - 1, sz, is_better_cmp, out_degree_sz);
    }

    template <class RandomIt, class Comparator = LessEqualCmp, size_t OUT_DEGREE_SZ = DEFAULT_OUT_DEGREE_SZ>
    void sort_heap(RandomIt first, RandomIt last, Comparator&& is_better_cmp = Comparator{},
                   const std::integral_constant<size_t, OUT_DEGREE_SZ> out_degree_sz = std::integral_constant<size_t, OUT_DEGREE_SZ>{})
    {
        intmax_t sz = std::distance(first, last);

        if (sz < 0) [[unlikely]]
        {
            std::abort();
        }

        for (size_t i = 0u; i < sz; ++i)
        {
            pop_heap(first, std::prev(last, i), is_better_cmp, out_degree_sz);
        }
    }
}

#endif