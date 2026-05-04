//HEADER_CONTROL 2

#ifndef __MATRIX_THE_MATRIX_INTERFACE_H__
#define __MATRIX_THE_MATRIX_INTERFACE_H__

#include "matrix_projector_interface.h"
#include <vector>
#include <general_definition/float_def.h>
#include "tensor_model.h"
#include <memory>

namespace the_matrix
{
    using tensor_std_float_t = tensor_model::tensor_std_float_t;
 
    class MatrixInterface : public virtual matrix_projector::MatrixProjectorInterface
    {
        public:

            virtual ~MatrixInterface() noexcept = default;

            virtual auto get_coefficient_vector() -> std::vector<tensor_std_float_t> = 0;
            virtual void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec) = 0;
            virtual auto clone() -> std::shared_ptr<MatrixInterface> = 0;
    };
}

#endif