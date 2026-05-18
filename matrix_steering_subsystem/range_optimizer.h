#ifndef __EXPONENTIAL_RANGE_OPTIMIZER_H__
#define __EXPONENTIAL_RANGE_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include "branch_optimizer.h"
#include <stl_extension/stdx.h>

namespace range_optimizer
{
    using branch_float_t = long double;

    class RangePredictionResultInterface
    {
        public:

            virtual ~RangePredictionResultInterface() = default;

            virtual auto get_range() -> size_t = 0;
            virtual void feedback(branch_float_t score) = 0;
    };

    class RangePredictorInterface
    {
        public:

            virtual ~RangePredictorInterface() noexcept = default;

            virtual auto next() -> std::unique_ptr<RangePredictionResultInterface> = 0;
            virtual auto size() -> size_t = 0;
    };

    class ExponentialRangePredictor: public virtual RangePredictorInterface
    {
        private:

            std::unique_ptr<branch_optimizer::BranchPredictorInterface> branch_predictor;
            size_t sz;

            static inline constexpr size_t EXP_BASE = 2u;

        public:

            ExponentialRangePredictor(size_t sz): branch_predictor(branch_optimizer::BranchPredictorFactory::get_square_branch_predictor(stdx::ulog2(stdx::ceil2(sz)) + 1u)),
                                                  sz(sz){}

            auto next() -> std::unique_ptr<RangePredictionResultInterface>
            {
                std::unique_ptr<branch_optimizer::BranchPredictionResultInterface> branch_prediction_rs = this->branch_predictor->next();

                size_t tentative_sz = size_t{1} << branch_prediction_rs->get_enumeration();
                size_t actual_sz    = std::min(tentative_sz, this->sz);

                return std::make_unique<InternalPredictionResult>
                (
                    actual_sz,
                    std::move(branch_prediction_rs)
                );
            }

            auto size() -> size_t
            {
                return this->sz;
            }
        
        private:

            class InternalPredictionResult: public virtual RangePredictionResultInterface
            {
                private:

                    size_t predicted_sz;
                    std::unique_ptr<branch_optimizer::BranchPredictionResultInterface> feedbackable;
                
                public:

                    InternalPredictionResult(size_t predicted_sz,
                                             std::unique_ptr<branch_optimizer::BranchPredictionResultInterface> feedbackable) noexcept: predicted_sz(predicted_sz),
                                                                                                                                        feedbackable(std::move(feedbackable)){}

                    auto get_range() -> size_t
                    {
                        return this->predicted_sz;
                    }

                    void feedback(branch_float_t score)
                    {
                        this->feedbackable->feedback(score);
                    }
            };
    };
}

#endif