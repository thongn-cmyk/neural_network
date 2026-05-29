//HEADER_CONTROL 0

#ifndef __MATRIX_TENSOR_MODEL_H__
#define __MATRIX_TENSOR_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <array>
#include <memory>

namespace tensor_model
{
    using tensor_std_float_t    = float;

    //I reckoned that this is compatible and it's better to just ... 

    static inline constexpr size_t PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ      = 1u;
    static inline constexpr size_t PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ  = 8u;

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
        std::shared_ptr<std::shared_ptr<ProcessGroup>[]> process_group_vec;
        size_t process_group_vec_sz;
    };

    struct Matrix
    {
        std::shared_ptr<std::shared_ptr<BeingUnit>[]> being_vec;
        size_t being_vec_sz;
    };
}

#endif