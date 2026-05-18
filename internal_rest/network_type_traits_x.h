#ifndef __NETWORK_TYPE_TRAITS_X_H__
#define __NETWORK_TYPE_TRAITS_X_H__

//define HEADER_CONTROL 0 

#include <type_traits>
#include <tuple>
#include <stdint.h>
#include <stddef.h>
#include <utility>
#include <expected>
#include <string>
#include <optional>

namespace dg_sock::network_type_traits_x{
    
    template <class ...Args>
    struct tags{};

    struct empty{}; 

    template <class T>
    struct base_type: std::enable_if<true, T>{};

    template <class T>
    struct base_type<const T>: base_type<T>{};

    template <class T>
    struct base_type<volatile T>: base_type<T>{};

    template <class T>
    struct base_type<T&>: base_type<T>{};

    template <class T>
    struct base_type<T&&>: base_type<T>{};

    template <class T>
    using base_type_t = typename base_type<T>::type;
    
    template <class T, class = void>
    struct is_base_type: std::false_type{}; 

    template <class T>
    struct is_base_type<T, std::void_t<std::enable_if_t<std::is_same_v<T, base_type_t<T>>>>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_base_type_v = is_base_type<T>::value;

    template <class T, class = void>
    struct is_tuple: std::false_type{};

    template <class T>
    struct is_tuple<T, std::void_t<decltype(std::tuple_size<T>::value)>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_tuple_v = is_tuple<T>::value;

    template <class T>
    struct is_basic_string: std::false_type{};

    template <class ...Args>
    struct is_basic_string<std::basic_string<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_basic_string_v = is_basic_string<T>::value;

    template <class T>
    struct mono_reduction_type_helper{};

    template <class First, class Second, class ...Args>
    struct mono_reduction_type_helper<tags<First, Second, Args...>>: std::conditional_t<std::is_same_v<First, Second>, 
                                                                                        mono_reduction_type_helper<tags<Second, Args...>>,
                                                                                        empty>{}; //this is not sfinae - consider std::void_t<> - this is a bad hack

    template <class T>
    struct remove_expected{};

    template <class T, class err>
    struct remove_expected<std::expected<T, err>>{
        using type = T;
    };

    template <class T>
    using remove_expected_t = typename remove_expected<T>::type; 

    template <class T>
    struct mono_reduction_type_helper<tags<T>>: std::enable_if<true, T>{};

    template <class ...Args>
    struct mono_reduction_type: mono_reduction_type_helper<tags<Args...>>{};

    template <class ...Args>
    using mono_reduction_type_t = typename mono_reduction_type<Args...>::type; 

    template <size_t BYTE_LENGTH>
    struct unsigned_of_byte{};

    template <>
    struct unsigned_of_byte<1>{
        using type = uint8_t;
    };

    template <>
    struct unsigned_of_byte<2>{
        using type = uint16_t;
    };

    template <>
    struct unsigned_of_byte<4>{
        using type = uint32_t;
    };

    template <>
    struct unsigned_of_byte<8>{
        using type = uint64_t;
    };

    template <size_t BYTE_LENGTH>
    using unsigned_of_byte_t = typename unsigned_of_byte<BYTE_LENGTH>::type;

    template <class T>
    struct remove_optional{};

    template <class T>
    struct remove_optional<std::optional<T>>{
        using type = T;
    };

    template <class T>
    using remove_optional_t = typename remove_optional<T>::type;
}

#endif