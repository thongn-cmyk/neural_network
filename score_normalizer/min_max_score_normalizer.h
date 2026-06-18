#ifndef __SCORE_NORMALIZER_MIN_MAX_SCORE_NORMALIZER_H__
#define __SCORE_NORMALIZER_MIN_MAX_SCORE_NORMALIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>

namespace score_normalizer
{
    template <class FloatType>
    class MinMaxScoreNormalizer
    {
        private:

            std::optional<FloatType> min_value;
            std::optional<FloatType> max_value;
        
        public:

            MinMaxScoreNormalizer(): min_value(std::nullopt),
                                     max_value(std::nullopt){}

            auto normalize(FloatType x) -> FloatType
            {
                if (std::isnan(x))
                {
                    return x;
                }

                if (!this->min_value.has_value())
                {
                    this->min_value = x;
                }

                if (this->min_value.value() > x)
                {
                    this->min_value = x;
                }

                if (!this->max_value.has_value())
                {
                    this->max_value = x;
                }

                if (this->max_value.value() < x)
                {
                    this->max_value = x;
                }

                FloatType tentative_value   = (x - this->min_value.value()) / (this->max_value.value() - this->min_value.value());

                if (std::isnan(tentative_value))
                {
                    tentative_value = this->min_value.value();
                }

                return std::clamp(tentative_value, this->min_value.value(), this->max_value.value());
            }
    };
}

#endif