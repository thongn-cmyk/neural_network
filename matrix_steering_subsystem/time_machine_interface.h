//HEADER_CONTROL 1

#ifndef __TIME_MACHINE_INTERFACE_H__
#define __TIME_MACHINE_INTERFACE_H__

#include <general_definition/float_def.h>

namespace time_machine
{
    using std_float_t   = float_def::std_float_t;
    using tm_float_t    = float_def::tm_float_t;

    class TimeMachineInterface
    {
        public:

            virtual ~TimeMachineInterface() noexcept = default;
            virtual auto f(std_float_t t) -> tm_float_t = 0;
    };
}

#endif