#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <taylor_matrix/host_matrix/the_host_matrix.h>

int main()
{
    using namespace taylor_matrix::host_matrix::the_host_matrix;

    const size_t VECTOR_SZ_RANGE        = size_t{1} << 10;

    for (size_t i = 0u; i < VECTOR_SZ_RANGE; ++i)
    {
        const size_t VECTOR_SZ              = i;
        auto factory                        = TheHostMatrixFactory{};

        factory.set_vector_size(VECTOR_SZ);

        std::vector<size_t> matrix_shape    = factory.get_matrix_shape();
        auto matrix_projector               = factory.get();

        auto matrix                         = taylor_matrix::host_matrix::tensor_matrix_operation::make_matrix_from_shape_vec(matrix_shape);
        auto rs                             = matrix_projector->project({matrix});

        matrix_projector->set_coefficient_vector(matrix_projector->get_coefficient_vector());

        std::cout << i << "<iteration>" << rs.size() << "<size>\n";
    }
}