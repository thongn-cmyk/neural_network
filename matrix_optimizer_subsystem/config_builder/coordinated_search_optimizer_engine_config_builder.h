#ifndef __COORDINATED_SEARCH_OPTIMIZER_ENGINE_CONFIG_BUILDER_H__
#define __COORDINATED_SEARCH_OPTIMIZER_ENGINE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix_optimizer_subsystem/generic_optimizer_engine.h>

namespace matrix_optimizer_subsystem
{
    class CoordinatedSearchOptimizerEngineConfigBuilder
    {
        private:

            std::optional<uint64_t> matrix_cache_map_cap;
            std::optional<uint64_t> time_machine_cache_map_cap;
            uint64_t optimization_epoch_sz;
            uint64_t optimization_step_sz;
            std::optional<uint64_t> optimization_loop_sz;
            std::optional<uint64_t> coefficient_projector_float_byte_width;
            std::optional<uint64_t> time_machine_optimizer_float_byte_width;

            static inline constexpr size_t DEFAULT_OPTIMIZATION_EPOCH_SZ    = 64u;
            static inline constexpr size_t DEFAULT_OPTIMIZATION_STEP_SZ     = 16u;

            using self = CoordinatedSearchOptimizerEngineConfigBuilder;

        public:

            CoordinatedSearchOptimizerEngineConfigBuilder(): matrix_cache_map_cap(),
                                                             time_machine_cache_map_cap(),
                                                             optimization_epoch_sz(DEFAULT_OPTIMIZATION_EPOCH_SZ),
                                                             optimization_step_sz(DEFAULT_OPTIMIZATION_STEP_SZ),
                                                             optimization_loop_sz(),
                                                             coefficient_projector_float_byte_width(),
                                                             time_machine_optimizer_float_byte_width(){}

            auto set_matrix_cache_map_capacity(size_t cap) -> self&
            {
                this->matrix_cache_map_cap = static_cast<uint64_t>(cap);

                return *this;
            }

            auto set_time_machine_cache_map_capacity(size_t cap) -> self&
            {
                this->time_machine_cache_map_cap    = static_cast<uint64_t>(cap);

                return *this;
            }

            auto set_optimization_epoch_size(size_t sz) -> self&
            {
                this->optimization_epoch_sz = sz;

                return *this;
            }

            auto set_optimization_step_size(size_t sz) -> self&
            {
                this->optimization_step_sz  = sz;

                return *this;
            }

            auto set_optimization_loop_size(size_t sz) -> self&
            {
                this->optimization_loop_sz  = static_cast<uint64_t>(sz);

                return *this;
            }

            auto set_coefficient_projector_float_byte_width(size_t sz) -> self&
            {
                this->coefficient_projector_float_byte_width    = static_cast<uint64_t>(sz);

                return *this;
            }

            auto set_time_machine_optimizer_float_byte_width(size_t sz) -> self&
            {
                this->time_machine_optimizer_float_byte_width   = static_cast<uint64_t>(sz);

                return *this;
            }

            auto build() -> ExternalGenericOptimizerEngineConfig
            {
                return this->get_external_generic_optimizer_engine_config();
            }

        protected:

            auto get_external_coordinated_search_optimizer_config() -> ExternalCoordinatedSearchOptimizerEngineConfig
            {
                return to_external_coordinated_search_optimizer_engine_config(this->get_internal_coordinated_search_optimizer_config());
            }

        private:

            auto get_internal_coordinated_search_optimizer_config() -> CoordinatedSearchOptimizerEngineConfig
            {
                return CoordinatedSearchOptimizerEngineConfig
                {
                    .matrix_cache_map_cap                       = this->matrix_cache_map_cap,
                    .time_machine_cache_map_cap                 = this->time_machine_cache_map_cap,
                    .optimization_epoch_sz                      = this->optimization_epoch_sz,
                    .optimization_step_sz                       = this->optimization_step_sz,
                    .optimization_loop_sz                       = this->optimization_loop_sz,
                    .coefficient_projector_float_byte_width     = this->coefficient_projector_float_byte_width,
                    .time_machine_optimizer_float_byte_width    = this->time_machine_optimizer_float_byte_width
                };
            }

            auto get_internal_generic_optimizer_engine_config() -> GenericOptimizerEngineConfig
            {
                return GenericOptimizerEngineConfig
                {
                    .config = this->get_external_coordinated_search_optimizer_config()
                };
            }

            auto get_external_generic_optimizer_engine_config() -> ExternalGenericOptimizerEngineConfig
            {
                return to_external_generic_optimizer_engine_config(this->get_internal_generic_optimizer_engine_config());
            }
    };
}

#endif