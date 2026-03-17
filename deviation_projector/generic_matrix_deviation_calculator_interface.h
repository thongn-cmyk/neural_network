#ifndef __DG_GENERIC_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__
#define __DG_GENERIC_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__

#include <vector>
#include <general_definition/float_def.h>
#include <memory>

namespace deviation_projector
{
    using mdc_float_t = float_def::mdc_float_t;

    class GenericMatrixDeviationCalculatorInterface
    {
        public:

            virtual ~GenericMatrixDeviationCalculatorInterface() noexcept = default;
            virtual auto get_deviation(const std::vector<std::pair<std::shared_ptr<std::string>, std::shared_ptr<std::string>>>& arg) -> mdc_float_t = 0;
    };
}

#endif