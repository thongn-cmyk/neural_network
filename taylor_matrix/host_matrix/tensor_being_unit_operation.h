//HEADER_CONTROL 5

#ifndef __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_TENSOR_BEING_UNIT_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include <memory>
#include <vector>
#include <matrix/tensor_model.h>
#include <stdexcept>
#include "tensor_process_group_operation.h"
#include <general_definition/float_def.h>

namespace taylor_matrix::host_matrix::tensor_being_unit_operation
{
    template <class ...Args, class Allocator = std::allocator<char>>
    constexpr auto make_being_unit_from_shape_vec(const std::vector<size_t, Args...>& space,
                                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        if (space.empty())
        {
            throw std::invalid_argument("bad space shape, void space");
        }

        size_t dimension_sz = space.front();
        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, dimension_sz);

        for (size_t i = 0u; i < dimension_sz; ++i)
        {
            rs[i] = tensor_process_group_operation::make_process_group_from_shape_vec({std::next(space.begin()), space.end()}, allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = dimension_sz});

    }

    constexpr void internal_unflatten(const std::shared_ptr<tensor_model::BeingUnit>& arg,
                                      const tensor_model::tensor_std_float_t * input_arr, size_t input_arr_sz,
                                      size_t& offset)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->process_group_vec_sz; ++i)
        {
            tensor_process_group_operation::internal_unflatten(arg->process_group_vec[i], input_arr, input_arr_sz, offset);
        }
    }

    template <class ...Args, class ...Args1, class Allocator = std::allocator<char>>
    constexpr auto make_being_unit_from_flat_vec(const std::vector<size_t, Args...>& space,
                                                 const std::vector<tensor_model::tensor_std_float_t, Args1...>& input_vec,
                                                 const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        std::shared_ptr<tensor_model::BeingUnit> rs = make_being_unit_from_shape_vec(space, allocator);
        size_t offset = 0u;
        internal_unflatten(rs, input_vec.data(), input_vec.size(), offset);

        return rs;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto empty_as(const std::shared_ptr<tensor_model::BeingUnit>& being,
                            const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(being.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, being->process_group_vec_sz);

        for (size_t i = 0u; i < being->process_group_vec_sz; ++i)
        {
            rs[i] = tensor_process_group_operation::empty_as(being->process_group_vec[i], allocator);
        }

        return std::make_shared<tensor_model::BeingUnit>(tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                 .process_group_vec_sz  = being->process_group_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::BeingUnit>& lhs,
                              const std::shared_ptr<tensor_model::BeingUnit>& rhs,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        if (lhs->process_group_vec_sz != rhs->process_group_vec_sz)
        {
            throw std::invalid_argument("incompatible pairwise size");
        }

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, lhs->process_group_vec_sz);

        for (size_t i = 0u; i < lhs->process_group_vec_sz; ++i)
        {
            rs[i] = tensor_process_group_operation::accumulate(lhs->process_group_vec[i], rhs->process_group_vec[i], allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = lhs->process_group_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto accumulate(const std::shared_ptr<tensor_model::BeingUnit> * being_arr,
                              size_t being_arr_sz,
                              const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        if (being_arr_sz == 0u)
        {
            throw std::invalid_argument("bad accumulation size, 0");
        }

        std::shared_ptr<tensor_model::BeingUnit> rs = being_arr[0];

        for (size_t i = 1u; i < being_arr_sz; ++i)
        {
            rs = accumulate(rs, being_arr[i], allocator);
        }

        return rs;
    }

    template <class ValueType,
              class Allocator = std::allocator<char>>
    constexpr auto div(const std::shared_ptr<tensor_model::BeingUnit>& being_unit,
                       const ValueType& value,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(being_unit.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, being_unit->process_group_vec_sz);

        for (size_t i = 0u; i < being_unit->process_group_vec_sz; ++i)
        {
            rs[i] = tensor_process_group_operation::div(being_unit->process_group_vec[i], value, allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = being_unit->process_group_vec_sz});
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto avg(const std::shared_ptr<tensor_model::BeingUnit> * being_unit_arr,
                       size_t being_unit_arr_sz,
                       const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        return div(accumulate(being_unit_arr, being_unit_arr_sz, allocator),
                   stdx::safe_non_zero_access(being_unit_arr_sz),
                   allocator);
    }

    template <class TaylorBaseCoeffSizeContainer,
              class TaylorBasePromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto left_major_intercourse_being_unit(const std::shared_ptr<tensor_model::BeingUnit>& lhs,
                                                                               const std::shared_ptr<tensor_model::BeingUnit>& rhs,
                                                                               TaylorBaseCoeffSizeContainer base_coeff_sz_container,
                                                                               const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                               const stdx::Tag<TaylorBasePromotedFloatType>& promotion_tag = stdx::Tag<TaylorBasePromotedFloatType>{},
                                                                               bool has_process_unit_logit_reuse_tag = true,
                                                                               bool has_process_group_logit_reuse_tag = true,
                                                                               bool has_being_logit_reuse_tag = true,
                                                                               const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, lhs->process_group_vec_sz);

        const size_t saved_coeff_arr_offset         = coeff_arr_offset;

        for (size_t i = 0u; i < lhs->process_group_vec_sz; ++i)
        {
            if (has_being_logit_reuse_tag)
            {
                coeff_arr_offset        = saved_coeff_arr_offset;
            }

            std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> accum_arr = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, rhs->process_group_vec_sz);

            for (size_t j = 0u; j < rhs->process_group_vec_sz; ++j)
            {
                accum_arr[j]     = tensor_process_group_operation::left_major_intercourse_process_group(lhs->process_group_vec[i],
                                                                                                        rhs->process_group_vec[j],
                                                                                                        base_coeff_sz_container,
                                                                                                        coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                                        promotion_tag,
                                                                                                        has_process_unit_logit_reuse_tag,
                                                                                                        has_process_group_logit_reuse_tag,
                                                                                                        allocator);
            }

            rs[i] = tensor_process_group_operation::avg(accum_arr.get(), rhs->process_group_vec_sz, allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = lhs->process_group_vec_sz});
    }

    template <class QuantizationMachine1D,
              class QuantizationMachine2D,
              class PromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto left_major_interpolate_being_unit(const std::shared_ptr<tensor_model::BeingUnit>& lhs,
                                                                               const std::shared_ptr<tensor_model::BeingUnit>& rhs,
                                                                               QuantizationMachine1D&& quant_machine_1d,
                                                                               QuantizationMachine2D&& quant_machine_2d,
                                                                               const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                                               const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{},
                                                                               bool has_process_unit_logit_reuse_tag = true,
                                                                               bool has_process_group_logit_reuse_tag = true,
                                                                               bool has_being_logit_reuse_tag = true,
                                                                               const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(lhs.get());
        stdx::safe_ptr_access(rhs.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, lhs->process_group_vec_sz);

        const size_t saved_coeff_arr_offset         = coeff_arr_offset;

        for (size_t i = 0u; i < lhs->process_group_vec_sz; ++i)
        {
            if (has_being_logit_reuse_tag)
            {
                coeff_arr_offset        = saved_coeff_arr_offset;
            }

            std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> accum_arr = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, rhs->process_group_vec_sz);

            for (size_t j = 0u; j < rhs->process_group_vec_sz; ++j)
            {
                accum_arr[j]     = tensor_process_group_operation::left_major_interpolate_process_group(lhs->process_group_vec[i],
                                                                                                        rhs->process_group_vec[j],
                                                                                                        quant_machine_1d,
                                                                                                        quant_machine_2d,
                                                                                                        coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                                        promotion_tag,
                                                                                                        has_process_unit_logit_reuse_tag,
                                                                                                        has_process_group_logit_reuse_tag,
                                                                                                        allocator);
            }

            rs[i] = tensor_process_group_operation::avg(accum_arr.get(), rhs->process_group_vec_sz, allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = lhs->process_group_vec_sz});
    }

    template <class QuantizationMachine1D,
              class QuantizationMachine2D,
              class PromotedFloatType = tensor_model::tensor_std_float_t,
              class Allocator = std::allocator<char>>
    constexpr __attribute__((noinline)) auto mono_transform(const std::shared_ptr<tensor_model::BeingUnit>& arg,
                                                            QuantizationMachine1D&& quant_machine_1d,
                                                            QuantizationMachine2D&& quant_machine_2d,
                                                            const tensor_model::tensor_std_float_t * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                                            const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>(),
                                                            const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(arg.get());

        std::shared_ptr<tensor_model::BeingUnit> rs0    = {};

        {
            std::shared_ptr<tensor_model::BeingUnit> xx = left_major_interpolate_being_unit(arg,
                                                                                            arg,
                                                                                            quant_machine_1d,
                                                                                            quant_machine_2d,
                                                                                            coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                            promotion_tag,
                                                                                            false,
                                                                                            false,
                                                                                            false,
                                                                                            allocator);

            std::shared_ptr<tensor_model::BeingUnit> avg_arr[]{arg, xx};
            rs0 = avg(avg_arr, 2u, allocator);
        }

        std::shared_ptr<tensor_model::BeingUnit> rs1    = {};

        {
            std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> process_group_vec = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator,
                                                                                                                                                                   rs0->process_group_vec_sz);

            for (size_t i = 0u; i < rs0->process_group_vec_sz; ++i)
            {
                process_group_vec[i]   = tensor_process_group_operation::mono_transform(rs0->process_group_vec[i],
                                                                                        quant_machine_1d,
                                                                                        quant_machine_2d,
                                                                                        coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                        promotion_tag,
                                                                                        allocator);
            }

            std::shared_ptr<tensor_model::BeingUnit> sub_xx =  std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(process_group_vec),
                                                                                                                                     .process_group_vec_sz  = rs0->process_group_vec_sz});

            std::shared_ptr<tensor_model::BeingUnit> avg_arr[]{rs0, sub_xx};
            rs1 = avg(avg_arr, 2u, allocator);
        }

        return rs1;
    }

    template <class Allocator = std::allocator<char>>
    constexpr auto deparameterize(const std::shared_ptr<tensor_model::BeingUnit>& being,
                                  double perc,
                                  const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(being.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, being->process_group_vec_sz);

        for (size_t i = 0u; i < being->process_group_vec_sz; ++i)
        {
            rs[i] = tensor_process_group_operation::deparameterize(being->process_group_vec[i], perc, allocator);
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = being->process_group_vec_sz});
    }

    template <class FloatType,
              class Allocator = std::allocator<char>>
    constexpr auto positional_encode(const std::shared_ptr<tensor_model::BeingUnit>& being_unit,
                                     const FloatType& amplitude,
                                     const FloatType& frequency_multiplier,
                                     const FloatType& x_offset,
                                     const FloatType& y_offset,
                                     size_t pe_dimension_idx,
                                     size_t dedicated_pe_sz,
                                     const Allocator& allocator = Allocator()) -> std::shared_ptr<tensor_model::BeingUnit>
    {
        stdx::safe_ptr_access(being_unit.get());

        std::shared_ptr<std::shared_ptr<tensor_model::ProcessGroup>[]> rs = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, being_unit->process_group_vec_sz);
        size_t actual_pe_sz = std::min(being_unit->process_group_vec_sz, dedicated_pe_sz);

        for (size_t i = 0u; i < being_unit->process_group_vec_sz; ++i)
        {
            if (i < actual_pe_sz)
            {
                rs[i] = tensor_process_group_operation::positional_encode(being_unit->process_group_vec[i], amplitude, frequency_multiplier, x_offset, y_offset, pe_dimension_idx, allocator);
            }
            else
            {
                rs[i] = being_unit->process_group_vec[i];
            }
        }

        return std::allocate_shared<tensor_model::BeingUnit>(allocator,
                                                             tensor_model::BeingUnit{.process_group_vec     = std::move(rs),
                                                                                     .process_group_vec_sz  = being_unit->process_group_vec_sz});
    }

    template <class ...Args>
    constexpr void flatten(const std::shared_ptr<tensor_model::BeingUnit>& arg, std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->process_group_vec_sz; ++i)
        {
            tensor_process_group_operation::flatten(arg->process_group_vec[i], output_vec);
        }
    }

    template <class ...Args>
    constexpr void get_shape(const std::shared_ptr<tensor_model::BeingUnit>& arg,
                             std::vector<size_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        if (arg->process_group_vec_sz == 0u)
        {
            throw std::invalid_argument("bad being unit, 0 item");
        }

        output_vec.push_back(arg->process_group_vec_sz);

        stdx::safe_ptr_access(arg->process_group_vec.get());
        tensor_process_group_operation::get_shape(arg->process_group_vec[0], output_vec);
    }
}

#endif