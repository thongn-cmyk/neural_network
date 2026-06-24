#ifndef __DEVIATION_PROJECTOR_HOST_DEVICE_DOUBLE_BAG_DEVICE_H__
#define __DEVIATION_PROJECTOR_HOST_DEVICE_DOUBLE_BAG_DEVICE_H__

#include <deviation_projector/matrix_deviation_calculator_interface.h>
#include <general_definition/float_def.h>
#include <matrix/tensor_model.h>
#include <memory>
#include <matrix/tensor_factory.h>

namespace deviation_projector::host_device
{
    class MatrixDoubleBagDeviationCalculator: public virtual MatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<MatrixDeviationCalculatorInterface> base;

            using tensor_std_float_t = tensor_model::tensor_std_float_t;

        public:

            MatrixDoubleBagDeviationCalculator(std::unique_ptr<MatrixDeviationCalculatorInterface> base): base(std::move(base)){}

            auto get_deviation(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& arg) -> mdc_float_t
            {
                mdc_float_t result_1    = this->base->get_deviation(arg);
                mdc_float_t result_2    = this->base->get_deviation(arg);

                if (stdx::nan_cmp(result_1, result_2) != 0)
                {
                    throw std::runtime_error("calculation went wrong");
                }

                return result_1;
            }
    };
}

#endif