//HEADER_CONTROL 7

#ifndef __DG_MATRIX_DEVIATION_CALCULATOR_H__
#define __DG_MATRIX_DEVIATION_CALCULATOR_H__

#include "matrix_deviation_calculator_interface.h"
#include <general_definition/float_def.h>
#include <matrix/tensor_model.h>
#include <memory>
#include <matrix/tensor_factory.h>

namespace deviation_projector
{
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    template <class PromotedFloatType = tensor_std_float_t>
    class MatrixSquareDeviationCalculator: public virtual MatrixDeviationCalculatorInterface
    {
        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            auto get_deviation(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& arg) -> mdc_float_t
            {
                PromotedFloatType prenormed_result                  = 0;
                std::vector<tensor_std_float_t> lhs_logit_vec       = {};
                std::vector<tensor_std_float_t> rhs_logit_vec       = {};
                std::optional<size_t> flat_shape                    = std::nullopt;

                for (const auto& [lhs, rhs]: arg)
                {
                    lhs_logit_vec.clear();
                    rhs_logit_vec.clear();

                    tensor_factory::flatten(lhs, lhs_logit_vec);
                    tensor_factory::flatten(rhs, rhs_logit_vec);

                    if (!flat_shape.has_value())
                    {
                        flat_shape = lhs_logit_vec.size();
                    }

                    if (flat_shape.value() != lhs_logit_vec.size())
                    {
                        throw std::invalid_argument("incompatible shape for square deviation calculator");
                    }

                    if (flat_shape.value() != rhs_logit_vec.size())
                    {
                        throw std::invalid_argument("incompatible shape for square deviation calculator");
                    }

                    if (this->is_vec_contains_nan(lhs_logit_vec))
                    {
                        return stdx::generic_nan();
                    }

                    if (this->is_vec_contains_nan(rhs_logit_vec))
                    {
                        return stdx::generic_nan();
                    }

                    if (PromotedFloatType incremental_value = this->get_pair_deviation(lhs_logit_vec, rhs_logit_vec); !std::isnan(incremental_value))
                    {
                        prenormed_result += incremental_value;
                    }
                    else
                    {
                        return stdx::generic_nan();
                    }
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

            auto get_pair_deviation(const std::vector<tensor_std_float_t>& lhs_logit_vec,
                                    const std::vector<tensor_std_float_t>& rhs_logit_vec) -> PromotedFloatType
            {
                PromotedFloatType sum_sqr_difference = 0;

                if (lhs_logit_vec.size() != rhs_logit_vec.size())
                {
                    throw std::invalid_argument("incompatible matrix pair evaluation");
                }

                for (size_t i = 0u; i < lhs_logit_vec.size(); ++i)
                {
                    PromotedFloatType difference        = static_cast<PromotedFloatType>(lhs_logit_vec[i]) - static_cast<PromotedFloatType>(rhs_logit_vec[i]);
                    PromotedFloatType sqr_difference    = difference * difference;

                    if (std::isnan(sqr_difference))
                    {
                        return stdx::generic_nan();
                    }

                    sum_sqr_difference += sqr_difference;
                }

                return sum_sqr_difference / stdx::safe_non_zero_access(lhs_logit_vec.size());
            }
    };

    class DoubleBagMatrixDeviationCalculator: public virtual MatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<MatrixDeviationCalculatorInterface> base;
        
        public:

            DoubleBagMatrixDeviationCalculator(std::unique_ptr<MatrixDeviationCalculatorInterface> base): base(std::move(base)){}

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

    class DeviationCalculatorFactory
    {
        public:

            template <class PromotedFloatType = tensor_std_float_t>
            static auto get_square_deviation_calculator(const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<MatrixDeviationCalculatorInterface>
            {
                return std::make_unique<MatrixSquareDeviationCalculator<PromotedFloatType>>();
            }

            static auto get_double_bag_deviation_calculator(std::unique_ptr<MatrixDeviationCalculatorInterface> deviation_calculator) -> std::unique_ptr<MatrixDeviationCalculatorInterface>
            {
                if (deviation_calculator == nullptr)
                {
                    throw std::invalid_argument("bad deviation calculator, null");
                }

                return std::make_unique<DoubleBagMatrixDeviationCalculator>(std::move(deviation_calculator));
            }
    };
}

#endif