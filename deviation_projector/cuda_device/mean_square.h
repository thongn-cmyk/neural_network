#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_MEAN_SQUARE_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_MEAN_SQUARE_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/device_tensor/model.h>
#include <matrix/device_tensor/common_operation.h>
#include <cuda_management/utility.h>

namespace deviation_projector::cuda_device::mean_square
{
    using namespace device_tensor::model;
    using namespace deviation_projector::cuda_device::local_exception;

    __device__ inline double mean_square(Matrix * lhs,
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

        double total = 0;

        for (size_t i = 0u; i < lhs->being_vec_sz; ++i)
        {
            for (size_t j = 0u; j < lhs->being_vec[i]->process_group_vec_sz; ++j)
            {
                for (size_t z = 0u; z < lhs->being_vec[i]->process_group_vec[j].process_vec.size(); ++z)
                {
                    for (size_t k = 0u; k < lhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec.size(); ++k)
                    {
                        double deviation        = lhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec[k] 
                                                    - rhs->being_vec[i]->process_group_vec[j].process_vec[z].logit_vec[k];

                        double sqr_deviation    = deviation * deviation;
                        total                   += sqr_deviation;
                    }
                }
            }
        }

        size_t shape_sz = shape_size(lhs_shape);

        if (shape_sz == 0u)
        {
            return 0;
        }

        return total / shape_sz;
    }
}

#endif