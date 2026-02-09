#ifndef __DEVIATION_PROJECTOR_INTERFACE_H__
#define __DEVIATION_PROJECTOR_INTERFACE_H__

#include <vector>
#include "float_def.h"
#include "tensor_model.h"
#include <memory>

namespace deviation_projector
{
    class DeviationProjectorInterface
    {
        public:

            virtual ~DeviationProjectorInterface() noexcept = default;

            virtual auto project(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& inp_out_vec) -> mdc_float_t = 0;
            virtual auto get_coefficient_vector() -> std::vector<tensor_std_float_t>= 0;
            virtual void set_coefficient_vector(const std::vector<tensor_std_float_t>& coefficient_vec) = 0;
            virtual auto clone() -> std::shared_ptr<DeviationProjectorInterface> = 0;
    };
}

#endif