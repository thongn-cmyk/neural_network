#ifndef __TAYLOR_MATRIX_HOST_MATRIX_TAYLOR_EXCEPTION_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_TAYLOR_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>

namespace taylor_matrix::host_matrix::local_exception
{
    struct insufficient_logit_vec_size: std::invalid_argument
    {
        insufficient_logit_vec_size(): std::invalid_argument("insufficient logit vec size"){}
    };
}

#endif