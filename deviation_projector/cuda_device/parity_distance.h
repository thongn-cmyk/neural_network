#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_PARITY_DISTANCE_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_PARITY_DISTANCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/device_tensor/model.h>
#include <matrix/device_tensor/common_operation.h>
#include <cuda_management/utility.h>

namespace deviation_projector::cuda_device::parity_distance
{
    using namespace device_tensor::model;
    using namespace deviation_projector::cuda_device::local_exception;

    __device__ inline double parity_distance(Matrix * lhs,
                                             Matrix * rhs,
                                             local_exception_t * err = nullptr)
    {
        using namespace device_tensor::common_operation;
        using namespace cuda_management::utility;

        safe_ptr_access(lhs);
        safe_ptr_access(rhs);

        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        auto lhs_shape = get_shape(lhs);
        auto rhs_shape = get_shape(rhs);

        if (lhs_shape != rhs_shape)
        {
            *err = INCOMPATIBLE_SHAPE_ERROR_CODE;
            return {};
        }

        double lhs_zero_sum = 0;
        double lhs_one_sum  = 0;
        double rhs_one_sum  = 0;
        double rhs_zero_sum = 0;

        for (size_t i = 0u; i < lhs->being_vec_sz; ++i)
        {
            for (size_t j = 0u; j < lhs->being_vec[i]->process_group_vec_sz; ++j)
            {
                for (size_t z = 0u; z < lhs->being_vec[i]->process_group_vec[j].process_vec.size(); ++z)
                {
                    for (size_t k = 0u; k < lhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec.size(); ++k)
                    {
                        double lhs_value        = lhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec[k];
                        double rhs_value        = rhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec[k];

                        rhs_one_sum             += rhs_value;
                        lhs_zero_sum            += lhs_value * (rhs_value - 1);
                        lhs_one_sum             += lhs_value * rhs_value;
                    }
                }
            }
        }

        lhs_zero_sum        = -lhs_zero_sum;

        double lhs_parity   = lhs_one_sum - lhs_zero_sum;
        double rhs_parity   = rhs_one_sum - rhs_zero_sum;
        double diff         = lhs_parity - rhs_parity;

        return diff * diff;
    }
}

#endif