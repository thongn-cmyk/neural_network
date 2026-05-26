#ifndef __STL_EXTENSION_DEEP_COPY_H__
#define __STL_EXTENSION_DEEP_COPY_H__

#include <serializer/compact_serializer.h>
#include "stdx.h"

namespace stdx
{
    template <class T, class BufferContainer = std::string>
    __attribute__((noinline)) auto reflectible_deep_copy(const T& obj, const stdx::Tag<BufferContainer>& tag = stdx::Tag<BufferContainer>{}) -> T
    {
        auto str_data = dg::network_compact_serializer::serialize<BufferContainer>(obj);
        T rs = dg::network_compact_serializer::deserialize<T>(str_data);

        return rs;
    }
}

#endif