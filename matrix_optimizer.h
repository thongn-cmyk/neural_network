//HEADER_CONTROL 8

#ifndef __MATRIX_OPTIMIZER_H__
#define __MATRIX_OPTIMIZER_H__

#include "matrix_optimizer_interface.h"
#include "cached_matrix_projector.h"
#include "cached_time_machine.h"
#include "temporal_coefficient_projector_interface.h"
#include "temporal_coefficient_projector.h"
#include "matrix_evaluator_interface.h"
#include "time_machine_optimizer_factory.h"
#include "the_matrix_interface.h"
#include "float_def.h"
#include "tensor_model.h"

namespace matrix_optimizer
{
    using std_float_t           = float_def::std_float_t;
    using tm_float_t            = float_def::tm_float_t;
    using tensor_std_float_t    = tensor_model::tensor_std_float_t;

    class TemporalCoefficientOptimizer
    {
        private:

            size_t cache_map_capacity;
            size_t time_machine_cache_map_capacity;

        public:

            TemporalCoefficientOptimizer(size_t cache_map_capacity,
                                         size_t time_machine_cache_map_capacity): cache_map_capacity(cache_map_capacity),
                                                                                  time_machine_cache_map_capacity(time_machine_cache_map_capacity){}

            auto optimize(the_matrix::MatrixInterface& matrix,
                          temporal_coefficient_projector::TemporalCoefficientProjectorInterface& projector,
                          matrix_evaluator::MatrixEvaluatorInterface& deviation_extractor,
                          global_optimality_approximator::TimeMachineOptimizerInterface& time_machine_optimizer) -> std::vector<tensor_std_float_t>
            {
                std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix = matrix.clone();
                CachedMatrix cached_matrix(tmp_matrix, this->cache_map_capacity);

                std::unique_ptr<time_machine::TimeMachineInterface> time_machine = std::make_unique<SpecificMatrixTimeMachine>(&cached_matrix, &projector, &deviation_extractor);
                time_machine::CachedTimeMachine cached_time_machine(std::move(time_machine), this->time_machine_cache_map_capacity);

                std_float_t t                       = time_machine_optimizer.optimize(cached_time_machine);
                std::vector<std_float_t> coeff_vec  = projector.project(t);

                return stdx::to_castable_vector_initializer(std::move(coeff_vec));
            }

            void set_cache_map_capacity(size_t sz)
            {
                this->cache_map_capacity = sz;
            }

            void set_time_machine_cache_map_capacity(size_t sz)
            {
                this->time_machine_cache_map_capacity = sz;
            }

        private:

            class CachedMatrix: public virtual the_matrix::MatrixInterface
            {
                private:

                    matrix_projector::CachedMatrixProjector cache_base;
                    std::shared_ptr<the_matrix::MatrixInterface> matrix_base;
                    size_t cache_map_capacity;

                public:

                    CachedMatrix(std::shared_ptr<the_matrix::MatrixInterface> matrix_base,
                                 size_t cache_map_capacity): cache_base(matrix_base, cache_map_capacity),
                                                             matrix_base(std::move(matrix_base)),
                                                             cache_map_capacity(cache_map_capacity){}

                    auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
                    {
                        return this->cache_base.project(matrix_vec);
                    }

                    auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
                    {
                        return this->matrix_base->get_coefficient_vector();
                    }

                    void set_coefficient_vector(const std::vector<tensor_std_float_t>& coeff_vec)
                    {
                        this->matrix_base->set_coefficient_vector(coeff_vec);
                        this->cache_base.clear_cache();
                    }

                    auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
                    {
                        return std::make_shared<CachedMatrix>(this->matrix_base->clone(), this->cache_map_capacity);
                    }
            };

            class SpecificMatrixTimeMachine: public virtual time_machine::TimeMachineInterface
            {
                private:

                    the_matrix::MatrixInterface * base_matrix;
                    temporal_coefficient_projector::TemporalCoefficientProjectorInterface * coefficient_projector;
                    matrix_evaluator::MatrixEvaluatorInterface * product_evaluator;

                public:

                    SpecificMatrixTimeMachine(the_matrix::MatrixInterface * base_matrix,
                                              temporal_coefficient_projector::TemporalCoefficientProjectorInterface * coefficient_projector,
                                              matrix_evaluator::MatrixEvaluatorInterface * product_evaluator): base_matrix(std::move(base_matrix)),
                                                                                                               coefficient_projector(std::move(coefficient_projector)),
                                                                                                               product_evaluator(std::move(product_evaluator)){}

                    auto f(std_float_t t) -> tm_float_t
                    {
                        std::vector<std_float_t> coeff_vec = this->coefficient_projector->project(t);
                        this->base_matrix->set_coefficient_vector(stdx::to_castable_vector_initializer(std::move(coeff_vec)));

                        return this->product_evaluator->get_deviation(*this->base_matrix);
                    }
            };
    };

    class BruteForceMatrixOptimizer: public virtual MatrixOptimizerInterface
    {
        private:

            TemporalCoefficientOptimizer optimizer;
            size_t optimization_epoch_sz;
            size_t optimization_step_sz;
            std::optional<std::vector<bool>> activation_coeff_vec;

            size_t product_evaluator_float_byte_width;
            bool product_evaluator_has_double_bag;
            size_t coefficient_projector_float_byte_width;
            size_t time_machine_optimizer_float_byte_width;

            static inline constexpr size_t DEFAULT_CACHE_MAP_CAPACITY               = 1024u;
            static inline constexpr size_t DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY  = 1024u;
            static inline constexpr size_t DEFAULT_OPTIMIZATION_EPOCH_SZ            = 8u;
            static inline constexpr size_t DEFAULT_OPTIMIZATION_STEP_SZ             = 8u;
            static inline constexpr size_t DEFAULT_FLOAT_BYTE_WIDTH                 = float_def::STD_FLOAT_TYPE_BYTE_WIDTH;

        public:

            BruteForceMatrixOptimizer(): optimizer(DEFAULT_CACHE_MAP_CAPACITY, DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY),
                                         optimization_epoch_sz(DEFAULT_OPTIMIZATION_EPOCH_SZ),
                                         optimization_step_sz(DEFAULT_OPTIMIZATION_STEP_SZ),
                                         activation_coeff_vec(std::nullopt),
                                         product_evaluator_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH),
                                         coefficient_projector_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH),
                                         time_machine_optimizer_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH){}

            void set_cache_map_capacity(size_t sz)
            {
                this->optimizer.set_cache_map_capacity(sz);
            }

            void set_time_machine_cache_map_capacity(size_t sz)
            {
                this->optimizer.set_time_machine_cache_map_capacity(sz);
            }

            void set_optimization_epoch_size(size_t sz)
            {
                if (sz == 0u)
                {
                    throw std::runtime_error("bad optimization epoch size");
                }

                this->optimization_epoch_sz = sz;
            }

            void set_optimization_step_size(size_t sz)
            {
                if (sz == 0u)
                {
                    throw std::runtime_error("bad optimization step size");
                }

                this->optimization_step_sz = sz;
            }

            void set_product_evaluator_float_byte_width(size_t sz)
            {
                float_def::get_float_type_by_byte_width([](auto&&...){}, sz);
                this->product_evaluator_float_byte_width = sz;
            }

            void set_product_evaluator_double_bag_status(bool flag)
            {
                this->product_evaluator_has_double_bag = flag;
            }

            void set_coefficient_projector_float_byte_width(size_t sz)
            {
                float_def::get_float_type_by_byte_width([](auto&&...){}, sz);
                this->coefficient_projector_float_byte_width = sz;
            }

            void set_time_machine_optimizer_float_byte_width(size_t sz)
            {
                float_def::get_float_type_by_byte_width([](auto&&...){}, sz);
                this->time_machine_optimizer_float_byte_width = sz;
            }

            void set_activation_vector(std::optional<std::vector<bool>> vec)
            {
                this->activation_coeff_vec = std::move(vec);
            }

            auto optimize(the_matrix::MatrixInterface& matrix,
                          const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& gold_std) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> product_evaluator   = this->get_product_evaluator(gold_std);
                std::shared_ptr<the_matrix::MatrixInterface> best_matrix                        = matrix.clone();

                for (size_t i = 0u; i < this->optimization_epoch_sz; ++i)
                {
                    std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix = best_matrix->clone();

                    for (size_t j = 0u; j < this->optimization_step_sz; ++j)
                    {
                        std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> coefficient_projector    = this->get_random_coefficient_projector(*tmp_matrix);
                        std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine_optimizer           = this->get_time_machine();

                        try
                        {
                            std::vector<tensor_std_float_t> new_coefficient_vec = this->optimizer.optimize(*tmp_matrix,
                                                                                                           *coefficient_projector,
                                                                                                           *product_evaluator,
                                                                                                           *time_machine_optimizer);

                            std::shared_ptr<the_matrix::MatrixInterface> test_matrix = tmp_matrix->clone();
                            test_matrix->set_coefficient_vector(new_coefficient_vec);

                            if (stdx::nan_cmp(product_evaluator->get_deviation(*test_matrix), product_evaluator->get_deviation(*tmp_matrix)) < 0)
                            {
                                tmp_matrix = test_matrix;
                            }
                        }
                        catch (...)
                        {
                            continue;
                        }
                    }

                    if (stdx::nan_cmp(product_evaluator->get_deviation(*tmp_matrix), product_evaluator->get_deviation(*best_matrix)) < 0)
                    {
                        best_matrix = tmp_matrix;
                    }
                }

                return best_matrix;
            }

        private:

            auto get_time_machine() -> std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface>
            {
                std::unique_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine;

                auto callback = [&]<class T>(T)
                {
                    time_machine = global_optimality_approximator::TimeMachineOptimizerFactory::get_random_taylor_time_machine_optimizer<T>();
                };

                float_def::get_float_type_by_byte_width(callback, this->time_machine_optimizer_float_byte_width);

                return time_machine;
            }

            auto get_product_evaluator(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& gold_std) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
            {
                std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> product_evaluator;

                auto callback = [&]<class T>(T)
                {
                    product_evaluator = matrix_evaluator::ProductEvaluatorFactory::get_immutable_shape_product_evaluator<T>(gold_std);
                };

                float_def::get_float_type_by_byte_width(callback, this->product_evaluator_float_byte_width);

                if (this->product_evaluator_has_double_bag)
                {
                    return matrix_evaluator::ProductEvaluatorFactory::get_double_bag_matrix_evaluator(std::move(product_evaluator));
                }
                else
                {
                    return product_evaluator;
                }
            }

            auto get_random_coefficient_projector(the_matrix::MatrixInterface& matrix) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> result;

                auto callback = [&]<class T>(T)
                {
                    std::vector<tensor_std_float_t> coeff_vec = matrix.get_coefficient_vector();

                    std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> point_projector  = temporal_coefficient_projector::CoefficientProjectorFactory::get_point_coefficient_projector(stdx::to_castable_vector_initializer(coeff_vec));
                    std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> random_projector = temporal_coefficient_projector::CoefficientProjectorFactory::get_random_coefficient_projector(coeff_vec.size());

                    if (this->activation_coeff_vec.has_value())
                    {
                        random_projector = temporal_coefficient_projector::CoefficientProjectorFactory::get_activation_projector(std::move(random_projector), this->activation_coeff_vec.value());
                    }

                    result = temporal_coefficient_projector::CoefficientProjectorFactory::get_chained_coefficient_projector(stdx::to_variadic_vector_initializer(std::move(point_projector), std::move(random_projector)));
                };

                float_def::get_float_type_by_byte_width(callback, this->coefficient_projector_float_byte_width);

                return result;
            }
    };
}

#endif