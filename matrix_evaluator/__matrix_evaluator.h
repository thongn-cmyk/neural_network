//HEADER_CONTROL 3

#ifndef __MATRIX_EVALULATOR_H__
#define __MATRIX_EVALULATOR_H__

#include "matrix_projector_interface.h"
#include "matrix_evaluator_interface.h"
#include "tensor_model.h"
#include "matrix_deviation_calculator_interface.h"
#include "float_def.h"
#include <vector>
#include <memory>

namespace matrix_evaluator
{
    class PointWiseDeviationExtractor: public virtual MatrixEvaluatorInterface
    {
        private:

            std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> input_output_pair_vec;
            std::unique_ptr<matrix_deviation_calculator::MatrixDeviationCalculatorInterface> deviation_calculator;

        public:

            PointWiseDeviationExtractor(std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> input_output_pair_vec,
                                        std::unique_ptr<matrix_deviation_calculator::MatrixDeviationCalculatorInterface> deviation_calculator) noexcept: input_output_pair_vec(std::move(input_output_pair_vec)),
                                                                                                                                                         deviation_calculator(std::move(deviation_calculator)){}

            auto get_deviation(the_matrix::MatrixInterface& matrix_projector) -> eval_float_t
            {  
                std::vector<std::shared_ptr<tensor_model::Matrix>> projecting_vec{};
                std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> result_vec{};
                
                projecting_vec.reserve(this->input_output_pair_vec.size());
                result_vec.reserve(this->input_output_pair_vec.size());

                for (const auto& [x, expected_y]: this->input_output_pair_vec)
                {
                    projecting_vec.push_back(x);
                }

                std::vector<std::shared_ptr<tensor_model::Matrix>> projected_vec = matrix_projector.project(projecting_vec);

                if (projecting_vec.size() != projected_vec.size())
                {
                    throw std::runtime_error("bad projection, incompatible unexpected size");
                }

                for (size_t i = 0u; i < projecting_vec.size(); ++i)
                {
                    result_vec.push_back({projecting_vec[i], projected_vec[i]});
                }

                return this->deviation_calculator->get_deviation(result_vec);
            }
    };
    
    class DoubleBagDeviationExtractor: public virtual MatrixEvaluatorInterface
    {
        private:

            std::unique_ptr<MatrixEvaluatorInterface> base;
        
        public:

            DoubleBagDeviationExtractor(std::unique_ptr<MatrixEvaluatorInterface> base): base(std::move(base)){}

            auto get_deviation(the_matrix::MatrixInterface& matrix_projector) -> eval_float_t
            {
                eval_float_t result_1   = this->base->get_deviation(matrix_projector);
                eval_float_t result_2   = this->base->get_deviation(matrix_projector);

                if (stdx::nan_cmp(result_1, result_2) != 0)
                {
                    throw std::runtime_error("calculation went wrong");
                }

                return result_1;
            }
    };
}

#endif