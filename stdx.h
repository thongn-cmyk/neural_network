//HEADER_CONTROL 0

#ifndef __STDX_H__
#define __STDX_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include <cmath>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <random>
#include <algorithm>
#include <chrono>
#include <array>
#include <type_traits>
#include <numbers>
#include <bit>
#include "fair_mutex.h"

namespace stdx
{
    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2(T val) noexcept -> size_t
    {
        return static_cast<size_t>(sizeof(T) * CHAR_BIT - 1u) - static_cast<size_t>(std::countl_zero(val));
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ceil2(T val) noexcept -> T
    {
        if (val < 2u) [[unlikely]]{
            return 1u;
        } else [[likely]]{
            T uplog_value = ulog2(static_cast<T>(val - 1u)) + 1u;
            return T{1u} << uplog_value;
        }
    }

    constexpr auto mul_ceil(size_t value, size_t multiplier) -> size_t
    {
        if (value == 0u)
        {
            return 0u;
        }

        if (multiplier == 0u)
        {
            throw std::invalid_argument("bad multiplier, 0");
        }

        size_t demoted_slot     = (value - 1u) / multiplier;
        size_t promoted_slot    = demoted_slot + 1u;

        return promoted_slot * multiplier;
    }

    class UnsignedInitializer
    {
        private:

            uint64_t value;

        public:

            template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
            constexpr UnsignedInitializer(T value)
            {
                if constexpr(std::is_unsigned_v<T>)
                {
                    if (value > std::numeric_limits<uint64_t>::max())
                    {
                        throw std::runtime_error("bad unsigned->unsigned conversion, max value reached");
                    }

                    this->value = value;
                }
                else
                {
                    if (value < 0)
                    {
                        throw std::runtime_error("bad signed-unsigned conversion, < 0");
                    }

                    if (value > std::numeric_limits<uint64_t>::max())
                    {
                        throw std::runtime_error("bad signed-unsigned conversion, max value reached");
                    }

                    this->value = value;
                }
            }

            auto get_value() -> uint64_t
            {
                return this->value;
            }
    };

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto is_pow2(T value) -> bool
    {
        if (value == 0u)
        {
            return false;
        }

        T value_one = value - 1;

        return (value & value_one) == 0u;
    }

    constexpr auto safe_non_zero_access(size_t sz) -> size_t
    {
        if (sz == 0u)
        {
            throw std::runtime_error("zero guard");
        }

        return sz;
    }

    template <class FloatType>
    constexpr void safe_float_range_access(const FloatType * float_arr, size_t float_arr_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < float_arr_sz; ++i)
        {
            if (std::isnan(float_arr[i]))
            {
                throw std::runtime_error("invalid float range, NaN");
            }
        }
    }

    template <class FloatType>
    constexpr void safe_float_access(const FloatType& val)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (std::isnan(val))
        {
            throw std::runtime_error("invalid float, NaN");
        }
    }

    template <class FloatType>
    constexpr void xsafe_float_range_access(const FloatType * float_arr, size_t float_arr_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < float_arr_sz; ++i)
        {
            if (std::isnan(float_arr[i]))
            {
                throw std::runtime_error("invalid float range, NaN");
            }

            if (std::isinf(float_arr[i]))
            {
                throw std::runtime_error("invalid float range, inf");
            }
        }
    }

    template <class FloatType>
    constexpr void xsafe_float_access(const FloatType& val)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (std::isnan(val))
        {
            throw std::runtime_error("invalid float range, NaN");
        }

        if (std::isinf(val))
        {
            throw std::runtime_error("invalid float range, inf");
        }
    }

    constexpr auto access_guard(size_t idx, size_t bound_sz) -> size_t
    {
        if (idx >= bound_sz)
        {
            throw std::runtime_error("out of bound access");
        }

        return idx;
    }

    template <class FloatType>
    constexpr auto float_clamp(FloatType arg, FloatType min_value, FloatType max_value) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (std::isnan(arg))
        {
            return min_value;
        }

        return std::clamp(arg, min_value, max_value);
    }

    template <class FloatType>
    constexpr auto nan_cmp(FloatType lhs, FloatType rhs) -> int
    {
        static_assert(std::is_floating_point_v<FloatType>);

        bool lhs_prefix = std::isnan(lhs);
        bool rhs_prefix = std::isnan(rhs);

        if (lhs_prefix < rhs_prefix)
        {
            return -1;
        }

        if (lhs_prefix > rhs_prefix)
        {
            return 1;
        }

        //we mess this up very easily, the cmp function is very fragile if we dont define the definition correctly
        //in the case of topological cmp, we care if the first <> other_first, second <> other_second etc.
        //but in the case of float, we'd have to white out the latter numerical values if the prefix is not a number
        //so in this case, if we detect that specific case of comparison, such is nan <-> nan, we'd return 0

        if (lhs_prefix == 1)
        {
            return 0;
        }

        if (lhs < rhs)
        {
            return -1;
        }

        if (lhs > rhs)
        {
            return 1;
        }

        return 0;
    }

    class GenericNaN
    {
        public:

            constexpr GenericNaN() noexcept = default;

            template <class FloatType, std::enable_if_t<std::is_floating_point_v<FloatType>, bool> = true>
            constexpr operator FloatType() const noexcept
            {
                return std::numeric_limits<FloatType>::quiet_NaN();
            }
    };

    consteval auto generic_nan() -> GenericNaN
    {
        return {};
    }

    // template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, float>, bool> = true>
    // consteval auto generic_nan() -> FloatType
    // {
    //     return std::numeric_limits<FloatType>::quiet_NaN();
    // }

    // template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, double>, bool> = true>
    // consteval auto generic_nan() -> FloatType
    // {
    //     return std::numeric_limits<FloatType>::quiet_NaN();
    // }

    // template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, long double>, bool> = true>
    // consteval auto generic_nan() -> FloatType
    // {
    //     return std::numeric_limits<FloatType>::quiet_NaN();
    // }

    template <class FloatType>
    constexpr auto deviation_clamp(FloatType x, FloatType a) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (std::isnan(a))
        {
            return stdx::generic_nan();
        }

        if (a < 0)
        {
            throw std::invalid_argument("bad deviation, negative deviation");
        }

        if (std::isnan(x))
        {
            return stdx::generic_nan();
        }

        return std::copysign(std::min(std::abs(x), a), x);
    }

    template <class T>
    struct Tag{};

    struct NormalSizeContainer
    {
        size_t value;

        constexpr NormalSizeContainer() noexcept = default;
        constexpr NormalSizeContainer(size_t value): value(value){}

        constexpr auto get() const noexcept -> size_t
        {
            return this->value;
        }
    };

    template <size_t SZ>
    struct IntegralSizeContainer
    {
        constexpr IntegralSizeContainer() = default;
        constexpr IntegralSizeContainer(std::integral_constant<size_t, SZ>) {}

        consteval auto get() -> size_t
        {
            return SZ;
        }
    };

    constexpr auto minus_one(const NormalSizeContainer& sz) -> NormalSizeContainer
    {
        return NormalSizeContainer(sz.get() - 1u);
    }

    template <size_t SZ>
    constexpr auto minus_one(const IntegralSizeContainer<SZ>& sz) -> IntegralSizeContainer<SZ - 1u>
    {
        return {};
    }

    constexpr auto safe_minus_one(const NormalSizeContainer& sz) -> NormalSizeContainer
    {
        if (sz.get() == 0u)
        {
            return NormalSizeContainer(0u);
        }

        return NormalSizeContainer(sz.get() - 1u);
    }

    template <size_t SZ>
    constexpr auto safe_minus_one(const IntegralSizeContainer<SZ>& sz)
    {
        if constexpr(SZ == 0u)
        {
            return IntegralSizeContainer<SZ>{};
        }
        else
        {
            return IntegralSizeContainer<SZ - 1u>{};
        }
    }

    constexpr auto to_size_container(size_t sz) -> NormalSizeContainer
    {
        return {sz};
    }

    template <size_t SZ>
    constexpr auto to_size_container(const std::integral_constant<size_t, SZ>) -> IntegralSizeContainer<SZ>
    {
        return {};
    }

    template <class T1>
    class ArrayDefaultInitializer
    {
        private:

            T1 value;

        public:

            constexpr ArrayDefaultInitializer(T1 value): value(std::move(value)){}

            template <class T, size_t SZ>
            constexpr operator std::array<T, SZ>() const
            {
                std::array<T, SZ> rs{};
                std::fill(rs.begin(), rs.end(), this->value);

                return rs;
            }
    };

    template <class ...Args>
    class CastableVectorInitializer
    {
        private:

            std::vector<Args...> value;
        
        public:

            constexpr CastableVectorInitializer(std::vector<Args...> value): value(std::move(value)){}

            template <class ...Args1>
            constexpr operator std::vector<Args1...>()
            {
                if constexpr(std::is_same_v<std::vector<Args...>, std::vector<Args1...>>)
                {
                    return std::vector<Args1...>(std::move(this->value));
                }
                else
                {
                    return std::vector<Args1...>(std::make_move_iterator(this->value.begin()), std::make_move_iterator(this->value.end()));
                }
            }
    };

    template <class ...Args>
    class EmplaceVectorInitializer
    {
        private:

            std::tuple<Args...> value;
        
        public:

            constexpr EmplaceVectorInitializer(std::tuple<Args...> value): value(std::move(value)){}

            template <class ...Args1>
            constexpr operator std::vector<Args1...>()
            {
                std::vector<Args1...> rs{};
                
                [&]<size_t... IDX>(const std::index_sequence<IDX...>)
                {
                    (
                        [&]
                        {
                            rs.emplace_back(std::move(std::get<IDX>(this->value)));
                        }(), ...
                    );
                }(std::make_index_sequence<sizeof...(Args)>());

                return rs;
            };
    };

    template <class FloatType>
    class PreciseFloatConversionContainer
    {
        private:
    
            FloatType value;

        public:

            static_assert(std::is_floating_point_v<FloatType>);

            constexpr PreciseFloatConversionContainer() = default;
            constexpr PreciseFloatConversionContainer(FloatType value): value(std::move(value)){}

            template <class OtherFloatType, std::enable_if_t<std::is_floating_point_v<OtherFloatType>, bool> = true>
            constexpr operator OtherFloatType() const
            {
                if (std::isnan(this->value))
                {
                    return stdx::generic_nan();
                }

                OtherFloatType casted_value = static_cast<OtherFloatType>(this->value);
                FloatType org_value         = static_cast<FloatType>(casted_value);

                if (org_value != this->value) [[unlikely]]
                {
                    throw std::runtime_error("float precision lost");
                }
                else [[likely]]
                {
                    return casted_value;
                }
            }
    };

    template <class ...Args>
    constexpr auto to_castable_vector_initializer(std::vector<Args...> value) -> CastableVectorInitializer<Args...>
    {
        return CastableVectorInitializer<Args...>(std::move(value));
    }

    template <class ...Args>
    constexpr auto to_variadic_vector_initializer(Args... args) -> EmplaceVectorInitializer<Args...>
    {
        std::tuple<Args...> rs(std::move(args)...);

        return EmplaceVectorInitializer<Args...>(std::move(rs));
    }

    template <class T>
    constexpr auto to_array_default_initializer(T default_value) -> ArrayDefaultInitializer<T>
    {
        return ArrayDefaultInitializer<T>(std::move(default_value));
    }

    template <class FloatType>
    constexpr auto to_precise_float_conversion_initializer(FloatType value) -> PreciseFloatConversionContainer<FloatType>
    {
        return PreciseFloatConversionContainer<FloatType>(std::move(value));
    }

    template <class T, class Allocator>
    constexpr auto enumerate_vector(const std::vector<T, Allocator>& arg) -> std::vector<std::pair<size_t, T>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<size_t, T>>>
    {
        std::vector<std::pair<size_t, T>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<size_t, T>>> rs(arg.size());

        for (size_t i = 0u; i < arg.size(); ++i)
        {
            rs[i] = {i, arg[i]};
        }

        return rs;
    }

    template <class T, class Allocator>
    constexpr auto deenumerate_vector(const std::vector<std::pair<size_t, T>, Allocator>& arg) -> std::vector<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>
    {
        if (arg.empty())
        {
            return {};
        }

        size_t max_idx  = std::max_element(arg.begin(), arg.end(), [](const auto& lhs, const auto& rhs){return std::get<0>(lhs) < std::get<0>(rhs);})->first;
        size_t sz       = max_idx + 1;
        auto rs         = std::vector<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>(sz);

        for (const auto& [idx, e]: arg)
        {
            rs[idx] = e;
        }

        return rs;
    }

    template <class T, class Allocator>
    constexpr auto split_vector(const std::vector<T, Allocator>& arg, size_t chunk_sz) -> std::vector<std::vector<T, Allocator>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::vector<T, Allocator>>>
    {
        size_t chunk_count = arg.size() / chunk_sz + static_cast<size_t>(arg.size() % chunk_sz != 0u);
        std::vector<std::vector<T, Allocator>, typename std::allocator_traits<Allocator>::template rebind_alloc<std::vector<T, Allocator>>> rs(chunk_count);

        for (size_t i = 0u; i < chunk_count; ++i)
        {
            size_t first    = chunk_sz * i;
            size_t last     = std::min(static_cast<size_t>(arg.size()), static_cast<size_t>(chunk_sz * (i + 1)));
            rs[i]           = std::vector<T, Allocator>(std::next(arg.begin(), first), std::next(arg.begin(), last));
        }

        return rs;
    }

    template <class T, class Allocator1, class Allocator2>
    constexpr auto unsplit_vector(const std::vector<std::vector<T, Allocator2>, Allocator1>& arg) -> std::vector<T, Allocator2>
    {
        std::vector<T, Allocator2> rs{};

        for (const auto& e: arg)
        {
            std::copy(e.begin(), e.end(), std::back_inserter(rs));
        }

        return rs;
    }

    template <class ...Args, class DefaultAsGenerator>
    constexpr auto copy_and_trail_defaultize(const std::vector<Args...>& arg,
                                             double perc,
                                             DefaultAsGenerator&& gen) -> std::vector<Args...>
    {
        size_t tentative_deparam_sz = arg.size() * perc;
        size_t deparam_sz           = std::clamp(tentative_deparam_sz, size_t{0u}, arg.size());
        size_t active_sz            = arg.size() - deparam_sz;

        std::vector<Args...> result = {};

        for (size_t i = 0u; i < active_sz; ++i)
        {
            result.push_back(arg[i]);
        }

        for (size_t i = 0u; i < deparam_sz; ++i)
        {
            result.push_back(gen(arg[i + active_sz]));
        }

        return result;
    }

    template <class T, size_t ARR_SZ, class DefaultAsGenerator>
    constexpr auto copy_and_trail_defaultize(const std::array<T, ARR_SZ>& arg,
                                             double perc,
                                             DefaultAsGenerator&& gen) -> std::array<T, ARR_SZ>
    {
        size_t tentative_deparam_sz     = arg.size() * perc;
        size_t deparam_sz               = std::clamp(tentative_deparam_sz, size_t{0u}, arg.size());
        size_t active_sz                = arg.size() - deparam_sz;

        std::array<T, ARR_SZ> result    = {};

        for (size_t i = 0u; i < active_sz; ++i)
        {
            result[i] = arg[i];
        }

        for (size_t i = 0u; i < deparam_sz; ++i)
        {
            result[active_sz + i] = gen(arg[i + active_sz]);
        }

        return result;
    }

    template <class T>
    constexpr auto safe_ptr_access(T * ptr) -> T *
    {
        if (ptr == nullptr)
        {
            throw std::runtime_error("invalid pointer, null pointer");
        }

        return ptr;
    }

    template <class T, class T1>
    constexpr auto zip(const std::vector<T>& lhs, const std::vector<T1>& rhs) -> std::vector<std::pair<T, T1>>
    {
        if (lhs.size() != rhs.size())
        {
            throw std::invalid_argument("incompatible zip, mismatched vector size");
        }

        std::vector<std::pair<T, T1>> result_vec{};

        for (size_t i = 0u; i < lhs.size(); ++i)
        {
            result_vec.push_back(std::make_pair(lhs[i], rhs[i]));
        }

        return result_vec;
    }

    template <class T, class Allocator = std::allocator<char>>
    using transparent_vector = std::vector<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

    template <class Key, class Value, class Allocator = std::allocator<char>, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using transparent_unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual, typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<const Key, Value>>>;

    template <class Allocator = std::allocator<char>>
    using transparent_string = std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<Allocator>::template rebind_alloc<char>>;

    template <class T, class Signature = void>
    class singleton_container
    {
        private:

            static inline T value{};

        public:

            static inline auto get() noexcept -> T&
            {
                return value;
            }
    };

    template <class T, class Signature = void>
    class thread_safe_singleton_container
    {
        private:

            static inline T value{};
            static inline std::unique_ptr<fair_mutex::fair_atomic_flag> mtx = fair_mutex::make_unique_fair_atomic_flag();

        public:

            template <class Accessor>
            static inline void access(Accessor&& accessor)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*mtx);
                accessor(value);
            }
    };
}

#endif