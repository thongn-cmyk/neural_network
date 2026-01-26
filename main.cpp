#define STRONG_MEMORY_ORDERING_FLAG true

#include "matrix_optimizer.h"
#include "the_host_matrix.h"
#include "matrix_encoder_decoder.h"
#include "stock_solution.h"

int main()
{
    matrix_encoder_decoder::BitStreamReader reader(std::string_view("123"));
    
    auto matrix = the_host_matrix::make_the_matrix({},
                                                   {},
                                                   {},
                                                   {},
                                                   {},
                                                   {},
                                                   {},
                                                   {},
                                                   {},
                                                   std::integral_constant<size_t, 1u>{},
                                                   std::integral_constant<size_t, 1u>{});

    matrix_optimizer::BruteForceMatrixOptimizer optimizer{};

    optimizer.optimize(*matrix, {});
}