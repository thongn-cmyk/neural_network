#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/device_tensor/model.h>
#include "mean_square.h"
#include "parity_distance.h"

namespace deviation_projector::cuda_device
{
    using namespace device_tensor::model;
    using namespace deviation_projector::cuda_device::local_exception;

    using deviation_calculator_function = double (*)(Matrix *, Matrix *, local_exception_t *);

    __device__ static constexpr inline size_t DEVIATION_CALCULATOR_FUNCTION_TABLE_SZ  = 2u;

    __device__ static inline deviation_calculator_function deviation_calculation_table[DEVIATION_CALCULATOR_FUNCTION_TABLE_SZ]
    {
        deviation_projector::cuda_device::mean_square::mean_square,
        deviation_projector::cuda_device::parity_distance::parity_distance
    };

    __device__ static inline uint8_t MEAN_SQUARE_DEVICE     = 0u;
    __device__ static inline uint8_t PARITY_DISTANCE_DEVICE = 1u;

    __device__ inline double get_deviation(uint8_t calculator_id,
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
            *err = INVALID_CALCULATOR_DEVICE_ID_CODE;
            return {};
        }

        return deviation_calculation_table[calculator_id](lhs, rhs, err);
    }
}

#endif