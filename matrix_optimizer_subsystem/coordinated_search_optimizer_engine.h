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
                std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> iteration_projector_gen                = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<IterationHeuristicProjectorGenerator>(matrix.get_coefficient_vector().size(),
                                                                                                                                                                                                                                                                                          this->coefficient_projector_float_byte_width));

                std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> iteration_iteration_time_machine_gen   = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<IterationIterationHeuristicTimeMachineGenerator>(this->time_machine_optimizer_float_byte_width));
                std::shared_ptr<the_matrix::MatrixInterface> best_matrix                                                            = matrix.clone();

                std::unique_ptr<score_context_optimizer::IterationContextInterface> iteration_projector_gen_it                      = iteration_projector_gen->get();
                std::unique_ptr<score_context_optimizer::IterationContextInterface> iteration_iteration_time_machine_gen_it         = iteration_iteration_time_machine_gen->get();

                for (size_t i = 0u; i < this->optimization_epoch_sz; ++i)
                {
                    std::unique_ptr<score_context_optimizer::ActionableResultInterface> projector_iteration_ctx_wrapper             = iteration_projector_gen_it->next();
                    std::unique_ptr<score_context_optimizer::ActionableResultInterface> outer_time_machine_iteration_ctx_wrapper    = iteration_iteration_time_machine_gen_it->next();

                    std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix                                                         = best_matrix->clone();

                    std::unique_ptr<score_context_optimizer::IterationContextInterface> projector_iteration_ctx                     = std::dynamic_pointer_cast<score_context_optimizer::IterationContextGeneratorInterface>(projector_iteration_ctx_wrapper->get_statistical_machine())->get();
                    std::unique_ptr<score_context_optimizer::IterationContextInterface> outer_time_machine_iteration_ctx            = std::dynamic_pointer_cast<score_context_optimizer::IterationContextGeneratorInterface>(outer_time_machine_iteration_ctx_wrapper->get_statistical_machine())->get();

                    for (size_t j = 0u; j < this->optimization_step_sz; ++j)
                    {
                        std::unique_ptr<score_context_optimizer::ActionableResultInterface> heuristic_projector_wrapper                         = projector_iteration_ctx->next();
                        std::shared_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> heuristic_projector   = std::dynamic_pointer_cast<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface>(heuristic_projector_wrapper->get_statistical_machine());
                        std::shared_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorContainerInterface> projector_container   = heuristic_projector->get();
                        std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> coefficient_projector            = this->get_projector_from(best_matrix->get_coefficient_vector(), projector_container->get());
                        double projection_score                                                                                                 = 0;

                        std::unique_ptr<score_context_optimizer::ActionableResultInterface> time_machine_iteration_ctx_wrapper                  = outer_time_machine_iteration_ctx->next();
                        std::unique_ptr<score_context_optimizer::IterationContextInterface> time_machine_iteration_ctx                          = std::dynamic_pointer_cast<score_context_optimizer::IterationContextGeneratorInterface>(time_machine_iteration_ctx_wrapper->get_statistical_machine())->get();

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
                        time_machine_iteration_ctx_wrapper->feedback(projection_score);
                    }

                    if (stdx::nan_cmp(matrix_evaluator.get_deviation(*tmp_matrix), matrix_evaluator.get_deviation(*best_matrix)) < 0)
                    {
                        best_matrix = tmp_matrix;
                        outer_time_machine_iteration_ctx_wrapper->feedback(1u);
                        projector_iteration_ctx_wrapper->feedback(1u);
                    }
                    else
                    {
                        outer_time_machine_iteration_ctx_wrapper->feedback(0u);
                        projector_iteration_ctx_wrapper->feedback(0u);
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

                std::shared_ptr<bool> is_in_stack = std::make_shared<bool>(true);
                stdx::StackGuard stack_grd
                (
                    [=]
                    {
                        *is_in_stack = false;
                    }
                );

                std::unique_ptr<time_machine::TimeMachineInterface> time_machine = std::make_unique<SpecificMatrixTimeMachine>(&cached_matrix,
                                                                                                                               &projector,
                                                                                                                               &deviation_extractor,
                                                                                                                               is_in_stack);

                time_machine::CachedTimeMachine cached_time_machine(std::move(time_machine),
                                                                    this->time_machine_cache_map_cap);

                std::shared_ptr<bool> is_in_stack_2 = std::make_shared<bool>(true);
                stdx::StackGuard stack_grd_2
                {
                    [=]
                    {
                        *is_in_stack_2 = false;
                    }
                };

                DeviationCapturedTimeMachine deviation_captured_time_machine(&cached_time_machine,
                                                                             is_in_stack_2);

                std_float_t t                                           = time_machine_optimizer.optimize(deviation_captured_time_machine);
                std::optional<std::pair<std_float_t, tm_float_t>> cand  = deviation_captured_time_machine.best();

                if (cand.has_value())
                {
                    t = cand->first;
                }

                std::vector<std_float_t> coeff_vec  = projector.project(t);

                return stdx::to_castable_vector_initializer(std::move(coeff_vec));
            }

            class DeviationCapturedTimeMachine: public virtual time_machine::TimeMachineInterface
            {
                private:

                    time_machine::TimeMachineInterface * base;

                    std::optional<std_float_t> best_x;
                    std::optional<tm_float_t> best_y;

                    std::shared_ptr<bool> is_in_stack; //we keep this as peace of mind, because if we have race condition, or multithreading, we aren't splitting the responsibility correctly, so that is never going to happen
                
                public:

                    DeviationCapturedTimeMachine(time_machine::TimeMachineInterface * base,
                                                 std::shared_ptr<bool> is_in_stack): base(base),
                                                                                     best_x(),
                                                                                     best_y(),
                                                                                     is_in_stack(std::move(is_in_stack)){}

                    auto f(std_float_t t) -> tm_float_t
                    {
                        if (*this->is_in_stack == false)
                        {
                            throw std::invalid_argument("illegal invoke, out of stack");
                        }

                        tm_float_t rs = this->base->f(t);

                        if (!std::isnan(t) && !std::isnan(rs))
                        {
                            if (!this->best_y.has_value())
                            {
                                this->best_x    = t;
                                this->best_y    = rs;
                            }

                            if (this->best_y.value() > rs)
                            {
                                this->best_x    = t;
                                this->best_y    = rs;
                            }
                        }

                        return rs;
                    }

                    auto best() -> std::optional<std::pair<std_float_t, tm_float_t>>
                    {
                        if (this->best_x.has_value())
                        {
                            if (!this->best_y.has_value())
                            {
                                std::abort();
                            }

                            return std::make_pair(this->best_x.value(), this->best_y.value());
                        }

                        return std::nullopt;
                    }
            };

            //__generic_resolution__
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
                    std::shared_ptr<bool> is_in_stack;

                public:

                    SpecificMatrixTimeMachine(the_matrix::MatrixInterface * base_matrix,
                                              temporal_coefficient_projector::TemporalCoefficientProjectorInterface * coefficient_projector,
                                              matrix_evaluator::MatrixEvaluatorInterface * product_evaluator,
                                              std::shared_ptr<bool> is_in_stack): base_matrix(base_matrix),
                                                                                  coefficient_projector(coefficient_projector),
                                                                                  product_evaluator(product_evaluator),
                                                                                  is_in_stack(std::move(is_in_stack)){}

                    auto f(std_float_t t) -> tm_float_t
                    {
                        if (*this->is_in_stack == false)
                        {
                            throw std::invalid_argument("illegal invoke, out of stack");
                        }

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
                    size_t float_byte_width;

                public:

                    HeuristicProjectorGenerator(size_t projection_sz,
                                                size_t float_byte_width): projection_sz(projection_sz),
                                                                          float_byte_width(float_byte_width){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        std::unique_ptr<score_context_optimizer::StatisticalMachineInterface> rs{};

                        auto callback = [&]<class T>(T)
                        {
                            rs = std::make_unique<HeuristicProjectorAsStatisticalMachine>(temporal_coefficient_projector_3::GeneratorFactory::get_best_generator<T>(this->projection_sz, 8u)); //memory
                        };

                        float_def::get_float_type_by_byte_width(callback, this->float_byte_width);

                        return rs;
                    }
            };

            class IterationHeuristicProjectorAsStatisticalMachine: public virtual score_context_optimizer::StatisticalMachineInterface,
                                                                   public virtual score_context_optimizer::IterationContextGeneratorInterface
            {
                private:

                    std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base;
                
                public:

                    IterationHeuristicProjectorAsStatisticalMachine(std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base): base(std::move(base)){}

                    auto get() -> std::unique_ptr<score_context_optimizer::IterationContextInterface>
                    {
                        return this->base->get();
                    }
            };

            class IterationHeuristicProjectorGenerator: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
            {
                private:

                    size_t projection_sz;
                    size_t float_byte_width;

                public:

                    IterationHeuristicProjectorGenerator(size_t projection_sz,
                                                         size_t float_byte_width): projection_sz(projection_sz),
                                                                                   float_byte_width(float_byte_width){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        auto optimizer = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<HeuristicProjectorGenerator>(this->projection_sz,
                                                                                                                                                                                    this->float_byte_width));

                        return std::make_unique<IterationHeuristicProjectorAsStatisticalMachine>(std::move(optimizer));
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
                private:

                    size_t float_byte_width;

                public:

                    HeuristicTimeMachineGenerator(size_t float_byte_width): float_byte_width(float_byte_width){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        std::unique_ptr<score_context_optimizer::StatisticalMachineInterface> rs{};

                        auto callback = [&]<class T>(T)
                        {
                            rs = std::make_unique<HeuristicTimeMachineAsStatisticalMachine>(global_optimality_approximator::TensorFactoryFactory::get_best_factory<T>());
                        };

                        float_def::get_float_type_by_byte_width(callback, this->float_byte_width);

                        return rs;
                    }
            };

            class IterationHeuristicTimeMachineAsStatisticalMachine: public virtual score_context_optimizer::StatisticalMachineInterface,
                                                                     public virtual score_context_optimizer::IterationContextGeneratorInterface
            {
                private:

                    std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base;

                public:

                    IterationHeuristicTimeMachineAsStatisticalMachine(std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base): base(std::move(base)){}

                    auto get() -> std::unique_ptr<score_context_optimizer::IterationContextInterface>
                    {
                        return this->base->get();
                    }
            };

            class IterationHeuristicTimeMachineGenerator: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
            {
                private:

                    size_t float_byte_width;

                public:

                    IterationHeuristicTimeMachineGenerator(size_t float_byte_width): float_byte_width(float_byte_width){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        auto optimizer = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<HeuristicTimeMachineGenerator>(this->float_byte_width));

                        return std::make_unique<IterationHeuristicTimeMachineAsStatisticalMachine>(std::move(optimizer));
                    }
            };

            class IterationIterationHeuristicTimeMachineAsStatisticalMachine: public virtual score_context_optimizer::StatisticalMachineInterface,
                                                                              public virtual score_context_optimizer::IterationContextGeneratorInterface
            {
                private:

                    std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base;

                public:

                    IterationIterationHeuristicTimeMachineAsStatisticalMachine(std::unique_ptr<score_context_optimizer::IterationContextGeneratorInterface> base): base(std::move(base)){}

                    auto get() -> std::unique_ptr<score_context_optimizer::IterationContextInterface>
                    {
                        return this->base->get();                        
                    }
            };

            class IterationIterationHeuristicTimeMachineGenerator: public virtual score_context_optimizer::StatisticalMachineGeneratorInterface
            {
                private:

                    size_t float_byte_width;

                public:

                    IterationIterationHeuristicTimeMachineGenerator(size_t float_byte_width): float_byte_width(float_byte_width){}

                    auto get() -> std::unique_ptr<score_context_optimizer::StatisticalMachineInterface>
                    {
                        auto optimizer = score_context_optimizer::ContextOptimizerFactory::get_best_binary_progress_context_optimizer(std::make_unique<IterationHeuristicTimeMachineGenerator>(this->float_byte_width));

                        return std::make_unique<IterationIterationHeuristicTimeMachineAsStatisticalMachine>(std::move(optimizer));
                    }
            };
    };
}

#endif