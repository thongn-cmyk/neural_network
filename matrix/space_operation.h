//HEADER_CONTROL 1

#ifndef __SPACE_OPERATION_H__
#define __SPACE_OPERATION_H__

#include "float_def.h"
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "stdx.h"
#include <stdexcept>

namespace space_operation
{
    template <class FloatType, class PromotedFloatType = FloatType>
    constexpr auto radian_normalize(FloatType x,
                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        constexpr PromotedFloatType divisor = std::numbers::pi_v<PromotedFloatType> * 2;

        return std::remainder(static_cast<PromotedFloatType>(x), divisor);
    }

    template <class T, class T1, class T2, class ArrSizeType>
    constexpr void restrict_scalar_mul_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                             T1 c,
                                             T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < stdx::to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = arg_arr[i] * c;
        }
    }

    template <class T, class T1, class T2, class ArrSizeType>
    constexpr void restrict_scalar_div_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                             T1 c,
                                             T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < stdx::to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = arg_arr[i] / c;
        }
    }

    template <class T, class T1, class T2, class ArrSizeType>
    constexpr void restrict_add_array(const T * __restrict__ lhs_arr, const T1 * __restrict__ rhs_arr, ArrSizeType arr_sz,
                                      T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < stdx::to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = lhs_arr[i] + rhs_arr[i];
        }
    }

    template <class FloatType1, class FloatType2, class FloatType3, class ArrSizeType, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2, FloatType3>>
    constexpr void restrict_multidimensional_oval_to_euclidean_array(const FloatType1 * __restrict__ radian_arr, ArrSizeType radian_space_sz,
                                                                     const FloatType2 * __restrict__ radius_arr,
                                                                     FloatType3  * __restrict__ output_arr,
                                                                     const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<FloatType3>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType carry_multiplier = 1;

        for (size_t i = 0u; i < stdx::to_size_container(radian_space_sz).get(); ++i)
        {
            output_arr[i]       = std::sin(static_cast<PromotedFloatType>(radian_arr[i])) * carry_multiplier * radius_arr[i];
            carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_arr[i]));
        }
    }

    template <class T, class T1, class ...Args>
    constexpr auto mul_vector(const std::vector<T, Args...>& vec, T1 c) -> std::vector<T, Args...>
    {
        std::vector<T, Args...> rs(vec.size());
        restrict_scalar_mul_array(vec.data(), vec.size(), c, rs.data());

        return rs;
    }

    template <class T, class T1, class ...Args>
    constexpr auto div_vector(const std::vector<T, Args...>& vec, T1 c) -> std::vector<T, Args...>
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
            throw std::invalid_argument("incompatible vector size");
        }

        std::vector<T, Args...> rs(vec1.size());
        restrict_add_array(vec1.data(), vec2.data(), vec1.size(), rs.data());

        return rs;
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr auto dot_product(const FloatType1 * lhs_arr,
                               const FloatType2 * rhs_arr,
                               size_t arr_sz,
                               const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>()) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType rs = 0;

        for (size_t i = 0u; i < arr_sz; ++i)
        {
            rs += static_cast<PromotedFloatType>(lhs_arr[i]) * rhs_arr[i];
        }

        return rs;
    }

    template <class FloatType, class PromotedFloatType = FloatType>
    constexpr auto coordinate_distance(const FloatType * coor_arr, size_t coor_arr_sz,
                                       const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return std::sqrt(dot_product(coor_arr, coor_arr, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr auto cosine_score(const FloatType1 * coor_arr_1,
                                const FloatType2 * coor_arr_2,
                                size_t coor_arr_sz,
                                const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        //cosine_score = (a.b) / (|a|*|b|)

        return dot_product(coor_arr_1, coor_arr_2, coor_arr_sz, promotion_tag) / (coordinate_distance(coor_arr_1, coor_arr_sz, promotion_tag) * coordinate_distance(coor_arr_2, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr auto cosine_angle(const FloatType1 * coor_arr_1,
                                const FloatType2 * coor_arr_2,
                                size_t coor_arr_sz,
                                const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return std::acos(cosine_score(coor_arr_1, coor_arr_2, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr void euclidean_to_radian_coordinate(const FloatType1 * euclid_coor_arr,
                                                  size_t euclid_coor_arr_sz,
                                                  FloatType2 * radian_coor_arr,
                                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType carry_multiplier  = 1;
        const double EPSILON                = 0.0001;

        for (size_t i = 0u; i < euclid_coor_arr_sz; ++i)
        {
            if (i + 1 == euclid_coor_arr_sz)
            {
                if (auto test_value = std::abs(std::abs(euclid_coor_arr[i]) - carry_multiplier); std::isnan(test_value) || test_value > EPSILON)
                {
                    radian_coor_arr[i] = stdx::generic_nan();
                }
                else
                {
                    radian_coor_arr[i] = 0;
                }

                continue;
            }

            radian_coor_arr[i]  = std::asin(euclid_coor_arr[i] / carry_multiplier);
            carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_coor_arr[i]));
        }
    }

    // template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    // constexpr void euclidean_to_radian_coordinate_2(const FloatType1 * euclid_coor_arr,
    //                                                 size_t euclid_coor_arr_sz,
    //                                                 FloatType2 * radian_coor_arr,
    //                                                 const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    // {
    //     euclidean_to_radian_coordinate(euclid_coor_arr,
    //                                    euclid_coor_arr_sz,
    //                                    radian_coor_arr,
    //                                    promotion_tag);

    //     for (size_t i = 0u; i < euclid_coor_arr_sz; ++i)
    //     {
    //         if (std::isnan(radian_coor_arr[i]))
    //         {
    //             radian_coor_arr[i] = 0;
    //         }
    //     }
    // }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr void radian_to_euclidean_coordinate(const FloatType1 * radian_coor_arr,
                                                  size_t radian_coor_arr_sz,
                                                  FloatType2 * euclid_coor_arr,
                                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        //sqrt(a^2 + b^2 + c^2 + ...) = 1
        //                            = C

        //C*sin(a) + C*cos(a)

        //dist = sqrt(C^2*sin2(a) + C^2*cos2(a)) = C
        //=> C^2(sin^2(a)+cos2(a)) = C^2
        //<=> C^2 == C^2

        //C^2 * 1 = C^2

        PromotedFloatType carry_multiplier = 1;

        for (size_t i = 0u; i < radian_coor_arr_sz; ++i)
        {
            if (i + 1 == radian_coor_arr_sz)
            {
                euclid_coor_arr[i] = carry_multiplier;
            }
            else
            {
                euclid_coor_arr[i] = carry_multiplier * std::sin(static_cast<PromotedFloatType>(radian_coor_arr[i]));
            }

            carry_multiplier *= std::cos(static_cast<PromotedFloatType>(radian_coor_arr[i]));
        }
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    constexpr void to_unit_vector(const FloatType1 * coordinate_arr, size_t coordinate_arr_sz,
                                  FloatType2 * output_arr,
                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType distance = coordinate_distance(coordinate_arr, coordinate_arr_sz, promotion_tag);

        if (distance == 0)
        {
            for (size_t i = 0u; i < coordinate_arr_sz; ++i)
            {
                output_arr[i] = stdx::generic_nan();
            }
        }
        else
        {
            for (size_t i = 0u; i < coordinate_arr_sz; ++i)
            {
                output_arr[i] = coordinate_arr[i] / distance;
            }
        }
    }

    template <class FloatType, class ...Args, class PromotedFloatType = FloatType>
    constexpr auto to_unit_vector(const std::vector<FloatType, Args...>& vec,
                                  const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> std::vector<FloatType, Args...>
    {

        std::vector<FloatType, Args...> rs(vec.size());
        to_unit_vector(vec.data(), vec.size(), rs.data(), promotion_tag);

        return rs;
    }

    template <class FloatType1, class FloatType2, class FloatType3, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2, FloatType3>>
    constexpr void rotate_euclidean_coordinate(const FloatType1 * coordinate_arr, size_t coordinate_arr_sz,
                                               const FloatType2 * radian_displacement_arr,
                                               FloatType3 * output_arr,
                                               const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<FloatType3>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType distance = coordinate_distance(coordinate_arr, coordinate_arr_sz, promotion_tag);

        std::vector<PromotedFloatType> unit_vector(coordinate_arr_sz);

        to_unit_vector(coordinate_arr, coordinate_arr_sz,
                       unit_vector.data());

        std::vector<PromotedFloatType> radian_vector(coordinate_arr_sz);

        euclidean_to_radian_coordinate(unit_vector.data(), unit_vector.size(),
                                       radian_vector.data());

        std::vector<PromotedFloatType> displaced_radian_vector(coordinate_arr_sz);

        restrict_add_array(radian_vector.data(), radian_displacement_arr, coordinate_arr_sz,
                           displaced_radian_vector.data());

        std::vector<PromotedFloatType> new_unit_vector(coordinate_arr_sz);

        radian_to_euclidean_coordinate(displaced_radian_vector.data(), displaced_radian_vector.size(),
                                       new_unit_vector.data());

        restrict_scalar_mul_array(new_unit_vector.data(), new_unit_vector.size(),
                                  distance,
                                  output_arr);
    }

}

#endif