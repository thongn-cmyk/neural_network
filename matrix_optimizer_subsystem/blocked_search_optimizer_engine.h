#ifndef __BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__
#define __BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include "matrix_optimizer_engine_interface.h"
#include <matrix/tensor_model.h>
#include <matrix/the_matrix_interface.h>
#include <common_exception/cancellation_token.h>
#include <common_exception/common_exception.h>
#include <vector>

namespace matrix_optimizer_subsystem
{
    using namespace float_def;

    using tensor_std_float_t    = tensor_model::tensor_std_float_t;

    //I normally would not through this decision to do blocked search, but it is actually one important decision to have lots of other optimizations built on top of this

    class BlockedSearchOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            std::shared_ptr<MatrixOptimizerEngineInterface> base;
            std::vector<bool> activation_vec;

        public:

            BlockedSearchOptimizerEngine(std::shared_ptr<MatrixOptimizerEngineInterface> base_arg,
                                         std::vector<bool> activation_vec_arg)
            {
                if (base_arg == nullptr)
                {
                    throw std::invalid_argument("bad base argument, null");
                }

                this->base              = std::move(base_arg);
                this->activation_vec    = std::move(activation_vec_arg);
            }

            auto optimize(the_matrix::MatrixInterface& matrix,
                          matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                InternalRetranslatedMatrix retranslated_matrix(matrix.clone(),
                                                               this->get_translation_table(matrix));

                InternalRetranslatedMatrixEvaluator retranslated_matrix_evaluator(std::make_shared<InternalRetranslatedMatrix>(matrix.clone(),
                                                                                                                               this->get_translation_table(matrix)),
                                                                                  &matrix_evaluator);

                return this->base->optimize(retranslated_matrix,
                                            retranslated_matrix_evaluator,
                                            cancellation_token);
            }

        private:

            auto get_translation_table(the_matrix::MatrixInterface& matrix) -> std::vector<size_t>
            {
                size_t logit_vec_sz     = matrix.get_coefficient_vector().size();
                std::vector<size_t> rs  = {};

                for (size_t i = 0u; i < logit_vec_sz; ++i)
                {
                    if (i < this->activation_vec.size())
                    {
                        if (this->activation_vec[i])
                        {
                            rs.push_back(i);
                        }

                        continue;
                    }

                    rs.push_back(i);
                }

                return rs;
            }

            class InternalRetranslatedMatrix: public virtual the_matrix::MatrixInterface
            {
                private:

                    std::shared_ptr<the_matrix::MatrixInterface> base_matrix;
                    std::vector<size_t> translation_table;

                public:

                    InternalRetranslatedMatrix(std::shared_ptr<the_matrix::MatrixInterface> base_matrix_arg,
                                               std::vector<size_t> translation_table_arg): base_matrix(std::move(base_matrix_arg)),
                                                                                           translation_table(std::move(translation_table_arg)){}

                    auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
                    {
                        return this->base_matrix->project(matrix_vec);
                    }

                    auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
                    {
                        std::vector<tensor_std_float_t> base_logit = this->base_matrix->get_coefficient_vector();
                        std::vector<tensor_std_float_t> rs{};

                        for (size_t idx: this->translation_table)
                        {
                            rs.push_back(base_logit[idx]);
                        }

                        return rs;
                    }

                    void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
                    {
                        if (coeff_vec.size() != this->translation_table.size())
                        {
                            throw std::invalid_argument("bad coefficient vector, mismatched size");
                        }

                        std::vector<tensor_std_float_t> full_coeff_vec  = this->base_matrix->get_coefficient_vector();

                        for (size_t i = 0u; i < coeff_vec.size(); ++i)
                        {
                            full_coeff_vec[this->translation_table[i]] = coeff_vec[i];
                        }

                        this->base_matrix->set_coefficient_vector(full_coeff_vec);
                    }

                    auto clone_original_matrix() -> std::shared_ptr<MatrixInterface>
                    {
                        return this->base_matrix->clone();
                    }

                    auto clone() -> std::shared_ptr<MatrixInterface>
                    {
                        return std::make_shared<InternalRetranslatedMatrix>(this->base_matrix->clone(),
                                                                            this->translation_table);
                    }
            };

            class InternalRetranslatedMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
            {
                private:

                    std::shared_ptr<InternalRetranslatedMatrix> original_matrix;
                    matrix_evaluator::MatrixEvaluatorInterface * matrix_evaluator; // we dont need stack guard because stack access is guaranteed by the & -> the callee function

                public:

                    InternalRetranslatedMatrixEvaluator(std::shared_ptr<InternalRetranslatedMatrix> original_matrix_arg,
                                                        matrix_evaluator::MatrixEvaluatorInterface * matrix_evaluator_arg): original_matrix(std::move(original_matrix_arg)),
                                                                                                                            matrix_evaluator(matrix_evaluator_arg){}

                    auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
                    {
                        this->original_matrix->set_coefficient_vector(matrix.get_coefficient_vector());
                        std::shared_ptr<the_matrix::MatrixInterface> org_matrix = this->original_matrix->clone_original_matrix();

                        return this->matrix_evaluator->get_deviation(*org_matrix);
                    }
            };
    };
}

#endif