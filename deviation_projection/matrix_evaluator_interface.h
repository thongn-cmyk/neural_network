//HEADER_CONTROL 2

#ifndef __MATRIX_EVALUATOR_INTERFACE_H__
#define __MATRIX_EVALUATOR_INTERFACE_H__

#include "float_def.h"
#include "matrix_projector_interface.h"

namespace matrix_evaluator
{
    using eval_float_t = float_def::eval_float_t;

    class MatrixEvaluatorInterface
    {
        public:

            virtual ~MatrixEvaluatorInterface() noexcept = default;
            virtual auto get_deviation(matrix_projector::MatrixProjectorInterface& matrix_projector) -> eval_float_t = 0;
    };
}

#endif