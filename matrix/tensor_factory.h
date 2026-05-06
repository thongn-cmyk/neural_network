#ifndef __MATRIX_TENSOR_FACTORY_H__
#define __MATRIX_TENSOR_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include "tensor_model.h"
#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>

namespace tensor_factory
{
    template <class ...Args>
    constexpr void check_shape(const std::vector<size_t, Args...>& space)
    {
        constexpr size_t RECURSIVE_DIMENSION_SZ = 4u; 

        if (space.size() != RECURSIVE_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad shape, incompatible dimension size");
        }

        if (space[0] == 0u)
        {
            throw std::invalid_argument("bad shape, empty space");
        }

        if (space[1] == 0u)
        {
            throw std::invalid_argument("bad shape, empty space");
        }

        if (space[2] != tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad shape, incompatible process group size");
        }

        if (space[3] != tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad shape, incompatible process unit size");
        }
    }

    template <class ...Args,
              class Allocator = std::allocator<char>>
    constexpr auto make_matrix_from_shape_vec(const std::vector<size_t, Args...>& space,
                                              Allocator&& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        check_shape(space);

        std::shared_ptr<tensor_model::Matrix> rs    = std::allocate_shared<tensor_model::Matrix>(allocator);
        rs->being_vec                               = std::allocate_shared<std::shared_ptr<tensor_model::BeingUnit>[]>(allocator, space[0]);
        rs->being_vec_sz                            = space[0];

        for (size_t i = 0u; i < space[0]; ++i)
        {
            rs->being_vec[i]                        = std::allocate_shared<tensor_model::BeingUnit>(allocator);
            rs->being_vec[i]->process_group_vec     = std::allocate_shared<std::shared_ptr<tensor_model::ProcessGroup>[]>(allocator, space[1]);
            rs->being_vec[i]->process_group_vec_sz  = space[1];

            for (size_t j = 0u; j < space[1]; ++j)
            {
                rs->being_vec[i]->process_group_vec[j] = std::allocate_shared<tensor_model::ProcessGroup>(allocator);
            }
        }

        return rs;
    }

    template <class ...Args,
              class ...Args1,
              class Allocator = std::allocator<char>>
    constexpr auto make_matrix_from_flat_vec(const std::vector<size_t, Args...>& space,
                                             const std::vector<tensor_model::tensor_std_float_t, Args1...>& input_vec,
                                             Allocator&& allocator = Allocator()) -> std::shared_ptr<tensor_model::Matrix>
    {
        constexpr size_t RECURSIVE_DIMENSION_SZ = 4u;

        if (space.size() != RECURSIVE_DIMENSION_SZ)
        {
            throw std::invalid_argument("bad shape, incompatible dimension size");
        }

        std::shared_ptr<tensor_model::Matrix> rs = make_matrix_from_shape_vec(space, allocator);
        size_t ptr = 0u; 

        for (size_t i = 0u; i < rs->being_vec_sz; ++i)
        {
            for (size_t j = 0u; j < rs->being_vec[i]->process_group_vec_sz; ++j)
            {
                for (size_t k = 0u; k < rs->being_vec[i]->process_group_vec[j]->process_vec.size(); ++k)
                {
                    for (size_t z = 0u; z < rs->being_vec[i]->process_group_vec[j]->process_vec[k].logit_vec.size(); ++z)
                    {
                        rs->being_vec[i]->process_group_vec[j]->process_vec[k].logit_vec[z] = input_vec[stdx::access_guard(ptr++, input_vec.size())];
                    }
                }
            }
        }

        return rs;
    }

    template <class ...Args>
    constexpr void flatten(const std::shared_ptr<tensor_model::Matrix>& arg,
                           std::vector<tensor_model::tensor_std_float_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        for (size_t i = 0u; i < arg->being_vec_sz; ++i)
        {
            for (size_t j = 0u; j < arg->being_vec[i]->process_group_vec_sz; ++j)
            {
                for (size_t k = 0u; k < arg->being_vec[i]->process_group_vec[j]->process_vec.size(); ++k)
                {
                    for (size_t z = 0u; z < arg->being_vec[i]->process_group_vec[j]->process_vec[k].logit_vec.size(); ++z)
                    {
                        output_vec.push_back(arg->being_vec[i]->process_group_vec[j]->process_vec[k].logit_vec[z]);
                    }
                }
            }
        }
    }

    template <class ...Args>
    constexpr void get_shape(const std::shared_ptr<tensor_model::Matrix>& arg,
                             std::vector<size_t, Args...>& output_vec)
    {
        stdx::safe_ptr_access(arg.get());

        output_vec.push_back(arg->being_vec_sz);

        if (arg->being_vec_sz == 0u)
        {
            return;
        }

        output_vec.push_back(arg->being_vec[0]->process_group_vec_sz);

        if (arg->being_vec[0]->process_group_vec_sz == 0u)
        {
            return;
        }

        output_vec.push_back(tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ);
        output_vec.push_back(tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ);
    }

    template <class ...Args>
    constexpr auto shape_size(const std::vector<size_t>& space) -> size_t
    {
        check_shape(space);

        size_t rs = 1u;

        for (size_t d_sz: space)
        {
            rs *= d_sz;
        }

        return rs;
    }
}

#endif