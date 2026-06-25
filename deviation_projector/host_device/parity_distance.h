#ifndef __DEVIATION_PROJECTOR_HOST_DEVICE_PARITY_DISTANCE_H__
#define __DEVIATION_PROJECTOR_HOST_DEVICE_PARITY_DISTANCE_H__

#include <deviation_projector/matrix_deviation_calculator_interface.h>
#include <general_definition/float_def.h>
#include <matrix/tensor_model.h>
#include <memory>
#include <matrix/tensor_factory.h>

namespace deviation_projector::host_device
{
    template <class PromotedFloatType = tensor_model::tensor_std_float_t>
    class MatrixParityDistanceDeviationCalculator: public virtual MatrixDeviationCalculatorInterface
    {
        private:

            using tensor_std_float_t = tensor_model::tensor_std_float_t;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            auto get_deviation(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& arg) -> mdc_float_t
            {
                PromotedFloatType prenormed_result                  = 0;
                std::vector<tensor_std_float_t> lhs_logit_vec       = {};
                std::vector<tensor_std_float_t> rhs_logit_vec       = {};

                for (const auto& [lhs, rhs]: arg)
                {
                    lhs_logit_vec.clear();
                    rhs_logit_vec.clear();

                    if (this->is_vec_contains_nan(lhs_logit_vec))
                    {
                        return stdx::generic_nan();
                    }

                    if (this->is_vec_contains_nan(rhs_logit_vec))
                    {
                        return stdx::generic_nan();
                    }

                    prenormed_result += this->get_parity_distance(lhs_logit_vec, rhs_logit_vec);
                }

                return prenormed_result / stdx::safe_non_zero_access(arg.size());
            }

        private:
            
            auto is_vec_contains_nan(const std::vector<tensor_std_float_t>& arg) -> bool
            {
                for (const auto& e: arg)
                {
                    if (std::isnan(e))
                    {
                        return true;
                    }
                }

                return false;
            }

            auto get_parity_distance(const std::vector<tensor_std_float_t>& lhs_logit_vec,
                                     const std::vector<tensor_std_float_t>& rhs_logit_vec) -> PromotedFloatType
            {
                if (lhs_logit_vec.size() != rhs_logit_vec.size())
                {
                    throw std::invalid_argument("incompatible matrix pair evaluation");
                }

                PromotedFloatType lhs_zero_sum  = 0;
                PromotedFloatType lhs_one_sum   = 0;
                PromotedFloatType rhs_one_sum   = 0;
                PromotedFloatType rhs_zero_sum  = 0;

                for (size_t i = 0u; i < lhs_logit_vec.size(); ++i)
                {
                    rhs_one_sum     += rhs_logit_vec[i];
                    lhs_zero_sum    += lhs_logit_vec[i] * (rhs_logit_vec[i] - 1);
                    lhs_one_sum     += lhs_logit_vec[i] * rhs_logit_vec[i];
                }

                lhs_zero_sum                    = -lhs_zero_sum;

                PromotedFloatType lhs_parity    = lhs_one_sum - rhs_one_sum;
                PromotedFloatType rhs_parity    = rhs_one_sum - rhs_zero_sum;
                PromotedFloatType diff          = lhs_parity - rhs_parity;

                return diff * diff;
            }
    };
}

#endif