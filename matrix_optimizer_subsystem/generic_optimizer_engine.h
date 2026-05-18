#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_GENERIC_MATRIX_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_GENERIC_MATRIX_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include "coordinated_search_optimizer_engine.h"
#include <serializer/compact_serializer.h>
#include "matrix_optimizer_engine_interface.h"
#include <variant>
#include <stl_extension/stdx.h>

namespace matrix_optimizer_subsystem
{
    struct GenericOptimizerEngineConfig
    {
        std::variant<stdx::reflectible_monostate, ExternalCoordinatedSearchOptimizerEngineConfig> config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config);
        }
    };

    struct ExternalGenericOptimizerEngineConfig
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

    auto to_external_generic_optimizer_engine_config(const GenericOptimizerEngineConfig& config) -> ExternalGenericOptimizerEngineConfig
    {
        return ExternalGenericOptimizerEngineConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_optimizer_engine_config(const ExternalGenericOptimizerEngineConfig& config) -> GenericOptimizerEngineConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericOptimizerEngineConfig>(config.config_bytestream);
    }

    class GenericOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            std::unique_ptr<MatrixOptimizerEngineInterface> base;

        public:

            GenericOptimizerEngine(const GenericOptimizerEngineConfig& config)
            {
                if (std::holds_alternative<ExternalCoordinatedSearchOptimizerEngineConfig>(config.config))
                {
                    this->base = std::make_unique<CoordinatedSearchOptimizerEngine>(std::get<ExternalCoordinatedSearchOptimizerEngineConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad config, dispatch code not found");
                }
            }

            GenericOptimizerEngine(const ExternalGenericOptimizerEngineConfig& config): GenericOptimizerEngine(to_internal_generic_optimizer_engine_config(config)){}

            auto optimize(the_matrix::MatrixInterface& matrix,
                          matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                return this->base->optimize(matrix,
                                            matrix_evaluator,
                                            cancellation_token);
            }
    };
}

#endif