//HEADER_CONTROL 1

#ifndef __DG_DEVIATION_PROJECTOR_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__
#define __DG_DEVIATION_PROJECTOR_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__

#include <vector>
#include <general_definition/float_def.h>
#include <matrix/tensor_model.h>
#include <memory>

namespace deviation_projector
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