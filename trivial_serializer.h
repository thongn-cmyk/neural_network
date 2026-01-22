#ifndef __DG_NETWORK_TRIVIAL_SERIALIZER_H__
#define __DG_NETWORK_TRIVIAL_SERIALIZER_H__

//define HEADER_CONTROL 0 

#include <climits>
#include <bit>
#include <optional>
#include <numeric>
#include <cstring>
#include <cstdint>
#include <tuple>
#include <variant>

namespace trivial_serializer::constants{

    static constexpr auto endianness = std::endian::little;
}

namespace trivial_serializer::types{

    using size_type = uint64_t;
    using variant_index_type = uint8_t;
}

namespace trivial_serializer::types_space{

    static constexpr auto nil_lambda = []<class ...Args>(Args&&...){}; 

    template <class T, class = void>
    struct is_tuple: std::false_type{};
    
    template <class T>
    struct is_tuple<T, std::void_t<decltype(std::tuple_size<T>::value)>>: std::true_type{};

    template <class T>
    struct is_optional: std::false_type{};

    template <class ...Args>
    struct is_optional<std::optional<Args...>>: std::true_type{}; 

    template <class T, class = void>
    struct is_reflectible: std::false_type{};

    template <class T>
    struct is_reflectible<T, std::void_t<decltype(std::declval<T>().dg_reflect(nil_lambda))>>: std::true_type{};
    
    template <class T, class = void>
    struct is_dg_arithmetic: std::is_arithmetic<T>{};

    template <class T>
    struct is_dg_arithmetic<T, std::void_t<std::enable_if_t<std::is_floating_point_v<T>>>>: std::bool_constant<std::numeric_limits<T>::is_iec559>{}; 

    template <class T, class = void>
    struct is_variant: std::false_type{};

    template <class ...Args>
    struct is_variant<std::variant<Args...>>: std::true_type{};

    template <class T>
    static constexpr bool is_tuple_v            = is_tuple<T>::value; 

    template <class T>
    static constexpr bool is_optional_v         = is_optional<T>::value;

    template <class T>
    static constexpr bool is_reflectible_v      = is_reflectible<T>::value;

    template <class T>
    static constexpr bool is_dg_arithmetic_v    = is_dg_arithmetic<T>::value;

    template <class T>
    static constexpr bool is_variant_v          = is_variant<T>::value;

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

    template <class T>
    using optional_containee_t = typename T::value_type;
}

namespace trivial_serializer::utility{

    using namespace trivial_serializer::types;

    template <size_t N>
    static constexpr void memcpy(char * dst, const char * src, const std::integral_constant<size_t, N>){

        [=]<size_t ...IDX>(const std::index_sequence<IDX...>){
            ((dst[IDX] = src[IDX]), ...);
        }(std::make_index_sequence<N>());
    }  

    struct SyncedEndiannessService{
        
        static constexpr auto is_native_big      = bool{std::endian::native == std::endian::big};
        static constexpr auto is_native_little   = bool{std::endian::native == std::endian::little};
        static constexpr auto precond            = bool{(is_native_big ^ is_native_little) != 0};
        static constexpr auto deflt              = constants::endianness; 
        static constexpr auto native_uint8       = is_native_big ? uint8_t{0} : uint8_t{1}; 

        static_assert(precond); //xor

        template <class T, std::enable_if_t<std::disjunction_v<std::is_integral<T>, std::is_floating_point<T>>, bool> = true>
        static constexpr T bswap(T value){
            
            auto src = std::array<char, sizeof(T)>{};
            auto dst = std::array<char, sizeof(T)>{};
            const auto idx_seq  = std::make_index_sequence<sizeof(T)>();
            
            src = std::bit_cast<decltype(src)>(value);
            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                ((dst[IDX] = src[sizeof(T) - IDX - 1]), ...);
            }(idx_seq);
            value = std::bit_cast<T>(dst);

            return value;
        }

        template <class T, std::enable_if_t<std::disjunction_v<std::is_integral<T>, std::is_floating_point<T>>, bool> = true>
        static constexpr void dump(char * dst, T data) noexcept{    

            if constexpr(std::endian::native != deflt){
                data = bswap(data);
            }

            std::array<char, sizeof(T)> data_buf{};
            data_buf = std::bit_cast<decltype(data_buf)>(data);
            memcpy(dst, data_buf.data(), std::integral_constant<size_t, sizeof(T)>());
        }

        template <class T, std::enable_if_t<std::disjunction_v<std::is_integral<T>, std::is_floating_point<T>>, bool> = true>
        static constexpr T load(const char * src) noexcept{
            
            std::array<char, sizeof(T)> tmp{};
            memcpy(tmp.data(), src, std::integral_constant<size_t, sizeof(T)>());
            T rs = std::bit_cast<T>(tmp);

            if constexpr(std::endian::native != deflt){
                rs = bswap(rs);
            }

            return rs;
        }
    };

    template <class First, class ...Args>
    static constexpr auto max_many(First first, Args... args) -> First
    {
        First result = first;
        std::array<First, sizeof...(Args)> second_arr{args...}; 

        for (size_t i = 0u; i < sizeof...(Args); ++i){
            result = std::max(result, second_arr[i]);
        }

        return result;
    }
}

namespace trivial_serializer::archive{
    
    struct IsSerializable{

        template <class T>
        constexpr auto is_serializable(T&& data) const noexcept -> bool{

            using btype = types_space::base_type_t<T>;

            if constexpr(types_space::is_dg_arithmetic_v<btype>){
                return true;
            } else if constexpr(types_space::is_optional_v<btype>){
                using containee_t = types_space::optional_containee_t<btype>; 

                if constexpr(std::is_default_constructible_v<containee_t>){
                    return IsSerializable().is_serializable(containee_t());
                } else{
                    return false;
                }
            } else if constexpr(types_space::is_tuple_v<btype>){
                return [&]<size_t ...IDX>(const std::index_sequence<IDX...>) noexcept{
                    return (IsSerializable{}.is_serializable(std::get<IDX>(data)) && ...);
                }(std::make_index_sequence<std::tuple_size_v<btype>>{});
            } else if constexpr(types_space::is_reflectible_v<btype>){
                if constexpr(std::is_default_constructible_v<btype>){
                    bool rs = true;
                    auto archiver = [&rs]<class ...Args>(Args&& ...args) noexcept{
                        rs &= (IsSerializable().is_serializable(std::forward<Args>(args)) && ...);
                    };
                    data.dg_reflect(archiver);
                    return rs;
                } else{
                    return false;
                }
            } else if constexpr(types_space::is_variant_v<btype>){
                bool rs = true;

                [&]<class ...Args>(const std::variant<Args...>&) noexcept{
                    (
                        [&]{
                            if constexpr(std::is_default_constructible_v<Args>){
                                rs &= IsSerializable().is_serializable(Args());
                            } else{
                                rs &= false;
                            }
                        }(), ...
                    );
                }(data);

                return rs;
            } else {
                return false;
            }
        }
    };

    struct Counter{

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            return sizeof(types_space::base_type_t<T>);
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            using value_type = typename types_space::base_type_t<T>::value_type;
            return count(bool{}) + count(value_type());
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            using btype         = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<btype>>{};

            return []<size_t ...IDX>(T&& data, const std::index_sequence<IDX...>) noexcept{
                return (Counter{}.count(std::get<IDX>(data)) + ...);
            }(std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            size_t rs{};
            auto archiver = [&rs]<class ...Args>(Args&& ...args) noexcept{
                rs += (Counter{}.count(std::forward<Args>(args)) + ...);
            };

            // static_assert(noexcept(data.dg_reflect(archiver)));
            data.dg_reflect(archiver);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            using btype = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<btype>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u; 

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            size_t rs = count(types::variant_index_type{});

            [&]<class ...Args>(const std::variant<Args...>&) noexcept{
                rs += utility::max_many(Counter{}.count(Args())...);
            }(data);

            return rs;
        }
    };

    struct Forward{

        using Self = Forward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            using _MemIO = utility::SyncedEndiannessService;
            _MemIO::dump(buf, data);
            buf += sizeof(types_space::base_type_t<T>);
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            using value_type = typename types_space::base_type_t<T>::value_type;
            char * tmp = buf;
            put(tmp, static_cast<bool>(data.has_value()));

            if (data){
                put(tmp, data.value());
            }

            buf += Counter{}.count(data);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            using btype = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<btype>>{};

            []<size_t ...IDX>(char *& buf, T&& data, const std::index_sequence<IDX...>) noexcept{
                (Self{}.put(buf, std::get<IDX>(data)), ...);
            }(buf, std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            auto archiver = [&buf]<class ...Args>(Args&& ...args) noexcept{
                (Self{}.put(buf, std::forward<Args>(args)), ...);
            };

            // static_assert(noexcept(data.dg_reflect(archiver)));
            data.dg_reflect(archiver);
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            using btype = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<btype>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            auto variant_idx = data.index();
            char * tmp = buf;

            // if constexpr(DEBUG_MODE_FLAG){
            //     if (variant_idx == std::variant_npos){
            //         std::abort();
            //     }
            // }

            types::variant_index_type casted_variant_idx = static_cast<types::variant_index_type>(variant_idx);
            Self().put(tmp, casted_variant_idx);

            auto visitor = [&tmp]<class Arg>(Arg&& arg){
                Self().put(tmp, std::forward<Arg>(arg));
            };

            std::visit(visitor, data);
            buf += Counter().count(data);
        }
    };

    struct Backward{

        using Self  = Backward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const noexcept{

            using btype     = types_space::base_type_t<T>;
            using _MemIO    = utility::SyncedEndiannessService;
            data            = _MemIO::load<btype>(buf);
            buf             += sizeof(btype);
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const noexcept{

            using btype     = types_space::base_type_t<T>;
            using obj_type  = types_space::optional_containee_t<btype>;

            auto tmp        = buf;
            bool status     = {}; 
            put(tmp, status);

            if (status){
                obj_type obj;
                put(tmp, obj);
                data = std::move(obj);
            } else{
                data = {};
            }

            buf += Counter{}.count(data);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const noexcept{

            using btype         = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<btype>>{};

            []<size_t ...IDX>(const Self& _self, const char *& buf, T&& data, const std::index_sequence<IDX...>){
                (_self.put(buf, std::get<IDX>(data)), ...);
            }(*this, buf, std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const noexcept{

            auto archiver   = [&buf]<class ...Args>(Args&& ...args){
                (Self().put(buf, std::forward<Args>(args)), ...);
            };

            // static_assert(noexcept(data.dg_reflect(archiver)));
            data.dg_reflect(archiver);
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const noexcept{

            using btype = types_space::base_type_t<T>;
            static constexpr size_t VARIANT_COUNT = std::variant_size_v<btype>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            types::variant_index_type variant_idx;
            const char * tmp = buf;
            Self().put(tmp, variant_idx);

            //we have this line just to make sure that this is no-except compatible in case of etc.
            //such that the deserialized struct is in a "safe" state in the sense of operatable range as long as it has the specified static layout size

            variant_idx = std::clamp(static_cast<size_t>(variant_idx),
                                     size_t{0u},
                                     static_cast<size_t>(VARIANT_COUNT - 1u));

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                (
                    [&]
                    {
                        if (IDX == variant_idx){
                            std::variant_alternative_t<IDX, btype> containee;
                            Self().put(tmp, containee);
                            data = btype(std::in_place_index_t<IDX>{}, std::move(containee));
                        }
                    }(), ...
                );
            }(std::make_index_sequence<VARIANT_COUNT>());

            buf += Counter{}.count(data);
        }
    };
}

namespace trivial_serializer{

    template <class T, class = void>
    struct is_serializable_helper: std::false_type{};

    template <class T>
    struct is_serializable_helper<T, std::void_t<std::enable_if_t<archive::IsSerializable().is_serializable(T{})>>>: std::true_type{};

    template <class T, class = void>
    struct is_serializable: std::false_type{};

    template <class T>
    struct is_serializable<T, std::void_t<std::enable_if_t<std::is_default_constructible_v<T>>>>: is_serializable_helper<T>{};

    template <class T>
    static inline constexpr bool is_serializable_v = is_serializable<T>::value;
    
    template <class T>
    constexpr auto size(T&& obj) noexcept -> size_t{

        static_assert(is_serializable_v<types_space::base_type_t<T>>);

        return archive::Counter{}.count(std::forward<T>(obj));
    }

    template <class T>
    constexpr auto serialize_into(char * buf, const T& obj) noexcept -> char *{

        static_assert(is_serializable_v<types_space::base_type_t<T>>);

        archive::Forward().put(buf, obj);
        return buf;
    }

    template <class T>
    constexpr auto deserialize_into(T& obj, const char * buf) noexcept -> const char *{

        static_assert(is_serializable_v<types_space::base_type_t<T>>); //

        archive::Backward().put(buf, obj);
        return buf;
    }

    template <class T>
    constexpr auto reflectible_is_equal(const T& lhs, const T& rhs) noexcept -> bool{

        constexpr size_t REFLECTIBLE_SZ = trivial_serializer::size(T{});

        std::array<char, REFLECTIBLE_SZ> lhs_byte_representation{};
        std::array<char, REFLECTIBLE_SZ> rhs_byte_representation{};

        trivial_serializer::serialize_into(lhs_byte_representation.data(), lhs);
        trivial_serializer::serialize_into(rhs_byte_representation.data(), rhs);

        return lhs_byte_representation == rhs_byte_representation;
    }
}

#endif