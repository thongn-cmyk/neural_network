#ifndef __DG_MATRIX_DEVIATION_CALCULATOR_FACTORY_H__
#define __DG_MATRIX_DEVIATION_CALCULATOR_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "matrix_deviation_calculator_interface.h"

namespace deviation_projector
{
    struct NoTransformDeviationCalculatorResource
    {
        std::string promoted_float_kind;
        uint8_t deviation_calculator_machine_kind;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(promoted_float_kind, deviation_calculator_machine_kind);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(promoted_float_kind, deviation_calculator_machine_kind);
        }
    };

    class NoTransformDeviationCalculatorResourceBuilder
    {
        
    };

    class NoTransformDeviationCalculatorLoader
    {
        public:

            auto load(const NoTransformDeviationCalculatorResource& resource) -> std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface>
            {
                return {};
            }
    };
    
}

#endif