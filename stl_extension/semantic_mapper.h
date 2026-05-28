#ifndef __STL_EXTENSION_SEMANTIC_MAPPER_H__
#define __STL_EXTENSION_SEMANTIC_MAPPER_H__

#include <serializer/compact_serializer.h>
#include "stdx.h"

namespace stdx
{
    template <class ToType, class FromType, class BufferContainer = std::string>
    __attribute__((noinline)) auto semantic_map(const FromType& fr_obj,
                                                const stdx::Tag<BufferContainer>& tag = stdx::Tag<BufferContainer>{}) -> ToType
    {
        auto str_data   = dg::network_compact_serializer::dgstd_serialize<BufferContainer>(fr_obj);
        ToType rs       = dg::network_compact_serializer::dgstd_deserialize<ToType>(str_data);

        return rs;
    }

    template <class T, class BufferContainer = std::string>
    class AutoMapperContainer
    {
        private:

            T obj;

        public:

            AutoMapperContainer(T obj): obj(std::move(obj)){}

            template <class U>
            operator U() const
            {
                return semantic_map<U>(obj, stdx::Tag<BufferContainer>{});
            }
    };

    template <class FromType, class BufferContainer = std::string>
    auto to_automap_object(FromType&& fr_obj,
                           const stdx::Tag<BufferContainer>& tag = stdx::Tag<BufferContainer>{}) -> AutoMapperContainer<std::decay_t<FromType>, BufferContainer>
    {
        return AutoMapperContainer<std::decay_t<FromType>, BufferContainer>(std::forward<FromType>(fr_obj));
    }
}

#endif