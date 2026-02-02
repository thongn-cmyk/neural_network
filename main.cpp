#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include "matrix_optimizer.h"
#include "the_host_matrix.h"
#include "matrix_encoder_decoder.h"
#include "stock_solution.h"
#include "generic_matrix_factory.h"
#include "conventional_randomizer.h"

int main()
{
    // matrix_encoder_decoder::BitStreamReader reader(std::string_view("123"));
    
    // auto matrix = the_host_matrix::make_the_matrix({},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                {},
    //                                                std::integral_constant<size_t, 1u>{},
    //                                                std::integral_constant<size_t, 1u>{});

    // matrix_optimizer::BruteForceMatrixOptimizer optimizer{};

    // optimizer.optimize(*matrix, {});

    // generic_matrix_factory::GenericMatrixLoader{}.load_resource({});

    const size_t TEST_SZ = 100u;
    conventional_randomizer::ApplicationRandomizerObject randomizer{};

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::cout << randomizer.ld_randomize_focal_2() << std::endl;
    }
}