#ifndef __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__
#define __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>

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
}

#endif