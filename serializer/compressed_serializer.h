#ifndef __COMPRESSED_SERIALIZER_H__
#define __COMPRESSED_SERIALIZER_H__

#include "huffman_encoder.h"
#include "compact_serializer.h"
#include <variant>

namespace compressed_serializer
{
    struct corrupted_format: std::invalid_argument
    {
        corrupted_format(): std::invalid_argument("serializable object is corrupted"){}
    };

    template <class Streamable>
    struct NormalSerializable
    {
        Streamable data;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data);
        }
    };

    template <class Streamable>
    struct HuffmanSerializable
    {
        Streamable data;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data);
        }
    };

    template <class Streamable>
    struct GenericSerializable
    {
        std::variant<stdx::reflectible_monostate, NormalSerializable<Streamable>, HuffmanSerializable<Streamable>> data;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data);
        }
    };

    template <class Streamable, class T>
    auto huffman_serialize(const T& obj) -> HuffmanSerializable<Streamable>
    {
        return HuffmanSerializable<Streamable>
        {
            .data = dg::network_huffman_encoder::encode(dg::network_compact_serializer::dgstd_serialize<Streamable>(obj))
        };
    }

    template <class T, class Streamable>
    auto huffman_deserialize(const HuffmanSerializable<Streamable>& obj) -> T
    {
        return dg::network_compact_serializer::dgstd_deserialize<T>(dg::network_huffman_encoder::decode(obj.data));
    }

    template <class Streamable, class T>
    auto normal_serialize(const T& obj) -> NormalSerializable<Streamable>
    {
        return NormalSerializable<Streamable>
        {
            .data = dg::network_compact_serializer::dgstd_serialize<Streamable>(obj)
        };
    }

    template <class T, class Streamable>
    auto normal_deserialize(const NormalSerializable<Streamable>& obj) -> T
    {
        return dg::network_compact_serializer::dgstd_deserialize<T>(obj.data);
    }

    template <class Streamable>
    auto to_generic(HuffmanSerializable<Streamable> data) -> GenericSerializable<Streamable>
    {
        return GenericSerializable<Streamable>
        {
            .data = std::move(data)
        };
    }

    template <class Streamable>
    auto to_generic(NormalSerializable<Streamable> data) -> GenericSerializable<Streamable>
    {
        return GenericSerializable<Streamable>
        {
            .data = std::move(data)
        };
    }

    template <class Streamable>
    auto to_str(const GenericSerializable<Streamable>& data) -> Streamable
    {
        return dg::network_compact_serializer::dgstd_serialize<Streamable>(data);
    }

    template <class Streamable, class T>
    auto best_serialize(const T& obj) -> GenericSerializable<Streamable>
    {
        HuffmanSerializable<Streamable> opt_0   = huffman_serialize<Streamable>(obj);
        NormalSerializable<Streamable> opt_1    = normal_serialize<Streamable>(obj);

        if (opt_0.data.size() < opt_1.data.size())
        {
            return to_generic(std::move(opt_0));
        }
        else
        {
            return to_generic(std::move(opt_1));
        }
    }

    template <class T, class Streamable>
    auto deserialize(const Streamable& streamable) -> T
    {
        GenericSerializable<Streamable> semantic_serializable = dg::network_compact_serializer::dgstd_deserialize<GenericSerializable<Streamable>>(streamable);

        if (std::holds_alternative<NormalSerializable<Streamable>>(semantic_serializable.data))
        {
            return normal_deserialize<T>(std::get<NormalSerializable<Streamable>>(semantic_serializable.data));
        }
        else if (std::holds_alternative<HuffmanSerializable<Streamable>>(semantic_serializable.data))
        {
            return huffman_deserialize<T>(std::get<HuffmanSerializable<Streamable>>(semantic_serializable.data));
        }
        else
        {
            throw corrupted_format();
        }
    }
}

#endif