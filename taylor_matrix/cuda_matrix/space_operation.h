//HEADER_CONTROL 1

#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_SPACE_OPERATION_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_SPACE_OPERATION_H__

#include <general_definition/float_def.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdexcept>
#include "utility.h"
#include <numbers>

namespace taylor_matrix::cuda_matrix::space_operation
{
    using namespace taylor_matrix::cuda_matrix::utility;

    template <class FloatType, class PromotedFloatType = FloatType>
    __device__ constexpr auto radian_normalize(FloatType x,
                                               const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        constexpr PromotedFloatType divisor = std::numbers::pi_v<PromotedFloatType> * 2;

        return std::remainder(static_cast<PromotedFloatType>(x), divisor);
    }

    template <class T, class T1, class T2, class ArrSizeType>
    __device__ constexpr void restrict_scalar_mul_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                                        T1 c,
                                                        T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = arg_arr[i] * c;
        }
    }

    template <class T, class T1, class T2, class ArrSizeType>
    __device__ constexpr void restrict_scalar_div_array(const T * __restrict__ arg_arr, ArrSizeType arr_sz,
                                                        T1 c,
                                                        T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = arg_arr[i] / c;
        }
    }

    template <class T, class T1, class T2, class ArrSizeType>
    __device__ constexpr void restrict_add_array(const T * __restrict__ lhs_arr, const T1 * __restrict__ rhs_arr, ArrSizeType arr_sz,
                                                 T2 * __restrict__ output_arr)
    {
        for (size_t i = 0u; i < to_size_container(arr_sz).get(); ++i)
        {
            output_arr[i] = lhs_arr[i] + rhs_arr[i];
        }
    }

    template <class FloatType1, class FloatType2, class FloatType3, class ArrSizeType, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2, FloatType3>>
    __device__ constexpr void restrict_multidimensional_oval_to_euclidean_array(const FloatType1 * __restrict__ radian_arr, ArrSizeType radian_space_sz,
                                                                                const FloatType2 * __restrict__ radius_arr,
                                                                                FloatType3  * __restrict__ output_arr,
                                                                                const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<FloatType3>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

        PromotedFloatType carry_multiplier = 1;

        for (size_t i = 0u; i < to_size_container(radian_space_sz).get(); ++i)
        {
            output_arr[i]       = std::sin(static_cast<PromotedFloatType>(radian_arr[i])) * carry_multiplier * radius_arr[i];
            carry_multiplier    *= std::cos(static_cast<PromotedFloatType>(radian_arr[i]));
        }
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    __device__ constexpr auto dot_product(const FloatType1 * lhs_arr,
                                          const FloatType2 * rhs_arr,
                                          size_t arr_sz,
                                          const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>()) -> PromotedFloatType
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
    __device__ constexpr auto coordinate_distance(const FloatType * coor_arr, size_t coor_arr_sz,
                                                  const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return std::sqrt(dot_product(coor_arr, coor_arr, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    __device__ constexpr auto cosine_score(const FloatType1 * coor_arr_1,
                                           const FloatType2 * coor_arr_2,
                                           size_t coor_arr_sz,
                                           const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return dot_product(coor_arr_1, coor_arr_2, coor_arr_sz, promotion_tag) / (coordinate_distance(coor_arr_1, coor_arr_sz, promotion_tag) * coordinate_distance(coor_arr_2, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    __device__ constexpr auto cosine_angle(const FloatType1 * coor_arr_1,
                                           const FloatType2 * coor_arr_2,
                                           size_t coor_arr_sz,
                                           const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{}) -> PromotedFloatType
    {
        return std::acos(cosine_score(coor_arr_1, coor_arr_2, coor_arr_sz, promotion_tag));
    }

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    __device__ constexpr void euclidean_to_radian_coordinate(const FloatType1 * euclid_coor_arr,
                                                             size_t euclid_coor_arr_sz,
                                                             FloatType2 * radian_coor_arr,
                                                             const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{})
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
                    radian_coor_arr[i] = generic_nan();
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

    template <class FloatType1, class FloatType2, class PromotedFloatType = float_def::most_byte_width_float_t<FloatType1, FloatType2>>
    __device__ constexpr void radian_to_euclidean_coordinate(const FloatType1 * radian_coor_arr,
                                                             size_t radian_coor_arr_sz,
                                                             FloatType2 * euclid_coor_arr,
                                                             const Tag<PromotedFloatType>& promotion_tag = Tag<PromotedFloatType>{})
    {
        static_assert(std::is_floating_point_v<FloatType1>);
        static_assert(std::is_floating_point_v<FloatType2>);
        static_assert(std::is_floating_point_v<PromotedFloatType>);

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
}

#endif