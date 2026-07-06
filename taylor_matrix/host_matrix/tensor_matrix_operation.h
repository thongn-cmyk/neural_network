//HEADER_CONTROL 6

#ifndef __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_MATRIX_OPERATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_MATRIX_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include <memory>
#include <vector>
#include <matrix/tensor_model.h>
#include <stdexcept>
#include "tensor_being_unit_operation.h"
#include <general_definition/float_def.h>
#include <stl_extension/hasher.h>
#include <matrix/matrix_serializer.h>
#include "dispatch_code_generator.h"

namespace taylor_matrix::host_matrix::tensor_matrix_operation
{
    using DispatchCodeGenerator = taylor_matrix::host_matrix::dispatch_code_generator::DispatchCodeGenerator;

    template <class T, class ...Args, class Allocator = std::allocator<char>>
    constexpr auto to_shared_array(const std::vector<T, Args...>& arg,
                                   const Allocator& allocator = Allocator()) -> std::shared_ptr<T[]>
    {
        std::shared_ptr<T[]> rs = std::allocate_shared<T[]>(allocator, arg.size());
        std::copy(arg.begin(), arg.end(), rs.get());

        return rs;
    }

    template <class ...Args,
              class Allocator = std::allocator<char>>
    constexpr auto make_matrix_from_shape_vec(const std::vector<size_t, Args...>& space,
                                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        if (space.empty())
        {
            throw std::invalid_argument("bad space shape, void space");
        }

        size_t dimension_sz = space.front();
        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, dimension_sz);

        for (size_t i = 0u; i < dimension_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::make_being_unit_from_shape_vec({std::next(space.begin()), space.end()}, allocator);
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = dimension_sz});
    }

    constexpr void internal_unflatten(const std::shared_ptr<tensor_model::Matrix>& arg,
                                      const tensor_model::tensor_std_float_t * input_arr, size_t input_arr_sz,
                                      size_t& offset)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            tensor_being_unit_operation::internal_unflatten(arg->being_vec[i], input_arr, input_arr_sz, offset);
        }
    }

    template <class ...Args, class ...Args1, class Allocator = std::allocator<char>>
    constexpr auto make_matrix_from_flat_vec(const std::vector<size_t, Args...>& space,
                                             const std::vector<tensor_model::tensor_std_float_t, Args1...>& input_vec,
                                             const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        auto rs = make_matrix_from_shape_vec(space, allocator);
        size_t offset = 0u;
        internal_unflatten(rs, input_vec.data(), input_vec.size(), offset);

        return rs;
    }

    // template <class FloatType, class Allocator = std::allocator<char>>
    // constexpr auto positional_encode_current_position(const std::shared_ptr<tensor_model::Matrix>& matrix,
    //                                                   const FloatType& amplitude_multiplier,
    //                                                   const FloatType& frequency_multiplier,
    //                                                   const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    // {
    //     stdx::safe_ptr_access(matrix.get());

    //     std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

    //     for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
    //     {
    //         rs[i] = tensor_being_unit_operation::positional_encode(matrix->being_vec[i], i, amplitude_multiplier, frequency_multiplier, allocator);
    //     }

    //     return std::allocate_shared<tensor_model::Matrix>(allocator,
    //                                                       tensor_model::Matrix{.being_vec       = std::move(rs),
    //                                                                            .being_vec_sz    = matrix->being_vec_sz});
    // }

    template <class FloatType, class Allocator = std::allocator<char>>
    constexpr auto positional_encode_current_position(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                                      const FloatType& frequency_multiplier,
                                                      const FloatType& amplitude_discrete_unit,
                                                      size_t pe_positional_idx,
                                                      size_t pe_dimension_idx,
                                                      size_t dedicated_pe_sz,
                                                      const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            FloatType amplitude_multiplier  = pe_positional_idx * amplitude_discrete_unit;
            FloatType x_offset              = 0;

            rs[i]                           = tensor_being_unit_operation::positional_encode(matrix->being_vec[i],
                                                                                             amplitude_multiplier,
                                                                                             frequency_multiplier,
                                                                                             x_offset,
                                                                                             FloatType(0),
                                                                                             pe_dimension_idx,
                                                                                             dedicated_pe_sz,
                                                                                             allocator);
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = matrix->being_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto matrix_to_focal(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                   size_t i,
                                   const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& focal_suffix_map,
                                   const Allocator& allocator = Allocator()) -> stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator>
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

        stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator> result_vec(allocator);

        for (const auto& suffix_arr: *focal_dictionary)
        {
            std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> result = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);
            size_t result_sz = matrix->being_vec_sz;

            for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
            {
                size_t suffix = suffix_arr[stdx::access_guard(i, suffix_arr.size())];
                result[stdx::access_guard(suffix, result_sz)] = matrix->being_vec[i];
            }

            std::shared_ptr<tensor_model::Matrix> focal = std::allocate_shared<tensor_model::Matrix>(allocator,
                                                                                                     tensor_model::Matrix{.being_vec    = std::move(result),
                                                                                                                          .being_vec_sz = result_sz});

            result_vec.push_back(std::move(focal));
        }

        return result_vec;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto focal_split_matrix(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                      size_t group_by_sz,
                                      const Allocator& allocator = Allocator()) -> stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator>
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

        stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator> result(allocator);

        for (size_t i = 0u; i < group_by_sz; ++i)
        {
            size_t first    = i * focal_sz;
            size_t last     = static_cast<size_t>((i + 1) * focal_sz);

            std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> focal_content = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, last - first);
            std::copy(std::next(matrix->being_vec.get(), first), std::next(matrix->being_vec.get(), last), focal_content.get());

            std::shared_ptr<tensor_model::Matrix> focal = std::allocate_shared<tensor_model::Matrix>(allocator,
                                                                                                     tensor_model::Matrix{.being_vec    = std::move(focal_content),
                                                                                                                          .being_vec_sz = last - first});

            result.push_back(std::move(focal));
        }

        return result;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto focal_unsplit_matrix(const stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator>& matrix_vec,
                                        size_t group_by_sz,
                                        const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        stdx::transparent_vector<std::shared_ptr<tensor_model::BeingUnit>, Allocator> result(allocator);

        for (const auto& matrix: matrix_vec)
        {
            std::copy(matrix->being_vec.get(), std::next(matrix->being_vec.get(), matrix->being_vec_sz), std::back_inserter(result));
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = to_shared_array(result, allocator),
                                                                               .being_vec_sz    = result.size()});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto deparameterize(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                  double perc,
                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::deparameterize(matrix->being_vec[i], perc, allocator);
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = matrix->being_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::Matrix>& lhs,
                              const std::shared_ptr<tensor_model::Matrix>& rhs,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
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

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = lhs->being_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::Matrix> * matrix_arr,
                              size_t matrix_arr_sz,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        if (matrix_arr_sz == 0u)
        {
            throw std::invalid_argument("bad accumulation size, 0");
        }

        std::shared_ptr<tensor_model::Matrix> result = matrix_arr[0];

        for (size_t i = 1u; i < matrix_arr_sz; ++i)
        {
            result = accumulate(result, matrix_arr[i], allocator);
        }

        return result;
    }

    template <class ValueType,
              class Allocator = std::allocator<char>>
    constexpr auto div(const std::shared_ptr<tensor_model::Matrix>& matrix,
                       const ValueType& value,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            rs[i] = tensor_being_unit_operation::div(matrix->being_vec[i], value, allocator);
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                               .being_vec_sz    = matrix->being_vec_sz});

    }

    template <class Allocator = std::allocator<char>>
    constexpr auto avg(const std::shared_ptr<tensor_model::Matrix> * matrix_arr,
                       size_t matrix_arr_sz,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        return div(accumulate(matrix_arr, matrix_arr_sz, allocator), stdx::safe_non_zero_access(matrix_arr_sz));
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto series_normalize(const std::shared_ptr<tensor_model::Matrix> * matrix_arr,
                                    size_t matrix_arr_sz,
                                    const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        constexpr double NORMALIZATION_EXP_BASE = 2;

        std::shared_ptr<std::shared_ptr<tensor_model::Matrix>[]> rs  = std::allocate_shared<std::shared_ptr<tensor_model::Matrix>[]>(allocator, matrix_arr_sz);
        double normalization_ptr = 1;

        for (size_t i = 0u; i < matrix_arr_sz; ++i)
        {
            rs[i]               = div(matrix_arr[i], normalization_ptr, allocator);
            normalization_ptr   *= NORMALIZATION_EXP_BASE;
        }

        return accumulate(rs.get(), matrix_arr_sz, allocator);
    }

    template <class ...Args>
    constexpr void flatten(const std::shared_ptr<tensor_model::Matrix>& arg,
                           std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            tensor_being_unit_operation::flatten(arg->being_vec[i], output_vec);
        }
    }

    template <class ...Args>
    constexpr void get_shape(const std::shared_ptr<tensor_model::Matrix>& arg,
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
    constexpr auto immutable_shape_matrix_to_unique_representation(const std::shared_ptr<tensor_model::Matrix>& arg,
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
    constexpr auto matrix_to_unique_representation(const std::shared_ptr<tensor_model::Matrix>& arg,
                                                   const Allocator& allocator = Allocator()) -> stdx::transparent_string<Allocator>
    {
        return immutable_shape_matrix_to_unique_representation(arg, allocator);
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto unfocal_matrix(const stdx::transparent_vector<std::shared_ptr<tensor_model::Matrix>, Allocator>& matrix_vec,
                                  size_t i,
                                  const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& focal_suffix_map,
                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
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

            std::shared_ptr<tensor_model::Matrix> org_matrix = std::allocate_shared<tensor_model::Matrix>(allocator,
                                                                                                          tensor_model::Matrix{.being_vec       = std::move(rs),
                                                                                                                               .being_vec_sz    = matrix_vec[i]->being_vec_sz});

            result_vec.push_back(std::move(org_matrix));
        }

        return avg(result_vec.data(), result_vec.size(), allocator);
    }
    
    constexpr auto get_hash_index(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                  size_t function_idx,
                                  size_t hash_table_sz) -> size_t
    {
        if (hash_table_sz == 0u)
        {
            throw std::invalid_argument("bad hash table size, 0");
        }

        auto serializable           = matrix_serializer::serialize(matrix);
        auto hashable               = std::make_pair(std::move(serializable), function_idx);
        std::string str_hashable    = dg::network_compact_serializer::serialize<std::string>(hashable);
        size_t hash_clue            = hasher::hash_bytes(str_hashable.data(), str_hashable.size());

        return hash_clue % hash_table_sz;
    }

    template <class TaylorBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto mono_transform(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                                            TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                            const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                            const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                            const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        stdx::safe_ptr_access(matrix.get());

        std::shared_ptr<std::shared_ptr<tensor_model::BeingUnit>[]> being_vec  = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, matrix->being_vec_sz);

        for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
        {
            being_vec[i] = tensor_being_unit_operation::mono_transform(matrix->being_vec[i],
                                                                       base_coeff_sz_container,
                                                                       coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                       taylor_base_promotion_tag,
                                                                       allocator);
        }

        return std::allocate_shared<tensor_model::Matrix>(allocator,
                                                          tensor_model::Matrix{.being_vec       = std::move(being_vec),
                                                                               .being_vec_sz    = matrix->being_vec_sz});
    }

    template <class TaylorBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto feed_forward_transform(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                                                    TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                    const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                    const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                    const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        std::shared_ptr<tensor_model::Matrix> y = mono_transform(matrix,
                                                                 base_coeff_sz_container,
                                                                 coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                 taylor_base_promotion_tag,
                                                                 allocator);

        std::shared_ptr<tensor_model::Matrix> avg_arr[]{matrix, y};

        return avg(avg_arr, 2u, allocator);
    }

    template <class TaylorBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto matrix_transform(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                                              const stdx::transparent_vector<size_t, Allocator>& focal_sz_vec,
                                                              const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& focal_suffix_map,
                                                              const stdx::transparent_unordered_map<size_t, stdx::transparent_unordered_map<size_t, stdx::transparent_vector<stdx::transparent_vector<size_t, Allocator>, Allocator>, Allocator>, Allocator>& accum_suffix_map,

                                                              const stdx::transparent_vector<size_t, Allocator>& rotation_sz_vec,
                                                              const stdx::transparent_vector<double, Allocator>& parameter_bound_ratio_vec,
                                                              TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                              const std::add_pointer_t<tensor_model::tensor_std_float_t> * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,

                                                              DispatchCodeGenerator& dispatch_code_gen,

                                                              const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},

                                                              bool has_logit_unit_reuse_tag = true,
                                                              bool has_logit_group_logit_reuse_tag = true,
                                                              bool has_being_logit_reuse_tag = true,
                                                              bool has_base_matrix_logit_reuse_tag = true,

                                                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
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

            std::shared_ptr<BeingUnit> lhs = tensor_being_unit_operation::left_major_intercourse_being_unit(matrix->being_vec[0],
                                                                                                            matrix->being_vec[1],
                                                                                                            base_coeff_sz_container,
                                                                                                            coeff_arr[dispatch_code_gen.get_dispatch_code()], coeff_arr_offset, coeff_arr_cap,
                                                                                                            taylor_base_promotion_tag,
                                                                                                            has_logit_unit_reuse_tag,
                                                                                                            has_logit_group_logit_reuse_tag,
                                                                                                            has_being_logit_reuse_tag,
                                                                                                            allocator);

            if (has_base_matrix_logit_reuse_tag)
            {
                coeff_arr_offset        = saved_coeff_arr_offset;
            }

            std::shared_ptr<BeingUnit> rhs = tensor_being_unit_operation::left_major_intercourse_being_unit(matrix->being_vec[1],
                                                                                                            matrix->being_vec[0],
                                                                                                            base_coeff_sz_container,
                                                                                                            coeff_arr[dispatch_code_gen.get_dispatch_code()], coeff_arr_offset, coeff_arr_cap,
                                                                                                            taylor_base_promotion_tag,
                                                                                                            has_logit_unit_reuse_tag,
                                                                                                            has_logit_group_logit_reuse_tag,
                                                                                                            has_being_logit_reuse_tag,
                                                                                                            allocator);

            decltype(lhs) lhs_combined[]{lhs, matrix->being_vec[0]};
            decltype(rhs) rhs_combined[]{rhs, matrix->being_vec[1]};
            
            auto final_lhs  = tensor_being_unit_operation::avg(lhs_combined, 2u, allocator);
            auto final_rhs  = tensor_being_unit_operation::avg(rhs_combined, 2u, allocator);
            
            std::shared_ptr<Matrix> tmp = std::allocate_shared<Matrix>(allocator,
                                                                       Matrix{.being_vec       = to_shared_array(stdx::transparent_vector<std::shared_ptr<BeingUnit>, Allocator>{final_lhs, final_rhs}, allocator),
                                                                              .being_vec_sz    = 2u});

            return feed_forward_transform(tmp,
                                          base_coeff_sz_container,
                                          coeff_arr[dispatch_code_gen.get_dispatch_code()], coeff_arr_offset, coeff_arr_cap,
                                          taylor_base_promotion_tag,
                                          allocator);
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
            if (i != 0u)
            {
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
                    size_t positional_idx                                                                   = 0u;

                    for (const auto& focal: focal_deparamed_matrix_vec)
                    {
                        coeff_arr_offset        = saved_coeff_arr_offset_1;

                        std::shared_ptr<Matrix> transformed_focal = matrix_transform(focal,
                                                                                    {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                                    focal_suffix_map,
                                                                                    accum_suffix_map,
                                                                                    {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                                    {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                                    base_coeff_sz_container,
                                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                    dispatch_code_gen,
                                                                                    taylor_base_promotion_tag,
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

                std::shared_ptr<Matrix> incremental_result  = unfocal_matrix(accum_matrix_vec, i, accum_suffix_map, allocator);
                incremental_matrix_vec.push_back(std::move(incremental_result));
            }

            if (i + 1 != rotation_sz)
            {
                stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> up_to_point_incremental_matrix_vec(allocator);

                stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focused_matrix_vec = matrix_to_focal(up_to_point_matrix,
                                                                                                                  i,
                                                                                                                  focal_suffix_map,
                                                                                                                  allocator);

                for (const auto& focused_matrix: focused_matrix_vec)
                {
                    stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> focal_matrix_vec       = focal_split_matrix(focused_matrix, focal_sz, allocator);
                    stdx::transparent_vector<std::shared_ptr<Matrix>, Allocator> transformed_focal_vec(allocator);
                    const size_t saved_coeff_arr_offset_0                                               = coeff_arr_offset;
                    size_t positional_idx                                                               = 0u;

                    for (const auto& focal: focal_matrix_vec)
                    {
                        coeff_arr_offset        = saved_coeff_arr_offset_0;

                        std::shared_ptr<Matrix> transformed_focal = matrix_transform(focal,
                                                                                     {std::next(focal_sz_vec.begin()), focal_sz_vec.end()},
                                                                                     focal_suffix_map,
                                                                                     accum_suffix_map,
                                                                                     {std::next(rotation_sz_vec.begin()), rotation_sz_vec.end()},
                                                                                     {std::next(parameter_bound_ratio_vec.begin()), parameter_bound_ratio_vec.end()},
                                                                                     base_coeff_sz_container,
                                                                                     coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                     dispatch_code_gen,
                                                                                     taylor_base_promotion_tag,
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

                    std::shared_ptr<Matrix> incremental_up_to_point_matrix  = unfocal_matrix(up_to_point_incremental_matrix_vec, i, focal_suffix_map, allocator);
                    auto avg_arr                                            = std::array<std::shared_ptr<Matrix>, 2u>{up_to_point_matrix, incremental_up_to_point_matrix};
                    up_to_point_matrix                                      = avg(avg_arr.data(), avg_arr.size(), allocator);
                }
            }
        }

        std::shared_ptr<Matrix> tmp = series_normalize(incremental_matrix_vec.data(), incremental_matrix_vec.size(), allocator);

        return feed_forward_transform(tmp,
                                      base_coeff_sz_container,
                                      coeff_arr[dispatch_code_gen.get_dispatch_code()], coeff_arr_offset, coeff_arr_cap,
                                      taylor_base_promotion_tag,
                                      allocator);
    }
}

#endif