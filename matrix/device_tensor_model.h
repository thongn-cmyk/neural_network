#ifndef __MATRIX_DEVICE_TENSOR_MODEL_H__
#define __MATRIX_DEVICE_TENSOR_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <arary>

namespace device_tensor_model
{
    //__CONFIGURATION_SYNCHRONIZATION__

    using tensor_std_float_t    = float;

    static inline constexpr size_t PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ      = 2u;
    static inline constexpr size_t PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ  = 16u;

    struct ProcessUnit
    {
        std::array<tensor_std_float_t, PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ> logit_vec;
    };

    struct ProcessGroup
    {
        std::array<ProcessUnit, PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ> process_vec;
    };

    struct BeingUnit
    {
        ProcessGroup * process_group_vec;
        uint64_t process_group_vec_sz;
    };

    struct Matrix
    {
        BeingUnit ** being_vec;
        uint64_t being_vec_sz;
    };
}

#endif