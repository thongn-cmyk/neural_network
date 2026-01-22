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
#include <iostream>

static inline constexpr size_t MAX_BASE_COEFFICIENT = 20u;

constexpr auto simple_factorial(size_t n) -> size_t
{
    if (n == 0u)
    {
        return 1u;
    }

    return simple_factorial(n - 1u) * n;
}

constexpr auto factorial(size_t n) -> size_t
{
    constexpr size_t FACTORIAL_CAP = 20u;

    static std::vector<size_t> factorial_table([=]{
        std::vector<size_t> tmp_table{};

        for (size_t i = 0u; i < FACTORIAL_CAP; ++i)
        {
            tmp_table.push_back(simple_factorial(i));
        }

        return tmp_table;
    }());

    if (n >= FACTORIAL_CAP)
    {
        throw std::runtime_error("invalid argument");
    }

    return factorial_table[n];
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
    if (std::isnan(arg))
    {
        return min_value;
    }

    return std::clamp(arg, min_value, max_value);
}

template <class FloatType>
constexpr auto nan_cmp(FloatType lhs, FloatType rhs) -> int
{
    if (std::isnan(lhs))
    {
        if (std::isnan(rhs))
        {
            return 0;
        }

        return 1;
    }

    if (std::isnan(rhs))
    {
        return -1;
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

template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, float>, bool> = true>
consteval auto generic_nan() -> FloatType
{
    return std::numeric_limits<FloatType>::quiet_NaN();
}

template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, double>, bool> = true>
consteval auto generic_nan() -> FloatType
{
    return std::numeric_limits<FloatType>::quiet_NaN();
}

template <class FloatType, std::enable_if_t<std::is_same_v<FloatType, long double>, bool> = true>
consteval auto generic_nan() -> FloatType
{
    return std::numeric_limits<FloatType>::quiet_NaN();
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

// ------

template <class T, class ArrSizeType>
constexpr void restrict_scalar_mul_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                         T c,
                                         T * __restrict__ output_arr)
{
    for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
    {
        output_arr[i] = arg_arr[i] * c;
    }
}

template <class T, class ArrSizeType>
constexpr void restrict_scalar_div_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                         T c,
                                         T * __restrict__ output_arr)
{
    for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
    {
        output_arr[i] = arg_arr[i] / c;
    }
}

template <class T, class ArrSizeType>
constexpr void restrict_add_array(const T * __restrict__ lhs_arr, const T * __restrict__ rhs_arr, ArrSizeType arr_sz,
                                  T * __restrict__ output_arr)
{
    for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
    {
        output_arr[i] = lhs_arr[i] + rhs_arr[i];
    }
}

template <class FloatType, class ArrSizeType>
constexpr void restrict_multidimensional_oval_to_euclidean_array(const FloatType * __restrict__ radian_arr, ArrSizeType radian_space_sz,
                                                                 const FloatType * __restrict__ radius_arr,
                                                                 FloatType  * __restrict__ output_arr)
{
    FloatType carry_multiplier = 1.f;

    for (size_t i = 0u; i < to_size_container(radian_space_sz).get(); ++i)
    {
        output_arr[i]       = std::sin(radian_arr[i]) * carry_multiplier * radius_arr[i];
        carry_multiplier    *= std::cos(radian_arr[i]);
    }
}

template <class T, class ...Args>
constexpr auto mul_vector(const std::vector<T, Args...>& vec, T c) -> std::vector<T, Args...>
{
    std::vector<T, Args...> rs(vec.size());
    restrict_scalar_mul_array(vec.data(), vec.size(), c, rs.data());

    return rs;
}

template <class T, class ...Args>
constexpr auto div_vector(const std::vector<T, Args...>& vec, T c) -> std::vector<T, Args...>
{
    std::vector<T, Args...> rs(vec.size());
    restrict_scalar_div_array(vec.data(), vec.size(), c, rs.data());

    return rs;
}

template <class T, class ...Args>
constexpr auto add_vector(const std::vector<T, Args...>& vec1, const std::vector<T, Args...>& vec2) -> std::vector<T, Args...>
{
    if (vec1.size() != vec2.size())
    {
        throw std::runtime_error("invalid vector size");
    }

    std::vector<T, Args...> rs(vec1.size());
    restrict_add_array(vec1.data(), vec2.data(), vec1.size(), rs.data());

    return rs;
}

template <class T>
constexpr auto enumerate_vector(const std::vector<T>& arg) -> std::vector<std::pair<size_t, T>>
{
    std::vector<std::pair<size_t, T>> rs(arg.size());

    for (size_t i = 0u; i < arg.size(); ++i)
    {
        rs[i] = {i, arg[i]};
    }

    return rs;
}

template <class T>
constexpr auto deenumerate_vector(const std::vector<std::pair<size_t, T>>& arg) -> std::vector<T>
{
    if (arg.empty())
    {
        return {};
    }

    size_t max_idx  = std::max_element(arg.begin(), arg.end(), [](const auto& lhs, const auto& rhs){return std::get<0>(lhs) < std::get<0>(rhs);})->first;
    size_t sz       = max_idx + 1;
    auto rs         = std::vector<T>(sz);

    for (const auto& [idx, e]: arg)
    {
        rs[idx] = e;
    }

    return rs;
}

// ------

template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
constexpr auto taylor_project(FloatType x,
                              const FloatType * coeff_arr, SzContainer coeff_arr_sz_container,
                              const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
{
    static_assert(std::is_floating_point_v<FloatType>);
    static_assert(std::is_floating_point_v<PromotedFloatType>);

    if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
    {
        throw std::runtime_error("invalid argument");
    }

    PromotedFloatType projected_result  = 0;
    PromotedFloatType x_multiplier      = 1;
    size_t factorial_denorm             = 1;

    for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
    {
        PromotedFloatType delta_result  = static_cast<PromotedFloatType>(coeff_arr[i]) / static_cast<PromotedFloatType>(factorial_denorm) * x_multiplier;
        projected_result                += delta_result;
        x_multiplier                    *= x;
        factorial_denorm                *= i + 1;
    }

    return projected_result;
}

constexpr auto get_multivariate_taylor_projection_coefficient_size(size_t in_feature_sz, size_t base_coeff_sz) -> size_t
{
    if (in_feature_sz == 0u)
    {
        throw std::runtime_error("invalid argument");
    }

    if (base_coeff_sz > MAX_BASE_COEFFICIENT)
    {
        throw std::runtime_error("invalid argument");
    }

    return std::pow(base_coeff_sz, in_feature_sz);
}

template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
constexpr auto multivariate_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                           CoeffSizeContainer base_coeff_sz_container,
                                           const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                           const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
{
    static_assert(std::is_floating_point_v<FloatType>);
    static_assert(std::is_floating_point_v<PromotedFloatType>);

    if (x_arr_sz_container.get() == 0u)
    {
        throw std::runtime_error("bad input feature size");
    }

    if (x_arr_sz_container.get() == 1u)
    {
        size_t tentative_nxt_offset = coeff_arr_offset + base_coeff_sz_container.get();

        if (tentative_nxt_offset > coeff_arr_cap) [[unlikely]]
        {
            throw std::runtime_error("coefficient vector ran out of space");
        }
        else [[likely]]
        {
            const FloatType * coeff_arr_arg = std::next(coeff_arr, coeff_arr_offset);
            coeff_arr_offset                = tentative_nxt_offset;

            return taylor_project(x_arr[0], coeff_arr_arg, base_coeff_sz_container, promotion_tag);
        }
    }

    PromotedFloatType projected_result  = 0;
    PromotedFloatType x_multiplier      = 1;
    size_t factorial_denorm             = 1u;

    for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
    {
        PromotedFloatType coeff         = multivariate_taylor_project(std::next(x_arr), safe_minus_one(x_arr_sz_container),
                                                                      base_coeff_sz_container,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                      promotion_tag);

        PromotedFloatType delta_result  = coeff / static_cast<PromotedFloatType>(factorial_denorm) * x_multiplier;
        projected_result                += delta_result;
        x_multiplier                    *= x_arr[0];
        factorial_denorm                *= i + 1;
    }

    return projected_result;
}

constexpr auto get_multidimensional_taylor_projection_coefficient_size(size_t in_feature_sz,
                                                                       size_t base_coeff_sz,
                                                                       size_t out_feature_sz) -> size_t
{
    return get_multivariate_taylor_projection_coefficient_size(in_feature_sz, base_coeff_sz) * out_feature_sz;
}

template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
constexpr __attribute__((noinline)) void multidimensional_taylor_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                         CoeffSizeContainer base_coeff_sz_container,
                                                                         const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                         FloatType * output_arr, size_t output_arr_sz,
                                                                         const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                         bool has_logit_reuse_tag = true)
{
    const size_t saved_coeff_arr_offset = coeff_arr_offset;

    for (size_t i = 0u; i < output_arr_sz; ++i)
    {
        if (has_logit_reuse_tag)
        {
            coeff_arr_offset = saved_coeff_arr_offset;
        }

        output_arr[i]   = multivariate_taylor_project(x_arr, x_arr_sz_container,
                                                      base_coeff_sz_container,
                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                      promotion_tag);
    }
}

// ------

template <class FloatType, class PromotedFloatType = FloatType>
constexpr auto radian_normalize(FloatType x,
                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> FloatType
{
    static_assert(std::is_floating_point_v<FloatType>);
    static_assert(std::is_floating_point_v<PromotedFloatType>);

    return static_cast<FloatType>(std::remainder(static_cast<PromotedFloatType>(x), 2.0f * std::numbers::pi_v<PromotedFloatType>));
}

template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
constexpr void taylor_radian_to_euclidean_space(const FloatType * radian_coeff_arr,
                                                SzContainer coeff_arr_sz_container,
                                                FloatType * euclid_coeff_arr,
                                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{})
{
    static_assert(std::is_floating_point_v<FloatType>);
    static_assert(std::is_floating_point_v<PromotedFloatType>);

    PromotedFloatType carry_multiplier = 1u;

    for (size_t i = 0u; i < coeff_arr_sz_container.get(); ++i)
    {
        euclid_coeff_arr[i] = carry_multiplier * std::sin(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
        carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_coeff_arr[i]));
    }
}

template <class FloatType, class SzContainer, class PromotedFloatType = FloatType>
constexpr auto taylor_shape_project(FloatType x,
                                    const FloatType * radian_coeff_arr, SzContainer coeff_arr_sz_container,
                                    const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
{
    if (coeff_arr_sz_container.get() > MAX_BASE_COEFFICIENT)
    {
        throw std::runtime_error("invalid argument");
    }

    FloatType euclidean_coeff_space[MAX_BASE_COEFFICIENT];
    taylor_radian_to_euclidean_space(radian_coeff_arr, coeff_arr_sz_container, euclidean_coeff_space, promotion_tag);

    return taylor_project(x,
                          euclidean_coeff_space, coeff_arr_sz_container,
                          promotion_tag);
}

constexpr auto get_multivariate_taylor_shape_projection_coefficient_size(size_t in_feature_sz, size_t base_coeff_sz) -> size_t
{
    if (in_feature_sz == 0u)
    {
        throw std::runtime_error("invalid argument");
    }

    if (base_coeff_sz > MAX_BASE_COEFFICIENT)
    {
        throw std::runtime_error("invalid argument");
    }

    return std::pow(base_coeff_sz, in_feature_sz);
}

template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
constexpr auto multivariate_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                 CoeffSizeContainer base_coeff_sz_container,
                                                 const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                 const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
{
    static_assert(std::is_floating_point_v<FloatType>);
    static_assert(std::is_floating_point_v<PromotedFloatType>);

    if (x_arr_sz_container.get() == 0u)
    {
        throw std::runtime_error("invalid argument");
    }

    if (x_arr_sz_container.get() == 1u)
    {
        size_t tentative_nxt_offset = coeff_arr_offset + base_coeff_sz_container.get();

        if (tentative_nxt_offset > coeff_arr_cap)
        {
            throw std::runtime_error("coefficient vector ran out of space");
        }

        const FloatType * coeff_arr_arg = std::next(coeff_arr, coeff_arr_offset);
        coeff_arr_offset                = tentative_nxt_offset;

        return taylor_shape_project(x_arr[0], coeff_arr_arg, base_coeff_sz_container);
    }

    PromotedFloatType projected_result  = 0;
    PromotedFloatType x_multiplier      = 1;
    size_t factorial_denorm             = 1u;

    for (size_t i = 0u; i < base_coeff_sz_container.get(); ++i)
    {
        PromotedFloatType coeff         = multivariate_taylor_shape_project(std::next(x_arr), safe_minus_one(x_arr_sz_container),
                                                                            base_coeff_sz_container,
                                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap);

        PromotedFloatType delta_result  = coeff / static_cast<PromotedFloatType>(factorial_denorm) * x_multiplier;
        projected_result                += delta_result;
        x_multiplier                    *= x_arr[0];
        factorial_denorm                *= i + 1;
    }

    return projected_result;
}

constexpr auto get_multidimensional_taylor_shape_projection_coefficient_size(size_t in_feature_sz,
                                                                             size_t base_coeff_sz,
                                                                             size_t out_feature_sz) -> size_t
{
    return get_multivariate_taylor_shape_projection_coefficient_size(in_feature_sz, base_coeff_sz) * out_feature_sz;
}

template <class FloatType, class XArrSizeContainer, class CoeffSizeContainer, class PromotedFloatType = FloatType>
constexpr __attribute__((noinline)) void multidimensional_taylor_shape_project(const FloatType * x_arr, XArrSizeContainer x_arr_sz_container,
                                                                               CoeffSizeContainer base_coeff_sz_container,
                                                                               const FloatType * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                               FloatType * output_arr, size_t output_arr_sz,
                                                                               const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{},
                                                                               bool has_logit_reuse_tag = true)
{
    const size_t saved_coeff_arr_offset = coeff_arr_offset;

    for (size_t i = 0u; i < output_arr_sz; ++i)
    {
        if (has_logit_reuse_tag)
        {
            coeff_arr_offset = saved_coeff_arr_offset;
        }

        output_arr[i]   = multivariate_taylor_shape_project(x_arr, x_arr_sz_container,
                                                            base_coeff_sz_container,
                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                            promotion_tag);
    }
}

// ------

using matrix_std_float_t    = float;

static inline constexpr size_t PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ      = 2u;
static inline constexpr size_t PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ  = 4u;

struct ProcessUnit
{
    std::array<matrix_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> logit_vec;
};

struct ProcessGroup
{
    std::array<ProcessUnit, PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ> process_vec;
};

struct BeingUnit
{
    std::vector<std::shared_ptr<ProcessGroup>> process_group_vec;
};

struct Matrix
{
    std::vector<std::shared_ptr<BeingUnit>> being_vec;
};

// ------

constexpr auto safe_non_zero_access(size_t sz) -> size_t
{
    if (sz == 0u)
    {
        throw std::runtime_error("zero guard");
    }

    return sz;
}

template <class T, class DefaultAsGenerator>
constexpr auto copy_and_trail_defaultize(const std::vector<T>& arg,
                                         double perc,
                                         DefaultAsGenerator&& gen) -> std::vector<T>
{
    size_t tentative_deparam_sz = arg.size() * perc;
    size_t deparam_sz           = std::clamp(tentative_deparam_sz, size_t{0u}, arg.size());
    size_t active_sz            = arg.size() - deparam_sz;

    std::vector<T> result       = {};

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
struct PreciseFloatConversionContainer
{
    static_assert(std::is_floating_point_v<FloatType>);

    FloatType value;

    constexpr PreciseFloatConversionContainer() = default;
    constexpr PreciseFloatConversionContainer(FloatType value): value(std::move(value)){}

    template <class OtherFloatType, std::enable_if_t<std::is_floating_point_v<OtherFloatType>, bool> = true>
    constexpr operator OtherFloatType() const
    {
        if (std::isnan(this->value))
        {
            return generic_nan<OtherFloatType>();
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

// ------

constexpr auto make_process_unit_from_shape_vec(const std::vector<size_t>& space) -> ProcessUnit
{
    if (space.size() != 1u)
    {
        throw std::runtime_error("invalid space shape");
    }

    if (space.front() != PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ)
    {
        throw std::runtime_error("invalid space shape");
    }

    return {.logit_vec = to_array_default_initializer(0.f)};
}

template <class ...Args>
constexpr void internal_unflatten(ProcessUnit& arg,
                                  const std::vector<matrix_std_float_t, Args...>& input_vec,
                                  const std::unique_ptr<size_t>& offset_container = std::make_unique<size_t>(0u))
{
    for (auto& logit: arg.logit_vec)
    {
        logit = input_vec[access_guard((*offset_container)++, input_vec.size())];
    }
}

constexpr auto make_process_unit_from_flat_vec(const std::vector<size_t>& space,
                                               const std::vector<matrix_std_float_t>& input_vec) -> ProcessUnit
{
    ProcessUnit rs = make_process_unit_from_shape_vec(space);
    internal_unflatten(rs, input_vec);

    return rs;
}

constexpr auto empty_as(const matrix_std_float_t&) -> matrix_std_float_t
{
    return 0;
}

constexpr auto empty_as(const ProcessUnit&) -> ProcessUnit
{
    return {.logit_vec = to_array_default_initializer(0)};
}

template <class TaylorBaseCoeffSizeContainer, class ShapeBaseCoeffSizeContainer,
          class TaylorBasePromotedFloatType = matrix_std_float_t, class ShapeBasePromotedFloatType = matrix_std_float_t>
constexpr __attribute__((noinline)) auto intercourse_process_unit(const ProcessUnit& lhs,
                                                                  const ProcessUnit& rhs,
                                                                  TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                  const matrix_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                  ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                  const matrix_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                  const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = Tag<TaylorBasePromotedFloatType>{},
                                                                  const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                  bool has_logit_reuse_tag = true) -> ProcessUnit
{
    constexpr size_t COMBINED_DIMENSION_SZ = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ * 2u;

    std::array<matrix_std_float_t, COMBINED_DIMENSION_SZ> combined{};

    std::copy(lhs.logit_vec.begin(), lhs.logit_vec.end(), combined.data());
    std::copy(rhs.logit_vec.begin(), rhs.logit_vec.end(), std::next(combined.data(), lhs.logit_vec.size()));

    std::array<matrix_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> child{};

    multidimensional_taylor_project(combined.data(), to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}),
                                    base_coeff_sz_container,
                                    coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                    child.data(), child.size(),
                                    taylor_base_promotion_tag,
                                    has_logit_reuse_tag);

    std::array<matrix_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> child2{};

    multidimensional_taylor_shape_project(combined.data(), to_size_container(std::integral_constant<size_t, COMBINED_DIMENSION_SZ>{}),
                                          base_shape_coeff_sz_container,
                                          shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                          child2.data(), child2.size(),
                                          shape_base_promotion_tag,
                                          has_logit_reuse_tag);

    for (size_t i = 0u; i < child.size(); ++i)
    {
        child[i]    += child2[i];
        child[i]    /= 2;
    }

    return {.logit_vec = child};
}

constexpr auto deparameterize(const ProcessUnit& process_unit, double perc) -> ProcessUnit
{
    return {.logit_vec = copy_and_trail_defaultize(process_unit.logit_vec,
                                                   perc,
                                                   static_cast<matrix_std_float_t (*)(const matrix_std_float_t&)>(empty_as))};
}

constexpr auto accumulate(const ProcessUnit& lhs, const ProcessUnit& rhs) -> ProcessUnit
{
    ProcessUnit rs{};

    for (size_t i = 0u; i < lhs.logit_vec.size(); ++i)
    {
        rs.logit_vec[i] = lhs.logit_vec[i] + rhs.logit_vec[i];
    }

    return rs;
}

constexpr auto accumulate(const ProcessUnit * process_vec, size_t process_vec_sz) -> ProcessUnit
{
    if (process_vec_sz == 0u)
    {
        throw std::runtime_error("invalid argument");
    }

    ProcessUnit result = process_vec[0];

    for (size_t i = 1u; i < process_vec_sz; ++i)
    {
        result = accumulate(result, process_vec[i]);
    }

    return result;
}

template <class FloatType>
constexpr auto positional_encode(const ProcessUnit& arg,
                                 size_t idx,
                                 const FloatType& amplitude,
                                 const FloatType& frequency_multiplier) -> ProcessUnit
{
    ProcessUnit rs{};

    for (size_t i = 0u; i < rs.logit_vec.size(); ++i)
    {
        rs.logit_vec[i] = arg.logit_vec[i] + amplitude * idx * std::sin(frequency_multiplier * arg.logit_vec[i]);
    }

    return rs;
}

template <class ValueType>
constexpr auto div(const ProcessUnit& process_unit, const ValueType& value) -> ProcessUnit
{
    ProcessUnit rs{};

    for (size_t i = 0u; i < rs.logit_vec.size(); ++i)
    {
        rs.logit_vec[i] = process_unit.logit_vec[i] / value;
    }

    return rs;
}

constexpr auto avg(const ProcessUnit * process_vec, size_t process_vec_sz) -> ProcessUnit
{
    return div(accumulate(process_vec, process_vec_sz), safe_non_zero_access(process_vec_sz));
}

template <class ...Args>
constexpr void flatten(const ProcessUnit& arg, std::vector<matrix_std_float_t, Args...>& output_vec)
{
    for (const matrix_std_float_t& logit: arg.logit_vec)
    {
        output_vec.push_back(logit);
    }
}

// ------

constexpr auto make_process_group_from_shape_vec(const std::vector<size_t>& space) -> std::shared_ptr<ProcessGroup>
{
    if (space.empty())
    {
        throw std::runtime_error("invalid space shape");
    }

    size_t dimension_sz = space.front();

    if (dimension_sz != PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ)
    {
        throw std::runtime_error("invalid space shape");
    }

    ProcessGroup rs{};

    for (auto& e: rs.process_vec)
    {
        e = make_process_unit_from_shape_vec({std::next(space.begin()), space.end()});
    }

    return std::make_shared<ProcessGroup>(rs);
}

template <class ...Args>
constexpr void internal_unflatten(const std::shared_ptr<ProcessGroup>& arg,
                                  const std::vector<matrix_std_float_t, Args...>& input_vec,
                                  const std::unique_ptr<size_t>& offset_container = std::make_unique<size_t>(0u))
{
    for (auto& e: arg->process_vec)
    {
        internal_unflatten(e, input_vec, offset_container);
    }
}

template <class ...Args>
constexpr auto make_process_group_from_flat_vec(const std::vector<size_t>& space,
                                                const std::vector<matrix_std_float_t, Args...>& input_vec) -> std::shared_ptr<ProcessGroup>
{
    std::shared_ptr<ProcessGroup> rs = make_process_group_from_shape_vec(space);
    internal_unflatten(rs, input_vec);

    return rs;
}

constexpr auto empty_as(const std::shared_ptr<ProcessGroup>& process_group) -> std::shared_ptr<ProcessGroup>
{
    ProcessGroup rs{};

    for (size_t i = 0u; i < rs.process_vec.size(); ++i)
    {
        rs.process_vec[i] = empty_as(process_group->process_vec[i]);
    }

    return std::make_shared<ProcessGroup>(std::move(rs));
}

template <class TaylorBaseCoeffSizeContainer, class ShapeBaseCoeffSizeContainer,
          class TaylorBasePromotedFloatType = matrix_std_float_t, class ShapeBasePromotedFloatType = matrix_std_float_t>
constexpr __attribute__((noinline)) auto left_major_intercourse_process_group(const std::shared_ptr<ProcessGroup>& lhs,
                                                                              const std::shared_ptr<ProcessGroup>& rhs,
                                                                              TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                              const matrix_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                              ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                              const matrix_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                              const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = Tag<TaylorBasePromotedFloatType>{},
                                                                              const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                              bool has_process_unit_logit_reuse_tag = true,
                                                                              bool has_process_group_logit_reuse_tag = true) -> std::shared_ptr<ProcessGroup>
{
    ProcessGroup rs{};

    const size_t saved_coeff_arr_offset       = coeff_arr_offset;
    const size_t saved_shape_coeff_arr_offset = shape_coeff_arr_offset;

    for (size_t i = 0u; i < lhs->process_vec.size(); ++i)
    {
        if (has_process_group_logit_reuse_tag)
        {
            coeff_arr_offset        = saved_coeff_arr_offset;
            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
        }

        ProcessGroup accum_vec{};

        for (size_t j = 0u; j < rhs->process_vec.size(); ++j)
        {
            accum_vec.process_vec[j] = intercourse_process_unit(lhs->process_vec[i],
                                                                rhs->process_vec[j],
                                                                base_coeff_sz_container,
                                                                coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                base_shape_coeff_sz_container,
                                                                shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                taylor_base_promotion_tag,
                                                                shape_base_promotion_tag,
                                                                has_process_unit_logit_reuse_tag);
        }

        rs.process_vec[i] = avg(accum_vec.process_vec.data(), accum_vec.process_vec.size());
    }

    return std::make_shared<ProcessGroup>(std::move(rs));
}

constexpr auto deparameterize(const std::shared_ptr<ProcessGroup>& process_group, double perc) -> std::shared_ptr<ProcessGroup>
{
    return std::make_shared<ProcessGroup>(ProcessGroup{.process_vec = copy_and_trail_defaultize(process_group->process_vec,
                                                                                                perc,
                                                                                                static_cast<ProcessUnit (*)(const ProcessUnit&)>(empty_as))});
}

constexpr auto accumulate(const std::shared_ptr<ProcessGroup>& lhs, const std::shared_ptr<ProcessGroup>& rhs) -> std::shared_ptr<ProcessGroup>
{
    const auto& lhs_content = lhs->process_vec;
    const auto& rhs_content = rhs->process_vec;

    ProcessGroup rs{};

    for (size_t i = 0u; i < lhs_content.size(); ++i)
    {
        rs.process_vec[i] = accumulate(lhs_content[i], rhs_content[i]);
    }

    return std::make_shared<ProcessGroup>(std::move(rs));
}

constexpr auto accumulate(const std::vector<std::shared_ptr<ProcessGroup>>& process_group_vec) -> std::shared_ptr<ProcessGroup>
{
    if (process_group_vec.empty())
    {
        throw std::runtime_error("invalid argument");
    }

    std::shared_ptr<ProcessGroup> result    = process_group_vec[0];

    for (size_t i = 1u; i < process_group_vec.size(); ++i)
    {
        result = accumulate(result, process_group_vec[i]);
    }

    return result;
}

template <class FloatType>
constexpr auto positional_encode(const std::shared_ptr<ProcessGroup>& process_group,
                                 size_t idx,
                                 const FloatType& amplitude,
                                 const FloatType& frequency_multiplier) -> std::shared_ptr<ProcessGroup>
{
    ProcessGroup rs{};

    for (size_t i = 0u; i < rs.process_vec.size(); ++i)
    {
        rs.process_vec[i] = positional_encode(process_group->process_vec[i], idx, amplitude, frequency_multiplier);
    }

    return std::make_shared<ProcessGroup>(std::move(rs));
}

template <class ValueType>
constexpr auto div(const std::shared_ptr<ProcessGroup>& process_group, const ValueType& value) -> std::shared_ptr<ProcessGroup>
{
    ProcessGroup rs{};

    for (size_t i = 0u; i < rs.process_vec.size(); ++i)
    {
        rs.process_vec[i] = div(process_group->process_vec[i], value);
    }

    return std::make_shared<ProcessGroup>(std::move(rs));
}

constexpr auto avg(const std::vector<std::shared_ptr<ProcessGroup>>& process_group_vec) -> std::shared_ptr<ProcessGroup>
{
    return div(accumulate(process_group_vec), safe_non_zero_access(process_group_vec.size()));
}

template <class ...Args>
constexpr void flatten(const std::shared_ptr<ProcessGroup>& arg, std::vector<matrix_std_float_t, Args...>& output_vec)
{
    for (const auto& e: arg->process_vec)
    {
        flatten(e, output_vec);
    }
}

//

constexpr auto make_being_unit_from_shape_vec(const std::vector<size_t>& space) -> std::shared_ptr<BeingUnit>
{
    if (space.empty())
    {
        throw std::runtime_error("invalid space shape");
    }

    size_t dimension_sz = space.front();
    std::vector<std::shared_ptr<ProcessGroup>> rs(dimension_sz);

    for (size_t i = 0u; i < dimension_sz; ++i)
    {
        rs[i] = make_process_group_from_shape_vec({std::next(space.begin()), space.end()});
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(rs)});
}

template <class ...Args>
constexpr void internal_unflatten(const std::shared_ptr<BeingUnit>& arg,
                                  const std::vector<matrix_std_float_t, Args...>& input_vec,
                                  const std::unique_ptr<size_t>& offset_container = std::make_unique<size_t>(0u))
{
    for (auto& e: arg->process_group_vec)
    {
        internal_unflatten(e, input_vec, offset_container);
    }
}

template <class ...Args>
constexpr auto make_being_unit_from_flat_vec(const std::vector<size_t>& space,
                                             const std::vector<matrix_std_float_t, Args...>& input_vec) -> std::shared_ptr<BeingUnit>
{
    std::shared_ptr<BeingUnit> rs = make_being_unit_from_shape_vec(space);
    internal_unflatten(rs, input_vec);

    return rs;
}

constexpr auto empty_as(const std::shared_ptr<BeingUnit>& being) -> std::shared_ptr<BeingUnit>
{
    std::vector<std::shared_ptr<ProcessGroup>> result{};

    for (const auto& e: being->process_group_vec)
    {
        result.push_back(empty_as(e));
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(result)});
}

template <class TaylorBaseCoeffSizeContainer, class ShapeBaseCoeffSizeContainer,
          class TaylorBasePromotedFloatType = matrix_std_float_t, class ShapeBasePromotedFloatType = matrix_std_float_t>
constexpr __attribute__((noinline)) auto left_major_intercourse_being_unit(const std::shared_ptr<BeingUnit>& lhs,
                                                                           const std::shared_ptr<BeingUnit>& rhs,
                                                                           TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                           const matrix_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                           ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                           const matrix_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                           const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = Tag<TaylorBasePromotedFloatType>{},
                                                                           const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                                           bool has_process_unit_logit_reuse_tag = true,
                                                                           bool has_process_group_logit_reuse_tag = true,
                                                                           bool has_being_logit_reuse_tag = true) -> std::shared_ptr<BeingUnit>
{
    std::vector<std::shared_ptr<ProcessGroup>> result_vec{};

    const size_t saved_coeff_arr_offset         = coeff_arr_offset;
    const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

    for (size_t i = 0u; i < lhs->process_group_vec.size(); ++i)
    {
        if (has_being_logit_reuse_tag)
        {
            coeff_arr_offset        = saved_coeff_arr_offset;
            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
        }

        std::vector<std::shared_ptr<ProcessGroup>> accum_vec{lhs->process_group_vec[i]};

        for (size_t j = 0u; j < rhs->process_group_vec.size(); ++j)
        {
            accum_vec.push_back(left_major_intercourse_process_group(lhs->process_group_vec[i],
                                                                     rhs->process_group_vec[j],
                                                                     base_coeff_sz_container,
                                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                     base_shape_coeff_sz_container,
                                                                     shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                     taylor_base_promotion_tag,
                                                                     shape_base_promotion_tag,
                                                                     has_process_unit_logit_reuse_tag,
                                                                     has_process_group_logit_reuse_tag));
        }

        result_vec.push_back(avg(accum_vec));
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(result_vec)});
}

constexpr auto deparameterize(const std::shared_ptr<BeingUnit>& val, double perc) -> std::shared_ptr<BeingUnit>
{
    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = copy_and_trail_defaultize(val->process_group_vec,
                                                                                                perc,
                                                                                                static_cast<std::shared_ptr<ProcessGroup> (*) (const std::shared_ptr<ProcessGroup>&)>(empty_as))});
}

constexpr auto accumulate(const std::shared_ptr<BeingUnit>& lhs, const std::shared_ptr<BeingUnit>& rhs) -> std::shared_ptr<BeingUnit>
{
    const std::vector<std::shared_ptr<ProcessGroup>>& lhs_process_group = lhs->process_group_vec;
    const std::vector<std::shared_ptr<ProcessGroup>>& rhs_process_group = rhs->process_group_vec;

    if (lhs_process_group.size() != rhs_process_group.size())
    {
        throw std::runtime_error("invalid argument");
    }

    std::vector<std::shared_ptr<ProcessGroup>> result{};

    for (size_t i = 0u; i < lhs_process_group.size(); ++i)
    {
        result.push_back(accumulate(lhs_process_group[i], rhs_process_group[i]));
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(result)});
}

constexpr auto accumulate(const std::vector<std::shared_ptr<BeingUnit>>& being_vec) -> std::shared_ptr<BeingUnit>
{
    if (being_vec.empty())
    {
        throw std::runtime_error("invalid argument");
    }

    std::shared_ptr<BeingUnit> result = being_vec[0];

    for (size_t i = 1u; i < being_vec.size(); ++i)
    {
        result = accumulate(result, being_vec[i]);
    }

    return result;
}

template <class FloatType>
constexpr auto positional_encode(const std::shared_ptr<BeingUnit>& being_unit,
                                 size_t idx,
                                 const FloatType& amplitude,
                                 const FloatType& frequency_multiplier) -> std::shared_ptr<BeingUnit>
{
    std::vector<std::shared_ptr<ProcessGroup>> result{};

    for (const auto& e: being_unit->process_group_vec)
    {
        result.push_back(positional_encode(e, idx, amplitude, frequency_multiplier));
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(result)});
}

template <class ValueType>
constexpr auto div(const std::shared_ptr<BeingUnit>& being_unit, const ValueType& value) -> std::shared_ptr<BeingUnit>
{
    std::vector<std::shared_ptr<ProcessGroup>> result{};

    for (const auto& e: being_unit->process_group_vec)
    {
        result.push_back(div(e, value));
    }

    return std::make_shared<BeingUnit>(BeingUnit{.process_group_vec = std::move(result)});
}

constexpr auto avg(const std::vector<std::shared_ptr<BeingUnit>>& being_unit_vec) -> std::shared_ptr<BeingUnit>
{
    return div(accumulate(being_unit_vec), safe_non_zero_access(being_unit_vec.size()));
}

template <class ...Args>
constexpr void flatten(const std::shared_ptr<BeingUnit>& arg, std::vector<matrix_std_float_t, Args...>& output_vec)
{
    for (const auto& e: arg->process_group_vec)
    {
        flatten(e, output_vec);
    }
}

// ------

constexpr auto make_matrix_from_shape_vec(const std::vector<size_t>& space) -> std::shared_ptr<Matrix>
{
    if (space.empty())
    {
        throw std::runtime_error("invalid space shape");
    }

    size_t dimension_sz = space.front();
    std::vector<std::shared_ptr<BeingUnit>> rs(dimension_sz);

    for (size_t i = 0u; i < dimension_sz; ++i)
    {
        rs[i] = make_being_unit_from_shape_vec({std::next(space.begin()), space.end()});
    }

    return std::make_shared<Matrix>(Matrix{.being_vec = std::move(rs)});
}


template <class ...Args>
constexpr void internal_unflatten(const std::shared_ptr<Matrix>& arg,
                                  const std::vector<matrix_std_float_t, Args...>& input_vec,
                                  const std::unique_ptr<size_t>& offset_container = std::make_unique<size_t>(0u))
{
    for (auto& e: arg->being_vec)
    {
        internal_unflatten(e, input_vec, offset_container);
    }
}

template <class ...Args>
constexpr auto make_matrix_from_flat_vec(const std::vector<size_t>& space,
                                         const std::vector<matrix_std_float_t, Args...>& input_vec) -> std::shared_ptr<Matrix>
{
    auto rs = make_matrix_from_shape_vec(space);
    internal_unflatten(rs, input_vec);

    return rs;
}

template <class FloatType>
constexpr auto positional_encode_matrix(const std::shared_ptr<Matrix>& matrix,
                                        const FloatType& amplitude_multiplier,
                                        const FloatType& frequency_multiplier) -> std::shared_ptr<Matrix>
{
    std::vector<std::shared_ptr<BeingUnit>> result(matrix->being_vec.size());

    for (size_t i = 0u; i < matrix->being_vec.size(); ++i)
    {
        result[i] = positional_encode(matrix->being_vec[i], i, amplitude_multiplier, frequency_multiplier);
    }

    return std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)});
}

constexpr auto matrix_to_focal(const std::shared_ptr<Matrix>& matrix,
                               size_t i,
                               const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map) -> std::vector<std::shared_ptr<Matrix>>
{
    const std::vector<std::vector<size_t>>& focal_dictionary = [&]() -> const std::vector<std::vector<size_t>>&
    {
        static std::vector<std::vector<size_t>> empty_vec{};

        auto map_ptr = focal_suffix_map.find(matrix->being_vec.size());

        if (map_ptr == focal_suffix_map.end())
        {
            return empty_vec;
        }

        auto map_ptr2 = map_ptr->second.find(i);

        if (map_ptr2 == map_ptr->second.end())
        {
            return empty_vec;
        }

        return map_ptr2->second;
    }();

    std::vector<std::shared_ptr<Matrix>> result_vec{};

    for (const auto& suffix_arr: focal_dictionary)
    {
        std::vector<std::shared_ptr<BeingUnit>> result(matrix->being_vec.size());

        for (size_t i = 0u; i < matrix->being_vec.size(); ++i)
        {
            size_t suffix = suffix_arr[access_guard(i, suffix_arr.size())];
            result[access_guard(suffix, result.size())] = matrix->being_vec[access_guard(i, matrix->being_vec.size())];
        }

        result_vec.push_back(std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)}));
    }

    return result_vec;
}

constexpr auto focal_split_matrix(const std::shared_ptr<Matrix>& matrix, size_t group_by_sz) -> std::vector<std::shared_ptr<Matrix>>
{
    if (group_by_sz == 0u)
    {
        throw std::runtime_error("invalid group_by_sz argument");
    }

    if (matrix->being_vec.size() % group_by_sz != 0u)
    {
        throw std::runtime_error("bad focal split");
    }

    size_t focal_sz = matrix->being_vec.size() / group_by_sz;

    std::vector<std::shared_ptr<Matrix>> result = {};

    for (size_t i = 0u; i < group_by_sz; ++i)
    {
        size_t first    = i * focal_sz;
        size_t last     = static_cast<size_t>((i + 1) * focal_sz);

        result.push_back(std::make_shared<Matrix>(Matrix{.being_vec = {std::next(matrix->being_vec.begin(), first), std::next(matrix->being_vec.begin(), last)}}));
    }

    return result;
}

constexpr auto focal_unsplit_matrix(const std::vector<std::shared_ptr<Matrix>>& matrix_vec, size_t group_by_sz) -> std::shared_ptr<Matrix>
{
    std::vector<std::shared_ptr<BeingUnit>> result = {};

    for (const auto& matrix: matrix_vec)
    {
        std::copy(matrix->being_vec.begin(), matrix->being_vec.end(), std::back_inserter(result));
    }

    return std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)});
}

constexpr auto deparameterize(const std::shared_ptr<Matrix>& matrix, double perc) -> std::shared_ptr<Matrix>
{
    return std::make_shared<Matrix>(Matrix{.being_vec = copy_and_trail_defaultize(matrix->being_vec,
                                                                                  perc,
                                                                                  static_cast<std::shared_ptr<BeingUnit> (*)(const std::shared_ptr<BeingUnit>&)>(empty_as))});
}

constexpr auto accumulate(const std::shared_ptr<Matrix>& lhs, const std::shared_ptr<Matrix>& rhs) -> std::shared_ptr<Matrix>
{
    const std::vector<std::shared_ptr<BeingUnit>>& lhs_being_vec    = lhs->being_vec;
    const std::vector<std::shared_ptr<BeingUnit>>& rhs_being_vec    = rhs->being_vec;

    if (lhs_being_vec.size() != rhs_being_vec.size())
    {
        throw std::runtime_error("invalid argument");
    }

    std::vector<std::shared_ptr<BeingUnit>> result                  = {};

    for (size_t i = 0u; i < lhs_being_vec.size(); ++i)
    {
        result.push_back(accumulate(lhs_being_vec[i], rhs_being_vec[i]));
    }

    return std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)});
}

constexpr auto accumulate(const std::vector<std::shared_ptr<Matrix>>& matrix_vec) -> std::shared_ptr<Matrix>
{
    if (matrix_vec.empty())
    {
        throw std::runtime_error("invalid argument");
    }

    std::shared_ptr<Matrix> result = matrix_vec[0];

    for (size_t i = 1u; i < matrix_vec.size(); ++i)
    {
        result = accumulate(result, matrix_vec[i]);
    }

    return result;
}

template <class ValueType>
constexpr auto div(const std::shared_ptr<Matrix>& matrix_unit, const ValueType& value) -> std::shared_ptr<Matrix>
{
    std::vector<std::shared_ptr<BeingUnit>> result{};

    for (const auto& e: matrix_unit->being_vec)
    {
        result.push_back(div(e, value));
    }

    return std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)});
}

constexpr auto avg(const std::vector<std::shared_ptr<Matrix>>& matrix_vec) -> std::shared_ptr<Matrix>
{
    return div(accumulate(matrix_vec), safe_non_zero_access(matrix_vec.size()));
}

template <class ...Args>
constexpr void flatten(const std::shared_ptr<Matrix>& arg, std::vector<matrix_std_float_t, Args...>& output_vec)
{
    for (const auto& e: arg->being_vec)
    {
        flatten(e, output_vec);
    }
}

constexpr auto immutable_shape_matrix_to_unique_representation(const std::shared_ptr<Matrix>& arg) -> std::string
{
    std::vector<matrix_std_float_t> content{};
    flatten(arg, content);

    size_t total_sz     = content.size() * sizeof(matrix_std_float_t) + sizeof(size_t);
    size_t content_sz   = content.size();

    std::string rs(total_sz, 0);

    std::memcpy(rs.data(), &content_sz, sizeof(size_t));
    std::memcpy(std::next(rs.data(), sizeof(size_t)), content.data(), content.size() * sizeof(matrix_std_float_t));

    return rs;
}

constexpr auto unfocal_matrix(const std::vector<std::shared_ptr<Matrix>>& matrix_vec,
                              size_t i,
                              const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map) -> std::shared_ptr<Matrix>
{
    const std::vector<std::vector<size_t>>& focal_dictionary = [&]() -> const std::vector<std::vector<size_t>>&
    {
        static std::vector<std::vector<size_t>> empty_vec{};

        if (matrix_vec.empty())
        {
            return empty_vec;
        }

        auto map_ptr = focal_suffix_map.find(matrix_vec.front()->being_vec.size());

        if (map_ptr == focal_suffix_map.end())
        {
            return empty_vec;
        }

        auto map_ptr2 = map_ptr->second.find(i);

        if (map_ptr2 == map_ptr->second.end())
        {
            return empty_vec;
        }

        return map_ptr2->second;
    }();

    std::vector<std::shared_ptr<Matrix>> result_vec{};

    for (size_t i = 0u; i < matrix_vec.size(); ++i)
    {
        const auto& suffix_arr = focal_dictionary[i];
        std::vector<std::shared_ptr<BeingUnit>> result(matrix_vec.front()->being_vec.size());

        for (size_t i = 0u; i < matrix_vec[i]->being_vec.size(); ++i)
        {
            size_t suffix = suffix_arr[access_guard(i, suffix_arr.size())];
            result[access_guard(i, result.size())] = matrix_vec[i]->being_vec[access_guard(suffix, matrix_vec[i]->being_vec.size())];
        }

        result_vec.push_back(std::make_shared<Matrix>(Matrix{.being_vec = std::move(result)}));
    }

    return avg(result_vec);
}

// ------

//I guess that the way we came up with the function is because this is the best possible option for continuous approximation, really!

//think about all the possible form of continuous, approximatable, trainable function, they always share a form of convergable Taylor's coefficients, sqrt, or friends are not convergable
//so we concluded that at the basis of the network, there has to be a multivariate + multidimensional Taylor Series projection up to the saturation limit
//the headcount of the coefficient group falls probably around 4-8, because the otherwise would be too vast a space to be relevant, or too small a space to do continuous compression
//this is also to adhere to the number one rule of AIMD, addictive increase, multiplicative decrease, we iterate through all possible form of polynomials and put a coefficient in front of it (implicitly via Taylor Series)
//the other actionable action would be to join different groups of Taylor Series projection via accumulate function, note that we are somewhat saturating the responsibility of coefficient space by doing so (and this turns out to be the only way to do so ...)

//the way that we do "focus" or "focal" is to cancel the multiplicative complexity part of the final equation, such that if we clear the basis of approximation at the lower function, we can build the semantic on top of that function
//and as we discussed the other day, running two sigmas, x = x + f(x), and y = y + x is approximation-complete and builds a nice pyramid of exponents, via our proof of 1-slack temporary slot

//the problem of the focal is that it would cancel the original positional semantic of the domain, efficiently turning our focal transformation into a less efficient compression function, so we'd add a bijective layer of positions by using different amplitudes at different phases
//the reuses of the functions, or saved_coeff_point would mimic that of a real function whose purpose is to constexpr a certain input, this is multi-layer perceptron of human intellects

//at the basis, we would have to be able to approx a synth wave (sin(x)/x)^2 to cancel our, which means that our coefficient space is at least of 8-10 elements for input space size of 1

//we have seriously spent a consideratble amount of time to think about the unit of approximation-completeness, for taylor series projection, we can do 1 degree projection without initial offset for a simple linear, or matrix multiplication
//maybe, the intermediate representation is not the final approximation-complete representation, but our unit must be achieved after a sequence of transformation

//solving the unit problem is a hard problem that is not very straightforward to grasp, but we can prove that if the basis is to withhold that of a second order synth wave, and we are to reuse that projection function A LOT OF TIME, for a really big matrix, we'd have an approximation-complete function

template <class TaylorBaseCoeffSizeContainer, class ShapeBaseCoeffSizeContainer,
          class TaylorBasePromotedFloatType = matrix_std_float_t, class ShapeBasePromotedFloatType = matrix_std_float_t>
constexpr __attribute__((noinline)) auto matrix_transform(const std::shared_ptr<Matrix>& matrix,
                                                          const std::vector<size_t>& focal_sz_vec,
                                                          const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                                                          const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,

                                                          const std::vector<size_t>& rotation_sz_vec,
                                                          const std::vector<double>& parameter_bound_ratio_vec,
                                                          TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                          const matrix_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                          ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                          const matrix_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                          matrix_std_float_t pe_amplitude, matrix_std_float_t pe_frequency_multiplier, matrix_std_float_t pe_amplitude_decay_rate, matrix_std_float_t pe_frequency_multiplier_decay_rate,

                                                          const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = Tag<TaylorBasePromotedFloatType>{},
                                                          const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                                                          bool has_logit_unit_reuse_tag = true,
                                                          bool has_logit_group_logit_reuse_tag = true,
                                                          bool has_being_logit_reuse_tag = true,
                                                          bool has_base_matrix_logit_reuse_tag = true) -> std::shared_ptr<Matrix>
{
    if (matrix->being_vec.size() == 0u)
    {
        throw std::runtime_error("invalid matrix transform");
    }

    if (matrix->being_vec.size() == 1u)
    {
        return matrix;
    }

    if (matrix->being_vec.size() == 2u)
    {
        const size_t saved_coeff_arr_offset         = coeff_arr_offset;
        const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

        std::shared_ptr<BeingUnit> lhs = left_major_intercourse_being_unit(matrix->being_vec[0],
                                                                           matrix->being_vec[1],
                                                                           base_coeff_sz_container,
                                                                           coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                           base_shape_coeff_sz_container,
                                                                           shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                           taylor_base_promotion_tag,
                                                                           shape_base_promotion_tag,
                                                                           has_logit_unit_reuse_tag,
                                                                           has_logit_group_logit_reuse_tag,
                                                                           has_being_logit_reuse_tag);

        if (has_base_matrix_logit_reuse_tag)
        {
            coeff_arr_offset        = saved_coeff_arr_offset;
            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
        }

        std::shared_ptr<BeingUnit> rhs = left_major_intercourse_being_unit(matrix->being_vec[1],
                                                                           matrix->being_vec[0],
                                                                           base_coeff_sz_container,
                                                                           coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                           base_shape_coeff_sz_container,
                                                                           shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                           taylor_base_promotion_tag,
                                                                           shape_base_promotion_tag,
                                                                           has_logit_unit_reuse_tag,
                                                                           has_logit_group_logit_reuse_tag),
                                                                           has_being_logit_reuse_tag;

        return std::make_shared<Matrix>(Matrix{.being_vec = {lhs, rhs}});
    }

    if (focal_sz_vec.empty())
    {
        throw std::runtime_error("invalid focal_sz_vec size");
    }

    if (rotation_sz_vec.empty())
    {
        throw std::runtime_error("invalid rotation_sz_vec size");
    }

    if (parameter_bound_ratio_vec.empty())
    {
        throw std::runtime_error("invalid parameter_bound_ratio_vec size");
    }

    //in this context, domain and range are interchangable, because domain is the previous range and range is the next domain, etc.

    const size_t focal_sz                       = focal_sz_vec.front();
    const size_t rotation_sz                    = rotation_sz_vec.front();
    const double parameter_bound_ratio          = parameter_bound_ratio_vec.front();
    std::shared_ptr<Matrix> up_to_point_matrix  = matrix;

    std::vector<std::shared_ptr<Matrix>> incremental_matrix_vec{matrix};

    for (size_t i = 0u; i < rotation_sz; ++i)
    {
        std::vector<std::shared_ptr<Matrix>> focused_matrix_vec                 = matrix_to_focal(positional_encode_matrix(up_to_point_matrix, pe_amplitude, pe_frequency_multiplier),
                                                                                                  i,
                                                                                                  focal_suffix_map); //i think this is somewhat lacking, we are forcing a bijective map while in fact we'd want to shorten the trees
        std::vector<std::shared_ptr<Matrix>> up_to_point_incremental_matrix_vec = {};

        for (const auto& focused_matrix: focused_matrix_vec)
        {
            std::vector<std::shared_ptr<Matrix>> focal_matrix_vec               = focal_split_matrix(focused_matrix, focal_sz);
            std::vector<std::shared_ptr<Matrix>> transformed_focal_vec          = {};
            const size_t saved_coeff_arr_offset_0                               = coeff_arr_offset;
            const size_t saved_shape_coeff_arr_offset_0                         = shape_coeff_arr_offset;

            for (const auto& focal: focal_matrix_vec)
            {
                coeff_arr_offset        = saved_coeff_arr_offset_0;
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset_0;

                std::shared_ptr<Matrix> transformed_focal = matrix_transform(focal,
                                                                             {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                             focal_suffix_map,
                                                                             accum_suffix_map,
                                                                             {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                             {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                             base_coeff_sz_container,
                                                                             coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                             base_shape_coeff_sz_container,
                                                                             shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                             pe_amplitude * pe_amplitude_decay_rate, pe_frequency_multiplier * pe_frequency_multiplier_decay_rate, pe_amplitude_decay_rate, pe_frequency_multiplier_decay_rate,
                                                                             taylor_base_promotion_tag,
                                                                             shape_base_promotion_tag,
                                                                             has_logit_unit_reuse_tag,
                                                                             has_logit_group_logit_reuse_tag,
                                                                             has_being_logit_reuse_tag,
                                                                             has_base_matrix_logit_reuse_tag);

                transformed_focal_vec.push_back(transformed_focal);
            }

            std::shared_ptr<Matrix> transformed_focused_matrix = focal_unsplit_matrix(transformed_focal_vec, focal_sz);
            up_to_point_incremental_matrix_vec.push_back(transformed_focused_matrix);
        }

        std::vector<std::shared_ptr<Matrix>> focused_deparameterized_matrix_vec = matrix_to_focal(deparameterize(positional_encode_matrix(up_to_point_matrix, pe_amplitude, pe_frequency_multiplier), parameter_bound_ratio),
                                                                                                  i,
                                                                                                  accum_suffix_map);
        std::vector<std::shared_ptr<Matrix>> accum_matrix_vec                   = {};

        for (const auto& focused_matrix: focused_deparameterized_matrix_vec)
        {
            std::vector<std::shared_ptr<Matrix>> focal_deparamed_matrix_vec     = focal_split_matrix(focused_matrix, focal_sz);
            std::vector<std::shared_ptr<Matrix>> incremental_vec                = {};
            const size_t saved_coeff_arr_offset_1                               = coeff_arr_offset;
            const size_t saved_shape_coeff_arr_offset_1                         = shape_coeff_arr_offset;

            for (const auto& focal: focal_deparamed_matrix_vec)
            {
                coeff_arr_offset        = saved_coeff_arr_offset_1;
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset_1;

                std::shared_ptr<Matrix> transformed_focal = matrix_transform(focal,
                                                                             {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                             focal_suffix_map,
                                                                             accum_suffix_map,
                                                                             {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                             {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                             base_coeff_sz_container,
                                                                             coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                             base_shape_coeff_sz_container,
                                                                             shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                             pe_amplitude * pe_amplitude_decay_rate, pe_frequency_multiplier * pe_frequency_multiplier_decay_rate, pe_amplitude_decay_rate, pe_frequency_multiplier_decay_rate,
                                                                             taylor_base_promotion_tag,
                                                                             shape_base_promotion_tag,
                                                                             has_logit_unit_reuse_tag,
                                                                             has_logit_group_logit_reuse_tag,
                                                                             has_being_logit_reuse_tag,
                                                                             has_base_matrix_logit_reuse_tag);

                incremental_vec.push_back(transformed_focal);
            }

            std::shared_ptr<Matrix> transformed_focused_deparamed_matrix = focal_unsplit_matrix(incremental_vec, focal_sz);
            accum_matrix_vec.push_back(transformed_focused_deparamed_matrix);
        }

        std::shared_ptr<Matrix> incremental_up_to_point_matrix              = unfocal_matrix(up_to_point_incremental_matrix_vec, i, focal_suffix_map);
        std::shared_ptr<Matrix> incremental_result                          = unfocal_matrix(accum_matrix_vec, i, focal_suffix_map);

        up_to_point_matrix                                                  = avg({up_to_point_matrix, incremental_up_to_point_matrix});
        incremental_matrix_vec.push_back(std::move(incremental_result));

        pe_amplitude                                                        *= pe_amplitude_decay_rate;
        pe_frequency_multiplier                                             *= pe_frequency_multiplier_decay_rate;
    }

    return avg(incremental_matrix_vec);
}

// ------

template <class FloatType, class Function>
constexpr auto get_derivative_at(Function&& f,
                                 FloatType x,
                                 size_t derivative_order,
                                 FloatType x_a = to_precise_float_conversion_initializer(0.001)) -> FloatType
{
    if (derivative_order == 0u)
    {
        return f(x);
    }

    FloatType y0    = get_derivative_at(f, x, derivative_order - 1u, x_a);

    if (std::isnan(y0))
    {
        return generic_nan<FloatType>();
    }

    FloatType y1    = get_derivative_at(f, x + x_a, derivative_order - 1u, x_a);

    if (std::isnan(y1))
    {
        return generic_nan<FloatType>();
    }

    FloatType slope = (y1 - y0) / x_a;

    return slope;
}

template <class T = matrix_std_float_t>
static inline constexpr auto get_derivative_at_lambda = []<class ...Args>(Args&& ...args)
{
    return get_derivative_at<T>(std::forward<Args>(args)...);
};

template <class Function, class FloatType, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
constexpr auto newton_first_order_approx(Function&& f,
                                         FloatType x,
                                         size_t iteration_sz,
                                         const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
{
    FloatType cand                      = x;

    std::optional<FloatType> best_x     = std::nullopt;
    std::optional<FloatType> best_y     = std::nullopt;

    for (size_t i = 0u; i < iteration_sz; ++i)
    {
        if (std::isnan(cand))
        {
            break;
        }

        FloatType slope             = derivative_extractor(f, cand, 1u);

        if (std::isnan(slope))
        {
            break;
        }

        FloatType elevated_value    = f(cand);

        if (std::isnan(elevated_value))
        {
            break;
        }

        if (!best_y.has_value())
        {
            best_y  = elevated_value;
            best_x  = cand;
        }

        if (std::abs(best_y.value()) > std::abs(elevated_value))
        {
            best_y  = elevated_value;
            best_x  = cand;
        }

        FloatType previous_delta_x  = elevated_value / slope;
        cand                        -= previous_delta_x;
    }

    if (!best_x.has_value())
    {
        return generic_nan<FloatType>();
    }

    return best_x.value();
}

template <class Function, class FloatType, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
constexpr auto newton_second_order_approx(Function&& f,
                                          FloatType x,
                                          size_t iteration_sz,
                                          const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
{
    if (iteration_sz == 0u)
    {
        return x;
    }

    FloatType a         = derivative_extractor(f, x, 2u) / 2.f;

    if (std::isnan(a))
    {
        return generic_nan<FloatType>();
    }

    FloatType b         = derivative_extractor(f, x, 1u);

    if (std::isnan(b))
    {
        return generic_nan<FloatType>();
    }

    FloatType c         = derivative_extractor(f, x, 0u);

    if (std::isnan(c))
    {
        return generic_nan<FloatType>();
    }

    FloatType d2        = b * b - 4 * a * c;

    if (std::isnan(d2))
    {
        return generic_nan<FloatType>();
    }

    if (d2 < 0.f)
    {
        return generic_nan<FloatType>();
    }

    FloatType d         = std::sqrt(d2);

    if (std::isnan(d))
    {
        return generic_nan<FloatType>();
    }

    FloatType rel_x1    = (-b + d) / (2.f * a);
    FloatType rel_x2    = (-b - d) / (2.f * a);

    FloatType x1        = x + rel_x1;
    FloatType x2        = x + rel_x2;

    if (std::isnan(x1))
    {
        return generic_nan<FloatType>();
    }

    if (std::isnan(x2))
    {
        return generic_nan<FloatType>();
    }

    FloatType xx1       = newton_second_order_approx(f, x1, iteration_sz - 1u, derivative_extractor);
    FloatType xx2       = newton_second_order_approx(f, x2, iteration_sz - 1u, derivative_extractor);
    
    auto is_less = [](const auto& lhs, const auto& rhs)
    {
        return nan_cmp(lhs.first, rhs.first) < 0;
    };

    std::array<std::pair<FloatType, FloatType>, 5u> cand_arr{std::make_pair(std::abs(f(x)), x),
                                                             std::make_pair(std::abs(f(x1)), x1),
                                                             std::make_pair(std::abs(f(x2)), x2),
                                                             std::make_pair(std::abs(f(xx1)), xx1),
                                                             std::make_pair(std::abs(f(xx2)), xx2)};
    
    auto rs = std::min_element(cand_arr.begin(), cand_arr.end(), is_less);
    
    if (std::isnan(rs->first))
    {
        return generic_nan<FloatType>();
    }

    return rs->second;
}

// ------
//let's keep everything float as a gold standard for now, we'd want to fix that later

using tm_float_t    = float;
using mdc_float_t   = float;
using eval_float_t  = float;

class MatrixProjectorInterface
{
    public:

        virtual ~MatrixProjectorInterface() noexcept = default;
        virtual auto project(const std::shared_ptr<Matrix>& matrix) -> std::shared_ptr<Matrix> = 0;
};

class MatrixInterface : public virtual MatrixProjectorInterface
{
    public:

        virtual ~MatrixInterface() noexcept = default;
        virtual auto get_coefficient_vector() -> std::vector<matrix_std_float_t> = 0;
        virtual void set_coefficient_vector(const std::vector<matrix_std_float_t>& coeff_vec) = 0;
        virtual auto clone() -> std::shared_ptr<MatrixInterface> = 0;
};

class TimeMachineInterface
{
    public:

        virtual ~TimeMachineInterface() noexcept = default;
        virtual auto f(matrix_std_float_t t) -> tm_float_t = 0;
};

class OptimalityApproximatorInterface
{
    public:

        virtual ~OptimalityApproximatorInterface() noexcept = default;
        virtual auto approx_x(TimeMachineInterface& time_machine, matrix_std_float_t x) -> matrix_std_float_t = 0;
};

class TimeMachineOptimizerInterface
{
    public:

        virtual ~TimeMachineOptimizerInterface() noexcept = default;
        virtual auto optimize(TimeMachineInterface& time_machine) -> matrix_std_float_t = 0;
};

class MatrixDeviationCalculatorInterface
{
    public:

        virtual ~MatrixDeviationCalculatorInterface() noexcept = default;
        virtual auto get_deviation(const std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>>& arg) -> mdc_float_t = 0;
};

class MatrixEvaluatorInterface
{
    public:

        virtual ~MatrixEvaluatorInterface() noexcept = default;
        virtual auto get_deviation(MatrixProjectorInterface& matrix_projector) -> eval_float_t = 0;
};

class TemporalCoefficientProjectorInterface
{
    public:

        virtual ~TemporalCoefficientProjectorInterface() noexcept = default;
        virtual auto project(matrix_std_float_t t) -> std::vector<matrix_std_float_t> = 0;
};

class TemporalCoefficientOptimizerInterface
{
    public:

        virtual ~TemporalCoefficientOptimizerInterface() noexcept = default;
        virtual auto optimize(MatrixInterface& the_matrix,
                              TemporalCoefficientProjectorInterface& projector,
                              MatrixEvaluatorInterface& deviation_extractor,
                              TimeMachineOptimizerInterface& time_machine_optimizer) -> std::vector<matrix_std_float_t> = 0;
};

class MatrixOptimizerInterface
{
    public:

        virtual ~MatrixOptimizerInterface() noexcept = default;
        virtual auto optimize(MatrixInterface& matrix,
                              const std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>>& gold_std) -> std::shared_ptr<MatrixInterface> = 0;
};

// ------

template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
class TheMatrix: public virtual MatrixInterface
{
    private:

        std::vector<size_t> focal_sz_vec;
        std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
        std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map;
        std::vector<size_t> rotation_sz_vec;
        std::vector<double> parameter_bound_ratio_vec;
        bool has_process_unit_logit_reuse_tag;
        bool has_process_group_logit_reuse_tag;
        bool has_being_logit_reuse_tag;
        bool has_base_matrix_logit_reuse_tag;
        std::vector<matrix_std_float_t> coeff_vec;
        std::vector<matrix_std_float_t> shape_coeff_vec;
        matrix_std_float_t pe_amplitude;
        matrix_std_float_t pe_frequency_multiplier;
        matrix_std_float_t pe_amplitude_decay_rate;
        matrix_std_float_t pe_frequency_multiplier_decay_rate;

    public:

        using self = TheMatrix;

        TheMatrix(std::vector<size_t> focal_sz_vec,
                  std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                  std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> accum_suffix_map,
                  std::vector<size_t> rotation_sz_vec,
                  std::vector<double> parameter_bound_ratio_vec,
                  const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>,
                  const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>,
                  const Tag<TaylorBasePromotedFloatType>,
                  const Tag<ShapeBasePromotedFloatType>,
                  bool has_process_unit_logit_reuse_tag,
                  bool has_process_group_logit_reuse_tag,
                  bool has_being_logit_reuse_tag,
                  bool has_base_matrix_logit_reuse_tag,
                  std::vector<matrix_std_float_t> coeff_vec,
                  std::vector<matrix_std_float_t> shape_coeff_vec,
                  matrix_std_float_t pe_amplitude,
                  matrix_std_float_t pe_frequency_multiplier,
                  matrix_std_float_t pe_amplitude_decay_rate,
                  matrix_std_float_t pe_frequency_multiplier_decay_rate) noexcept: focal_sz_vec(std::move(focal_sz_vec)),
                                                                                   focal_suffix_map(std::move(focal_suffix_map)),
                                                                                   accum_suffix_map(std::move(accum_suffix_map)),
                                                                                   rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                                   parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                                   has_process_unit_logit_reuse_tag(has_process_unit_logit_reuse_tag),
                                                                                   has_process_group_logit_reuse_tag(has_process_group_logit_reuse_tag),
                                                                                   has_being_logit_reuse_tag(has_being_logit_reuse_tag),
                                                                                   has_base_matrix_logit_reuse_tag(has_base_matrix_logit_reuse_tag),
                                                                                   coeff_vec(std::move(coeff_vec)),
                                                                                   shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                                   pe_amplitude(pe_amplitude),
                                                                                   pe_frequency_multiplier(pe_frequency_multiplier),
                                                                                   pe_amplitude_decay_rate(pe_amplitude_decay_rate),
                                                                                   pe_frequency_multiplier_decay_rate(pe_frequency_multiplier_decay_rate){}

        auto project(const std::shared_ptr<Matrix>& matrix) -> std::shared_ptr<Matrix>
        {
            size_t coeff_arr_offset         = 0u;
            size_t shape_coeff_arr_offset   = 0u;

            return matrix_transform(matrix,
                                    this->focal_sz_vec,
                                    this->focal_suffix_map,
                                    this->accum_suffix_map,
                                    this->rotation_sz_vec,
                                    this->parameter_bound_ratio_vec,
                                    to_size_container(std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>{}),
                                    this->coeff_vec.data(), coeff_arr_offset, this->coeff_vec.size(),
                                    to_size_container(std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>{}),
                                    this->shape_coeff_vec.data(), shape_coeff_arr_offset, this->shape_coeff_vec.size(),
                                    this->pe_amplitude, this->pe_frequency_multiplier, this->pe_amplitude_decay_rate, this->pe_frequency_multiplier_decay_rate,
                                    Tag<TaylorBasePromotedFloatType>{},
                                    Tag<ShapeBasePromotedFloatType>{},
                                    has_process_unit_logit_reuse_tag,
                                    has_process_group_logit_reuse_tag,
                                    has_being_logit_reuse_tag,
                                    has_base_matrix_logit_reuse_tag);
        }

        auto get_coefficient_vector() -> std::vector<matrix_std_float_t>
        {
            std::vector<matrix_std_float_t> rs{};

            std::copy(this->coeff_vec.begin(), this->coeff_vec.end(), std::back_inserter(rs));
            std::copy(this->shape_coeff_vec.begin(), this->shape_coeff_vec.end(), std::back_inserter(rs));

            return rs;
        }

        void set_coefficient_vector(const std::vector<matrix_std_float_t>& new_coeff_vec)
        {
            if (this->coeff_vec.size() + this->shape_coeff_vec.size() != new_coeff_vec.size())
            {
                throw std::runtime_error("invalid new_coeff_vec shape");
            }

            std::vector<matrix_std_float_t> shadow_coeff_vec(this->coeff_vec.size());
            std::vector<matrix_std_float_t> shadow_shape_coeff_vec(this->shape_coeff_vec.size());

            for (size_t i = 0u; i < shadow_coeff_vec.size(); ++i)
            {
                if (std::isnan(new_coeff_vec[i]))
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }

                shadow_coeff_vec[i] = new_coeff_vec[i];
            }

            for (size_t i = 0u; i < shadow_shape_coeff_vec.size(); ++i)
            {
                shadow_shape_coeff_vec[i] = radian_normalize(new_coeff_vec[i + shadow_coeff_vec.size()]);

                if (std::isnan(shadow_shape_coeff_vec[i]))
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }
            }

            this->coeff_vec         = std::move(shadow_coeff_vec);
            this->shape_coeff_vec   = std::move(shadow_shape_coeff_vec);
        }

        auto clone() -> std::shared_ptr<MatrixInterface>
        {
            return std::make_shared<self>(*this);
        }
};

template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ, class TaylorBasePromotedFloatType, class ShapeBasePromotedFloatType>
void check_make_the_matrix_args(const std::vector<size_t>& matrix_shape,
                                const std::vector<size_t>& focal_sz_vec,
                                const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                                const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,
                                const std::vector<size_t>& rotation_sz_vec,
                                const std::vector<double>& parameter_bound_ratio_vec,
                                matrix_std_float_t pe_amplitude,
                                matrix_std_float_t pe_frequency_multiplier,
                                matrix_std_float_t pe_amplitude_decay_rate,
                                matrix_std_float_t pe_frequency_multiplier_decay_rate,
                                const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>& taylor_base_coeff_sz,
                                const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>& shape_base_coeff_sz,
                                const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag,
                                const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag,
                                bool has_process_unit_logit_reuse_tag,
                                bool has_process_group_logit_reuse_tag,
                                bool has_being_logit_reuse_tag,
                                bool has_base_matrix_logit_reuse_tag)
{
    (void) matrix_shape;
}

template <size_t TAYLOR_BASE_COEFF_SZ, size_t SHAPE_BASE_COEFF_SZ,
          class TaylorBasePromotedFloatType = matrix_std_float_t, class ShapeBasePromotedFloatType = matrix_std_float_t>
auto make_the_matrix(const std::vector<size_t>& matrix_shape,
                     const std::vector<size_t>& focal_sz_vec,
                     const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& focal_suffix_map,
                     const std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>& accum_suffix_map,
                     const std::vector<size_t>& rotation_sz_vec,
                     const std::vector<double>& parameter_bound_ratio_vec,
                     matrix_std_float_t pe_amplitude,
                     matrix_std_float_t pe_frequency_multiplier,
                     matrix_std_float_t pe_amplitude_decay_rate,
                     matrix_std_float_t pe_frequency_multiplier_decay_rate,
                     const std::integral_constant<size_t, TAYLOR_BASE_COEFF_SZ>& taylor_base_coeff_sz,
                     const std::integral_constant<size_t, SHAPE_BASE_COEFF_SZ>& shape_base_coeff_sz,
                     const Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = Tag<TaylorBasePromotedFloatType>{},
                     const Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = Tag<ShapeBasePromotedFloatType>{},
                     bool has_process_unit_logit_reuse_tag = true,
                     bool has_process_group_logit_reuse_tag = true,
                     bool has_being_logit_reuse_tag = true,
                     bool has_base_matrix_logit_reuse_tag = true) -> std::unique_ptr<MatrixInterface>
{
    check_make_the_matrix_args(matrix_shape,
                               focal_sz_vec,
                               focal_suffix_map,
                               accum_suffix_map,
                               rotation_sz_vec,
                               parameter_bound_ratio_vec,
                               pe_amplitude,
                               pe_frequency_multiplier,
                               pe_amplitude_decay_rate,
                               pe_frequency_multiplier_decay_rate,
                               taylor_base_coeff_sz,
                               shape_base_coeff_sz,
                               taylor_base_promotion_tag,
                               shape_base_promotion_tag,
                               has_process_unit_logit_reuse_tag,
                               has_process_group_logit_reuse_tag,
                               has_being_logit_reuse_tag,
                               has_base_matrix_logit_reuse_tag);

    constexpr size_t LOGIT_VEC_CAPACITY = size_t{1} << 28u;

    std::vector<matrix_std_float_t> coeff_vec(LOGIT_VEC_CAPACITY);
    std::vector<matrix_std_float_t> shape_coeff_vec(LOGIT_VEC_CAPACITY);

    size_t coeff_vec_sz         = 0u;
    size_t shape_coeff_vec_sz   = 0u;

    matrix_transform(make_matrix_from_shape_vec(matrix_shape),
                     focal_sz_vec,
                     focal_suffix_map,
                     accum_suffix_map,
                     rotation_sz_vec,
                     parameter_bound_ratio_vec,
                     to_size_container(taylor_base_coeff_sz),
                     coeff_vec.data(), coeff_vec_sz, LOGIT_VEC_CAPACITY,
                     to_size_container(shape_base_coeff_sz),
                     shape_coeff_vec.data(), shape_coeff_vec_sz, LOGIT_VEC_CAPACITY,
                     pe_amplitude, pe_frequency_multiplier, pe_amplitude_decay_rate, pe_frequency_multiplier_decay_rate,
                     taylor_base_promotion_tag,
                     shape_base_promotion_tag,
                     has_process_unit_logit_reuse_tag,
                     has_process_group_logit_reuse_tag,
                     has_being_logit_reuse_tag,
                     has_base_matrix_logit_reuse_tag);

    TheMatrix matrix(focal_sz_vec,
                     focal_suffix_map,
                     accum_suffix_map,
                     rotation_sz_vec,
                     parameter_bound_ratio_vec,
                     taylor_base_coeff_sz,
                     shape_base_coeff_sz,
                     taylor_base_promotion_tag,
                     shape_base_promotion_tag,
                     has_process_unit_logit_reuse_tag,
                     has_process_group_logit_reuse_tag,
                     has_being_logit_reuse_tag,
                     has_base_matrix_logit_reuse_tag,
                     std::vector<matrix_std_float_t>(coeff_vec_sz, 0.f),
                     std::vector<matrix_std_float_t>(shape_coeff_vec_sz, 0.f),
                     pe_amplitude,
                     pe_frequency_multiplier,
                     pe_amplitude_decay_rate,
                     pe_frequency_multiplier_decay_rate);

    return std::make_unique<decltype(matrix)>(std::move(matrix));
}

class ImmutableShapeCachedMatrixProjector: public virtual MatrixProjectorInterface
{
    private:

        std::shared_ptr<MatrixProjectorInterface> base_matrix;
        std::unordered_map<std::string, std::shared_ptr<Matrix>> cache_map;
        size_t cache_map_capacity;

    public:

        ImmutableShapeCachedMatrixProjector(std::shared_ptr<MatrixProjectorInterface> base_matrix,
                                            size_t cache_map_capacity)
        {
            if (base_matrix == nullptr)
            {
                throw std::runtime_error("invalid argument");
            }

            this->base_matrix           = std::move(base_matrix);
            this->cache_map             = std::unordered_map<std::string, std::shared_ptr<Matrix>>();
            this->cache_map_capacity    = cache_map_capacity;
        }

        auto project(const std::shared_ptr<Matrix>& matrix) -> std::shared_ptr<Matrix>
        {
            std::string serialized_matrix   = immutable_shape_matrix_to_unique_representation(matrix);

            if (auto map_ptr = this->cache_map.find(serialized_matrix); map_ptr != this->cache_map.end())
            {
                return map_ptr->second;
            }

            std::shared_ptr<Matrix> result  = this->base_matrix->project(matrix);

            if (this->cache_map.size() == this->cache_map_capacity)
            {
                this->cache_map.clear();
            }

            this->cache_map[serialized_matrix]  = result;

            return result;
        }

        void clear_cache() noexcept
        {
            this->cache_map.clear();
        }
};

// ------

class SpaceOperationFacility
{
    public:

        template <class FloatType, class PromotedFloatType = FloatType>
        static auto coordinate_distance(FloatType * coor_arr, size_t coor_arr_sz) -> FloatType
        {
            static_assert(std::is_floating_point_v<FloatType>);
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            PromotedFloatType rs = 0;

            for (size_t i = 0u; i < coor_arr_sz; ++i)
            {
                rs += static_cast<PromotedFloatType>(coor_arr[i]) * coor_arr[i];
            }

            return std::sqrt(rs);
        }
};

template <class Signature = void>
class RandomizerFacility
{
    public:

        template <class FloatType, class PromotedFloatType = FloatType>
        static auto randomize_fixed_point_float(FloatType first, FloatType last, size_t discretization_sz,
                                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> FloatType
        {
            static_assert(std::is_floating_point_v<FloatType>);
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            const size_t MIN_DISCRETIZATION_SZ  = 1u;
            const size_t MAX_DISCRETIZATION_SZ  = size_t{1} << 30;

            if (std::clamp(discretization_sz, MIN_DISCRETIZATION_SZ, MAX_DISCRETIZATION_SZ) != discretization_sz) [[unlikely]]
            {
                throw std::runtime_error("invalid argument");
            }

            if (std::isnan(first)) [[unlikely]]
            {
                throw std::runtime_error("invalid argument");
            }

            if (std::isnan(last)) [[unlikely]]
            {
                throw std::runtime_error("invalid argument");
            }

            if (first > last) [[unlikely]]
            {
                throw std::runtime_error("invalid argument");
            }

            static auto randomizer = std::bind(std::uniform_int_distribution<size_t>(0, discretization_sz - 1u), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

            PromotedFloatType discrete_unit = (static_cast<PromotedFloatType>(last) - first) / discretization_sz;
            PromotedFloatType rs = discrete_unit * (randomizer() % discretization_sz);

            return float_clamp<FloatType>(rs, first, last);
        }

        static auto randomize_uint(size_t first, size_t last) -> size_t
        {
            if (first >= last)
            {
                throw std::runtime_error("invalid argument");
            }

            static auto randomizer = std::bind(std::uniform_int_distribution<size_t>(first, last - 1), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

            return randomizer();
        }

        template <class FloatType, class PromotedFloatType = FloatType>
        static auto randomize_exponential_float(FloatType base,
                                                FloatType exponent_first, FloatType exponent_last, size_t discretization_sz,
                                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> FloatType
        {
            static_assert(std::is_floating_point_v<FloatType>);
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            if (std::isnan(base))
            {
                throw std::runtime_error("invalid argument");
            }

            if (std::signbit(base))
            {
                throw std::runtime_error("invalid argument");
            }

            return std::pow(base, randomize_fixed_point_float<FloatType, PromotedFloatType>(exponent_first, exponent_last, discretization_sz));
        }

        static auto flip_a_coin() -> bool
        {
            static auto randomizer = std::bind(std::uniform_int_distribution<uint8_t>(0, 1), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

            return static_cast<bool>(randomizer());
        }
};

template <class Signature = void>
class ThreadSafeRandomizerFacility
{
    public:

        using Base = RandomizerFacility<ThreadSafeRandomizerFacility<Signature>>;

        template <class ...Args>
        static auto randomize_fixed_point_float(Args&& ...args) -> decltype(auto)
        {
            static std::mutex mtx{};

            std::lock_guard<std::mutex> lck_grd(mtx);
            return Base::randomize_fixed_point_float(std::forward<Args>(args)...);
        }

        template <class ...Args>
        static auto randomize_uint(Args&& ...args) -> decltype(auto)
        {
            static std::mutex mtx{};

            std::lock_guard<std::mutex> lck_grd(mtx);
            return Base::randomize_uint(std::forward<Args>(args)...);
        }

        template <class ...Args>
        static auto randomize_exponenial_foat(Args&& ...args) -> decltype(auto)
        {
            static std::mutex mtx{};

            std::lock_guard<std::mutex> lck_grd(mtx);
            return Base::randomize_exponential_float(std::forward<Args>(args)...);
        }

        template <class ...Args>
        static auto flip_a_coin(Args&& ...args) -> decltype(auto)
        {
            static std::mutex mtx{};

            std::lock_guard<std::mutex> lck_grd(mtx);
            return Base::flip_a_coin(std::forward<Args>(args)...);
        }
};

template <class Base = RandomizerFacility<void>>
class ApplicationRandomizerFacility
{
    public:

        static auto randomize_focal(bool has_sign = false) -> double
        {
            const double MIN_RESULT_VALUE           = 0.0000001;
            const double MAX_RESULT_VALUE           = size_t{1} << 20;

            const double BASE_FIRST                 = 0.1f;
            const double BASE_LAST                  = 10.f;
            const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000ULL;

            const double EXPONENT_FIRST             = -6.f;
            const double EXPONENT_LAST              = 6.f;
            const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000ULL;

            const double tentative_result           = Base::randomize_exponential_float(Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ),
                                                                                        EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);

            if (has_sign)
            {
                const double signness = Base::flip_a_coin() ? -1.0 : 1.0;
                return float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE) * signness;
            }
            else
            {
                return float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE);    
            }
        }

        static auto randomize_percentage_focal() -> double
        {
            const double MIN_RESULT_VALUE           = 0.0000001;
            const double MAX_RESULT_VALUE           = size_t{1} << 20;

            const double BASE_FIRST                 = 1.0;
            const double BASE_LAST                  = 10.0;
            const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000ULL;

            const double EXPONENT_FIRST             = 0;
            const double EXPONENT_LAST              = 6;
            const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000ULL;

            const double base                       = Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ);
            const double norm_threshold             = std::pow(base, EXPONENT_LAST);
            const double pre_denormed_value         = Base::randomize_exponential_float(base, EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);
            const double denormed_value             = pre_denormed_value / norm_threshold;

            return float_clamp(denormed_value, MIN_RESULT_VALUE, MAX_RESULT_VALUE);
        }
};

template <class Base = RandomizerFacility<void>>
class VectorRandomizerFacility
{
    private:

        using SpaceOpsFacility = SpaceOperationFacility;

    public:

        static auto randomize_space(size_t dimension_sz,
                                    const double value_first             = -10,
                                    const double value_last              = 10,
                                    const size_t value_discretization_sz = 1'000'000'000ULL) -> std::vector<double>
        {            
            auto gen = [=]{
                return Base::randomize_fixed_point_float(value_first, value_last, value_discretization_sz);
            };

            std::vector<double> rs(dimension_sz);
            std::generate(rs.begin(), rs.end(), gen);

            return rs;
        }

        static auto randomize_unit_space(size_t dimension_sz) -> std::vector<double>
        {
            const size_t LUCKY_ITERATION_SZ = 10u;

            for (size_t i = 0u; i < LUCKY_ITERATION_SZ; ++i)
            {
                std::vector<double> random_space    = randomize_space(dimension_sz);
                std::vector<double> tentative_rs    = div_vector(random_space, SpaceOpsFacility::coordinate_distance<double>(random_space.data(), random_space.size()));
                bool flag                           = false;

                for (double e: tentative_rs)
                {
                    if (std::isnan(e))
                    {
                        flag = true;
                        break;
                    }
                }

                if (!flag)
                {
                    return tentative_rs;
                }
            }

            throw std::runtime_error("randomization went wrong");
        }

        static auto randomize_radian_space(size_t dimension_sz) -> std::vector<double>
        {
            constexpr double PI_FIRST                   = 0;
            constexpr double PI_LAST                    = std::numbers::pi_v<double>;
            constexpr size_t PI_DISCRETIZATION_SZ       = 1'000'000'000ULL;

            auto pi_random_gen = [=]
            {
                return Base::randomize_fixed_point_float(PI_FIRST, PI_LAST, PI_DISCRETIZATION_SZ);
            };

            std::vector<double> rs(dimension_sz);
            std::generate(rs.begin(), rs.end(), pi_random_gen);

            return rs;
        }
};

// ------

template <class PromotedFloatType = matrix_std_float_t>
class FirstOrderNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
{
    private:

        size_t iteration_sz;
        matrix_std_float_t x_a;

    public:

        FirstOrderNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                    matrix_std_float_t x_a) noexcept: iteration_sz(iteration_sz),
                                                                                      x_a(x_a){}

        auto approx_x(TimeMachineInterface& time_machine, matrix_std_float_t x) -> matrix_std_float_t
        {
            auto lambda_wrapped_time_machine = [&time_machine](matrix_std_float_t t)
            {
                return time_machine.f(t);
            };

            auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

            return newton_first_order_approx(lambda_wrapped_time_machine,
                                             static_cast<PromotedFloatType>(x),
                                             this->iteration_sz,
                                             derivative_func);
        }
};

template <class PromotedFloatType = matrix_std_float_t>
class SecondOrderNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
{
    private:

        size_t iteration_sz;
        matrix_std_float_t x_a;

    public:

        SecondOrderNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                     matrix_std_float_t x_a) noexcept: iteration_sz(iteration_sz),
                                                                                       x_a(x_a){}

        auto approx_x(TimeMachineInterface& time_machine, matrix_std_float_t x) -> matrix_std_float_t
        {
            auto lambda_wrapped_time_machine = [&time_machine](matrix_std_float_t t)
            {
                return time_machine.f(t);
            };

            auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

            return newton_second_order_approx(lambda_wrapped_time_machine,
                                              static_cast<PromotedFloatType>(x),
                                              this->iteration_sz,
                                              derivative_func);
        }
};

class OptimalityApproximatorFactory
{
    private:

        struct Signature{};
        using Randomizer = RandomizerFacility<Signature>;

    public:

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_first_order_newton_naive_optimality_approximator(matrix_std_float_t x_a = to_precise_float_conversion_initializer<double>(0.001)) -> std::unique_ptr<OptimalityApproximatorInterface>
        {
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            const size_t ITERATION_SZ_FIRST         = 1u;
            const size_t ITERATION_SZ_LAST          = 10u;

            const matrix_std_float_t MIN_X_A        = to_precise_float_conversion_initializer<double>(0.00000001);
            const matrix_std_float_t MAX_X_A        = to_precise_float_conversion_initializer<double>(1);

            if (std::isnan(x_a))
            {
                throw std::runtime_error("invalid derivative deviation");
            }

            if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
            {
                throw std::runtime_error("invalid derivative deviation");
            }

            return std::make_unique<FirstOrderNewtonNaiveOptimalityApproximator<PromotedFloatType>>(Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST),
                                                                                                    x_a);
        };

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_second_order_newton_naive_optimality_approximator(matrix_std_float_t x_a = to_precise_float_conversion_initializer<double>(0.001)) -> std::unique_ptr<OptimalityApproximatorInterface>
        {
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            const size_t ITERATION_SZ_FIRST         = 1u;
            const size_t ITERATION_SZ_LAST          = 10u;

            const matrix_std_float_t MIN_X_A        = to_precise_float_conversion_initializer<double>(0.00000001);
            const matrix_std_float_t MAX_X_A        = to_precise_float_conversion_initializer<double>(1);

            if (std::isnan(x_a))
            {
                throw std::runtime_error("invalid derivative deviation");
            }

            if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
            {
                throw std::runtime_error("invalid derivative deviation");
            }

            return std::make_unique<SecondOrderNewtonNaiveOptimalityApproximator<PromotedFloatType>>(Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST),
                                                                                                     x_a);
        }
};

// ------

class LinearTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
{
    private:

        matrix_std_float_t seed;
        matrix_std_float_t step;
        size_t step_count;
        std::unique_ptr<OptimalityApproximatorInterface> optimality_approximator;

    public:

        LinearTimeMachineOptimizer(matrix_std_float_t seed,
                                   matrix_std_float_t step,
                                   size_t step_count,
                                   std::unique_ptr<OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                       step(step),
                                                                                                                       step_count(step_count),
                                                                                                                       optimality_approximator(std::move(optimality_approximator)){}

        auto optimize(TimeMachineInterface& time_machine) -> matrix_std_float_t
        {
            std::optional<matrix_std_float_t> best_x    = std::nullopt;
            std::optional<tm_float_t> best_y            = std::nullopt;

            for (size_t i = 0u; i < this->step_count; ++i)
            {
                matrix_std_float_t x = this->seed + i * this->step;

                if (std::isnan(x))
                {
                    continue;
                }

                matrix_std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                if (std::isnan(new_x))
                {
                    continue;
                }

                tm_float_t new_y = time_machine.f(new_x);

                if (std::isnan(new_y))
                {
                    continue;
                }

                if (!best_y.has_value())
                {
                    best_y = new_y;
                    best_x = new_x;
                }

                if (best_y.value() > new_y)
                {
                    best_y = new_y;
                    best_x = new_x;
                }
            }

            if (!best_x.has_value())
            {
                return generic_nan<matrix_std_float_t>();
            }

            return best_x.value();
        }
};

class ExponentialTimeMachineOptimizer: public virtual TimeMachineOptimizerInterface
{
    private:

        matrix_std_float_t seed;
        matrix_std_float_t exp_base;
        size_t step_count;
        std::unique_ptr<OptimalityApproximatorInterface> optimality_approximator;
    
    public:

        ExponentialTimeMachineOptimizer(matrix_std_float_t seed,
                                        matrix_std_float_t exp_base,
                                        size_t step_count,
                                        std::unique_ptr<OptimalityApproximatorInterface> optimality_approximator) noexcept: seed(seed),
                                                                                                                            exp_base(exp_base),
                                                                                                                            step_count(step_count),
                                                                                                                            optimality_approximator(std::move(optimality_approximator)){}

        auto optimize(TimeMachineInterface& time_machine) -> matrix_std_float_t
        {
            std::optional<matrix_std_float_t> best_x    = std::nullopt;
            std::optional<tm_float_t> best_y            = std::nullopt;

            for (size_t i = 0u; i < this->step_count; ++i)
            {
                matrix_std_float_t x = this->seed + std::pow(this->exp_base, i);

                if (std::isnan(x))
                {
                    continue;
                }

                matrix_std_float_t new_x = this->optimality_approximator->approx_x(time_machine, x);

                if (std::isnan(new_x))
                {
                    continue;
                }

                tm_float_t new_y = time_machine.f(new_x);

                if (std::isnan(new_y))
                {
                    continue;
                }

                if (!best_y.has_value())
                {
                    best_y = new_y;
                    best_x = new_x;
                }

                if (best_y.value() > new_y)
                {
                    best_y = new_y;
                    best_x = new_x;
                }
            }

            if (!best_x.has_value())
            {
                return generic_nan<matrix_std_float_t>();
            }

            return best_x.value();
        }
};

class TimeMachineOptimizerFactory
{
    private:

        struct Signature{};

        using Randomizer            = RandomizerFacility<Signature>;
        using ApplicationRandomizer = ApplicationRandomizerFacility<Randomizer>;

    public:

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_taylor_linear_time_machine_optimizer() -> std::unique_ptr<TimeMachineOptimizerInterface>
        {
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            const size_t STEP_COUNT_FIRST       = 1u;
            const size_t STEP_COUNT_LAST        = size_t{1} << 4;

            double seed                         = ApplicationRandomizer::randomize_focal();
            double step                         = ApplicationRandomizer::randomize_focal();
            size_t step_count                   = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

            return std::make_unique<LinearTimeMachineOptimizer>(seed,
                                                                step,
                                                                step_count,
                                                                OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>());
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_taylor_exponential_time_machine_optimizer() -> std::unique_ptr<TimeMachineOptimizerInterface>
        {
            static_assert(std::is_floating_point_v<PromotedFloatType>);

            const double EXP_BASE_FIRST                 = 0.1f;
            const double EXP_BASE_LAST                  = 10.f;
            const size_t EXP_BASE_DISCRETIZATION_SZ     = 100u;

            const size_t STEP_COUNT_FIRST               = 1u;
            const size_t STEP_COUNT_LAST                = size_t{1} << 4;

            double seed                                 = ApplicationRandomizer::randomize_focal();
            double exp_base                             = Randomizer::randomize_fixed_point_float(EXP_BASE_FIRST, EXP_BASE_LAST, EXP_BASE_DISCRETIZATION_SZ);
            size_t step_count                           = Randomizer::randomize_uint(STEP_COUNT_FIRST, STEP_COUNT_LAST);

            return std::make_unique<ExponentialTimeMachineOptimizer>(seed,
                                                                     exp_base,
                                                                     step_count,
                                                                     OptimalityApproximatorFactory::get_second_order_newton_naive_optimality_approximator<PromotedFloatType>());
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_taylor_time_machine_optimizer() -> std::unique_ptr<TimeMachineOptimizerInterface>
        {
            if (Randomizer::flip_a_coin())
            {
                return get_random_taylor_linear_time_machine_optimizer<PromotedFloatType>();
            }
            else
            {
                return get_random_taylor_exponential_time_machine_optimizer<PromotedFloatType>();
            }
        }
};

// ------

class CachedTimeMachine: public virtual TimeMachineInterface
{
    private:

        std::shared_ptr<TimeMachineInterface> base;
        std::unordered_map<matrix_std_float_t, tm_float_t> cached_result;
        size_t cache_map_capacity;

    public:

        CachedTimeMachine(std::shared_ptr<TimeMachineInterface> base,
                          size_t cache_map_capacity)
        {
            this->base                  = std::move(base);
            this->cached_result         = std::unordered_map<matrix_std_float_t, tm_float_t>{};
            this->cache_map_capacity    = cache_map_capacity;
        }

        auto f(matrix_std_float_t t) -> tm_float_t
        {
            if (auto map_ptr = this->cached_result.find(t); map_ptr != this->cached_result.end())
            {
                return map_ptr->second;
            }

            tm_float_t new_result = this->base->f(t);

            if (this->cached_result.size() == this->cache_map_capacity)
            {
                this->cached_result.clear();
            }

            this->cached_result.insert({t, new_result});

            return new_result;
        }

        void clear_cache_map() noexcept
        {
            this->cached_result.clear();
        }
};

// ------

//in real life applicable solution, we'd want to make sure that we discretize the neural network appropriately and there is no "spikes" for production usages, this is an advanced topic
//we'd clamp the input into the "defined range" or numerical value so we could consistently and reliably return output to users
//if we could tell, this is actually a tree shape of unordered accum and pairwise operations of deviation

template <class PromotedFloatType = matrix_std_float_t>
class MatrixSquareDeviationCalculator: public virtual MatrixDeviationCalculatorInterface
{
    public:

        static_assert(std::is_floating_point_v<PromotedFloatType>);

        auto get_deviation(const std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>>& arg) -> mdc_float_t
        {
            PromotedFloatType prenormed_result                  = 0;
            std::vector<matrix_std_float_t> lhs_logit_vec       = {};
            std::vector<matrix_std_float_t> rhs_logit_vec       = {};
            std::optional<size_t> flat_shape                    = std::nullopt;

            for (const auto& [lhs, rhs]: arg)
            {
                lhs_logit_vec.clear();
                rhs_logit_vec.clear();

                flatten(lhs, lhs_logit_vec);
                flatten(rhs, rhs_logit_vec);

                if (!flat_shape.has_value())
                {
                    flat_shape = lhs_logit_vec.size();
                }

                if (flat_shape.value() != lhs_logit_vec.size())
                {
                    throw std::runtime_error("incompatible shape for square deviation calculator");
                }

                if (flat_shape.value() != rhs_logit_vec.size())
                {
                    throw std::runtime_error("incompatible shape for square deviation calculator");
                }

                if (is_vec_contains_nan(lhs_logit_vec))
                {
                    return generic_nan<mdc_float_t>();
                }

                if (is_vec_contains_nan(rhs_logit_vec))
                {
                    return generic_nan<mdc_float_t>();
                }

                if (PromotedFloatType incremental_value = this->get_pair_deviation(lhs_logit_vec, rhs_logit_vec); !std::isnan(incremental_value))
                {
                    prenormed_result += incremental_value;
                }
                else
                {
                    return generic_nan<mdc_float_t>();
                }
            }

            return static_cast<mdc_float_t>(prenormed_result / safe_non_zero_access(arg.size()));
        }

    private:

        auto is_vec_contains_nan(const std::vector<matrix_std_float_t>& arg) -> bool
        {
            for (const auto& e: arg)
            {
                if (std::isnan(e))
                {
                    return true;
                }
            }

            return false;
        }

        auto get_pair_deviation(const std::vector<matrix_std_float_t>& lhs_logit_vec, const std::vector<matrix_std_float_t>& rhs_logit_vec) -> PromotedFloatType
        {
            PromotedFloatType sum_sqr_difference = 0;

            if (lhs_logit_vec.size() != rhs_logit_vec.size())
            {
                throw std::runtime_error("incompatible matrix pair evaluation");
            }

            for (size_t i = 0u; i < lhs_logit_vec.size(); ++i)
            {
                PromotedFloatType difference        = static_cast<PromotedFloatType>(lhs_logit_vec[i]) - static_cast<PromotedFloatType>(rhs_logit_vec[i]);
                PromotedFloatType sqr_difference    = difference * difference;

                if (std::isnan(sqr_difference))
                {
                    return generic_nan<mdc_float_t>();
                }

                sum_sqr_difference += sqr_difference;
            }

            return sum_sqr_difference;
        }
};

class DeviationCalculatorFactory
{
    public:

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_prenormalized_deviation_calculator(const Tag<PromotedFloatType>& tag = Tag<PromotedFloatType>{})
        {
            return std::make_unique<MatrixSquareDeviationCalculator<PromotedFloatType>>();
        }
};

// ------

class PointWiseDeviationExtractor: public virtual MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>> input_output_pair_vec;
        std::unique_ptr<MatrixDeviationCalculatorInterface> deviation_calculator;

    public:

        PointWiseDeviationExtractor(std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>> input_output_pair_vec,
                                    std::unique_ptr<MatrixDeviationCalculatorInterface> deviation_calculator) noexcept: input_output_pair_vec(std::move(input_output_pair_vec)),
                                                                                                                        deviation_calculator(std::move(deviation_calculator)){}

        auto get_deviation(MatrixProjectorInterface& matrix_projector) -> eval_float_t
        {
            std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>> result_vec{};
            result_vec.reserve(this->input_output_pair_vec.size());

            for (const auto& [x, expected_y]: this->input_output_pair_vec)
            {
                std::shared_ptr<Matrix> y = matrix_projector.project(x);
                result_vec.push_back({expected_y, std::move(y)});
            }

            return this->deviation_calculator->get_deviation(result_vec);
        }
};

class ProductEvaluatorFactory
{
    public:

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_immutable_shape_product_evaluator(const std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>>& gold_std,
                                                          const Tag<PromotedFloatType>& tag = Tag<PromotedFloatType>{}) -> std::unique_ptr<MatrixEvaluatorInterface>
        {
            return std::make_unique<PointWiseDeviationExtractor>(gold_std,
                                                                 DeviationCalculatorFactory::get_prenormalized_deviation_calculator<PromotedFloatType>(tag));
        }
};

// ------

class PointCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
{
    private:

        std::vector<matrix_std_float_t> space;
    
    public:

        PointCoefficientProjector(std::vector<matrix_std_float_t> space) noexcept: space(space){}

        auto project(matrix_std_float_t t) -> std::vector<matrix_std_float_t>
        {
            return this->space;
        }
};

template <class PromotedFloatType = matrix_std_float_t>
class LineTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
{
    private:

        std::vector<PromotedFloatType> space;

    public:

        static_assert(std::is_floating_point_v<PromotedFloatType>);

        LineTemporalCoefficientProjector(std::vector<PromotedFloatType> space) noexcept: space(std::move(space)){}

        auto high_resolution_project(matrix_std_float_t t) -> std::vector<PromotedFloatType>
        {
            std::vector<PromotedFloatType> rs(this->space.size());

            restrict_scalar_mul_array<PromotedFloatType>(this->space.data(), this->space.size(),
                                                         t,
                                                         rs.data());

            return rs;
        }

        auto project(matrix_std_float_t t) -> std::vector<matrix_std_float_t>
        {
            return to_castable_vector_initializer(this->high_resolution_project(t));
        }
};

template <class PromotedFloatType = matrix_std_float_t>
class OvalTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
{
    private:

        LineTemporalCoefficientProjector<PromotedFloatType> base_projector;
        std::vector<PromotedFloatType> radian_space;
        std::vector<PromotedFloatType> radius_space;

    public:

        static_assert(std::is_floating_point_v<PromotedFloatType>);

        OvalTemporalCoefficientProjector(std::vector<PromotedFloatType> directional_space,
                                         std::vector<PromotedFloatType> radian_space,
                                         std::vector<PromotedFloatType> radius_space) noexcept: base_projector(std::move(directional_space)),
                                                                                                radian_space(std::move(radian_space)),
                                                                                                radius_space(std::move(radius_space)){}

        auto project(matrix_std_float_t t) -> std::vector<matrix_std_float_t>
        {
            std::vector<PromotedFloatType> radian_incremental_space = this->base_projector.high_resolution_project(t);
            std::vector<PromotedFloatType> current_radian_space     = std::vector<PromotedFloatType>(this->radian_space.size());

            restrict_add_array<PromotedFloatType>(this->radian_space.data(), radian_incremental_space.data(),
                                                  this->radian_space.size(),
                                                  current_radian_space.data());

            std::vector<PromotedFloatType> rs(this->radian_space.size());

            restrict_multidimensional_oval_to_euclidean_array<PromotedFloatType>(current_radian_space.data(), current_radian_space.size(),
                                                                                 this->radius_space.data(),
                                                                                 rs.data());

            return to_castable_vector_initializer(std::move(rs));
        }
};

class ChainedTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
{
    private:

        std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>> projector_vec;

    public:

        ChainedTemporalCoefficientProjector(std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>> projector_vec) noexcept: projector_vec(std::move(projector_vec)){}

        auto project(matrix_std_float_t t) -> std::vector<matrix_std_float_t>
        {
            std::optional<std::vector<matrix_std_float_t>> result_vec{};

            for (const auto& projector: this->projector_vec)
            {
                auto incremental_vec = projector->project(t);

                if (!result_vec.has_value())
                {
                    result_vec = std::move(incremental_vec);
                }
                else
                {
                    if (result_vec->size() != incremental_vec.size())
                    {
                        throw std::runtime_error("incompatible chained dimension size");
                    }
                    
                    result_vec = add_vector(*result_vec, incremental_vec);
                }
            }

            if (!result_vec.has_value())
            {
                throw std::runtime_error("internal corruption");
            }

            return std::move(result_vec.value());
        }
};

class CoefficientProjectorFactory
{
    private:

        static inline const matrix_std_float_t MIN_LOGIT_VALUE   = to_precise_float_conversion_initializer(0.0000001);
        static inline const matrix_std_float_t MAX_LOGIT_VALUE   = to_precise_float_conversion_initializer<double>(size_t{1} << 20);

        struct Signature{};

        using Randomizer            = RandomizerFacility<Signature>;
        using VectorRandomizer      = VectorRandomizerFacility<Randomizer>;
        using ApplicationRandomizer = ApplicationRandomizerFacility<Randomizer>;

        static auto get_random_radian_coordinate(size_t dimension_sz) -> std::vector<matrix_std_float_t>
        {
            return to_castable_vector_initializer(VectorRandomizer::randomize_radian_space(dimension_sz));
        }

        static auto get_random_vector(size_t coefficient_sz) -> std::vector<matrix_std_float_t>
        {
            return to_castable_vector_initializer(VectorRandomizer::randomize_space(coefficient_sz));
        }

        static auto clamp_vector(const std::vector<matrix_std_float_t>& vec) -> std::vector<matrix_std_float_t>
        {
            std::vector<matrix_std_float_t> rs{};

            for (const auto& e: vec)
            {
                rs.push_back(float_clamp(e, MIN_LOGIT_VALUE, MAX_LOGIT_VALUE));
            }

            return rs;
        }

        static auto get_random_unit_vector(size_t coefficient_sz) -> std::vector<matrix_std_float_t>
        {
            return to_castable_vector_initializer(VectorRandomizer::randomize_unit_space(coefficient_sz));
        }

        static auto base_get_random_deactivated_vector(const std::vector<matrix_std_float_t>& vec, size_t tentative_deactivated_sz) -> std::vector<matrix_std_float_t>
        {
            static auto random_device = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

            std::vector<std::pair<size_t, matrix_std_float_t>> enumerated_vec = enumerate_vector(vec);
            std::shuffle(enumerated_vec.begin(), enumerated_vec.end(), random_device);

            size_t deactivated_sz = std::min(vec.size(), tentative_deactivated_sz);

            for (size_t i = 0u; i < deactivated_sz; ++i)
            {
                enumerated_vec[i].second = 0;
            }

            return deenumerate_vector(enumerated_vec);
        }

        static auto get_random_deactivated_vector(const std::vector<matrix_std_float_t>& vec, double perc) -> std::vector<matrix_std_float_t>
        {
            return base_get_random_deactivated_vector(vec, vec.size() * perc);
        }

        static auto get_least_one_deactivated_vector(const std::vector<matrix_std_float_t>& vec, double perc) -> std::vector<matrix_std_float_t>
        {
            return base_get_random_deactivated_vector(vec, std::max(size_t{1}, static_cast<size_t>(vec.size() * perc)));
        }

        static auto randomize_deactivation_perc() -> double
        {
            return ApplicationRandomizer::randomize_percentage_focal();
        }

    public:

        static auto get_point_coefficient_projector(const std::vector<matrix_std_float_t>& coor) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            for (const auto& e: coor)
            {
                if (std::isnan(e))
                {
                    throw std::runtime_error("invalid argument");
                }
            }

            return std::make_unique<PointCoefficientProjector>(coor);
        }

        static auto get_chained_coefficient_projector(std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>> projector_vec) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            for (const auto& e: projector_vec)
            {
                if (e == nullptr)
                {
                    throw std::runtime_error("invalid argument");
                }
            }

            return std::make_unique<ChainedTemporalCoefficientProjector>(std::move(projector_vec));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_line0_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            std::vector<matrix_std_float_t> unit_vec = clamp_vector(get_least_one_deactivated_vector(get_random_unit_vector(coefficient_sz), randomize_deactivation_perc()));

            return std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(to_castable_vector_initializer<PromotedFloatType>(std::move(unit_vec)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_line_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            std::vector<matrix_std_float_t> unit_vec = clamp_vector(get_random_unit_vector(coefficient_sz));

            return std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(to_castable_vector_initializer<PromotedFloatType>(std::move(unit_vec)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_oval0_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            std::vector<matrix_std_float_t> directional_vec  = clamp_vector(get_least_one_deactivated_vector(get_random_unit_vector(coefficient_sz), randomize_deactivation_perc()));
            std::vector<matrix_std_float_t> radian_vec       = clamp_vector(get_least_one_deactivated_vector(get_random_radian_coordinate(coefficient_sz), randomize_deactivation_perc()));
            std::vector<matrix_std_float_t> radius_vec       = clamp_vector(mul_vector(get_least_one_deactivated_vector(get_random_unit_vector(coefficient_sz), randomize_deactivation_perc()),
                                                                                      static_cast<matrix_std_float_t>(ApplicationRandomizer::randomize_focal())));

            return std::make_unique<OvalTemporalCoefficientProjector<PromotedFloatType>>(to_castable_vector_initializer<PromotedFloatType>(std::move(directional_vec)),
                                                                                         to_castable_vector_initializer<PromotedFloatType>(std::move(radian_vec)),
                                                                                         to_castable_vector_initializer<PromotedFloatType>(std::move(radius_vec)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_oval_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            std::vector<matrix_std_float_t> directional_vec  = clamp_vector(get_random_unit_vector(coefficient_sz));
            std::vector<matrix_std_float_t> radian_vec       = clamp_vector(get_random_radian_coordinate(coefficient_sz));
            std::vector<matrix_std_float_t> radius_vec       = clamp_vector(mul_vector(get_random_unit_vector(coefficient_sz), static_cast<matrix_std_float_t>(ApplicationRandomizer::randomize_focal())));

            return std::make_unique<OvalTemporalCoefficientProjector<PromotedFloatType>>(to_castable_vector_initializer<PromotedFloatType>(std::move(directional_vec)),
                                                                                         to_castable_vector_initializer<PromotedFloatType>(std::move(radian_vec)),
                                                                                         to_castable_vector_initializer<PromotedFloatType>(std::move(radius_vec)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_rotating_arm0_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_oval0_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                        get_random_oval0_coefficient_projector<PromotedFloatType>(coefficient_sz)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_rotating_arm_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                        get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_line_oval0_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_line0_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                        get_random_oval0_coefficient_projector<PromotedFloatType>(coefficient_sz)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_line_oval_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_line_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                        get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz)));
        }

        template <class PromotedFloatType = matrix_std_float_t>
        static auto get_random_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
        {
            using projector_func_ptr = std::unique_ptr<TemporalCoefficientProjectorInterface> (*) (size_t);

            static std::vector<projector_func_ptr> func_ptr_vec{
                get_random_line0_coefficient_projector<PromotedFloatType>,
                get_random_line_coefficient_projector<PromotedFloatType>,
                get_random_oval0_coefficient_projector<PromotedFloatType>,
                get_random_oval_coefficient_projector<PromotedFloatType>,
                get_random_rotating_arm0_coefficient_projector<PromotedFloatType>,
                get_random_rotating_arm_coefficient_projector<PromotedFloatType>,
                get_random_line_oval0_coefficient_projector<PromotedFloatType>,
                get_random_line_oval_coefficient_projector<PromotedFloatType>
            };

            return func_ptr_vec[Randomizer::randomize_uint(0u, func_ptr_vec.size())](coefficient_sz);
        }
};

// ------

class TemporalCoefficientOptimizer: public virtual TemporalCoefficientOptimizerInterface
{
    private:

        size_t cache_map_capacity;
        size_t time_machine_cache_map_capacity;

    public:

        TemporalCoefficientOptimizer(size_t cache_map_capacity,
                                     size_t time_machine_cache_map_capacity) noexcept: cache_map_capacity(cache_map_capacity),
                                                                                       time_machine_cache_map_capacity(time_machine_cache_map_capacity){}

        auto optimize(MatrixInterface& matrix,
                      TemporalCoefficientProjectorInterface& projector,
                      MatrixEvaluatorInterface& deviation_extractor,
                      TimeMachineOptimizerInterface& time_machine_optimizer) -> std::vector<matrix_std_float_t>
        {
            std::shared_ptr<MatrixInterface> tmp_matrix = matrix.clone();
            CachedMatrix cached_matrix(tmp_matrix, this->cache_map_capacity);

            std::unique_ptr<TimeMachineInterface> time_machine = std::make_unique<SpecificMatrixTimeMachine>(&cached_matrix, &projector, &deviation_extractor);
            CachedTimeMachine cached_time_machine(std::move(time_machine), this->time_machine_cache_map_capacity);

            matrix_std_float_t t                             = time_machine_optimizer.optimize(cached_time_machine);

            std::vector<matrix_std_float_t> coeff_extra_vec  = projector.project(t);
            std::vector<matrix_std_float_t> coeff_vec        = matrix.get_coefficient_vector();
            std::vector<matrix_std_float_t> result_vec       = std::vector<matrix_std_float_t>(coeff_vec.size());

            restrict_add_array(coeff_vec.data(), coeff_extra_vec.data(), coeff_vec.size(),
                               result_vec.data());

            return result_vec;
        }

    private:

        class CachedMatrix: public virtual MatrixInterface
        {
            private:

                ImmutableShapeCachedMatrixProjector cache_base;
                std::shared_ptr<MatrixInterface> matrix_base;
                size_t cache_map_capacity;
            
            public:

                CachedMatrix(std::shared_ptr<MatrixInterface> matrix_base,
                             size_t cache_map_capacity): cache_base(matrix_base, cache_map_capacity),
                                                         matrix_base(matrix_base),
                                                         cache_map_capacity(cache_map_capacity){}

                auto project(const std::shared_ptr<Matrix>& matrix) -> std::shared_ptr<Matrix>
                {
                    return cache_base.project(matrix);
                }

                auto get_coefficient_vector() -> std::vector<matrix_std_float_t>
                {
                    return this->matrix_base->get_coefficient_vector();
                }

                void set_coefficient_vector(const std::vector<matrix_std_float_t>& coeff_vec)
                {
                    this->matrix_base->set_coefficient_vector(coeff_vec);
                    this->cache_base.clear_cache();
                }

                auto clone() -> std::shared_ptr<MatrixInterface>
                {
                    return std::make_shared<CachedMatrix>(matrix_base->clone(), this->cache_map_capacity);
                }
        };

        class SpecificMatrixTimeMachine: public virtual TimeMachineInterface
        {
            private:

                MatrixInterface * base_matrix;
                TemporalCoefficientProjectorInterface * coefficient_projector;
                MatrixEvaluatorInterface * product_evaluator;
            
            public:

                SpecificMatrixTimeMachine(MatrixInterface * base_matrix,
                                          TemporalCoefficientProjectorInterface * coefficient_projector,
                                          MatrixEvaluatorInterface * product_evaluator): base_matrix(std::move(base_matrix)),
                                                                                                  coefficient_projector(std::move(coefficient_projector)),
                                                                                                  product_evaluator(std::move(product_evaluator)){}

                auto f(matrix_std_float_t t) -> tm_float_t
                {
                    std::vector<matrix_std_float_t> coeff_vec = this->coefficient_projector->project(t);
                    this->base_matrix->set_coefficient_vector(coeff_vec);

                    return this->product_evaluator->get_deviation(*this->base_matrix);
                }
        };
};

class BruteForceMatrixOptimizer: public virtual MatrixOptimizerInterface
{
    private:

        std::shared_ptr<TemporalCoefficientOptimizerInterface> optimizer;
        size_t optimization_epoch_sz;

    public:

        BruteForceMatrixOptimizer(std::shared_ptr<TemporalCoefficientOptimizerInterface> optimizer,
                                  size_t optimization_epoch_sz) noexcept: optimizer(std::move(optimizer)),
                                                                          optimization_epoch_sz(optimization_epoch_sz){}

        auto optimize(MatrixInterface& matrix,
                      const std::vector<std::pair<std::shared_ptr<Matrix>, std::shared_ptr<Matrix>>>& gold_std) -> std::shared_ptr<MatrixInterface>
        {
            std::unique_ptr<MatrixEvaluatorInterface> product_evaluator = ProductEvaluatorFactory::get_immutable_shape_product_evaluator(gold_std);
            std::shared_ptr<MatrixInterface> best_matrix                = matrix.clone();
            std::shared_ptr<MatrixInterface> carry_along_matrix         = matrix.clone();

            for (size_t i = 0u; i < optimization_epoch_sz; ++i)
            {
                std::unique_ptr<TemporalCoefficientProjectorInterface> coefficient_projector    = CoefficientProjectorFactory::get_chained_coefficient_projector(to_variadic_vector_initializer(CoefficientProjectorFactory::get_point_coefficient_projector(carry_along_matrix->get_coefficient_vector()),
                                                                                                                                                                                                CoefficientProjectorFactory::get_random_coefficient_projector(carry_along_matrix->get_coefficient_vector().size())));
                std::unique_ptr<TimeMachineOptimizerInterface> time_machine_optimizer           = TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer();

                try
                {
                    std::vector<matrix_std_float_t> new_coefficient_vec = this->optimizer->optimize(*carry_along_matrix,
                                                                                                    *coefficient_projector,
                                                                                                    *product_evaluator,
                                                                                                    *time_machine_optimizer);

                    carry_along_matrix->set_coefficient_vector(new_coefficient_vec);

                    if (nan_cmp(product_evaluator->get_deviation(*carry_along_matrix), product_evaluator->get_deviation(*best_matrix)) < 0)
                    {
                        best_matrix = carry_along_matrix->clone();
                    }
                }
                catch (...)
                {
                    continue;
                }
            }

            return best_matrix;
        }
};

class MatrixOptimizerFactory
{
    private:

        static auto get_temporal_coefficient_optimizer(size_t cache_map_capacity, size_t time_machine_cache_map_capacity) -> std::unique_ptr<TemporalCoefficientOptimizerInterface>
        {
            if (cache_map_capacity == 0u)
            {
                throw std::runtime_error("invalid argument");
            }

            if (time_machine_cache_map_capacity == 0u)
            {
                throw std::runtime_error("invalid argument");
            }

            return std::make_unique<TemporalCoefficientOptimizer>(cache_map_capacity, time_machine_cache_map_capacity);
        }

    public:

        static auto get_brute_force_matrix_optimizer(size_t cache_map_capacity,
                                                     size_t time_machine_cache_map_capacity,
                                                     size_t optimization_epoch_sz) -> std::unique_ptr<MatrixOptimizerInterface>
        {
            if (optimization_epoch_sz == 0u)
            {
                throw std::runtime_error("invalid argument");
            }

            return std::make_unique<BruteForceMatrixOptimizer>(get_temporal_coefficient_optimizer(cache_map_capacity, time_machine_cache_map_capacity),
                                                               optimization_epoch_sz);
        }
};

//this is good enough to make a fortune enough for another projection expansion (we'll be millionaire next week, words!)
//to sum it up, we focus on the unit problem and the problem of euclidean relevancy by splitting the mind buffer into 2 parts to offset the cost of euclidean irrelevancy

//we are searching in a real multi-dimensional continous space
//we as of now can only do Taylor Space and its shape space (we would have to research into another continuous coordinate that covers the frequent shapes of projection)

//we leverage focal randomization to achieve hyper accurate ever-growing data-burn, not linear randomization or any other method of randomization
//we have done all we could for low level optimizations

//deviation calculation requires probably an accuracy rate of 100s of kilobytes, we need multiprecision if we are to compare to the gold_std of 1MM data points (this is super very important)