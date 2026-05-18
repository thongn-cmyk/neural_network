#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_LOCAL_EXCEPTION_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>

namespace deviation_projector::cuda_device::local_exception
{
    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                           = 0u;
    static inline constexpr local_exception_t INCOMPATIBLE_SHAPE_ERROR_CODE     = 1u;
    static inline constexpr local_exception_t INVALID_CALCULATOR_DEVICE_ID_CODE = 2u;
}

#endif