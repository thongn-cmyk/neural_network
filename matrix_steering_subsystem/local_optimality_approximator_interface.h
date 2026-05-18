//HEADER_CONTROL 1

#ifndef __LOCAL_OPTIMALITY_APPROXIMATOR_INTERFACE_H__
#define __LOCAL_OPTIMALITY_APPROXIMATOR_INTERFACE_H__

#include <general_definition/float_def.h>
#include "time_machine_interface.h"

namespace local_optimality_approximator
{
    using std_float_t = float_def::std_float_t;

    class OptimalityApproximatorInterface
    {
        public:

            virtual ~OptimalityApproximatorInterface() noexcept = default;

            virtual auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t = 0;
    };
}

#endif