#ifndef __NETWORK_EXCEPTION_H__
#define __NETWORK_EXCEPTION_H__

//define HEADER_CONTROL 0

#include <common_exception/common_exception.h>

using exception_t = uint16_t; 

namespace dg_sock::network_exception
{
    using namespace common_exception;
}

#endif