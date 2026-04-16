//HEADER_CONTROL 2

#ifndef __MATRIX_EVALUATOR_INTERFACE_H__
#define __MATRIX_EVALUATOR_INTERFACE_H__

#include <general_definition/float_def.h>
#include <matrix/the_matrix_interface.h>

namespace matrix_evaluator
{
    using eval_float_t = float_def::eval_float_t;

    class MatrixEvaluatorInterface
    {
        public:

            virtual ~MatrixEvaluatorInterface() noexcept = default;

            virtual auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t = 0;
    };
}

#endif