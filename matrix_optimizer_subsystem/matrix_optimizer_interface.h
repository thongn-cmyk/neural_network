//HEADER_CONTROL 3

#ifndef __MATRIX_OPTIMIZER_INTERFACE_H__
#define __MATRIX_OPTIMIZER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include "tensor_model.h"
#include "the_matrix_interface.h"

namespace matrix_optimizer
{
    class MatrixOptimizerInterface
    {
        public:

            virtual ~MatrixOptimizerInterface() noexcept = default;
            virtual auto optimize(the_matrix::MatrixInterface& matrix,
                                  const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& gold_std) -> std::shared_ptr<the_matrix::MatrixInterface> = 0;
    };
}

#endif

