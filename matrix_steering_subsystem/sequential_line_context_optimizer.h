#ifndef __SEQUENTIAL_LINE_CONTEXT_OPTIMIZER_H__
#define __SEQUENTIAL_LINE_CONTEXT_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include "temporal_coefficient_projector_3.h"

namespace sequential_line_context_optimizer
{
    class ActionableResult
    {
        public:

            virtual ~ActionableResult() = default;

            virtual void feedback(ctx_float_t score) = 0;
            virtual auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> = 0;
    };

    class IterationContextInterface
    {
        public:

            virtual ~IterationContextInterface() = default;

            virtual auto next() -> std::unique_ptr<ActionableResult> = 0;
    };

    class IterationContextGeneratorInterface
    {
        public:

            virtual ~IterationContextGeneratorInterface() = default;

            virtual auto get() -> std::unique_ptr<IterationContextInterface> = 0;
    };


}

#endif