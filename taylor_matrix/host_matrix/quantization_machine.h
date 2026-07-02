#ifndef __TAYLOR_MATRIX_HOST_MATRIX_EXPONENTIAL_QUANTIZATION_MACHINE_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_EXPONENTIAL_QUANTIZATION_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include "utility.h"
#include <float.h>

namespace taylor_matrix::host_matrix::quantization_machine
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

class StandardCubicInterpolationUniformQuantizationMachine
{
    private:

        static inline constexpr double RANGE_FIRST      = -1;
        static inline constexpr double RANGE_LAST       = 1;

        static inline constexpr size_t QUANTIZATION_SZ  = 32;

        static inline constexpr double STEP = (RANGE_LAST - RANGE_FIRST)
                                             / static_cast<double>(QUANTIZATION_SZ);

    public:

        template <class T>
        constexpr auto quantitize(T x) const noexcept -> size_t
        {
            static_assert(std::is_floating_point_v<T>);

            // normalize into [0, QUANTIZATION_SZ) as a real number
            double normalized = (static_cast<double>(x) - RANGE_FIRST) / STEP;

            // clamp to [0, QUANTIZATION_SZ - 1] in the *continuous* domain first,
            // so flooring afterward can't push an in-range value out of bounds
            double clamped = std::min(std::max(normalized, 0.0),
                                       static_cast<double>(QUANTIZATION_SZ - 1));

            return std::min(static_cast<size_t>(clamped),
                            static_cast<size_t>(QUANTIZATION_SZ - 1));
        }

        constexpr auto quantization_size() const noexcept -> size_t
        {
            return QUANTIZATION_SZ;
        }

        template <class T = float>
        constexpr auto region_first(size_t idx) const noexcept -> T
        {
            static_assert(std::is_floating_point_v<T>);

            return static_cast<T>(RANGE_FIRST + static_cast<double>(idx) * STEP);
        }

        template <class T = float>
        constexpr auto region_last(size_t idx) const noexcept -> T
        {
            static_assert(std::is_floating_point_v<T>);

            return static_cast<T>(RANGE_FIRST + static_cast<double>(idx + 1) * STEP);
        }
};
}

#endif