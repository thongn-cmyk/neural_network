//HEADER_CONTROL 8

#ifndef __MATRIX_OPTIMIZER_H__
#define __MATRIX_OPTIMIZER_H__

#include "matrix_optimizer_interface.h"
#include "cached_matrix_projector.h"
#include "cached_time_machine.h"
#include "temporal_coefficient_projector_interface.h"
#include "temporal_coefficient_projector.h"
#include "temporal_coefficient_projector_3.h"
#include "matrix_evaluator_interface.h"
#include "time_machine_optimizer_factory.h"
#include "time_machine_optimizer_2_factory.h"
#include "the_matrix_interface.h"
#include "float_def.h"
#include "tensor_model.h"
#include "matrix_evaluator_factory.h"

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

            auto set_cache_map_capacity(size_t sz) -> TemporalCoefficientOptimizer&
            {
                this->cache_map_capacity = sz;

                return *this;
            }

            auto set_time_machine_cache_map_capacity(size_t sz) -> TemporalCoefficientOptimizer&
            {
                this->time_machine_cache_map_capacity = sz;

                return *this;
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
                                                             matrix_base(matrix_base),
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
                                              matrix_evaluator::MatrixEvaluatorInterface * product_evaluator): base_matrix(base_matrix),
                                                                                                               coefficient_projector(coefficient_projector),
                                                                                                               product_evaluator(product_evaluator){}

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
            size_t space_iteration_sz;
            std::optional<std::vector<bool>> activation_coeff_vec;

            size_t product_evaluator_float_byte_width;
            bool product_evaluator_has_double_bag;
            size_t coefficient_projector_float_byte_width;
            size_t time_machine_optimizer_float_byte_width;

            static inline constexpr size_t DEFAULT_CACHE_MAP_CAPACITY               = 1024u;
            static inline constexpr size_t DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY  = 1024u;
            static inline constexpr size_t DEFAULT_OPTIMIZATION_EPOCH_SZ            = 8u;
            static inline constexpr size_t DEFAULT_OPTIMIZATION_STEP_SZ             = 8u;
            static inline constexpr size_t DEFAULT_SPACE_ITERATION_SZ               = 32u;
            static inline constexpr size_t DEFAULT_FLOAT_BYTE_WIDTH                 = float_def::STD_FLOAT_TYPE_BYTE_WIDTH;

        public:

            BruteForceMatrixOptimizer(): optimizer(DEFAULT_CACHE_MAP_CAPACITY, DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY),
                                         optimization_epoch_sz(DEFAULT_OPTIMIZATION_EPOCH_SZ),
                                         optimization_step_sz(DEFAULT_OPTIMIZATION_STEP_SZ),
                                         space_iteration_sz(DEFAULT_SPACE_ITERATION_SZ),
                                         activation_coeff_vec(std::nullopt),
                                         product_evaluator_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH),
                                         product_evaluator_has_double_bag(false),
                                         coefficient_projector_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH),
                                         time_machine_optimizer_float_byte_width(DEFAULT_FLOAT_BYTE_WIDTH){}

            auto set_cache_map_capacity(size_t sz) -> BruteForceMatrixOptimizer&
            {
                this->optimizer.set_cache_map_capacity(sz);

                return *this;
            }

            auto set_time_machine_cache_map_capacity(size_t sz) -> BruteForceMatrixOptimizer&
            {
                this->optimizer.set_time_machine_cache_map_capacity(sz);

                return *this;
            }

            auto set_optimization_epoch_size(size_t sz) -> BruteForceMatrixOptimizer&
            {
                if (sz == 0u)
                {
                    throw std::runtime_error("bad optimization epoch size");
                }

                this->optimization_epoch_sz = sz;

                return *this;
            }

            auto set_optimization_step_size(size_t sz) -> BruteForceMatrixOptimizer&
            {
                if (sz == 0u)
                {
                    throw std::runtime_error("bad optimization step size");
                }

                this->optimization_step_sz = sz;

                return *this;
            }

            auto set_space_iteration_size(size_t sz) -> BruteForceMatrixOptimizer&
            {
                if (sz == 0u)
                {
                    throw std::runtime_error("bad space iteration size");
                }

                this->space_iteration_sz = sz;

                return *this;
            }

            auto set_product_evaluator_float_byte_width(size_t sz) -> BruteForceMatrixOptimizer&
            {
                float_def::check_float_type_by_byte_width(sz);
                this->product_evaluator_float_byte_width = sz;

                return *this;
            }

            auto set_product_evaluator_double_bag_status(bool flag) -> BruteForceMatrixOptimizer&
            {
                this->product_evaluator_has_double_bag = flag;

                return *this;
            }

            auto set_coefficient_projector_float_byte_width(size_t sz) -> BruteForceMatrixOptimizer&
            {
                float_def::check_float_type_by_byte_width(sz);
                this->coefficient_projector_float_byte_width = sz;

                return *this;
            }

            auto set_time_machine_optimizer_float_byte_width(size_t sz) -> BruteForceMatrixOptimizer&
            {
                float_def::check_float_type_by_byte_width(sz);
                this->time_machine_optimizer_float_byte_width = sz;

                return *this;
            }

            auto set_activation_vector(std::optional<std::vector<bool>> vec) -> BruteForceMatrixOptimizer&
            {
                this->activation_coeff_vec = std::move(vec);

                return *this;
            }

            auto optimize(the_matrix::MatrixInterface& matrix,
                          const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& gold_std) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                //we'd work on strategy optimizations, such is that for an action to form appropriately, we'd need to take a sequence of actions to achieve a predefined goal
                //our work is (context) -> immediate action

                //we'd work on two major works:

                //(context) optimization, providing all the argument variables with appropriate memory
                //immediate action, formed by a strategy of sequence of actions followed by a propagation of feedback

                //in this particular scenerio, we are interested in the projection space of the line, so the next line is "actionable"
                //and the container that holds the actionable is a heuristic, so our problem becomes (context) -> heuristic -> immediate action

                //in this sense, we can punch the context hole by one-dimensionalization, says that we lossless compress the score AND the previous projections into one dimensional integer space
                //then the heuristic is BY context hole

                //and we'd have to hone the projection line to be part of a strategy or a bigger picture

                std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> projector_gen = this->get_projector_generator(matrix.get_coefficient_vector().size());
                std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> time_machine_optimizer_factory          = this->get_time_machine_optimizer_factory();
                std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> product_evaluator                                   = this->get_product_evaluator(gold_std);
                std::shared_ptr<the_matrix::MatrixInterface> best_matrix                                                        = matrix.clone();

                for (size_t i = 0u; i < this->optimization_epoch_sz; ++i)
                {
                    std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix = best_matrix->clone();

                    for (size_t j = 0u; j < this->optimization_step_sz; ++j)
                    {
                        std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorContainerInterface> projector_container   = projector_gen->get();
                        std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> coefficient_projector            = this->get_projector_from(best_matrix->get_coefficient_vector(), projector_container->get());
                        double projection_score                                                                                                 = 0;

                        for (size_t z = 0u; z < this->space_iteration_sz; ++z)
                        {
                            std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine_optimizer = time_machine_optimizer_factory->get()->get();

                            try
                            {
                                std::vector<tensor_std_float_t> new_coefficient_vec         = this->optimizer.optimize(*tmp_matrix,
                                                                                                                       *coefficient_projector,
                                                                                                                       *product_evaluator,
                                                                                                                       *time_machine_optimizer);

                                std::shared_ptr<the_matrix::MatrixInterface> test_matrix    = tmp_matrix->clone();
                                test_matrix->set_coefficient_vector(new_coefficient_vec);

                                if (stdx::nan_cmp(product_evaluator->get_deviation(*test_matrix), product_evaluator->get_deviation(*tmp_matrix)) < 0)
                                {
                                    tmp_matrix          = test_matrix;
                                    projection_score    = 1;
                                }
                            }
                            catch (...)
                            {
                                continue;
                            }
                        }

                        projector_container->feedback(projection_score);
                    }

                    if (stdx::nan_cmp(product_evaluator->get_deviation(*tmp_matrix), product_evaluator->get_deviation(*best_matrix)) < 0)
                    {
                        best_matrix = tmp_matrix;
                    }
                }

                return best_matrix;
            }

        private:

            auto get_projector_from(const std::vector<tensor_model::tensor_std_float_t>& origin,
                                    const std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>& projectile) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                if (projectile == nullptr)
                {
                    throw std::invalid_argument("bad projectile, null");
                }

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> point_projector      = std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(origin));
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> up_projectile        = std::make_unique<temporal_coefficient_projector::SharedPointerProjector>(projectile);
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> chained_projector    = std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(up_projectile), std::move(point_projector)));
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> final_projector;

                if (this->activation_coeff_vec.has_value())
                {
                    final_projector = std::make_unique<temporal_coefficient_projector::ActivationProjector>(std::move(chained_projector), this->activation_coeff_vec.value());
                }
                else
                {
                    final_projector = std::move(chained_projector);
                }

                return final_projector;
            }

            auto get_projector_generator(size_t projection_sz) -> std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface>
            {
                std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> projector_gen;

                auto callback = [&]<class T>(T)
                {
                    projector_gen = temporal_coefficient_projector_3::GeneratorFactory::get_best_generator(projection_sz, 8u);
                };

                float_def::get_float_type_by_byte_width(callback, this->coefficient_projector_float_byte_width);

                return projector_gen;
            }

            auto get_time_machine_optimizer_factory() -> std::unique_ptr<global_optimality_approximator::TensorFactoryInterface>
            {
                std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> time_machine_optimizer_factory;

                auto callback = [&]<class T>(T)
                {
                    time_machine_optimizer_factory = global_optimality_approximator::TensorFactoryFactory::get_best_factory<T>();
                };

                float_def::get_float_type_by_byte_width(callback, this->time_machine_optimizer_float_byte_width);

                return time_machine_optimizer_factory;
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
    };
}

#endif