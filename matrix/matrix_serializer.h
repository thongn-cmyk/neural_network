#ifndef __MATRIX_SERILIZER_H__
#define __MATRIX_SERILIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <matrix/tensor_model.h>
#include <memory>

namespace matrix_serializer
{
    using GenericMatrix = std::string;

    auto serialize(const std::shared_ptr<tensor_model::Matrix>& matrix) -> GenericMatrix
    {
        return {};
    }

    auto deserialize(const GenericMatrix& matrix) -> std::shared_ptr<tensor_model::Matrix>
    {
        return {};
    }
}

#endif