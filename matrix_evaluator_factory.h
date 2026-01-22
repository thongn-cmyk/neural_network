//HEADER_CONTROL 8

#ifndef __MATRIX_EVALUATOR_FACTORY_H__
#define __MATRIX_EVALUATOR_FACTORY_H__

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <vector>
#include "stdx.h"
#include "tensor_model.h"
#include "matrix_evaluator.h"
#include "matrix_deviation_calculator.h"

namespace matrix_evaluator
{
    class ProductEvaluatorFactory
    {
        public:

            template <class PromotedFloatType = tensor_model::tensor_std_float_t>
            static auto get_immutable_shape_product_evaluator(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& gold_std,
                                                              const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<MatrixEvaluatorInterface>
            {
                for (const auto& [lhs, rhs]: gold_std)
                {
                    if (lhs == nullptr)
                    {
                        throw std::invalid_argument("bad gold_std pair, null");
                    }

                    if (rhs == nullptr)
                    {
                        throw std::invalid_argument("bad gold_std pair, null");
                    }
                }

                return std::make_unique<PointWiseDeviationExtractor>(gold_std,
                                                                     matrix_deviation_calculator::DeviationCalculatorFactory::get_prenormalized_deviation_calculator<PromotedFloatType>(tag));
            }

            static auto get_double_bag_matrix_evaluator(std::unique_ptr<MatrixEvaluatorInterface> base) -> std::unique_ptr<MatrixEvaluatorInterface>
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("bad matrix evaluator base, null");
                }

                return std::make_unique<DoubleBagDeviationExtractor>(std::move(base));
            }
    };
}

#endif