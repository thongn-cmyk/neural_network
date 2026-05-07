#ifndef __DG_GENERIC_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__
#define __DG_GENERIC_MATRIX_DEVIATION_CALCULATOR_INTERFACE_H__

#include <vector>
#include <general_definition/float_def.h>
#include <memory>
#include <immutable_memory/immutable_memory.h>

namespace deviation_projector
{
    using mdc_float_t = float_def::mdc_float_t;

    class GenericMatrixDeviationCalculatorInterface
    {
        public:

            virtual ~GenericMatrixDeviationCalculatorInterface() noexcept = default;

            //I dont think that specifying ManagedImmutableMemory is languagely accurate
            //I still consider using dynamic_cast<> for this particular use case

            virtual auto get_deviation(const std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>>& arg) -> mdc_float_t = 0;
    };
}

#endif