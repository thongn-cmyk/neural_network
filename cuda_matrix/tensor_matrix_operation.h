#ifndef __CUDA_MATRIX_TENSOR_MATRIX_OPERATION_H__
#define __CUDA_MATRIX_TENSOR_MATRIX_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include "tensor_model.h"
#include <stdexcept>
#include "tensor_being_unit_operation.h"
#include <array>

namespace cuda_matrix::tensor_matrix_operation
{
    using namespace cuda_matrix::tensor_model;
    using namespace cuda_matrix::utility;

    template <class FloatType>
    constexpr __device__ void positional_encode_current_position(const Matrix& arg,
                                                                 const FloatType& frequency_multiplier,
                                                                 const FloatType& amplitude_discrete_unit,
                                                                 size_t pe_positional_idx,
                                                                 size_t pe_dimension_idx,
                                                                 size_t dedicated_pe_sz,
                                                                 Matrix& rs)
    {
        if (arg.being_vec_sz != rs.being_vec_sz)
        {
            assert(false);
        }

        for (size_t i = 0u; i < arg.being_vec_sz; ++i)
        {
            FloatType amplitude_multiplier  = pe_positional_idx * amplitude_discrete_unit;
            FloatType x_offset              = 0;

            tensor_being_unit_operation::positional_encode(arg.being_vec[i],
                                                           rs.being_vec[i],
                                                           amplitude_multiplier,
                                                           frequency_multiplier,
                                                           x_offset,
                                                           FloatType(0),
                                                           pe_dimension_idx,
                                                           dedicated_pe_sz);
        }
    }

    constexpr __device__ void matrix_to_focal(const Matrix& matrix,
                                              size_t i,
                                              const inplace_unordered_map<size_t, inplace_unordered_map<size_t, inplace_vector<inplace_vector<size_t>>>>& focal_suffix_map,
                                              Matrix ** rs)
    {
        stdx::safe_ptr_access(matrix.get());

        const stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator> * focal_dictionary = [&]() -> const stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator> *
        {
            auto map_ptr = focal_suffix_map.find(matrix->being_vec_sz);

            if (map_ptr == focal_suffix_map.end())
            {
                return nullptr;
            }

            auto map_ptr2 = map_ptr->second.find(i);

            if (map_ptr2 == map_ptr->second.end())
            {
                return nullptr;
            }

            return &map_ptr2->second;
        }();

        if (focal_dictionary == nullptr)
        {
            return {};
        }

        stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> result_vec(allocator);

        for (const auto& suffix_arr: *focal_dictionary)
        {
            std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> result = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);
            size_t result_sz = matrix->being_vec_sz;

            for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
            {
                size_t suffix = suffix_arr[stdx::access_guard(i, suffix_arr.size())];
                result[stdx::access_guard(suffix, result_sz)] = matrix->being_vec[i];
            }

            std::shared_ptr<Matrix> focal = std::allocate_shared<Matrix>(allocator,
                                                                                                     Matrix{.being_vec    = std::move(result),
                                                                                                                          .being_vec_sz = result_sz});

            result_vec.push_back(std::move(focal));
        }

        return result_vec;
    }

    constexpr __device__ void focal_split_matrix(const Matrix& matrix,
                                                 size_t group_by_sz,
                                                 Matrix ** rs)
    {
        stdx::safe_ptr_access(matrix.get());

        if (group_by_sz == 0u)
        {
            throw std::invalid_argument("bad group size, 0");
        }

        if (matrix->being_vec_sz % group_by_sz != 0u)
        {
            throw std::invalid_argument("bad group count, uneven");
        }

        size_t focal_sz = matrix->being_vec_sz / group_by_sz;

        stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> result(allocator);

        for (size_t i = 0u; i < group_by_sz; ++i)
        {
            size_t first    = i * focal_sz;
            size_t last     = static_cast<size_t>((i + 1) * focal_sz);

            std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> focal_content = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, last - first);
            std::copy(std::next(matrix->being_vec.get(), first), std::next(matrix->being_vec.get(), last), focal_content.get());

            std::shared_ptr<Matrix> focal = std::allocate_shared<Matrix>(allocator,
                                                                                                     Matrix{.being_vec    = std::move(focal_content),
                                                                                                                          .being_vec_sz = last - first});

            result.push_back(std::move(focal));
        }

        return result;
    }

    constexpr __device__ void focal_unsplit_matrix(Matrix ** matrix_vec, size_t matrix_vec_sz,
                                                   size_t group_by_sz,
                                                   Matrix& rs)
    {
        stdx::transparent_vector<std::shared_ptr<tensor_model::BeingUnit>, Allocator> result(allocator);

        for (const auto& matrix: matrix_vec)
        {
            std::copy(matrix->being_vec.get(), std::next(matrix->being_vec.get(), matrix->being_vec_sz), std::back_inserter(result));
        }

        return std::allocate_shared<Matrix>(allocator,
                                                          Matrix{.being_vec       = to_shared_array(result, allocator),
                                                                               .being_vec_sz    = result.size()});
    }

    constexpr __device__ void deparameterize(const Matrix& matrix,
                                             double perc,
                                             Matrix& rs)
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::deparameterize(matrix->being_vec[i], perc, allocator);
        }

        return std::allocate_shared<Matrix>(allocator,
                                                          Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = matrix->being_vec_sz});
    }

    constexpr __device__ void accumulate(const Matrix& lhs,
                                         const Matrix& rhs,
                                         Matrix& rs)
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        if (lhs->being_vec_sz != rhs->being_vec_sz)
        {
            throw std::invalid_argument("incompatible pairwise size");
        }

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs  = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, lhs->being_vec_sz);

        for (size_t i = 0u; i < lhs->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::accumulate(lhs->being_vec[i], rhs->being_vec[i], allocator);
        }

        return std::allocate_shared<Matrix>(allocator,
                                                          Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = lhs->being_vec_sz});
    }

    constexpr __device__ void accumulate(const std::shared_ptr<Matrix ** matrix_arr,
                              size_t matrix_arr_sz,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<Matrix>
    {
        if (matrix_arr_sz == 0u)
        {
            throw std::invalid_argument("bad accumulation size, 0");
        }

        std::shared_ptr<Matrix> result = matrix_arr[0];

        for (size_t i = 1u; i < matrix_arr_sz; ++i)
        {
            result = accumulate(result, matrix_arr[i], allocator);
        }

        return result;
    }

    template <class ValueType,
              class Allocator = std::allocator<char>>
    constexpr auto div(const std::shared_ptr<Matrix>& matrix,
                       const ValueType& value,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<Matrix>
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::div(matrix->being_vec[i], value, allocator);
        }

        return std::allocate_shared<Matrix>(allocator,
                                                          Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = matrix->being_vec_sz});

    }

    template <class Allocator = std::allocator<char>>
    constexpr auto avg(const std::shared_ptr<Matrix> * matrix_arr,
                       size_t matrix_arr_sz,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<Matrix>
    {
        return div(accumulate(matrix_arr, matrix_arr_sz, allocator), stdx::safe_non_zero_access(matrix_arr_sz));
    }

    template <class ...Args>
    constexpr void flatten(const std::shared_ptr<Matrix>& arg,
                           std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            tensor_being_unit_operation::flatten(arg->being_vec[i], output_vec);
        }
    }

    template <class ...Args>
    constexpr void get_shape(const std::shared_ptr<Matrix>& arg,
                             std::vector<size_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        if (arg->being_vec_sz == 0u)
        {
            throw std::invalid_argument("bad matrix, 0 item");
        }

        output_vec.push_back(arg->being_vec_sz);

        stdx::safe_ptr_access(arg->being_vec.get());
        tensor_being_unit_operation::get_shape(arg->being_vec[0], output_vec);
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto immutable_shape_matrix_to_unique_representation(const std::shared_ptr<Matrix>& arg,
                                                                   const Allocator& allocator = Allocator()) -> stdx::transparent_string<Allocator>
    {
        stdx::safe_ptr_access(arg.get());

        stdx::transparent_vector<tensor_model::tensor_std_float_t, Allocator> content(allocator);
        flatten(arg, content);

        size_t total_sz     = content.size() * sizeof(tensor_model::tensor_std_float_t) + sizeof(size_t);
        size_t content_sz   = content.size();

        stdx::transparent_string<Allocator> rs(total_sz, 0, allocator);

        std::memcpy(rs.data(), &content_sz, sizeof(size_t));
        std::memcpy(std::next(rs.data(), sizeof(size_t)), content.data(), content.size() * sizeof(tensor_model::tensor_std_float_t));

        return rs;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto matrix_to_unique_representation(const std::shared_ptr<Matrix>& arg,
                                                   const Allocator& allocator = Allocator()) -> stdx::transparent_string<Allocator>
    {
        return immutable_shape_matrix_to_unique_representation(arg, allocator);
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto unfocal_matrix(const stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator>& matrix_vec,
                                  size_t i,
                                  const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& focal_suffix_map,
                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<Matrix>
    {
        using namespace tensor_model;

        const stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator> * focal_dictionary = [&]() -> const stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator> *
        {
            if (matrix_vec.empty())
            {
                return nullptr;
            }

            auto map_ptr = focal_suffix_map.find(matrix_vec.front()->being_vec_sz);

            if (map_ptr == focal_suffix_map.end())
            {
                return nullptr;
            }

            auto map_ptr2 = map_ptr->second.find(i);

            if (map_ptr2 == map_ptr->second.end())
            {
                return nullptr;
            }

            return &map_ptr2->second;
        }();

        if (focal_dictionary == nullptr)
        {
            throw std::invalid_argument("bad unfocal operation, dictionary not found");
        }

        stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> result_vec(allocator);

        for (size_t i = 0u; i < matrix_vec.size(); ++i)
        {
            const auto& suffix_arr = (*focal_dictionary)[stdx::access_guard(i, focal_dictionary->size())];
            std::shared_ptr<std::shared_ptr<BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<BeingUnit>[]>(allocator, matrix_vec[i]->being_vec_sz);

            for (size_t j = 0u; j < matrix_vec[i]->being_vec_sz; ++j)
            {
                size_t suffix   = suffix_arr[stdx::access_guard(j, suffix_arr.size())];
                rs[j]           = matrix_vec[i]->being_vec[stdx::access_guard(suffix, matrix_vec[i]->being_vec_sz)];
            }

            std::shared_ptr<Matrix> org_matrix = std::allocate_shared<Matrix>(allocator,
                                                                                                          Matrix{.being_vec       = std::move(rs),
                                                                                                                               .being_vec_sz    = matrix_vec[i]->being_vec_sz});

            result_vec.push_back(std::move(org_matrix));
        }

        return avg(result_vec.data(), result_vec.size(), allocator);
    }

    //let's get to the basic encoding method
    //<char><char><char><char> = 4 bytes = int

    //denormalized value (one dimensional value) = 256 * 256 * 256 * 256
    //= 1 << 32

    //assume each discrete value is 0.001, then a random idx has the value of x * 0.001

    //how do we apply that here in this algorithm to make sure that our encoding method is accurate
    //assume that our matrix focal is [256] [64] [8] [2]

    //one dimensional value = 256 * 64 * 8 * 2
    //assume that we are at the pointer <128> <8> <4> <1>

    //we are to flatten each dimension and add them up

    //<4> = 4 * 2 = 8 without loss of generality
    //<8> = 8 * 8 * 2 = 128 without loss of generality

    //then we'd have to multiply that with the discretization unit, 0.01 for example

    //I have to admit that this algorithm of positional encoding is not easy to implement

    //because at the heart of lossless encoding, we'd want to prove that there is a possibility of a reverse function
    //yet at the heart of continuity compression, we'd want to have relevant things closer together in the euclidean coordiante

    //now let's get back to the problem of one dimensionalist, we flatten the space vector of known shape <256, 256, 256, 256> in the case of int32_t, or <128, 64, 32, 16, 8, 4, 2> in the case of our matrix transform
    //the equation of transforming from multi-dimensional pointer -> one dimensional pointer is lossless compression, and the equation for that is offset * <succeeding_dimension_sz> + ...

    //we encode the value of the one dimensional pointer into the amplitude of a frequency equation, fine, we have met the lossless compression criteria

    //what we have not met is the euclidean relevancy of the pointer and the amplitude restriction of the space
    //euclidean relevancy can only be met if we do it as big endianness of encoding, such is the first dimension is the last dimension with respect to the current context

    //then our problem now becomes the problem of amplitude restriction of the space
    //so we are smart, we'd turn from being one dimensionalist into a multi-dimensionalist, and follow the equation of distance: coor_distance = sqrt(sqr_difference + ...)

    //the problem now is that we'd want to distinct each dimension very differently, such that there is no chance of euclidean relevancy for the corresponding dimension 0 and dimension 1 or dimension 2
    //now it sounds like a sphere encoding method, an extension of the sin, cos encoding, is it so?

    //we'd try to solve the equation today, and prove that our equation meets three very important points: (1) lossless compression
    //                                                                                                     (2) euclidean relevancy
    //                                                                                                     (3) amplitude requirements

    //apart from that, we'd work on the domain and range of our worst case Taylor Series, we'd definitely need the radian coordinate of the Series, yet we'd take other ranges factor into the considerations
    //we'd work on that tmr

    //today we'd work on very two important concepts of the equation, the coefficient space and the positional encoding

    //our coefficient space spans all the possibilities of the power series in terms of sigma(pow)...
    //yet the divisor constants would play a crucial role in shaping the search space, such is that the search base might be more suitable to our search algorithms than other spaces

    //let's look at our parallel training equation, (f(x) - expected(x)) ^2 + ...

    //without loss of generality assume that our coefficient is a and the equation is (a + t) * x^n * y^m + C
    //f(x) - expected(x) = (a + t) * C1 + C2
    //(f(x) - expected(x)) ^ 2 = ((a + t) * C1 + C2) ^ 2

    //the derivative with respect to t is 2 * ((a + t) * C1 + C2) * C1*a

    //for positional encoding
    //let's start with the obvious, we assign each dimension of the vector -> one positional encoded value
    //without loss of generality, [0, 1, 2, 0, 4] means that slot 0 of the first focal, slot 1 of the second focal, ...
    //we would turn this value into a frequency equation individually with amplitude, and paint the designated first slots of the BeingUnit with the value

    //in this sense, each of the dimension is now semantically distinct on their own, we have met the requirements of euclidean relevancy (maybe)
    //lossless compression => yes (in the sense of focal_idx... being a representation of the original idx)
    //amplitude requirements => yes, as long as we keep the focal size small

    //I think that the proof for this equation being the most suitable function, or optimized form of contiunous approximation would be at least 100 pages long
    //but I would try to be very brief about the directions of proof, or the sequence of statements that need to be proved

        //(1): every approximatable function by using calculus (slope and friends) must take a form of Taylor Series, with real coefficients
        //(2): a base approximation is only stable if operated on a group of less than or equal to k dimensions, for k is some number
        //(3): a function is only Taylor-Series-complete without exploding the coefficient space if we use the method x = x + f(x) and y = y + x (with the slack-one buffer that we proved the other day)
        //(4): a vertical shrink of a function is sufficient for x = x + f(x), with interchangable finite range and domain, every vertically shrinked function reflects one point on the radian coordinate

    //

    template <class TaylorBaseCoeffSizeContainer,
              class ShapeBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto matrix_transform(const std::shared_ptr<Matrix>& matrix,
                                                              const stdx::transparent_vector<size_t, Allocator>& focal_sz_vec,
                                                              const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& focal_suffix_map,
                                                              const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& accum_suffix_map,

                                                              const stdx::transparent_vector<size_t, Allocator>& rotation_sz_vec,
                                                              const stdx::transparent_vector<double, Allocator>& parameter_bound_ratio_vec,
                                                              TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                              const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                              ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                              const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                              tensor_model::tensor_std_float_t pe_frequency_multiplier, tensor_model::tensor_std_float_t pe_amplitude_discrete_unit, size_t pe_stack_offset, size_t pe_dedicated_pe_sz,

                                                              const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                              const stdx::Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = stdx::Tag<ShapeBasePromotedFloatType>{},

                                                              bool has_logit_unit_reuse_tag = true,
                                                              bool has_logit_group_logit_reuse_tag = true,
                                                              bool has_being_logit_reuse_tag = true,
                                                              bool has_base_matrix_logit_reuse_tag = true,

                                                              const Allocator& allocator = Allocator()) -> std::shared_ptr<Matrix>
    {
        using namespace tensor_model;

        stdx::safe_ptr_access(matrix.get());

        if (matrix->being_vec_sz == 0u)
        {
            throw std::invalid_argument("bad matrix's being vec size, 0");
        }

        if (matrix->being_vec_sz == 1u)
        {
            return matrix;
        }

        if (matrix->being_vec_sz == 2u)
        {
            const size_t saved_coeff_arr_offset         = coeff_arr_offset;
            const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

            std::shared_ptr<BeingUnit> lhs = tensor_being_unit_operation::left_major_intercourse_being_unit(matrix->being_vec[0],
                                                                                                            matrix->being_vec[1],
                                                                                                            base_coeff_sz_container,
                                                                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                                            base_shape_coeff_sz_container,
                                                                                                            shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                            taylor_base_promotion_tag,
                                                                                                            shape_base_promotion_tag,
                                                                                                            has_logit_unit_reuse_tag,
                                                                                                            has_logit_group_logit_reuse_tag,
                                                                                                            has_being_logit_reuse_tag,
                                                                                                            allocator);

            if (has_base_matrix_logit_reuse_tag)
            {
                coeff_arr_offset        = saved_coeff_arr_offset;
                shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;
            }

            std::shared_ptr<BeingUnit> rhs = tensor_being_unit_operation::left_major_intercourse_being_unit(matrix->being_vec[1],
                                                                                                            matrix->being_vec[0],
                                                                                                            base_coeff_sz_container,
                                                                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                                            base_shape_coeff_sz_container,
                                                                                                            shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                                            taylor_base_promotion_tag,
                                                                                                            shape_base_promotion_tag,
                                                                                                            has_logit_unit_reuse_tag,
                                                                                                            has_logit_group_logit_reuse_tag,
                                                                                                            has_being_logit_reuse_tag,
                                                                                                            allocator);

            return std::allocate_shared<Matrix>(allocator,
                                                              Matrix{.being_vec       = to_shared_array(stdx::transparent_vector<std::shared_ptr<BeingUnit>, Allocator>{lhs, rhs}, allocator),
                                                                                   .being_vec_sz    = 2u});
        }

        if (focal_sz_vec.empty())
        {
            throw std::invalid_argument("bad focal_sz_vec size, 0");
        }

        if (rotation_sz_vec.empty())
        {
            throw std::invalid_argument("bad rotation_sz_vec size, 0");
        }

        if (parameter_bound_ratio_vec.empty())
        {
            throw std::invalid_argument("bad parameter_bound_ratio_vec size, 0");
        }

        //in this context, domain and range are interchangable, because domain is the previous range and range is the next domain, etc.

        const size_t focal_sz                       = focal_sz_vec.front();
        const size_t rotation_sz                    = rotation_sz_vec.front();
        const double parameter_bound_ratio          = parameter_bound_ratio_vec.front();
        std::shared_ptr<Matrix> up_to_point_matrix  = matrix;

        stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> incremental_matrix_vec(allocator);
        incremental_matrix_vec.push_back(matrix);


        for (size_t i = 0u; i < rotation_sz; ++i)
        {
            stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focused_matrix_vec = matrix_to_focal(up_to_point_matrix,
                                                                                                              i,
                                                                                                              focal_suffix_map,
                                                                                                              allocator);

            stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> up_to_point_incremental_matrix_vec(allocator);

            if (i + 1 != rotation_sz)
            {
                for (const auto& focused_matrix: focused_matrix_vec)
                {
                    stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focal_matrix_vec       = focal_split_matrix(focused_matrix, focal_sz, allocator);
                    stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> transformed_focal_vec(allocator);
                    const size_t saved_coeff_arr_offset_0                                               = coeff_arr_offset;
                    const size_t saved_shape_coeff_arr_offset_0                                         = shape_coeff_arr_offset;
                    size_t positional_idx                                                               = 0u;

                    for (const auto& focal: focal_matrix_vec)
                    {
                        coeff_arr_offset        = saved_coeff_arr_offset_0;
                        shape_coeff_arr_offset  = saved_shape_coeff_arr_offset_0;

                        std::shared_ptr<Matrix> transformed_focal = matrix_transform(positional_encode_current_position(focal, pe_frequency_multiplier, pe_amplitude_discrete_unit, positional_idx, pe_stack_offset, pe_dedicated_pe_sz, allocator),
                                                                                     {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                                     focal_suffix_map,
                                                                                     accum_suffix_map,
                                                                                     {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                                     {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                                     base_coeff_sz_container,
                                                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                     base_shape_coeff_sz_container,
                                                                                     shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                     pe_frequency_multiplier, pe_amplitude_discrete_unit, pe_stack_offset + 1, pe_dedicated_pe_sz,
                                                                                     taylor_base_promotion_tag,
                                                                                     shape_base_promotion_tag,
                                                                                     has_logit_unit_reuse_tag,
                                                                                     has_logit_group_logit_reuse_tag,
                                                                                     has_being_logit_reuse_tag,
                                                                                     has_base_matrix_logit_reuse_tag,
                                                                                     allocator);

                        transformed_focal_vec.push_back(transformed_focal);
                        positional_idx += 1u;
                    }

                    std::shared_ptr<Matrix> transformed_focused_matrix = focal_unsplit_matrix(transformed_focal_vec, focal_sz, allocator);
                    up_to_point_incremental_matrix_vec.push_back(transformed_focused_matrix);
                }
            }

            stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focused_deparameterized_matrix_vec = matrix_to_focal(deparameterize(up_to_point_matrix, parameter_bound_ratio, allocator),
                                                                                                                              i,
                                                                                                                              accum_suffix_map,
                                                                                                                              allocator);
            stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> accum_matrix_vec(allocator);

            for (const auto& focused_matrix: focused_deparameterized_matrix_vec)
            {
                stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focal_deparamed_matrix_vec = focal_split_matrix(focused_matrix, focal_sz, allocator);
                stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> incremental_vec(allocator);
                const size_t saved_coeff_arr_offset_1                                                   = coeff_arr_offset;
                const size_t saved_shape_coeff_arr_offset_1                                             = shape_coeff_arr_offset;
                size_t positional_idx                                                                   = 0u;

                for (const auto& focal: focal_deparamed_matrix_vec)
                {
                    coeff_arr_offset        = saved_coeff_arr_offset_1;
                    shape_coeff_arr_offset  = saved_shape_coeff_arr_offset_1;

                    std::shared_ptr<Matrix> transformed_focal = matrix_transform(positional_encode_current_position(focal, pe_frequency_multiplier, pe_amplitude_discrete_unit, positional_idx, pe_stack_offset, pe_dedicated_pe_sz, allocator),
                                                                                 {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                                 focal_suffix_map,
                                                                                 accum_suffix_map,
                                                                                 {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                                 {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                                 base_coeff_sz_container,
                                                                                 coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                 base_shape_coeff_sz_container,
                                                                                 shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                                 pe_frequency_multiplier, pe_amplitude_discrete_unit, pe_stack_offset + 1, pe_dedicated_pe_sz,
                                                                                 taylor_base_promotion_tag,
                                                                                 shape_base_promotion_tag,
                                                                                 has_logit_unit_reuse_tag,
                                                                                 has_logit_group_logit_reuse_tag,
                                                                                 has_being_logit_reuse_tag,
                                                                                 has_base_matrix_logit_reuse_tag,
                                                                                 allocator);

                    incremental_vec.push_back(transformed_focal);
                    positional_idx += 1u;
                }

                std::shared_ptr<Matrix> transformed_focused_deparamed_matrix = focal_unsplit_matrix(incremental_vec, focal_sz, allocator);
                accum_matrix_vec.push_back(transformed_focused_deparamed_matrix);
            }

            if (i + 1 != rotation_sz)
            {
                std::shared_ptr<Matrix> incremental_up_to_point_matrix  = unfocal_matrix(up_to_point_incremental_matrix_vec, i, focal_suffix_map, allocator);
                auto avg_arr                                            = std::array<std::shared_ptr<Matrix>, 2u>{up_to_point_matrix, incremental_up_to_point_matrix};
                up_to_point_matrix                                      = avg(avg_arr.data(), avg_arr.size(), allocator);
            }

            std::shared_ptr<Matrix> incremental_result  = unfocal_matrix(accum_matrix_vec, i, focal_suffix_map, allocator);
            incremental_matrix_vec.push_back(std::move(incremental_result));
        }

        return avg(incremental_matrix_vec.data(), incremental_matrix_vec.size(), allocator);
    }
}

#endif