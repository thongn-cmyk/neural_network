#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <serializer/compact_serializer.h>
#include "matrix_optimizer_engine_interface.h"
#include <optional>

namespace matrix_optimizer_subsystem
{
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
                std::unique_ptr<temporal_coefficient_projector_3::TemporalCoefficientProjectorGeneratorInterface> projector_gen = this->get_projector_generator(matrix.get_coefficient_vector().size());
                std::unique_ptr<global_optimality_approximator::TensorFactoryInterface> time_machine_optimizer_factory          = this->get_time_machine_optimizer_factory();
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
                                }
                            }
                            catch (common_exception::operation_canceled_error& e)
                            {
                                throw;
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
                              common_exception::CancellationTokenInterface& cancellation_token)
            {
                if (cancellation_token.is_canceled())
                {
                    common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                }

                std::shared_ptr<the_matrix::MatrixInterface> tmp_matrix = matrix.clone();
                CachedMatrix cached_matrix(tmp_matrix, this->cache_map_capacity);

                std::unique_ptr<time_machine::TimeMachineInterface> time_machine = std::make_unique<SpecificMatrixTimeMachine>(&cached_matrix, &projector, &deviation_extractor);
                time_machine::CachedTimeMachine cached_time_machine(std::move(time_machine), this->time_machine_cache_map_capacity);

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
    };
}

#endif