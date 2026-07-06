#ifndef __TAYLOR_MATRIX_HOST_MATRIX_CUBIC_QUANTIZATION_MACHINE_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_CUBIC_QUANTIZATION_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include <float.h>
#include <assert.h>

namespace taylor_matrix::host_matrix::cubic_quantization_machine
{
    //this is so fundamentally important that without this our equation could not run
    //

    class StandardCubicInterpolationExponentialQuantizationMachine
    {
        private:

            static inline constexpr intmax_t FIRST_LOG_VALUE    = -8;
            static inline constexpr intmax_t LAST_LOG_VALUE     = 8;

            static inline constexpr size_t HALF_QUANTIZATION_SZ = 16;
            static inline constexpr size_t QUANTIZATION_SZ      = 32;

        public:

            template <class T>
            constexpr auto quantitize(T x) const noexcept -> size_t
            {
                intmax_t tentative_value = taylor_matrix::host_matrix::utility::intrinsic_abs_log(x);
                intmax_t clamped_value   = std::min(std::max(tentative_value, FIRST_LOG_VALUE),
                                                    static_cast<intmax_t>(LAST_LOG_VALUE - 1));

                intmax_t negflag    = static_cast<intmax_t>(std::signbit(x));

                intmax_t offset_neg = (LAST_LOG_VALUE - 1) - clamped_value;
                intmax_t offset_pos = clamped_value - FIRST_LOG_VALUE;

                size_t offset       = static_cast<size_t>(negflag * offset_neg + (1 - negflag) * offset_pos);

                // negative half occupies low indices [0, HALF), positive half occupies [HALF, SZ)
                size_t slot         = static_cast<size_t>(1 - negflag);

                size_t idx          = slot * HALF_QUANTIZATION_SZ + offset;

                return idx;
            }

            constexpr auto quantization_size() const noexcept -> size_t
            {
                return QUANTIZATION_SZ;
            }

            template <class T = float>
            constexpr auto region_first(size_t idx) const noexcept -> T
            {
                static_assert(std::is_floating_point_v<T>);

                const intmax_t half     = static_cast<intmax_t>(idx / HALF_QUANTIZATION_SZ); // 0 = neg half, 1 = pos half
                const intmax_t offset   = static_cast<intmax_t>(idx % HALF_QUANTIZATION_SZ);
                const intmax_t negflag  = 1 - half;

                const intmax_t exp_neg  = (LAST_LOG_VALUE - 1) - offset;
                const intmax_t exp_pos  = FIRST_LOG_VALUE + offset;
                const intmax_t exp      = negflag * exp_neg + (1 - negflag) * exp_pos;

                const T lo      = std::scalbn(static_cast<T>(1), static_cast<int>(exp));
                const T hi      = std::scalbn(static_cast<T>(1), static_cast<int>(exp + 1));
                const T negflag_f = static_cast<T>(negflag);

                // negative half -> -hi (lower bound of the negative bucket)
                // positive half -> lo
                return negflag_f * (-hi) + (T{1} - negflag_f) * lo;
            }

            template <class T = float>
            constexpr auto region_last(size_t idx) const noexcept -> T
            {
                static_assert(std::is_floating_point_v<T>);

                const intmax_t half     = static_cast<intmax_t>(idx / HALF_QUANTIZATION_SZ);
                const intmax_t offset   = static_cast<intmax_t>(idx % HALF_QUANTIZATION_SZ);
                const intmax_t negflag  = 1 - half;

                const intmax_t exp_neg = (LAST_LOG_VALUE - 1) - offset;
                const intmax_t exp_pos = FIRST_LOG_VALUE + offset;
                const intmax_t exp     = negflag * exp_neg + (1 - negflag) * exp_pos;

                const T lo      = std::scalbn(static_cast<T>(1), static_cast<int>(exp));
                const T hi      = std::scalbn(static_cast<T>(1), static_cast<int>(exp + 1));
                const T negflag_f = static_cast<T>(negflag);

                // negative half -> -lo (upper bound of the negative bucket)
                // positive half -> hi
                return negflag_f * (-lo) + (T{1} - negflag_f) * hi;
            }
    };

    template <class FloatType = double>
    class GenericCubicInterpolationExponentialQuantizationMachine
    {
        private:

            size_t discretization_sz;
            FloatType exp_base;
            FloatType multiplier_base;

        public:

            constexpr GenericCubicInterpolationExponentialQuantizationMachine(size_t discretization_sz,
                                                                              FloatType exp_base,
                                                                              FloatType multiplier_base)
            {
                if (!stdx::is_pow2(discretization_sz))
                {
                    throw std::invalid_argument("bad discretization size, not pow 2");
                }

                if (discretization_sz < 4)
                {
                    throw std::invalid_argument("bad discretization size, < 4");
                }

                if (std::isnan(exp_base))
                {
                    throw std::invalid_argument("bad exp base, NaN");
                }

                if (exp_base <= 1)
                {
                    throw std::invalid_argument("bad exp base, <= 1");
                }

                if (std::isnan(multiplier_base))
                {
                    throw std::invalid_argument("bad multiplier base, NaN");
                }

                if (multiplier_base <= 0)
                {
                    throw std::invalid_argument("bad multiplier base, <= 0");
                }

                this->discretization_sz = discretization_sz;
                this->exp_base          = exp_base;
                this->multiplier_base   = multiplier_base;
            }

            constexpr auto quantitize(FloatType x_arg) const noexcept -> size_t
            {
                const intmax_t FIRST_EXPONENT       = -static_cast<intmax_t>(this->discretization_sz / 4);
                const intmax_t LAST_EXPONENT        = static_cast<intmax_t>(this->discretization_sz / 4);
                const size_t HALF_SZ                = this->discretization_sz / 2;

                FloatType x                         = std::abs(x_arg);
                FloatType y                         = x / this->multiplier_base;
                FloatType exponent                  = std::log(y) / std::log(this->exp_base);
                FloatType upround                   = std::ceil(exponent);

                //it's fine if we treat 0 as a special bounded value, either in the positive or negative range
                //recall that this is a relative operation, we simply dont have the instrument to perfect all of these guys

                intmax_t upround_i                  = static_cast<intmax_t>(upround);
                intmax_t slot_i                     = upround_i - 1;
                intmax_t actual_slot_i              = std::min(std::max(slot_i, FIRST_EXPONENT), LAST_EXPONENT - 1);

                size_t signed_normalized_slot_i     = actual_slot_i - FIRST_EXPONENT;

                if (x_arg < 0)
                {
                    return HALF_SZ - signed_normalized_slot_i - 1;
                }
                else
                {
                    return HALF_SZ + signed_normalized_slot_i;
                }
            }

            constexpr auto quantization_size() const noexcept -> size_t
            {
                return this->discretization_sz;
            }

            //relative, not strong-guarantee
            constexpr auto region_first(size_t idx) const noexcept -> FloatType
            {
                const auto [is_negative, exponent] = slot_of(idx);

                const FloatType lo = this->multiplier_base * std::pow(this->exp_base, static_cast<FloatType>(exponent));
                const FloatType hi = this->multiplier_base * std::pow(this->exp_base, static_cast<FloatType>(exponent + 1));

                return is_negative ? -hi : lo;
            }

            //relative, not strong-guanratee
            constexpr auto region_last(size_t idx) const noexcept -> FloatType
            {
                const auto [is_negative, exponent] = slot_of(idx);

                const FloatType lo = this->multiplier_base * std::pow(this->exp_base, static_cast<FloatType>(exponent));
                const FloatType hi = this->multiplier_base * std::pow(this->exp_base, static_cast<FloatType>(exponent + 1));

                return is_negative ? -lo : hi;
            }

        private:

            constexpr auto slot_of(size_t idx) const noexcept -> std::pair<bool, intmax_t>
            {
                assert(idx < this->discretization_sz);

                const intmax_t FIRST_EXPONENT   = -static_cast<intmax_t>(this->discretization_sz / 4);
                const size_t   HALF_SZ          =  this->discretization_sz / 2;
                const bool is_negative          = idx < HALF_SZ;

                const intmax_t signed_normalized_slot_i = is_negative
                    ? static_cast<intmax_t>(HALF_SZ) - static_cast<intmax_t>(idx) - 1
                    : static_cast<intmax_t>(idx) - static_cast<intmax_t>(HALF_SZ);

                return {is_negative, FIRST_EXPONENT + signed_normalized_slot_i};
            }
    };

}

#endif