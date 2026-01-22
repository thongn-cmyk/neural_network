//HEADER_CONTROL 1

#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_INTERFACE_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_INTERFACE_H__

#include "float_def.h"
#include <vector>

namespace temporal_coefficient_projector
{
    using std_float_t = float_def::std_float_t;

    class TemporalCoefficientProjectorInterface
    {
        public:

            virtual ~TemporalCoefficientProjectorInterface() noexcept = default;
            virtual auto project(std_float_t t) -> std::vector<std_float_t> = 0;
    };
}

#endif