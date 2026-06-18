#ifndef __STL_EXTENSION_HASHER_H__
#define __STL_EXTENSION_HASHER_H__

//define HEADER_CONTROL 1 

#include <functional>
#include <serializer/trivial_serializer.h>
#include <stdint.h>
#include <stdlib.h>
#include <bit>
#include "sip_hasher.h"
#include "murmur_hasher.h"

namespace hasher
{
    constexpr auto murmur_hash(const char * buf, size_t len, const uint32_t seed = 0xFF) -> uint64_t
    {
        return murmur_hasher::murmur_hash(buf, len, seed);
    }

    template <size_t LEN, size_t SEED = 0xFF>
    constexpr auto murmur_hash(const char * buf, const std::integral_constant<size_t, LEN> len, const std::integral_constant<size_t, SEED> seed = std::integral_constant<size_t, SEED>{}) -> uint64_t
    {
        return murmur_hasher::murmur_hash(buf, len, seed);
    }

    constexpr auto sip_hash(const char * buf, size_t len, const uint32_t seed = 0xFF) -> uint64_t
    {
        return sip_hasher::SipHasher::hash(static_cast<__uint128_t>(seed), buf, len);
    }

    template <size_t LEN, size_t SEED = 0xFF>
    constexpr auto sip_hash(const char * buf, const std::integral_constant<size_t, LEN> len, const std::integral_constant<size_t, SEED> seed = std::integral_constant<size_t, SEED>{}) -> uint64_t
    {
        return sip_hasher::SipHasher::hash(static_cast<__uint128_t>(SEED), buf, len);
    }

    constexpr auto hash_bytes(const char * inp, size_t n) noexcept -> uint64_t
    {
        return sip_hash(inp, n);
    }

    constexpr auto hash_bytes(const char * inp, size_t n, uint32_t secret) noexcept -> uint64_t
    {
        return sip_hash(inp, n, secret);
    }

    template <size_t N>
    constexpr auto hash_bytes(const char * inp, const std::integral_constant<size_t, N> n) noexcept -> uint64_t
    {
        return sip_hash(inp, n);
    }

    template <class T>
    constexpr auto hash_reflectible(const T& obj) noexcept -> uint64_t
    {
        constexpr size_t MAX_REFLECTIBLE_SZ = size_t{1} << 8; //
        
        constexpr size_t SERIALIZATION_SZ = trivial_serializer::size(T{});
        static_assert(SERIALIZATION_SZ <= MAX_REFLECTIBLE_SZ);

        std::array<char, SERIALIZATION_SZ> buf{};
        trivial_serializer::serialize_into(buf.data(), obj); //stack overflow

        return hash_bytes(buf.data(), std::integral_constant<size_t, SERIALIZATION_SZ>{});
    }

    template <class T>
    constexpr auto hash_reflectible(const T& obj, uint32_t secret)
    {
        constexpr size_t MAX_REFLECTIBLE_SZ = size_t{1} << 8; //        
        constexpr size_t SERIALIZATION_SZ = trivial_serializer::size(T{});
        static_assert(SERIALIZATION_SZ <= MAX_REFLECTIBLE_SZ);

        std::array<char, SERIALIZATION_SZ> buf{};
        trivial_serializer::serialize_into(buf.data(), obj); //stack overflow

        return hash_bytes(buf.data(), SERIALIZATION_SZ, secret);
    }

    template <class T>
    using std_hasher = std::hash<T>;

    template <class T, class = void>
    struct is_std_hashable: std::false_type{};

    template <class T>
    struct is_std_hashable<T, std::void_t<decltype(std::declval<std::hash<T>&>()(std::declval<const T&>()))>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_std_hashable_v = is_std_hashable<T>::value;

    template <class T, std::enable_if_t<trivial_serializer::is_serializable_v<T>, bool> = true>
    struct trivial_reflectable_hasher
    {
        constexpr auto operator()(const T& value) const noexcept -> size_t
        {
            constexpr size_t SZ = trivial_serializer::size(T{});  
            std::array<char, SZ> buf = {};
            trivial_serializer::serialize_into(buf.data(), value);

            return hasher::hash_bytes(buf.data(), std::integral_constant<size_t, SZ>{});
        }
    };

    template <class T>
    static inline constexpr bool is_trivial_hashable_v = trivial_serializer::is_serializable_v<T>;

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;

    template <class T>
    struct type_container
    {
        using type = T;
    };

    template <>
    struct type_container<void>{};

    template <class T>
    auto internal_default_hasher_chooser()
    {
        if constexpr(is_std_hashable_v<T>)
        {
            return type_container<std_hasher<T>>();
        }
        else if constexpr(is_trivial_hashable_v<T>)
        {
            return type_container<trivial_reflectable_hasher<T>>();
        } else
        {
            return type_container<void>();
        }
    }

    template <class T>
    using default_hasher = typename decltype(internal_default_hasher_chooser<T>())::type;

    template <class T>
    using std_equal_to = std::equal_to<T>;

    template <class T, class = void>
    struct is_std_equal_to_able: std::false_type{};

    template <class T>
    struct is_std_equal_to_able<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_std_equal_to_able_v = is_std_equal_to_able<T>::value;

    template <class T, std::enable_if_t<trivial_serializer::is_serializable_v<T>, bool> = true>
    struct trivial_reflectable_equal_to{

        constexpr auto operator()(const T& lhs, const T& rhs) const noexcept -> bool
        {
            return trivial_serializer::reflectible_is_equal(lhs, rhs);
        }
    };

    template <class T>
    static inline constexpr bool is_trivial_equal_to_able_v = trivial_serializer::is_serializable_v<T>;

    template <class T>
    auto internal_default_equal_to_chooser(){

        if constexpr(is_std_equal_to_able_v<T>)
        {
            return type_container<std_equal_to<T>>();
        }
        else if constexpr(is_trivial_equal_to_able_v<T>)
        {
            return type_container<trivial_reflectable_equal_to<T>>();
        }
        else
        {
            return type_container<void>();
        }
    }

    template <class T>
    using default_equal_to = typename decltype(internal_default_equal_to_chooser<T>())::type;
} 

#endif