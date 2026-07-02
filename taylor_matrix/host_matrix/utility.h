#ifndef __TAYLOR_MATRIX_HOST_MATRIX_UTILITY_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_UTILITY_H__

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <type_traits>

namespace taylor_matrix::host_matrix::utility
{
    template <class T, std::enable_if_t<std::is_same_v<T, float>, bool> = true>
    auto intrinsic_abs_log(T x) -> intmax_t
    {
        return ilogbf(x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, double>, bool> = true>
    auto intrinsic_abs_log(T x) -> intmax_t
    {
        return ilogb(x);
    }

    template <class T, std::enable_if_t<std::is_same_v<T, long double>, bool> = true>
    auto intrinsic_abs_log(T x) -> intmax_t
    {
        return ilogbl(x);
    }
}

#endif