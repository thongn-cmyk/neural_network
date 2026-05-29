#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_MATRIX_OPTIMIZATION_SESSION_GENERATOR_INTERFACE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_MATRIX_OPTIMIZATION_SESSION_GENERATOR_INTERFACE_H__

#include "matrix_optimization_session_interface.h"
#include <memory>

namespace matrix_optimizer_subsystem
{
    class MatrixOptimizationSessionGeneratorInterface
    {
        public:

            virtual ~MatrixOptimizationSessionGeneratorInterface() noexcept = default;

            virtual auto get_session() -> std::unique_ptr<MatrixOptimizationSessionInterface> = 0;
    };
}

#endif