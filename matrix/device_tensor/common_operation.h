#ifndef __MATRIX_DEVICE_TENSOR_COMMON_OPERATION_H__
#define __MATRIX_DEVICE_TENSOR_COMMON_OPERATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <array>
#include "model.h"
#include <optional>
#include <cuda_management/utility.h>

namespace device_tensor::common_operation
{
    using namespace device_tensor::model;

    __device__ inline auto get_shape(const Matrix * arg) -> std::array<size_t, 4u>
    {
        using namespace cuda_management::utility;

        safe_ptr_access(arg);
        std::array<size_t, 4u> rs{0, 0, 0, 0};

        rs[0]   = arg->being_vec_sz;

        if (arg->being_vec_sz == 0u)
        {
            return;
        }

        rs[1]   = safe_ptr_access(arg->being_vec[0])->process_group_vec_sz;

        if (arg->being_vec[0]->process_group_vec_sz == 0u)
        {
            return;
        }

        rs[2]   = PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ;
        rs[3]   = PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;

        return rs;
    }

    __device__ inline auto shape_size(const std::array<size_t, 4u>& shape) -> size_t
    {
        size_t sz = 1u;

        for (size_t e: shape)
        {
            sz *= e;
        }

        return sz;
    }
}

#endif