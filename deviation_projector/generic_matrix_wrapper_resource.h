#ifndef __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__
#define __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "generic_resource.h"

namespace deviation_projector
{
    struct MatrixAsDeviationWrapperConfig
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct ExternalMatrixAsDeviationWrapperConfig
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    class MatrixAsDeviationWrapperFactory
    {
        public:

            template <class ...Args>
            auto wrap(Args&& ...) -> deviation_projector::GenericMatrixDeviationCalculatorResource
            {
                return {};
            }
    };
}

#endif