#ifndef __DG_NETWORK_COMPACT_TRIVIAL_SERIALIZER_H__
#define __DG_NETWORK_COMPACT_TRIVIAL_SERIALIZER_H__

#include "network_compact_serializer.h"
#include "network_trivial_serializer.h"

namespace dg_sock::network_compact_trivial_serializer{

    template <class T>
    struct is_byte_stream_container: std::false_type{};

    template <class ...Args>
    struct is_byte_stream_container<std::vector<char, Args...>>: std::true_type{};

    template <class ...Args>
    struct is_byte_stream_container<std::basic_string<char, Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_byte_stream_container_v = is_byte_stream_container<T>::value;

    template <class T>
    constexpr auto size(const T& obj) -> size_t{

        constexpr size_t TRIVIAL_SZ = dg_sock::network_trivial_serializer::size(obj);
        std::array<char, TRIVIAL_SZ> static_representation;

        return dg_sock::network_compact_serializer::dgstd_size(static_representation);
    }

    template <class T>
    constexpr auto serialize_into(char * buf, const T& obj, uint32_t secret = 0u) -> char *{

        constexpr size_t TRIVIAL_SZ = dg_sock::network_trivial_serializer::size(obj);
        std::array<char, TRIVIAL_SZ> static_representation{};

        dg_sock::network_trivial_serializer::serialize_into(static_representation.data(), obj);
        char * rs = dg_sock::network_compact_serializer::dgstd_serialize_into(buf, static_representation, secret);

        return rs;
    }

    template <class T>
    constexpr void deserialize_into(T& obj, const char * buf, size_t sz, uint32_t secret = 0u){

        constexpr size_t TRIVIAL_SZ = dg_sock::network_trivial_serializer::size(obj);
        std::array<char, TRIVIAL_SZ> static_representation;

        dg_sock::network_compact_serializer::dgstd_deserialize_into(static_representation, buf, sz, secret);
        dg_sock::network_trivial_serializer::deserialize_into(obj, static_representation.data());
    }

    template <class Stream, class T, std::enable_if_t<is_byte_stream_container_v<Stream>, bool> = true>
    constexpr auto serialize(const T& obj, uint32_t secret = 0u) -> Stream{

        Stream stream{};
        stream.resize(dg_sock::network_compact_trivial_serializer::size(obj));
        dg_sock::network_compact_trivial_serializer::serialize_into(stream.data(), obj, secret);

        return stream;
    }

    template <class T, class Stream, std::enable_if_t<is_byte_stream_container_v<Stream>, bool> = true>
    constexpr auto deserialize(const Stream& stream, uint32_t secret = 0u) -> T{

        T rs;
        dg_sock::network_compact_trivial_serializer::deserialize_into(rs, stream.data(), stream.size(), secret);

        return rs;
    }
} 

#endif