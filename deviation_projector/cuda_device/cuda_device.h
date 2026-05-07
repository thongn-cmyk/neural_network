#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/device_tensor_model.h>
#include "mean_square.h"

namespace deviation_projector::cuda_device
{
    using namespace device_tensor_model;
    using namespace deviation_projector::cuda_device::local_exception;

    using deviation_calculator_function = __device__ double (*)(Matrix *, Matrix *, local_exception_t *);

    __device__ static inline size_t DEVIATION_CALCULATOR_FUNCTION_TABLE_SZ  = 1u;

    __device__ static inline deviation_calculator_function deviation_calculation_table[DEVIATION_CALCULATOR_FUNCTION_TABLE_SZ]
    {
        deviation_projector::cuda_device::mean_square::mean_square
    };

    __device__ double get_deviation(uint8_t calculator_id,
                                    Matrix * lhs,
                                    Matrix * rhs,
                                    local_exception_t * err = nullptr)
    {
        local_exception_t local_err = SUCCESS;

        if (err == nullptr)
        {
            err = &local_err;
        }

        if (calculator_id >= DEVIATION_CALCULATOR_FUNCTION_TABLE_SZ)
        {
            *err = INVALID_CALCULATOR_DEVICE_ID;
            return {};
        }

        return deviation_calculation_table[calculator_id](lhs, rhs, err);
    }
}

#endif