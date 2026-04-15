//HEADER_CONTROL 3

#ifndef __MATRIX_OPTIMIZER_ENGINE_INTERFACE_H__
#define __MATRIX_OPTIMIZER_ENGINE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include "tensor_model.h"
#include "the_matrix_interface.h"
#include <common_exception/cancellation_token.h>

namespace matrix_optimizer
{
    class MatrixOptimizerEngineInterface
    {
        public:

            virtual ~MatrixOptimizerEngineInterface() noexcept = default;

            virtual auto optimize(the_matrix::MatrixInterface& matrix,
                                  matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                                  common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface> = 0;
    };
}

#endif