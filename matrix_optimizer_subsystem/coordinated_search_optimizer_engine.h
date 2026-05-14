#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <serializer/compact_serializer.h>
#include "matrix_optimizer_engine_interface.h"
#include <optional>
#include <matrix/tensor_model.h>
#include <matrix/the_matrix_interface.h>
#include <matrix/cached_matrix_projector.h>
#include <matrix_evaluator/matrix_evaluator_interface.h>
#include <common_exception/cancellation_token.h>
#include <common_exception/common_exception.h>
#include <matrix_steering_subsystem/temporal_coefficient_projector_3.h>
#include <matrix_steering_subsystem/time_machine_optimizer_2_factory.h>
#include <matrix_steering_subsystem/cached_time_machine.h>
#include <general_definition/float_def.h>
#include <stl_extension/stdx.h>
#include <matrix_steering_subsystem/score_context_optimizer.h>

namespace matrix_optimizer_subsystem
{
    using namespace float_def;

    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    struct CoordinatedSearchOptimizerEngineConfig
    {
        std::optional<uint64_t> matrix_cache_map_cap;
        std::optional<uint64_t> time_machine_cache_map_cap;
        uint64_t optimization_epoch_sz;
        uint64_t optimization_step_sz;
        std::optional<uint64_t> optimization_loop_sz;
        std::optional<uint64_t> coefficient_projector_float_byte_width;
        std::optional<uint64_t> time_machine_optimizer_float_byte_width;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(matrix_cache_map_cap,
                      time_machine_cache_map_cap,
                      optimization_epoch_sz,
                      optimization_step_sz,
                      optimization_loop_sz,
                      coefficient_projector_float_byte_width,
                      time_machine_optimizer_float_byte_width);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(matrix_cache_map_cap,
                      time_machine_cache_map_cap,
                      optimization_epoch_sz,
                      optimization_step_sz,
                      optimization_loop_sz,
                      coefficient_projector_float_byte_width,
                      time_machine_optimizer_float_byte_width);
        }
    };

    struct ExternalCoordinatedSearchOptimizerEngineConfig
    {
        std::string config_bytestream;

        template <class Reflector>        
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_coordinated_search_optimizer_engine_config(const CoordinatedSearchOptimizerEngineConfig& config) -> ExternalCoordinatedSearchOptimizerEngineConfig
    {
        return ExternalCoordinatedSearchOptimizerEngineConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_coordinated_search_optimizer_engine_config(const ExternalCoordinatedSearchOptimizerEngineConfig& config) -> CoordinatedSearchOptimizerEngineConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<CoordinatedSearchOptimizerEngineConfig>(config.config_bytestream);
    }

    //this is the heuristic generator that I talked about, if we have the function context -> heuristic_generator then we'd find optimality if reasonable heuristics
    //so I guess that today that we'd do, first is to work on the context enumeration, according to the theory, that more context is better than less context (in the sense that more always yields better results, but we'd have to balance between memory, warmup and the benefits from the results

    //so there are three contexts: 
        //(activation_region context) => activation_region heuristic
        //(projection_line context) => projection_line heuristic
        //(time_machine context) => time_machine heuristic

    //remember that we are trying to optimize logit density, so all means to achieve that end is approved, not necessarily that we are too skewed here, too skewed there

    //what we see here is the hierarchy of the pictures

        //we guess activation_region -> we guess projection_line in the multidimensional_space -> we guess the time_machine
        //so there must be a depends-on relationship between the latter and the former contexts

        //if we just cluelessly use iterative score-based for every scope, then we have accidentially create a chain of maximized actions, this is a hard theory to grasp, so the latter actions actually are dynamically better than the immediate former actions
        //the example is that we 100 people before you have failed, now you have to try something exotic, never done before, and give me the course of action

        //what we have yet to optimize is to provide a cluster of actions, so we are talking about unit, multiple projectors into one context optimizer
        //because each projection is one doable, we can't really give insight into the context as much as a unit of actions

        //so it's complicated

    //

    class CoordinatedSearchOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            size_t matrix_cache_map_cap;
            size_t time_machine_cache_map_cap;
            size_t optimization_epoch_sz;
            size_t optimization_step_sz;
            size_t optimization_loop_sz;
            size_t coefficient_projector_float_byte_width;
            size_t time_machine_optimizer_float_byte_width;

            static inline constexpr size_t DEFAULT_MATRIX_CACHE_MAP_CAPACITY                = size_t{1} << 10;
            static inline constexpr size_t MIN_MATRIX_CACHE_MAP_CAPACITY                    = size_t{1} << 0;
            static inline constexpr size_t MAX_MATRIX_CACHE_MAP_CAPACITY                    = size_t{1} << 14;

            static inline constexpr size_t DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY          = size_t{1} << 10;
            static inline constexpr size_t MIN_TIME_MACHINE_CACHE_MAP_CAPACITY              = size_t{1} << 0;
            static inline constexpr size_t MAX_TIME_MACHINE_CACHE_MAP_CAPACITY              = size_t{1} << 14;

            static inline constexpr size_t DEFAULT_OPTIMIZATION_LOOP_SZ                     = size_t{1} << 4;
            static inline constexpr size_t DEFAULT_COEFFICIENT_PROJECTOR_FLOAT_BYTE_WIDTH   = size_t{1} << 3;
            static inline constexpr size_t DEFAULT_TIME_MACHINE_OPTIMIZER_FLOAT_BYTE_WIDTH  = size_t{1} << 3;

        public:

            CoordinatedSearchOptimizerEngine(const CoordinatedSearchOptimizerEngineConfig& config): matrix_cache_map_cap(DEFAULT_MATRIX_CACHE_MAP_CAPACITY),
                                                                                                    time_machine_cache_map_cap(DEFAULT_TIME_MACHINE_CACHE_MAP_CAPACITY),
                                                                                                    optimization_epoch_sz(config.optimization_epoch_sz),
                                                                                                    optimization_step_sz(config.optimization_step_sz),
                                                                                                    optimization_loop_sz(DEFAULT_OPTIMIZATION_LOOP_SZ),
                                                                                                    coefficient_projector_float_byte_width(DEFAULT_COEFFICIENT_PROJECTOR_FLOAT_BYTE_WIDTH),
                                                                                                    time_machine_optimizer_float_byte_width(DEFAULT_TIME_MACHINE_OPTIMIZER_FLOAT_BYTE_WIDTH)
            {
                if (config.matrix_cache_map_cap.has_value())
                {
                    this->matrix_cache_map_cap = std::clamp(static_cast<size_t>(config.matrix_cache_map_cap.value()),
                                                            MIN_MATRIX_CACHE_MAP_CAPACITY,
                                                            MAX_MATRIX_CACHE_MAP_CAPACITY);
                }

                if (config.time_machine_cache_map_cap.has_value())
                {
                    this->time_machine_cache_map_cap = std::clamp(static_cast<size_t>(config.time_machine_cache_map_cap.value()),
                                                                  MIN_TIME_MACHINE_CACHE_MAP_CAPACITY,
                                                                  MAX_TIME_MACHINE_CACHE_MAP_CAPACITY);
                }

                if (config.optimization_loop_sz.has_value())
                {
                    this->optimization_loop_sz = config.optimization_loop_sz.value();
                }

                if (config.coefficient_projector_float_byte_width.has_value())
                {
                    this->coefficient_projector_float_byte_width = config.coefficient_projector_float_byte_width.value();
                }

                if (config.time_machine_optimizer_float_byte_width.has_value())
                {
                    this->time_machine_optimizer_float_byte_width = config.time_machine_optimizer_float_byte_width.value();
                }
            }

            CoordinatedSearchOptimizerEngine(const ExternalCoordinatedSearchOptimizerEngineConfig& config): CoordinatedSearchOptimizerEngine(to_internal_coordinated_search_optimizer_engine_config(config)){}

            auto optimize(the_matrix::MatrixInterface& matrix,
                          matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> projector_gen      = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<HeuristicProjectorGenerator>(matrix.get_coefficient_vector().size()));
                std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> time_machine_gen   = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<HeuristicTimeMachineGenerator>());
                std::shared_ptr<the_matrix::MatrixInterface> best_matrix                                        = matrix.clone();

                for (size_t i = 0u; i < this->optimization_epoch_sz; ++i)
                {
                    std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix                                     = best_matrix->clone();
                    std::unique_ptr<score_context_optimizer::IterationContextInterface> projector_iteration_ctx = projector_gen->get();

                    for (size_t j = 0u; j < this->optimization_step_sz; ++j)
                    {
                        std::unique_ptr<score_context_optimizer::ActionableResultInterface> heuristic_projector_wrapper                         = projector_iteration_ctx->next();
                        std::shared_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> heuristic_projector   = std::dynamic_pointer_cast<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface>(heuristic_projector_wrapper->get_statistical_machine());
                        std::shared_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorContainerInterface> projector_container   = heuristic_projector->get();
                        
                        std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> coefficient_projector            = this->get_projector_from(best_matrix->get_coefficient_vector(), projector_container->get());
                        double projection_score                                                                                                 = 0;
                        std::unique_ptr<score_context_optimizer::IterationContextInterface> time_machine_iteration_ctx                          = time_machine_gen->get();

                        for (size_t z = 0u; z < this->optimization_loop_sz; ++z)
                        {
                            std::unique_ptr<score_context_optimizer::ActionableResultInterface> heuristic_time_machine_wrapper      = time_machine_iteration_ctx->next();
                            std::shared_ptr<global_optimality_approximator::TensorFactoryInterface> heuristic_time_machine          = std::dynamic_pointer_cast<global_optimality_approximator::TensorFactoryInterface>(heuristic_time_machine_wrapper->get_statistical_machine());
                            std::shared_ptr<global_optimality_approximator::FactoryTensorInterface> time_machine_container          = heuristic_time_machine->get();
                            std::shared_ptr<global_optimality_approximator::TimeMachineOptimizerInterface> time_machine_optimizer   = time_machine_container->get();
                            double time_machine_score                                                                               = 0;

                            try
                            {
                                std::vector<tensor_std_float_t> new_coefficient_vec         = this->optimize_one(*tmp_matrix,
                                                                                                                 *coefficient_projector,
                                                                                                                 matrix_evaluator,
                                                                                                                 *time_machine_optimizer,
                                                                                                                 cancellation_token);

                                std::shared_ptr<the_matrix::MatrixInterface> test_matrix    = tmp_matrix->clone();
                                test_matrix->set_coefficient_vector(new_coefficient_vec);

                                if (stdx::nan_cmp(matrix_evaluator.get_deviation(*test_matrix), matrix_evaluator.get_deviation(*tmp_matrix)) < 0)
                                {
                                    tmp_matrix          = test_matrix;
                                    projection_score    = 1;
                                    time_machine_score  = 1;
                                }
                            }
                            catch (common_exception::operation_canceled_error& e)
                            {
                                throw;
                            }
                            catch (...){}

                            time_machine_container->feedback(time_machine_score);
                            heuristic_time_machine_wrapper->feedback(time_machine_score);
                        }

                        projector_container->feedback(projection_score);
                        heuristic_projector_wrapper->feedback(projection_score);
                    }

                    if (stdx::nan_cmp(matrix_evaluator.get_deviation(*tmp_matrix), matrix_evaluator.get_deviation(*best_matrix)) < 0)
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

                return chained_projector;
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

            auto optimize_one(the_matrix::MatrixInterface& matrix,
                              temporal_coefficient_projector::TemporalCoefficientProjectorInterface& projector,
                              matrix_evaluator::MatrixEvaluatorInterface& deviation_extractor,
                              global_optimality_approximator::TimeMachineOptimizerInterface& time_machine_optimizer,
                              common_exception::CancellationTokenInterface& cancellation_token) -> std::vector<tensor_std_float_t>
            {
                if (cancellation_token.is_canceled())
                {
                    common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                }

                std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix = matrix.clone();
                CachedMatrix cached_matrix(tmp_matrix, this->matrix_cache_map_cap);

                std::unique_ptr<time_machine::TimeMachineInterface> time_machine = std::make_unique<SpecificMatrixTimeMachine>(&cached_matrix, &projector, &deviation_extractor);
                time_machine::CachedTimeMachine cached_time_machine(std::move(time_machine), this->time_machine_cache_map_cap);

                std_float_t t                       = time_machine_optimizer.optimize(cached_time_machine);
                std::vector<std_float_t> coeff_vec  = projector.project(t);

                return stdx::to_castable_vector_initializer(std::move(coeff_vec));
            }

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

            class HeuristicProjectorAsStatisticalMachine: public virtual score_context_optimizer::StatisticalMachineInterface,
                                                          public virtual temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface
            {
                private:

                    std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> projector;

                public:

                    HeuristicProjectorAsStatisticalMachine(std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> projector) noexcept: projector(std::move(projector)){}

                    auto get() -> std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorContainerInterface>
                    {
                        return this->projector->get();
                    }

                    auto space_size() -> size_t
                    {
                        return this->projector->space_size();
                    }
            };

            class HeuristicProjectorGenerator: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
            {
                private:

                    size_t projection_sz;

                public:

                    HeuristicProjectorGenerator(size_t projection_sz): projection_sz(projection_sz){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        return std::make_unique<HeuristicProjectorAsStatisticalMachine>(temporal_coefficient_projector_3::GeneratorFactory::get_best_generator(this->projection_sz, 8u)); //memory
                    }
            };

            class HeuristicTimeMachineAsStatisticalMachine: public virtual score_context_optimizer::StatisticalMachineInterface,
                                                            public virtual global_optimality_approximator::TensorFactoryInterface
            {
                private:

                    std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> time_machine;

                public:

                    HeuristicTimeMachineAsStatisticalMachine(std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> time_machine) noexcept: time_machine(std::move(time_machine)){}

                    auto get() -> std::unique_ptr<global_optimality_approximator::FactoryTensorInterface>
                    {
                        return this->time_machine->get();
                    }
            };

            class HeuristicTimeMachineGenerator: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
            {
                public:

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        return std::make_unique<HeuristicTimeMachineAsStatisticalMachine>(global_optimality_approximator::TensorFactoryFactory::get_best_factory());
                    }
            };
    };
}

#endif