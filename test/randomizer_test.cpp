#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <matrix_steering_subsystem/conventional_randomizer.h>

int main()
{
    using namespace conventional_randomizer;

    RangeRandomizerObject randomizer{};

    for (size_t i = 0u; i < 1024; ++i)
    {
        if (randomizer.randomize_range(1024) == 0)
        {
            std::cout << "zero\n";
        }

        std::cout << i << " > " << randomizer.randomize_range(1024) << "\n";
    }
}