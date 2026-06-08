#ifndef __BLOCK_COORDINATED_SEARCH_OPTIMIZER_ENGINE_CONFIG_BUILDER_H__
#define __BLOCK_COORDINATED_SEARCH_OPTIMIZER_ENGINE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "coordinated_search_optimizer_engine_config_builder.h"
#include <matrix_optimizer_subsystem/generic_optimizer_engine.h>

namespace matrix_optimizer_subsystem
{
    class BlockCoordinatedSearchOptimizerEngineConfigBuilder: private CoordinatedSearchOptimizerEngineConfigBuilder
    {
        private:

            double blk_exp_base;
            size_t blk_x0;
            size_t blk_iteration_sz;
            size_t blk_lvl_bar;
            double blk_lvl_bar_exp_base;

            static inline constexpr double DEFAULT_BLK_EXP_BASE             = 2;
            static inline constexpr size_t DEFAULT_BLK_X0                   = size_t{1} << 4;
            static inline constexpr size_t DEFAULT_BLK_ITERATION_SZ         = size_t{1} << 7;
            static inline constexpr size_t DEFAULT_BLK_LVL_BAR              = size_t{1} << 2;
            static inline constexpr double DEFAULT_BLK_LVL_BAR_EXP_BASE     = 1;

            using self  = BlockCoordinatedSearchOptimizerEngineConfigBuilder;
            using base  = CoordinatedSearchOptimizerEngineConfigBuilder;

        public:

            BlockCoordinatedSearchOptimizerEngineConfigBuilder(): CoordinatedSearchOptimizerEngineConfigBuilder(),
                                                                  blk_exp_base(DEFAULT_BLK_EXP_BASE),
                                                                  blk_x0(DEFAULT_BLK_X0),
                                                                  blk_iteration_sz(DEFAULT_BLK_ITERATION_SZ),
                                                                  blk_lvl_bar(DEFAULT_BLK_LVL_BAR),
                                                                  blk_lvl_bar_exp_base(DEFAULT_BLK_LVL_BAR_EXP_BASE){}

            auto set_matrix_cache_map_capacity(size_t cap) -> self&
            {
                base::set_matrix_cache_map_capacity(cap);

                return *this;
            }

            auto set_time_machine_cache_map_capacity(size_t cap) -> self&
            {
                base::set_time_machine_cache_map_capacity(cap);

                return *this;
            }

            auto set_optimization_epoch_size(size_t sz) -> self&
            {
                base::set_optimization_epoch_size(sz);

                return *this;
            }

            auto set_optimization_step_size(size_t sz) -> self&
            {
                base::set_optimization_step_size(sz);

                return *this;
            }

            auto set_optimization_loop_size(size_t sz) -> self&
            {
                base::set_optimization_loop_size(sz);

                return *this;
            }

            auto set_coefficient_projector_float_byte_width(size_t sz) -> self&
            {
                base::set_coefficient_projector_float_byte_width(sz);

                return *this;
            }

            auto set_time_machine_optimizer_float_byte_width(size_t sz) -> self&
            {
                base::set_time_machine_optimizer_float_byte_width(sz);

                return *this;
            }

            auto set_block_exponential_base(double val) -> self&
            {
                this->blk_exp_base  = val;

                return *this;
            }

            auto set_block_x0(size_t val) -> self& 
            {
                this->blk_x0    = val;

                return *this;
            }

            auto set_block_iteration_size(size_t val) -> self&
            {
                this->blk_iteration_sz  = val;

                return *this;
            }

            auto set_block_level_bar(size_t val) -> self&
            {
                this->blk_lvl_bar   = val;

                return *this;
            }

            auto set_block_level_bar_exponential_base(double val) -> self&
            {
                this->blk_lvl_bar_exp_base  = val;

                return *this;
            }

            auto build() -> ExternalGenericOptimizerEngineConfig
            {
                return this->get_external_generic_optimizer_engine_config();
            }
        
        protected:

            auto get_external_block_coordinated_search_optimizer_engine_config() -> ExternalBlockCoordinatedSearchOptimizerEngineConfig
            {
                return to_external_block_coordinated_search_optimizer_engine_config(this->get_internal_block_coordinated_search_optimizer_engine_config());
            }
        
        private:

            auto get_internal_block_coordinated_search_optimizer_engine_config() -> BlockCoordinatedSearchOptimizerEngineConfig
            {
                return BlockCoordinatedSearchOptimizerEngineConfig
                {
                    .base_config        = this->get_external_coordinated_search_optimizer_config(),
                    .exp_base           = this->blk_exp_base,
                    .x0                 = static_cast<uint64_t>(this->blk_x0),
                    .iteration_sz       = static_cast<uint64_t>(this->blk_iteration_sz),
                    .lvl_bar0           = static_cast<uint64_t>(this->blk_lvl_bar),
                    .lvl_bar_exp_base   = this->blk_lvl_bar_exp_base
                };
            }

            auto get_internal_generic_optimizer_engine_config() -> GenericOptimizerEngineConfig
            {
                return GenericOptimizerEngineConfig
                {
                    .config = this->get_external_block_coordinated_search_optimizer_engine_config()
                };
            }

            auto get_external_generic_optimizer_engine_config() -> ExternalGenericOptimizerEngineConfig
            {
                return to_external_generic_optimizer_engine_config(this->get_internal_generic_optimizer_engine_config());
            }
    };
}

#endif