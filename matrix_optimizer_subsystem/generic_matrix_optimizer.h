#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_GENERIC_MATRIX_OPTIMIZER_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_GENERIC_MATRIX_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>

namespace matrix_optimizer_subsystem
{
    struct GenericOptimizerConfig
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