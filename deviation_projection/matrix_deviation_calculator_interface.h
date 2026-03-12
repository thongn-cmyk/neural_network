//HEADER_CONTROL 1

#ifndef __MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__
#define __MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__

#include <vector>
#include "float_def.h"
#include "tensor_model.h"
#include <memory>

namespace matrix_deviation_calculator
{
    using mdc_float_t = float_def::mdc_float_t;

    class MatrixDeviationCalculatorInterface
    {
        public:

            virtual ~MatrixDeviationCalculatorInterface() noexcept = default;
            virtual auto get_deviation(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& arg) -> mdc_float_t = 0;
    };   
}

#endif