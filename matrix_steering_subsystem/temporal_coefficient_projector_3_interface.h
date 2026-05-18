//HEADER_CONTROL 2

#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_3_INTERFACE_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_3_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include "temporal_coefficient_projector_interface.h"

namespace temporal_coefficient_projector_3
{
    class TemporalCoefficientProjectorContainerInterface
    {
        public:

            virtual ~TemporalCoefficientProjectorContainerInterface() = default;

            virtual auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> = 0;
            virtual void feedback(double rating) = 0;
    };

    class TemporalCoefficientProjectorGeneratorInterface
    {
        public:

            virtual ~TemporalCoefficientProjectorGeneratorInterface() = default;

            virtual auto get() -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface> = 0;
            virtual auto space_size() -> size_t = 0;
    };
}

#endif