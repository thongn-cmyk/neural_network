#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_EXP_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_EXP_COORDINATED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include "coordinated_search_optimizer_engine.h"
#include "retranslation_optimizer/tail_blocked_search_optimizer_engine.h"

namespace matrix_optimizer_subsystem
{
    //I know we are wondering why we use config, when we use dependency injection, when we use etc.
    //in fact, we don't question at all

    //config when it's user-facing, and we use config_builder to build the component, instead of normal flow
    //dependency_injection when it's internal usage, and serialization is not an issue

    //we have been fighting over 30 years of how to serialize objects (unique_ptr, shared_ptr and friends), let's not argue about that anymore that we should construct an object and serialize it or serialize the config and construct the object

    struct BlockCoordinatedSearchOptimizerEngineConfig
    {
        ExternalCoordinatedSearchOptimizerEngineConfig base_config;
        double exp_base;
        uint64_t x0;
        uint64_t iteration_sz;
        uint64_t lvl_bar0;
        double lvl_bar_exp_base;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(base_config,
                      exp_base,
                      x0,
                      iteration_sz,
                      lvl_bar0,
                      lvl_bar_exp_base);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(base_config,
                      exp_base,
                      x0,
                      iteration_sz,
                      lvl_bar0,
                      lvl_bar_exp_base);
        }
    };

    struct ExternalBlockCoordinatedSearchOptimizerEngineConfig
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

    auto to_external_block_coordinated_search_optimizer_engine_config(const BlockCoordinatedSearchOptimizerEngineConfig& config) -> ExternalBlockCoordinatedSearchOptimizerEngineConfig
    {
        return ExternalBlockCoordinatedSearchOptimizerEngineConfig
        {
            .config_bytestream  = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_block_coordinated_search_optimizer_engine_config(const ExternalBlockCoordinatedSearchOptimizerEngineConfig& config) -> BlockCoordinatedSearchOptimizerEngineConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<BlockCoordinatedSearchOptimizerEngineConfig>(config.config_bytestream);
    }

    class BlockCoordinatedSearchOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            std::unique_ptr<MatrixOptimizerEngineInterface> base;

        public:

            BlockCoordinatedSearchOptimizerEngine(const BlockCoordinatedSearchOptimizerEngineConfig& config)
            {
                std::shared_ptr<MatrixOptimizerEngineInterface> base_0  = std::make_unique<CoordinatedSearchOptimizerEngine>(config.base_config);

                this->base  = std::make_unique<TailBlockedSearchOptimizerEngine>(base_0,
                                                                                 config.exp_base,
                                                                                 config.x0,
                                                                                 config.iteration_sz,
                                                                                 config.lvl_bar0,
                                                                                 config.lvl_bar_exp_base);
            }

            BlockCoordinatedSearchOptimizerEngine(const ExternalBlockCoordinatedSearchOptimizerEngineConfig& config): BlockCoordinatedSearchOptimizerEngine(to_internal_block_coordinated_search_optimizer_engine_config(config)){}

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