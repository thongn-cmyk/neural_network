//HEADER_CONTROL 2

#ifndef __GLOBAL_OPTIMALITY_APPROXIMATOR_INTERFACE_H__
#define __GLOBAL_OPTIMALITY_APPROXIMATOR_INTERFACE_H__

#include <general_definition/float_def.h>
#include "time_machine_interface.h"

namespace global_optimality_approximator
{
    using std_float_t = float_def::std_float_t;

    class TimeMachineOptimizerInterface
    {
        public:

            virtual ~TimeMachineOptimizerInterface() noexcept = default;

            virtual auto optimize(time_machine::TimeMachineInterface& time_machine) -> std_float_t = 0;
    };
}

#endif