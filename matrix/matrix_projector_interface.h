#ifndef __MATRIX_MATRIX_PROJECTOR_INTERFACE__
#define __MATRIX_MATRIX_PROJECTOR_INTERFACE__

#include <vector>
#include <memory>
#include "tensor_model.h"

namespace matrix_projector
{
    class MatrixProjectorInterface
    {
        public:

            virtual ~MatrixProjectorInterface() noexcept = default;

            virtual auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>> = 0;
    };
}

#endif