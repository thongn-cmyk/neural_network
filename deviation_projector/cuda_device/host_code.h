#ifndef __DEVIATION_PROJECTOR_CUDA_DEVICE_HOST_CODE_H__
#define __DEVIATION_PROJECTOR_CUDA_DEVICE_HOST_CODE_H__

#include <stdint.h>
#include <stdlib.h>

namespace deviation_projector::cuda_device::host_code
{
    static inline constexpr uint8_t MEAN_SQUARE_DEVICE      = 0u;
    static inline constexpr uint8_t PARITY_DISTANCE_DEVICE  = 1u;
}

#endif