//HEADER_CONTROL 1

#ifndef __MATRIX_PROJECTOR_INTERFACE_H__
#define __MATRIX_PROJECTOR_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include "tensor_model.h"
#include <memory>

namespace matrix_projector
{
    class MatrixProjectorInterface
    {
        public:

            virtual ~MatrixProjectorInterface() noexcept = default;
            virtual auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix) -> std::vector<std::shared_ptr<tensor_model::Matrix>> = 0;
    };
}

#endif