#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_RETRANSLATION_OPTIMIZER_TAIL_BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_RETRANSLATION_OPTIMIZER_TAIL_BLOCKED_SEARCH_OPTIMIZER_ENGINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include <matrix_optimizer_subsystem/matrix_optimizer_engine_interface.h>
#include <matrix/tensor_model.h>
#include <matrix/the_matrix_interface.h>
#include <common_exception/cancellation_token.h>
#include <common_exception/common_exception.h>
#include <vector>
#include "blocked_search_optimizer_engine.h"
#include <cmath>
#include <random>
#include <utility>
#include <stl_extension/stdx.h>
#include <bit>
#include <stl_extension/hasher.h>

namespace matrix_optimizer_subsystem
{
    //after we proved that this could work, we'd work on the "region" or ""

    class TailBlockedSearchOptimizerEngine: public virtual MatrixOptimizerEngineInterface
    {
        private:

            static_assert(sizeof(size_t) >= sizeof(uint64_t));

            std::shared_ptr<MatrixOptimizerEngineInterface> base;

            double exp_base;
            size_t x0;
            size_t iteration_sz;
            size_t lvl_bar0;
            double lvl_bar_exp_base;

            static inline constexpr double EXP_BASE_MIN         = 1;
            static inline constexpr double EXP_BASE_MAX         = 10;
            static inline constexpr double EXP_BASE_DEFAULT     = 2;

            static inline constexpr size_t X0_DEFAULT           = size_t{1} << 4;

            static inline constexpr size_t ITERATION_SZ_DEFAULT = size_t{1} << 7;

            static inline constexpr size_t LVL_BAR0_DEFAULT     = size_t{1} << 2;
            static inline constexpr double LVL_BAR_BASE_DEFAULT = 1;

            static inline constexpr size_t EXP_MAX              = size_t{1} << 50;

        public:

            TailBlockedSearchOptimizerEngine(std::shared_ptr<MatrixOptimizerEngineInterface> base_arg,
                                             double exp_base_arg,
                                             size_t x0_arg,
                                             size_t iteration_sz_arg,
                                             size_t lvl_bar0_arg,
                                             double lvl_bar_exp_base_arg)
            {
                if (base_arg == nullptr)
                {
                    throw std::invalid_argument("bad base argument, null");
                }

                if (std::isnan(exp_base_arg))
                {
                    exp_base_arg    = EXP_BASE_MIN;
                }

                exp_base_arg                = std::clamp(exp_base_arg, EXP_BASE_MIN, EXP_BASE_MAX);

                if (x0_arg == 0u)
                {
                    throw std::invalid_argument("bad x0, 0");
                }

                if (x0_arg > EXP_MAX)
                {
                    x0_arg  = EXP_MAX;
                }

                if (lvl_bar0_arg == 0u)
                {
                    throw std::invalid_argument("bad lvl bar, 0");
                }

                if (lvl_bar0_arg > EXP_MAX)
                {
                    lvl_bar0_arg = EXP_MAX;
                }

                if (std::isnan(lvl_bar_exp_base_arg))
                {
                    lvl_bar_exp_base_arg    = EXP_BASE_MIN;
                }

                lvl_bar_exp_base_arg        = std::clamp(lvl_bar_exp_base_arg, EXP_BASE_MIN, EXP_BASE_MAX);

                this->base                  = std::move(base_arg);
                this->exp_base              = exp_base_arg;
                this->x0                    = x0_arg;
                this->iteration_sz          = iteration_sz_arg;
                this->lvl_bar0              = lvl_bar0_arg;
                this->lvl_bar_exp_base      = lvl_bar_exp_base_arg;
            }

            TailBlockedSearchOptimizerEngine(std::shared_ptr<MatrixOptimizerEngineInterface> base_arg): TailBlockedSearchOptimizerEngine(std::move(base_arg),
                                                                                                                                         EXP_BASE_DEFAULT,
                                                                                                                                         X0_DEFAULT,
                                                                                                                                         ITERATION_SZ_DEFAULT,
                                                                                                                                         LVL_BAR0_DEFAULT,
                                                                                                                                         LVL_BAR_BASE_DEFAULT){}

            auto optimize(the_matrix::MatrixInterface& matrix,
                          matrix_evaluator::MatrixEvaluatorInterface& matrix_evaluator,
                          common_exception::CancellationTokenInterface& cancellation_token) -> std::shared_ptr<the_matrix::MatrixInterface>
            {
                size_t lvl_bar_counter  = 0u;
                size_t lvl_bar_goal     = this->lvl_bar0;
                size_t active_x         = this->x0;
                size_t matrix_sz        = matrix.get_coefficient_vector().size();

                std::shared_ptr<the_matrix::MatrixInterface> operating_matrix   = matrix.clone();

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (lvl_bar_counter == lvl_bar_goal)
                    {
                        active_x            = std::min(static_cast<size_t>(active_x * this->exp_base), EXP_MAX);
                        lvl_bar_goal        = std::min(static_cast<size_t>(lvl_bar_goal * this->lvl_bar_exp_base), EXP_MAX);
                        lvl_bar_counter     = 0u;
                    }

                    operating_matrix    = BlockedSearchOptimizerEngine(this->base,
                                                                       this->get_activation_vector(active_x, matrix_sz)).optimize(*operating_matrix, matrix_evaluator, cancellation_token);

                    lvl_bar_counter     += 1;
                }

                return operating_matrix;
            }

        private:

            auto get_activation_vector(size_t active_sz, size_t total_sz) -> std::vector<bool>
            {
                std::vector<bool> rs(total_sz, false);
                size_t actual_active_sz = std::min(active_sz, total_sz);
                std::fill(rs.begin(), std::next(rs.begin(), actual_active_sz), true);
                
                return rs;
            }
    };
}


#endif