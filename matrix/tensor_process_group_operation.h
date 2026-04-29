//HEADER_CONTROL 4

#ifndef __TENSOR_PROCESS_GROUP_OPERATION_H__
#define __TENSOR_PROCESS_GROUP_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include <vector>
#include "tensor_model.h"
#include <stdexcept>
#include "tensor_process_unit_operation.h"

namespace tensor_process_group_operation
{
    static inline constexpr size_t PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ = tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

    template <class ...Args, class Allocator = std::allocator<char>>
    constexpr auto make_process_group_from_shape_vec(const std::vector<size_t, Args...>& space,
                                                     const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        if (space.empty())
        {
            throw std::invalid_argument("bad space shape, void space");
        }

        size_t dimension_sz = space.front();

        if (dimension_sz != PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad space shape, incompatible size");
        }

        tensor_model::ProcessGroup rs{};

        for (auto& e: rs.process_vec)
        {
            e = tensor_process_unit_operation::make_process_unit_from_shape_vec({std::next(space.begin()), space.end()});
        }

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator, rs);
    }

    constexpr void internal_unflatten(const std::shared_ptr<tensor_model::ProcessGroup>& arg,
                                      const tensor_model::tensor_std_float_t * input_arr, size_t input_arr_sz,
                                      size_t& offset)
    {
        stdx::safe_ptr_access(arg.get());

        for (auto& e: arg->process_vec)
        {
            tensor_process_unit_operation::internal_unflatten(e, input_arr, input_arr_sz, offset);
        }
    }

    template <class ...Args, class ...Args1, class Allocator = std::allocator<char>>
    constexpr auto make_process_group_from_flat_vec(const std::vector<size_t, Args...>& space,
                                                    const std::vector<tensor_model::tensor_std_float_t, Args1...>& input_vec,
                                                    const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        std::shared_ptr<tensor_model::ProcessGroup> rs = make_process_group_from_shape_vec(space, allocator);
        size_t offset = 0u;
        internal_unflatten(rs, input_vec.data(), input_vec.size(), offset);

        return rs;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto empty_as(const std::shared_ptr<tensor_model::ProcessGroup>& process_group,
                            const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(process_group.get());
        tensor_model::ProcessGroup rs{};

        for (size_t i = 0u; i < rs.process_vec.size(); ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::empty_as(process_group->process_vec[i]);
        }

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator, std::move(rs));
    }

    template <class TaylorBaseCoeffSizeContainer,
              class ShapeBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class ShapeBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>,
              size_t BATCH_SZ = 64u>
    constexpr __attribute__((noinline)) auto left_major_intercourse_process_group(const std::shared_ptr<tensor_model::ProcessGroup>& lhs,
                                                                                  const std::shared_ptr<tensor_model::ProcessGroup>& rhs,
                                                                                  TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                                  const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                                  ShapeBaseCoeffSizeContainer base_shape_coeff_sz_container,
                                                                                  const tensor_model::tensor_std_float_t * shape_coeff_arr, size_t& shape_coeff_arr_offset, size_t shape_coeff_arr_cap,
                                                                                  const stdx::Tag<TaylorBasePromotedFloatType>& taylor_base_promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                                  const stdx::Tag<ShapeBasePromotedFloatType>& shape_base_promotion_tag = stdx::Tag<ShapeBasePromotedFloatType>{},
                                                                                  bool has_process_unit_logit_reuse_tag = true,
                                                                                  bool has_process_group_logit_reuse_tag = true,
                                                                                  const Allocator& allocator = Allocator(),
                                                                                  const std::integral_constant<size_t, BATCH_SZ>& batch_sz = std::integral_constant<size_t, BATCH_SZ>{}) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        tensor_model::ProcessGroup rs{};

        constexpr size_t TOTAL_ITERATION_SZ = tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ * tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
        constexpr size_t REVOLUTION_SZ      = TOTAL_ITERATION_SZ / BATCH_SZ;
        constexpr size_t OFFSET_SZ          = REVOLUTION_SZ * BATCH_SZ;
        constexpr size_t REM_SZ             = TOTAL_ITERATION_SZ - OFFSET_SZ;

        static_assert(REM_SZ == 0u);

        const size_t saved_coeff_arr_offset         = coeff_arr_offset;
        const size_t saved_shape_coeff_arr_offset   = shape_coeff_arr_offset;

        tensor_model::ProcessGroup accum_vec[tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ];

        for (size_t i = 0u; i < REVOLUTION_SZ; ++i)
        {
            tensor_model::ProcessUnit lhs_tensor_arr[BATCH_SZ];
            tensor_model::ProcessUnit rhs_tensor_arr[BATCH_SZ];
            tensor_model::ProcessUnit out_tensor_arr[BATCH_SZ];

            for (size_t j = 0u; j < BATCH_SZ; ++j)
            {
                const size_t virtual_idx    = i * BATCH_SZ + j;
                const size_t actual_i       = virtual_idx / tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
                const size_t actual_j       = virtual_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

                lhs_tensor_arr[j]           = lhs->process_vec[actual_i];
                rhs_tensor_arr[j]           = rhs->process_vec[actual_j];
            }

            coeff_arr_offset        = saved_coeff_arr_offset;
            shape_coeff_arr_offset  = saved_shape_coeff_arr_offset;

            tensor_process_unit_operation::batch_intercourse_process_unit(lhs_tensor_arr,
                                                                          rhs_tensor_arr,
                                                                          batch_sz,
                                                                          out_tensor_arr,
                                                                          base_coeff_sz_container,
                                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                          base_shape_coeff_sz_container,
                                                                          shape_coeff_arr, shape_coeff_arr_offset, shape_coeff_arr_cap,
                                                                          taylor_base_promotion_tag,
                                                                          shape_base_promotion_tag,
                                                                          has_process_unit_logit_reuse_tag);

            for (size_t j = 0u; j < BATCH_SZ; ++j)
            {
                const size_t virtual_idx    = i * BATCH_SZ + j;
                const size_t actual_i       = virtual_idx / tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
                const size_t actual_j       = virtual_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;

                accum_vec[actual_i].process_vec[actual_j] = out_tensor_arr[j];
            }
        }

        for (size_t i = 0u; i < tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ; ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::avg(accum_vec[i].process_vec.data(), accum_vec[i].process_vec.size());
        }

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator, std::move(rs));
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto deparameterize(const std::shared_ptr<tensor_model::ProcessGroup>& process_group,
                                  double perc,
                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(process_group.get());

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator,
                                                                tensor_model::ProcessGroup{.process_vec = stdx::copy_and_trail_defaultize(process_group->process_vec,
                                                                                                                                          perc,
                                                                                                                                          static_cast<tensor_model::ProcessUnit (*)(const tensor_model::ProcessUnit&)>(tensor_process_unit_operation::empty_as))});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::ProcessGroup>& lhs,
                              const std::shared_ptr<tensor_model::ProcessGroup>& rhs,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        const auto& lhs_content = lhs->process_vec;
        const auto& rhs_content = rhs->process_vec;

        tensor_model::ProcessGroup rs{};

        for (size_t i = 0u; i < lhs_content.size(); ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::accumulate(lhs_content[i], rhs_content[i]);
        }

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator,
                                                                std::move(rs));
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::ProcessGroup> * process_group_arr,
                              size_t process_group_arr_sz,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        if (process_group_arr_sz == 0u)
        {
            throw std::invalid_argument("bad accumulation size, 0");
        }

        std::shared_ptr<tensor_model::ProcessGroup> result = process_group_arr[0];

        for (size_t i = 1u; i < process_group_arr_sz; ++i)
        {
            result = accumulate(result, process_group_arr[i], allocator);
        }

        return result;
    }

    template <class FloatType, class Allocator = std::allocator<char>>
    constexpr auto positional_encode(const std::shared_ptr<tensor_model::ProcessGroup>& process_group,
                                     const FloatType& amplitude,
                                     const FloatType& frequency_multiplier,
                                     const FloatType& x_offset,
                                     const FloatType& y_offset,
                                     size_t dimension_idx,
                                     const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(process_group.get());

        tensor_model::ProcessGroup rs   = *process_group;
        size_t tentative_slot_idx       = dimension_idx / tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
        size_t slot_idx                 = tentative_slot_idx % tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
        size_t slot_offset              = dimension_idx % tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;

        rs.process_vec[slot_idx]        = tensor_process_unit_operation::positional_encode(rs.process_vec[slot_idx], amplitude, frequency_multiplier, x_offset, y_offset, slot_offset);

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator, std::move(rs));
    }

    template <class ValueType, class Allocator = std::allocator<char>>
    constexpr auto div(const std::shared_ptr<tensor_model::ProcessGroup>& process_group,
                       const ValueType& value,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        stdx::safe_ptr_access(process_group.get());

        tensor_model::ProcessGroup rs{};

        for (size_t i = 0u; i < rs.process_vec.size(); ++i)
        {
            rs.process_vec[i] = tensor_process_unit_operation::div(process_group->process_vec[i], value);
        }

        return std::allocate_shared<tensor_model::ProcessGroup>(allocator, std::move(rs));
    }

    template <class ...Args, class Allocator = std::allocator<char>>
    constexpr auto avg(const std::shared_ptr<tensor_model::ProcessGroup> * process_group_arr,
                       size_t process_group_arr_sz,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::ProcessGroup>
    {
        return div(accumulate(process_group_arr, process_group_arr_sz, allocator), stdx::safe_non_zero_access(process_group_arr_sz), allocator);
    }

    template <class ...Args>
    constexpr void flatten(const std::shared_ptr<tensor_model::ProcessGroup>& arg,
                           std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        for (const auto& e: arg->process_vec)
        {
            tensor_process_unit_operation::flatten(e, output_vec);
        }
    }

    template <class ...Args>
    constexpr void get_shape(const std::shared_ptr<tensor_model::ProcessGroup>& arg,
                             std::vector<size_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        output_vec.push_back(tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ);
        tensor_process_unit_operation::get_shape(arg->process_vec[0], output_vec);
    }
}

#endif