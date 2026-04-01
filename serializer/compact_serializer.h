#ifndef __DG_COMPACT_SERIALIZER_H__
#define __DG_COMPACT_SERIALIZER_H__

//define HEADER_CONTROL 0

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <climits>
#include <bit>
#include <optional>
#include <numeric>
#include <type_traits>
#include <array>
#include <variant>
#include <chrono>
#include <expected>
#include <cmath>

//I was thinking of making the trivial serializer an internal dependency to solve the VERSION_CONTROL problems
// 

namespace dg::network_compact_serializer::network_trivial_serializer::constants{

    static constexpr auto endianness = std::endian::little;
}

namespace dg::network_compact_serializer::network_trivial_serializer::types{

    using size_type = uint64_t;
    using variant_idx_t = uint8_t;
}

namespace dg::network_compact_serializer::network_trivial_serializer::types_space{

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

    template <class T>
    static constexpr bool is_tuple_v        = is_tuple<T>::value; 

    template <class T>
    static constexpr bool is_optional_v     = is_optional<T>::value;

    template <class T>
    static constexpr bool is_reflectible_v  = is_reflectible<T>::value;

    template <class T>
    static constexpr bool is_dg_arithmetic_v = is_dg_arithmetic<T>::value;

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
}

namespace dg::network_compact_serializer::network_trivial_serializer::utility{

    using namespace dg::network_compact_serializer::network_trivial_serializer::types;

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
        static constexpr auto deflt              = dg::network_compact_serializer::network_trivial_serializer::constants::endianness; 
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
}

namespace dg::network_compact_serializer::network_trivial_serializer::archive{
    
    struct IsSerializable{

        template <class T>
        constexpr auto is_serializable(T&& data) const noexcept -> bool{

            using btype = types_space::base_type_t<T>;
            
            if constexpr(types_space::is_dg_arithmetic_v<btype>){
                return true;
            } else if constexpr(types_space::is_tuple_v<btype>){
                return [&]<size_t ...IDX>(const std::index_sequence<IDX...>) noexcept{
                    return (IsSerializable{}.is_serializable(std::get<IDX>(data)) && ...);
                }(std::make_index_sequence<std::tuple_size_v<btype>>{});
            } else if constexpr(types_space::is_reflectible_v<btype>){
                bool rs = true;
                auto archiver = [&rs]<class ...Args>(Args&& ...args) noexcept{
                    rs &= (IsSerializable().is_serializable(std::forward<Args>(args)) && ...);
                };
                data.dg_reflect(archiver);
                return rs;
            } else{
                return false;
            }
        }
    };

    struct Counter{
        
        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(T&& data) const noexcept -> size_t{

            return sizeof(types_space::base_type_t<T>);
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
    };

    struct Forward{

        using Self = Forward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, T&& data) const noexcept{

            using _MemIO = utility::SyncedEndiannessService;
            _MemIO::dump(buf, data);
            buf += sizeof(types_space::base_type_t<T>);
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
    };
}

namespace dg::network_compact_serializer::network_trivial_serializer{

    template <class T, class = void>
    struct is_serializable: std::false_type{};

    template <class T>
    struct is_serializable<T, std::void_t<std::enable_if_t<archive::IsSerializable().is_serializable(T{})>>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_serializable_v = is_serializable<T>::value;

    template <class T>
    constexpr auto size(T&& obj) noexcept -> size_t{

        return dg::network_compact_serializer::network_trivial_serializer::archive::Counter{}.count(std::forward<T>(obj));
    }

    template <class T>
    constexpr auto serialize_into(char * buf, const T& obj) noexcept -> char *{

        dg::network_compact_serializer::network_trivial_serializer::archive::Forward().put(buf, obj);
        return buf;
    }

    template <class T>
    constexpr auto deserialize_into(T& obj, const char * buf) noexcept -> const char *{

        dg::network_compact_serializer::network_trivial_serializer::archive::Backward().put(buf, obj);
        return buf;
    }
}

namespace dg::network_compact_serializer::types{

    using hash_type                             = std::pair<uint64_t, uint64_t>;
    using size_type                             = uint64_t;
    using dgstd_unsigned_serialization_header_t = uint8_t;
    using version_control_t                     = uint8_t;
    using variant_index_type                    = uint8_t;
    using serializable_duration_t               = std::chrono::duration<int64_t, std::ratio<1L, 1000000000L>>;

    /*
        Casting from a floating-point duration to an integer duration is subject to undefined behavior when the floating-point value is NaN, infinity, 
        or too large to be representable by the target's integer type. Otherwise, casting to an integer duration is subject to truncation as with any static_cast to an integer type. 
    */
}

namespace dg::network_compact_serializer::constants{

    static constexpr auto endianness                                = std::endian::little;
    static constexpr bool IS_SAFE_INTEGER_CONVERSION_ENABLED        = true;
    static constexpr bool DESERIALIZATION_HAS_CLEAR_CONTAINER_RIGHT = true;
    static constexpr bool HAS_CONTAINER_RESERVATION                 = false;
    static constexpr uint8_t SERIALIZATION_VERSION_CONTROL          = 1;
}

namespace dg::network_compact_serializer::exception_space{

    struct corrupted: std::exception{};

    struct corrupted_format: corrupted{

        inline auto what() const noexcept -> const char *{

            return "corrupted_format";
        }
    };

    struct bad_version_control: corrupted{

        inline auto what() const noexcept -> const char *{

            return "bad version control";
        }
    };

    struct bad_format: corrupted{

        inline auto what() const noexcept -> const char *{

            return "bad format";
        }
    };
}

namespace dg::network_compact_serializer::types_space{

    static constexpr auto nil_lambda = []<class ...Args>(Args&&...){}; 

    template <class T, class = void>
    struct is_tuple: std::false_type{};
    
    template <class T>
    struct is_tuple<T, std::void_t<decltype(std::tuple_size<T>::value)>>: std::true_type{};

    template <class T>
    struct is_unique_ptr: std::false_type{};

    template <class T>
    struct is_unique_ptr<std::unique_ptr<T>>: std::bool_constant<!std::is_array_v<T>>{}; 

    template <class T>
    struct is_optional: std::false_type{};

    template <class ...Args>
    struct is_optional<std::optional<Args...>>: std::true_type{}; 

    template <class T>
    struct is_vector: std::false_type{};

    template <class ...Args>
    struct is_vector<std::vector<Args...>>: std::true_type{};

    template <class T>
    struct is_unordered_map: std::false_type{};

    template <class ...Args>
    struct is_unordered_map<std::unordered_map<Args...>>: std::true_type{}; 

    template <class T>
    struct is_unordered_set: std::false_type{};

    template <class ...Args>
    struct is_unordered_set<std::unordered_set<Args...>>: std::true_type{};

    template <class T>
    struct is_map: std::false_type{};

    template <class ...Args>
    struct is_map<std::map<Args...>>: std::true_type{}; 

    template <class T>
    struct is_set: std::false_type{};

    template <class ...Args>
    struct is_set<std::set<Args...>>: std::true_type{};

    template <class T>
    struct is_basic_string: std::false_type{};

    template <class ...Args>
    struct is_basic_string<std::basic_string<Args...>>: std::true_type{};

    template <class T>
    struct is_variant: std::false_type{};

    template <class ...Args>
    struct is_variant<std::variant<Args...>>: std::true_type{};

    template <class T>
    struct is_expected: std::false_type{};

    template <class ...Args>
    struct is_expected<std::expected<Args...>>: std::true_type{};

    template <class T>
    struct is_utc_timepoint: std::false_type{};

    template <>
    struct is_utc_timepoint<std::chrono::time_point<std::chrono::utc_clock, std::chrono::nanoseconds>>: std::bool_constant<std::is_same_v<typename std::chrono::utc_clock::rep, int64_t>>{};

    template <class T>
    struct is_std_duration: std::false_type{};

    template <class ...Args>
    struct is_std_duration<std::chrono::duration<Args...>>: std::true_type{};

    template <class T, class = void>
    struct is_reflectible: std::false_type{};

    template <class T>
    struct is_reflectible<T, std::void_t<decltype(std::declval<T>().dg_reflect(nil_lambda))>>: std::true_type{};

    template <class T, class = void>
    struct is_dg_arithmetic: std::disjunction<std::is_integral<T>, std::is_floating_point<T>>{};

    template <class T>
    struct is_dg_arithmetic<T, std::void_t<std::enable_if_t<std::is_floating_point_v<T>>>>: std::bool_constant<std::numeric_limits<T>::is_iec559>{}; 
    
    template <class T, class = void>
    struct can_reserve: std::false_type{};

    template <class T>
    struct can_reserve<T, std::void_t<decltype(std::declval<T>().reserve(std::declval<size_t>()))>>: std::true_type{};

    template <class T>
    struct is_byte_stream_container: std::false_type{};

    template <class ...Args>
    struct is_byte_stream_container<std::vector<char, Args...>>: std::true_type{};

    template <class ...Args>
    struct is_byte_stream_container<std::basic_string<char, Args...>>: std::true_type{};

    template <class T, class = void>
    struct container_value_or_empty{};

    template <class T>
    struct container_value_or_empty<T, std::void_t<typename T::value_type>>{
        using type = typename T::value_type;
    };

    template <class T, class = void>
    struct container_bucket_or_empty{};

    template <class T>
    struct container_bucket_or_empty<T, std::void_t<typename T::key_type, typename T::mapped_type>>{
        using type = std::pair<typename T::key_type, typename T::mapped_type>; //I dont want to complicate this further by adding const to key_type (since this is an application) - 
    };

    template <class T>
    using containee_or_none_t = std::conditional_t<std::disjunction_v<is_vector<T>, is_unordered_set<T>, is_set<T>, is_basic_string<T>>,
                                                                      container_value_or_empty<T>,
                                                                      std::conditional_t<std::disjunction_v<is_unordered_map<T>, is_map<T>>, 
                                                                                         container_bucket_or_empty<T>, 
                                                                                         void>>;

    template <class T>
    using containee_t = typename containee_or_none_t<T>::type;

    template <class T, class = void>
    struct containee_or_empty{
        using type = void;
    };

    template <class T>
    struct containee_or_empty<T, std::void_t<containee_t<T>>>{
        using type = containee_t<T>;
    };

    template <class T>
    using containee_or_empty_t = typename containee_or_empty<T>::type; 

    template <class T>
    static inline constexpr bool has_unique_serializable_representations_v              = std::disjunction_v<std::is_same<T, int8_t>, std::is_same<T, uint8_t>, 
                                                                                                             std::is_same<T, char>, std::is_same<T, unsigned char>, std::is_same<T, signed char>>;

    template <class T>
    static inline constexpr bool has_unique_serializable_sameendian_representations_v   = std::disjunction_v<std::is_same<T, int8_t>, std::is_same<T, uint8_t>, 
                                                                                                             std::is_same<T, char>, std::is_same<T, unsigned char>, std::is_same<T, signed char>,
                                                                                                             std::is_same<T, int16_t>, std::is_same<T, uint16_t>, 
                                                                                                             std::is_same<T, int32_t>, std::is_same<T, uint32_t>, 
                                                                                                             std::is_same<T, int64_t>, std::is_same<T, uint64_t>>;
    template <class T>
    static inline constexpr bool is_vector_v                                            = is_vector<T>::value;

    template <class T>
    static inline constexpr bool is_basic_string_v                                      = is_basic_string<T>::value;
    
    template <class T>
    static inline constexpr bool is_map_v                                               = is_map<T>::value;

    template <class T>
    static inline constexpr bool is_unordered_map_v                                     = is_unordered_map<T>::value;

    template <class T>
    static inline constexpr bool is_set_v                                               = is_set<T>::value;

    template <class T>
    static inline constexpr bool is_unordered_set_v                                     = is_unordered_set<T>::value;

    template <class T>
    static inline constexpr bool is_cpyable_linear_container_v                          = std::disjunction_v<is_vector<T>, is_basic_string<T>> && (has_unique_serializable_representations_v<containee_or_empty_t<T>> || has_unique_serializable_sameendian_representations_v<containee_or_empty_t<T>> && (std::endian::native == constants::endianness)); //this requires inter-compatible with noncpyable_linear

    template <class T>
    static inline constexpr bool is_noncpyable_linear_container_v                       = std::disjunction_v<is_vector<T>, is_basic_string<T>> && !is_cpyable_linear_container_v<T>;

    template <class T>
    static inline constexpr bool is_nonlinear_container_v                               = std::disjunction_v<is_unordered_map<T>, is_map<T>, is_unordered_set<T>, is_set<T>>;

    template <class T>
    static inline constexpr bool is_container_v                                         = is_cpyable_linear_container_v<T> || is_noncpyable_linear_container_v<T> || is_nonlinear_container_v<T>;

    template <class T>
    static inline constexpr bool is_tuple_v                                             = is_tuple<T>::value; 

    template <class T>
    static inline constexpr bool is_unique_ptr_v                                        = is_unique_ptr<T>::value;

    template <class T>
    static inline constexpr bool is_optional_v                                          = is_optional<T>::value;

    template <class T>
    static inline constexpr bool is_variant_v                                           = is_variant<T>::value;

    template <class T>
    static inline constexpr bool is_expected_v                                          = is_expected<T>::value;

    template <class T>
    static inline constexpr bool is_utc_timepoint_v                                     = is_utc_timepoint<T>::value;

    template <class T>
    static inline constexpr bool is_std_duration_v                                      = is_std_duration<T>::value;

    template <class T>
    static inline constexpr bool is_reflectible_v                                       = is_reflectible<T>::value;

    template <class T>
    static inline constexpr bool is_dg_arithmetic_v                                     = is_dg_arithmetic<T>::value;

    template <class T>
    static inline constexpr bool is_byte_stream_container_v                             = is_byte_stream_container<T>::value;

    template <class T>
    static inline constexpr bool can_reserve_v                                          = can_reserve<T>::value;

    template <class T>
    struct base_type: std::enable_if<true, T>{};

    //alright, I dont really know if this is future-proof, let's make it defined by using defined use-cases for now, we dont have time to iterate through every possibility
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

    template <class T, class CastedSize>
    struct ContainerWrapper{
        static_assert(is_container_v<base_type_t<T>>);
        static_assert(std::is_unsigned_v<CastedSize>);
        static_assert(sizeof(CastedSize) <= sizeof(size_t));

        using containee_type = T&&;
        using size_type = CastedSize;

        T&& reference;

        ContainerWrapper(T&& reference) noexcept: reference(std::forward<T>(reference)){}

        auto get() noexcept -> T&&{
           return static_cast<T&&>(this->reference); 
        }
    };

    template <class T>
    struct is_container_wrapper: std::false_type{};

    template <class ...Args>
    struct is_container_wrapper<ContainerWrapper<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_container_wrapper_v = is_container_wrapper<T>::value; 

    template <class T>
    struct is_container_wrapper_argument: std::false_type{};

    template <class ...Args>
    struct is_container_wrapper_argument<ContainerWrapper<Args...>&&>: std::true_type{};

    template <class T>
    static inline constexpr bool is_container_wrapper_argument_v = is_container_wrapper_argument<T>::value;

    template <class T, class = void>
    struct is_data_implemented : std::false_type{};

    template <class T>
    struct is_data_implemented<T, std::void_t<decltype(std::declval<T&>().data())>> : std::integral_constant<bool, std::is_same_v<std::add_pointer_t<char>, decltype(std::declval<T&>().data())>>{};

    template <class T, class = void>
    struct is_const_data_implemented : std::false_type{};

    template <class T>
    struct is_const_data_implemented<T, std::void_t<decltype(std::declval<const T&>().data())>> : std::integral_constant<bool, std::is_same_v<std::add_pointer_t<const char>, decltype(std::declval<const T&>().data())>>{};

    template <class T, class = void>
    struct is_resize_implemented : std::false_type{};

    template <class T>
    struct is_resize_implemented<T, std::void_t<decltype(std::declval<T&>().resize(0u))>> : std::true_type{};

    template <class T, class = void>
    struct is_size_implemented : std::false_type{};

    template <class T>
    struct is_size_implemented<T, std::void_t<decltype(std::declval<const T&>().size())>> : std::integral_constant<bool, std::numeric_limits<decltype(std::declval<const T&>().size())>::is_integer>{};

    template <class T>
    static inline constexpr bool is_streamable_implemented_v = is_data_implemented<T>::value && is_const_data_implemented<T>::value && is_resize_implemented<T>::value && is_size_implemented<T>::value;
}

namespace dg::network_compact_serializer::utility{

    using namespace network_compact_serializer::types;

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;

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

    static constexpr auto rotl64(uint64_t x, int8_t r) -> uint64_t{
    
        return (x << r) | (x >> (64 - r));
    }

    static constexpr auto fmix64(uint64_t k) -> uint64_t{
        
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccd;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53;
        k ^= k >> 33;

        return k;
    }

    static constexpr auto murmur_hash_base(const char * buf, size_t len, const uint32_t seed = 0xFF) -> std::pair<uint64_t, uint64_t>{

        const size_t nblocks = len / 16;

        uint64_t h1 = seed;
        uint64_t h2 = seed;

        const uint64_t c1 = 0x87c37b91114253d5;
        const uint64_t c2 = 0x4cf5ad432745937f;

        for(size_t i = 0; i < nblocks; i++)
        {   
            uint64_t k1{};
            uint64_t k2{};

            dg::network_compact_serializer::network_trivial_serializer::deserialize_into(k1, buf + (i * 2 + 0) * sizeof(uint64_t));
            dg::network_compact_serializer::network_trivial_serializer::deserialize_into(k2, buf + (i * 2 + 1) * sizeof(uint64_t));

            k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1 ^= k1;
            h1 = rotl64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;
            k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2 ^= k2;
            h2 = rotl64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
        }

        const char * tail = buf + nblocks*16;

        uint64_t k1 = 0;
        uint64_t k2 = 0;

        switch(len & 15)
        {
            case 15: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[14])) << 48;
            case 14: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[13])) << 40;
            case 13: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[12])) << 32;
            case 12: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[11])) << 24;
            case 11: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[10])) << 16;
            case 10: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[9])) << 8;
            case  9: k2 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[8])) << 0;
                    k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2 ^= k2;

            case  8: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[7])) << 56;
            case  7: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[6])) << 48;
            case  6: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[5])) << 40;
            case  5: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[4])) << 32;
            case  4: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[3])) << 24;
            case  3: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[2])) << 16;
            case  2: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[1])) << 8;
            case  1: k1 ^= static_cast<uint64_t>(std::bit_cast<uint8_t>(tail[0])) << 0;
                    k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1 ^= k1;
        };

        h1 ^= static_cast<uint64_t>(len); 
        h2 ^= static_cast<uint64_t>(len);

        h1 += h2;
        h2 += h1;

        h1 = fmix64(h1);
        h2 = fmix64(h2);

        h1 += h2;
        h2 += h1;

        return {h1, h2};
    } 

    auto hash(const char * buf, size_t sz, uint32_t secret) noexcept -> hash_type{

        return murmur_hash_base(buf, sz, secret);
    }

    template <class T, std::enable_if_t<std::disjunction_v<types_space::is_vector<T>, 
                                                           types_space::is_basic_string<T>>, bool> = true>
    constexpr auto get_inserter() noexcept{

        auto inserter   = []<class U, class K>(U&& container, K&& arg){
            container.push_back(std::forward<K>(arg));
        };

        return inserter;
    }

    template <class T, std::enable_if_t<std::disjunction_v<types_space::is_unordered_map<T>, 
                                                           types_space::is_unordered_set<T>, 
                                                           types_space::is_map<T>, 
                                                           types_space::is_set<T>>, bool> = true>
    constexpr auto get_inserter() noexcept{ 

        auto inserter   = []<class U, class K>(U&& container, K&& args){
            container.insert(std::forward<K>(args));
        };

        return inserter;
    }

    template <class T>
    constexpr void reserve_if_possible(T&& container, size_t sz){

        if constexpr(types_space::can_reserve_v<T&&> && constants::HAS_CONTAINER_RESERVATION){
            container.reserve(sz);
        }
    }

    template <class T>
    constexpr void loose_reserve_if_possible(T&& container, size_t sz){

        if constexpr(types_space::can_reserve_v<T&&>){
            container.reserve(sz);
        }
    }

    template <class T1, class T>
    constexpr auto safe_integer_cast(T value) noexcept -> T1{

        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(std::numeric_limits<T1>::is_integer);

        if constexpr(constants::IS_SAFE_INTEGER_CONVERSION_ENABLED){
            if constexpr(std::is_unsigned_v<T> && std::is_unsigned_v<T1>){
                (void) value;
            } else if constexpr(std::is_signed_v<T> && std::is_signed_v<T1>){
                (void) value;
            } else{
                if constexpr(std::is_signed_v<T>){
                    if constexpr(sizeof(T) > sizeof(T1)){
                        (void) value;
                    } else{
                        if (value < 0){
                            std::abort();
                        } else{
                            return value; //sizeof(signed) <= sizeof(unsigned)
                        }
                    }
                } else{
                    if constexpr(sizeof(T1) > sizeof(T)){
                        (void) value;
                    } else{
                        if (value > std::numeric_limits<T1>::max()){
                            std::abort();
                        } else{
                            return value; //sizeof(unsigned) >= sizeof(signed)
                        }
                    }
                }
            }

            if (value > std::numeric_limits<T1>::max()){
                std::abort();
            }

            if (value < std::numeric_limits<T1>::min()){
                std::abort();
            }

            return value;
        } else{
            return value;
        }
    }

    template <class T1, class T>
    constexpr auto is_safe_integer_castable(T value) noexcept -> bool{

        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(std::numeric_limits<T1>::is_integer);

        if constexpr(std::is_unsigned_v<T> && std::is_unsigned_v<T1>){
            (void) value;
        } else if constexpr(std::is_signed_v<T> && std::is_signed_v<T1>){
            (void) value;
        } else{
            if constexpr(std::is_signed_v<T>){
                if constexpr(sizeof(T) > sizeof(T1)){
                    (void) value;
                } else{
                    if (value < 0){
                        return false;
                    } else{
                        return true; //sizeof(signed) <= sizeof(unsigned)
                    }
                }
            } else{
                if constexpr(sizeof(T1) > sizeof(T)){
                    (void) value;
                } else{
                    if (value > std::numeric_limits<T1>::max()){
                        return false;
                    } else{
                        return true; //sizeof(unsigned) >= sizeof(signed)
                    }
                }
            }
        }

        if (value > std::numeric_limits<T1>::max()){
            return false;
        }

        if (value < std::numeric_limits<T1>::min()){
            return false;
        }

        return true;
    }

    template <class T>
    constexpr auto is_met_variant_index_requirements(T variant_idx) noexcept -> bool{

        return variant_idx != std::variant_npos 
                && is_safe_integer_castable<types::variant_index_type>(variant_idx)
                && safe_integer_cast<T>(safe_integer_cast<types::variant_index_type>(variant_idx)) == variant_idx;
    }

    template <class To, class Fr>
    constexpr auto safe_duration_cast(Fr dur) -> To
    {
        static_assert(types_space::is_std_duration_v<Fr>);
        static_assert(types_space::is_std_duration_v<To>);

        static_assert(!(std::is_floating_point_v<Fr> && std::numeric_limits<To>::is_integer));

        if (std::isnan(dur.count()))
        {
            throw dg::network_compact_serializer::exception_space::corrupted_format{};
        }

        auto dst = std::chrono::duration_cast<To>(dur);

        if (std::isnan(dst.count()))
        {
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        return dst;
    }
}

namespace dg::network_compact_serializer::archive{

    struct Counter{
        
        using Self = Counter;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            return sizeof(types_space::base_type_t<T>);
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs = this->count(bool{});

            if (data){
                rs += this->count(*data);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs = this->count(bool{}); 

            if (data){
                rs += this->count(*data);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (!utility::is_met_variant_index_requirements(data.index())){
                throw exception_space::bad_format();
            }

            size_t rs = this->count(types::variant_index_type{}); 

            auto visitor = [&rs]<class VisitorArg>(VisitorArg&& visitor_arg){
                rs += Self().count(std::forward<VisitorArg>(visitor_arg));
            };

            std::visit(visitor, data);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type = types_space::base_type_t<T>;

            size_t rs = this->count(bool{});

            if (data.has_value()){
                rs += this->count(data.value());
            } else{
                rs += this->count(data.error());
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<T>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            auto dur            = data.time_since_epoch();
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(dur);
            auto storable_value = casted_dur.count();
            
            return this->count(storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<T>, bool> = true>
        constexpr auto count(const T& data) const -> size_t
        {
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(data);
            auto storable_value = casted_dur.count();

            return this->count(storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};
            size_t rs           = 0u;

            [&rs]<size_t ...IDX>(const T& data, const std::index_sequence<IDX...>){
                rs += (Self().count(std::get<IDX>(data)) + ...);
            }(data, idx_seq);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            if (!utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            size_t rs = this->count(types::size_type{});

            for (const auto& e: data){
                rs += this->count(e);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const-> size_t{

            if (!utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            return this->count(types::size_type{}) + static_cast<size_t>(data.size()) * sizeof(types_space::containee_t<types_space::base_type_t<T>>);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs       = 0u;
            auto archiver   = [&rs]<class ...Args>(Args&& ...args){
                rs += (Self().count(std::forward<Args>(args)) + ...);
            };
            data.dg_reflect(archiver);
            
            return rs;
        }
    };

    struct Forward{

        using Self = Forward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            network_compact_serializer::utility::SyncedEndiannessService::dump(buf, data);
            std::advance(buf, sizeof(types_space::base_type_t<T>));
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            this->put(buf, static_cast<bool>(data));

            if (data){
                this->put(buf, *data);
            }
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            this->put(buf, static_cast<bool>(data.has_value()));

            if (data){
                this->put(buf, *data);
            }
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (!network_compact_serializer::utility::is_met_variant_index_requirements(data.index())){
                throw exception_space::bad_format();
            }

            types::variant_index_type demoted_variant_idx = network_compact_serializer::utility::safe_integer_cast<types::variant_index_type>(data.index());
            this->put(buf, demoted_variant_idx);

            auto visitor = [&buf]<class VariantArg>(VariantArg&& variant_arg){
                Self().put(buf, std::forward<VariantArg>(variant_arg));
            };

            std::visit(visitor, data);
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            this->put(buf, static_cast<bool>(data.has_value()));

            if (data.has_value()){
                this->put(buf, data.value());
            } else{
                this->put(buf, data.error());
            }
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            auto dur            = data.time_since_epoch();
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(dur);
            auto storable_value = casted_dur.count();

            this->put(buf, storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const
        {
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(data);
            auto storable_value = casted_dur.count();

            this->put(buf, storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            []<size_t ...IDX>(char *& buf, const T& data, const std::index_sequence<IDX...>){
                (Self().put(buf, std::get<IDX>(data)), ...);
            }(buf, data, idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            if (!network_compact_serializer::utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            this->put(buf, network_compact_serializer::utility::safe_integer_cast<types::size_type>(data.size()));

            for (const auto& e: data){
                this->put(buf, e);
            }
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;
            
            if (!network_compact_serializer::utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            this->put(buf, network_compact_serializer::utility::safe_integer_cast<types::size_type>(data.size()));

            void * dst          = buf;
            const void * src    = data.data();
            size_t cpy_sz       = data.size() * sizeof(elem_type);  

            std::memcpy(dst, src, cpy_sz);
            std::advance(buf, cpy_sz);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            auto archiver = [&buf]<class ...Args>(Args&& ...args){
                (Self().put(buf, std::forward<Args>(args)), ...);
            };

            data.dg_reflect(archiver);
        }
    };

    struct Backward{

        using Self = Backward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            data = network_compact_serializer::utility::SyncedEndiannessService::load<base_type>(buf);
            std::advance(buf, sizeof(base_type));
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using containee_type = typename types_space::base_type_t<T>::element_type;
            bool status = {};
            this->put(buf, status);

            if (status){
                containee_type obj;
                this->put(buf, obj);
                data = std::make_unique<containee_type>(std::move(obj));
            } else{
                data = nullptr;
            }
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using containee_type = typename base_type::value_type;

            bool status = {};
            this->put(buf, status);

            if (status){
                containee_type obj;
                this->put(buf, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                data = std::nullopt;
            }
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            types::variant_index_type variant_idx;
            this->put(buf, variant_idx);

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                (
                    [&]{
                        if (IDX == network_compact_serializer::utility::safe_integer_cast<size_t>(variant_idx)){
                            std::variant_alternative_t<IDX, base_type> containee;
                            this->put(buf, containee);
                            data = base_type(std::in_place_index_t<IDX>{}, std::move(containee));
                        }
                    }(), ...
                );
            }(std::make_index_sequence<std::variant_size_v<base_type>>());
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            using value_type    = typename base_type::value_type;
            using error_type    = typename base_type::error_type;

            bool status = {};
            this->put(buf, status);

            if (status){
                value_type obj;
                this->put(buf, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                error_type obj;
                this->put(buf, obj);
                data = base_type(std::unexpected(obj));
            }
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using data_timepoint_t = types_space::base_type_t<T>;
            using data_dur_t = typename data_timepoint_t::duration;

            typename types::serializable_duration_t::rep tick;

            this->put(buf, tick);
            auto serializable_dur = types::serializable_duration_t(tick);
            auto data_dur = utility::safe_duration_cast<data_dur_t>(serializable_dur);
            data = data_timepoint_t(data_dur);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const
        {
            using data_dur_t = types_space::base_type_t<T>;

            typename types::serializable_duration_t::rep tick;

            this->put(buf, tick);
            auto serializable_dur = types::serializable_duration_t(tick);
            data = utility::safe_duration_cast<data_dur_t>(serializable_dur);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            []<size_t ...IDX>(const char *& buf, T&& data, const std::index_sequence<IDX...>){
                (Self().put(buf, std::get<IDX>(std::forward<T>(data))), ...);
            }(buf, std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{
            
            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;

            auto sz         = types::size_type{};
            auto isrter     = network_compact_serializer::utility::get_inserter<base_type>();
            this->put(buf, sz);

            if constexpr(dg::network_compact_serializer::constants::DESERIALIZATION_HAS_CLEAR_CONTAINER_RIGHT){
                data.clear();
            }

            // data.reserve(sz);
            dg::network_compact_serializer::utility::loose_reserve_if_possible(data, sz);

            for (size_t i = 0; i < sz; ++i){
                elem_type e;
                this->put(buf, e);
                isrter(data, std::move(e));
            }
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;
            
            auto sz = types::size_type{};
            this->put(buf, sz);
            data.resize(sz);

            void * dst          = data.data();
            const void * src    = buf;
            size_t cpy_sz       = sz * sizeof(elem_type); 

            std::memcpy(dst, src, cpy_sz);
            std::advance(buf, cpy_sz);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, T&& data) const{

            auto archiver = [&buf]<class ...Args>(Args&& ...args){
                (Self().put(buf, std::forward<Args>(args)), ...);
            };

            data.dg_reflect(archiver);
        }
    };

    struct SafeBackward{

        using Self = SafeBackward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;

            if (buf_sz < sizeof(base_type)){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            data    = network_compact_serializer::utility::SyncedEndiannessService::load<base_type>(buf);
            std::advance(buf, sizeof(base_type));
            buf_sz  -= sizeof(base_type);
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using containee_type = typename types_space::base_type_t<T>::element_type;
            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                containee_type obj;
                this->put(buf, buf_sz, obj);
                data = std::make_unique<containee_type>(std::move(obj));
            } else{
                data = nullptr;
            }
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type         = types_space::base_type_t<T>;
            using containee_type    = typename base_type::value_type;

            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                containee_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                data = std::nullopt;
            }
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            types::variant_index_type variant_idx;
            this->put(buf, buf_sz, variant_idx);

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;   
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (variant_idx >= VARIANT_COUNT){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                (
                    [&]{
                        if (IDX == network_compact_serializer::utility::safe_integer_cast<size_t>(variant_idx)){
                            std::variant_alternative_t<IDX, base_type> containee;
                            this->put(buf, buf_sz, containee);
                            data = base_type(std::in_place_index_t<IDX>{}, std::move(containee));
                        }
                    }(), ...
                );
            }(std::make_index_sequence<VARIANT_COUNT>());
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            using value_type    = typename base_type::value_type;
            using error_type    = typename base_type::error_type;

            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                value_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                error_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::unexpected(obj));
            }
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using data_timepoint_t = types_space::base_type_t<T>;
            using data_dur_t = typename data_timepoint_t::duration;
            
            typename types::serializable_duration_t::rep tick;

            this->put(buf, buf_sz, tick);
            auto serializable_dur = types::serializable_duration_t(tick);
            auto data_dur = utility::safe_duration_cast<data_dur_t>(serializable_dur);
            data = data_timepoint_t(data_dur);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const
        {
            using data_dur_t = types_space::base_type_t<T>;

            typename types::serializable_duration_t::rep tick;

            this->put(buf, buf_sz, tick);
            auto serializable_dur = types::serializable_duration_t(tick);
            data = utility::safe_duration_cast<data_dur_t>(serializable_dur);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            []<size_t ...IDX>(const char *& buf, size_t& buf_sz, T&& data, const std::index_sequence<IDX...>){
                (Self().put(buf, buf_sz, std::get<IDX>(std::forward<T>(data))), ...);
            }(buf, buf_sz, std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;

            auto sz         = types::size_type{};
            auto isrter     = network_compact_serializer::utility::get_inserter<base_type>();
            this->put(buf, buf_sz, sz); 

            if constexpr(dg::network_compact_serializer::constants::DESERIALIZATION_HAS_CLEAR_CONTAINER_RIGHT){
                data.clear();
            }

            // data.reserve(sz);
            dg::network_compact_serializer::utility::reserve_if_possible(data, sz);

            for (size_t i = 0; i < sz; ++i){
                elem_type e;
                this->put(buf, buf_sz, e);
                isrter(data, std::move(e));
            }
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;
            
            auto sz = types::size_type{};
            this->put(buf, buf_sz, sz);

            size_t cpy_sz       = sz * sizeof(elem_type); 

            if (buf_sz < cpy_sz){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            data.resize(sz);

            void * dst          = data.data();
            const void * src    = buf;

            std::memcpy(dst, src, cpy_sz);
            std::advance(buf, cpy_sz);
            buf_sz -= cpy_sz;
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        void put(const char *& buf, size_t& buf_sz, T&& data) const{

            auto archiver = [&buf, &buf_sz]<class ...Args>(Args&& ...args){
                (Self().put(buf, buf_sz, std::forward<Args>(args)), ...);
            };

            data.dg_reflect(archiver);
        }
    };

    template <class = void>
    static inline constexpr bool FALSE_VAL = false;

    template <class T>
    static consteval auto get_dgstd_serialization_header() noexcept -> types::dgstd_unsigned_serialization_header_t{

        if constexpr(std::is_integral_v<T>){
            if constexpr(std::is_signed_v<T>){
                if constexpr(sizeof(T) == 1u){
                    return 201u;
                } else if constexpr(sizeof(T) == 2u){
                    return 202u;
                } else if constexpr(sizeof(T) == 4u){
                    return 203u;
                } else if constexpr(sizeof(T) == 8u){
                    return 204u;
                } else if constexpr(sizeof(T) == 16u){
                    return 205u;
                } else if constexpr(sizeof(T) == 32u){
                    return 206u;
                } else{
                    static_assert(FALSE_VAL<>);
                }
            } else if constexpr(std::is_unsigned_v<T>){
                if constexpr(sizeof(T) == 1u){
                    return 207u;
                } else if constexpr(sizeof(T) == 2u){
                    return 208u;
                } else if constexpr(sizeof(T) == 4u){
                    return 209u;
                } else if constexpr(sizeof(T) == 8u){
                    return 210u;
                } else if constexpr(sizeof(T) == 16u){
                    return 211u;
                } else if constexpr(sizeof(T) == 32u){
                    return 212u;
                } else{
                    static_assert(FALSE_VAL<>);
                }
            } else{
                static_assert(FALSE_VAL<>);
            }
        } else if constexpr(std::is_floating_point_v<T>){
            if constexpr(std::numeric_limits<T>::is_iec559){
                if constexpr(sizeof(T) == 1u){
                    return 213u;
                } else if constexpr(sizeof(T) == 2u){
                    return 214u; 
                } else if constexpr(sizeof(T) == 4u){
                    return 215u;
                } else if constexpr(sizeof(T) == 8u){
                    return 216u;
                } else if constexpr(sizeof(T) == 16u){
                    return 217u;
                } else if constexpr(sizeof(T) == 32u){
                    return 218u;
                } else if constexpr(sizeof(T) == 64u){
                    return 219u;
                } else{
                    static_assert(FALSE_VAL<>);
                }
            } else{
                static_assert(FALSE_VAL<>);
            }
        } else if constexpr(types_space::is_vector_v<T>){
            return 220u;
        } else if constexpr(types_space::is_basic_string_v<T>){
            return 221u;
        } else if constexpr(types_space::is_map_v<T>){
            return 222u;
        } else if constexpr(types_space::is_unordered_map_v<T>){
            return 223u;
        } else if constexpr(types_space::is_set_v<T>){
            return 224u;
        } else if constexpr(types_space::is_unordered_set_v<T>){
            return 225u;
        } else if constexpr(types_space::is_tuple_v<T>){
            return 226u;
        } else if constexpr(types_space::is_unique_ptr_v<T>){
            return 227u;
        } else if constexpr(types_space::is_optional_v<T>){
            return 228u;
        } else if constexpr(types_space::is_reflectible_v<T>){
            return 229u;
        } else if constexpr(types_space::is_variant_v<T>){
            return 230u;
        } else if constexpr(types_space::is_utc_timepoint_v<T>){
            return 231u;
        } else if constexpr(types_space::is_std_duration_v<T>){
            return 232u;
        } else if constexpr(types_space::is_container_wrapper_v<T>){
            return 233u;
        } else if constexpr(types_space::is_expected_v<T>){
            return 234u;
        } else{
            static_assert(FALSE_VAL<>);
        }
    }

    struct DgStdCounter{

        using Self = DgStdCounter;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            return sizeof(types::dgstd_unsigned_serialization_header_t) + sizeof(types_space::base_type_t<T>);
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(bool{});

            if (data){
                rs += this->count(*data);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(bool{}); 

            if (data){
                rs += this->count(*data);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (!network_compact_serializer::utility::is_met_variant_index_requirements(data.index())){
                throw exception_space::bad_format();
            }

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(types::variant_index_type{});

            auto visitor = [&rs]<class VisitorArg>(VisitorArg&& visitor_arg){
                rs += Self().count(std::forward<VisitorArg>(visitor_arg));
            };

            std::visit(visitor, data);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type = types_space::base_type_t<T>;

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(bool{});

            if (data.has_value()){
                rs += this->count(data.value());
            } else{
                rs += this->count(data.error());
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<T>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            auto dur            = data.time_since_epoch();
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(dur);
            auto storable_value = casted_dur.count();

            return this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<T>, bool> = true>
        constexpr auto count(const T& data) const -> size_t
        {
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(data);
            auto storable_value = casted_dur.count();

            return this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            size_t rs           = this->count(types::dgstd_unsigned_serialization_header_t{});

            [&rs]<size_t ...IDX>(const T& data, const std::index_sequence<IDX...>){
                rs += (Self().count(std::get<IDX>(data)) + ...);
            }(data, idx_seq);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            if (!network_compact_serializer::utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(types::size_type{});

            for (const auto& e: data){
                rs += this->count(e);
            }

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            if (!network_compact_serializer::utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            return this->count(types::dgstd_unsigned_serialization_header_t{})
                   + this->count(types::dgstd_unsigned_serialization_header_t{})  
                   + this->count(types::size_type{})
                   + static_cast<size_t>(data.size()) * sizeof(types_space::containee_t<types_space::base_type_t<T>>);
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr auto count(const T& data) const -> size_t{

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{});

            auto archiver   = [&rs]<class ...Args>(Args&& ...args){
                rs += (Self().count(std::forward<Args>(args)) + ...);
            };

            data.dg_reflect(archiver);

            return rs;
        }

        template <class T, std::enable_if_t<types_space::is_container_wrapper_argument_v<T&&>, bool> = true>
        constexpr auto count(T&& data) const -> size_t{

            using base_type             = types_space::base_type_t<T>;
            using container_type        = typename base_type::containee_type;
            using container_size_type   = typename base_type::size_type;

            container_type container    = data.get();

            if (!network_compact_serializer::utility::is_safe_integer_castable<container_size_type>(container.size())){
                throw exception_space::bad_format();
            }

            size_t rs = this->count(types::dgstd_unsigned_serialization_header_t{}) + this->count(container_size_type{});

            for (const auto& e: container){
                rs += this->count(e);
            }

            return rs;
        }
    };

    struct DgStdForward{

        using Self = DgStdForward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>; 

            types::dgstd_unsigned_serialization_header_t serialization_header = get_dgstd_serialization_header<base_type>();

            network_compact_serializer::utility::SyncedEndiannessService::dump(buf, serialization_header);
            std::advance(buf, sizeof(types::dgstd_unsigned_serialization_header_t));

            network_compact_serializer::utility::SyncedEndiannessService::dump(buf, data);
            std::advance(buf, sizeof(base_type));
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, static_cast<bool>(data));

            if (data){
                this->put(buf, *data);
            }
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>; 

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, static_cast<bool>(data.has_value()));

            if (data){
                this->put(buf, *data);
            }
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (!network_compact_serializer::utility::is_met_variant_index_requirements(data.index())){
                throw exception_space::bad_format();
            }

            types::variant_index_type demoted_variant_idx = network_compact_serializer::utility::safe_integer_cast<types::variant_index_type>(data.index());

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, demoted_variant_idx);

            auto visitor = [&buf]<class VariantArg>(VariantArg&& variant_arg){
                Self().put(buf, std::forward<VariantArg>(variant_arg));
            };

            std::visit(visitor, data);
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, static_cast<bool>(data.has_value()));

            if (data.has_value()){
                this->put(buf, data.value());
            } else{
                this->put(buf, data.error());
            }
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            this->put(buf, get_dgstd_serialization_header<base_type>());

            auto dur            = data.time_since_epoch();
            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(dur);
            auto storable_value = casted_dur.count();

            this->put(buf, storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const
        {
            using base_type = types_space::base_type_t<T>;

            this->put(buf, get_dgstd_serialization_header<base_type>());

            auto casted_dur     = utility::safe_duration_cast<types::serializable_duration_t>(data);
            auto storable_value = casted_dur.count();

            this->put(buf, storable_value);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            this->put(buf, get_dgstd_serialization_header<base_type>());

            []<size_t ...IDX>(char *& buf, const T& data, const std::index_sequence<IDX...>){
                (Self().put(buf, std::get<IDX>(data)), ...);
            }(buf, data, idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            if (!utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, network_compact_serializer::utility::safe_integer_cast<types::size_type>(data.size()));

            for (const auto& e: data){
                this->put(buf, e);
            }
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;

            if (!utility::is_safe_integer_castable<types::size_type>(data.size())){
                throw exception_space::bad_format();
            }

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, get_dgstd_serialization_header<elem_type>());
            this->put(buf, network_compact_serializer::utility::safe_integer_cast<types::size_type>(data.size()));

            void * dst          = buf;
            const void * src    = data.data();
            size_t cpy_sz       = data.size() * sizeof(elem_type);  

            std::memcpy(dst, src, cpy_sz);
            std::advance(buf, cpy_sz);
        }

        template <class T, std::enable_if_t<types_space::is_container_wrapper_argument_v<T&&>, bool> = true>
        constexpr void put(char *& buf, T&& data) const{

            using base_type             = types_space::base_type_t<T>;
            using container_type        = typename base_type::containee_type;
            using container_size_type   = typename base_type::size_type;

            container_type container    = data.get();

            if (!network_compact_serializer::utility::is_safe_integer_castable<container_size_type>(container.size())){
                throw exception_space::bad_format();
            }

            this->put(buf, get_dgstd_serialization_header<base_type>());
            this->put(buf, dg::network_compact_serializer::utility::safe_integer_cast<container_size_type>(container.size()));

            for (const auto& e: container){
                this->put(buf, e);
            }
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(char *& buf, const T& data) const{

            using base_type = types_space::base_type_t<T>;

            this->put(buf, get_dgstd_serialization_header<base_type>());

            auto archiver = [&buf]<class ...Args>(Args&& ...args){
                (Self().put(buf, std::forward<Args>(args)), ...);
            };

            data.dg_reflect(archiver);
        }
    };

    struct DgStdBackward{

        using Self = DgStdBackward;

        template <class T, std::enable_if_t<types_space::is_dg_arithmetic_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;

            if (buf_sz < sizeof(base_type) + sizeof(types::dgstd_unsigned_serialization_header_t)){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            types::dgstd_unsigned_serialization_header_t expected_header    = network_compact_serializer::utility::SyncedEndiannessService::load<types::dgstd_unsigned_serialization_header_t>(buf);
            std::advance(buf, sizeof(types::dgstd_unsigned_serialization_header_t));
            buf_sz                                                          -= sizeof(types::dgstd_unsigned_serialization_header_t);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>(); 

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            data    = network_compact_serializer::utility::SyncedEndiannessService::load<base_type>(buf);
            std::advance(buf, sizeof(base_type));
            buf_sz  -= sizeof(base_type);
        }

        template <class T, std::enable_if_t<types_space::is_unique_ptr_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type         = types_space::base_type_t<T>;
            using containee_type    = typename types_space::base_type_t<T>::element_type;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                containee_type obj;
                this->put(buf, buf_sz, obj);
                data = std::make_unique<containee_type>(std::move(obj));
            } else{
                data = nullptr;
            }
        }

        template <class T, std::enable_if_t<types_space::is_optional_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type         = types_space::base_type_t<T>;
            using containee_type    = typename types_space::base_type_t<T>::value_type;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                containee_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                data = std::nullopt;
            }
        }

        template <class T, std::enable_if_t<types_space::is_variant_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            types::variant_index_type variant_idx;
            this->put(buf, buf_sz, variant_idx);

            constexpr size_t VARIANT_COUNT = std::variant_size_v<base_type>;
            constexpr size_t MAX_CONTAINABLE_VARIANT = static_cast<size_t>(std::numeric_limits<types::variant_index_type>::max()) + 1u;  

            static_assert(VARIANT_COUNT <= MAX_CONTAINABLE_VARIANT);

            if (variant_idx >= VARIANT_COUNT){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                (
                    [&]
                    {
                        if (IDX == network_compact_serializer::utility::safe_integer_cast<size_t>(variant_idx)){
                            std::variant_alternative_t<IDX, base_type> containee;
                            this->put(buf, buf_sz, containee);
                            data = base_type(std::in_place_index_t<IDX>{}, std::move(containee));
                        }
                    }(), ...
                );
            }(std::make_index_sequence<VARIANT_COUNT>());
        }

        template <class T, std::enable_if_t<types_space::is_expected_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            using value_type    = typename base_type::value_type;
            using error_type    = typename base_type::error_type;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            bool status = {};
            this->put(buf, buf_sz, status);

            if (status){
                value_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::in_place_t{}, std::move(obj));
            } else{
                error_type obj;
                this->put(buf, buf_sz, obj);
                data = base_type(std::unexpected(obj));
            }
        }

        template <class T, std::enable_if_t<types_space::is_utc_timepoint_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            using data_dur_t    = typename base_type::duration;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            typename types::serializable_duration_t::rep tick;

            this->put(buf, buf_sz, tick);
            auto serialization_dur = types::serializable_duration_t(tick);
            auto data_dur = utility::safe_duration_cast<data_dur_t>(serialization_dur);
            data = base_type(data_dur);
        }

        template <class T, std::enable_if_t<types_space::is_std_duration_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const
        {
            using base_type     = types_space::base_type_t<T>;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header)
            {
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            typename types::serializable_duration_t::rep tick;

            this->put(buf, buf_sz, tick);
            auto serialization_dur = types::serializable_duration_t(tick);
            data = utility::safe_duration_cast<base_type>(serialization_dur);
        }

        template <class T, std::enable_if_t<types_space::is_tuple_v<types_space::base_type_t<T>>, bool> = true>
        void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type     = types_space::base_type_t<T>;
            const auto idx_seq  = std::make_index_sequence<std::tuple_size_v<base_type>>{};

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t header             = get_dgstd_serialization_header<base_type>();

            if (header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            []<size_t ...IDX>(const char *& buf, size_t& buf_sz, T&& data, const std::index_sequence<IDX...>){
                (Self().put(buf, buf_sz, std::get<IDX>(std::forward<T>(data))), ...);
            }(buf, buf_sz, std::forward<T>(data), idx_seq);
        }

        template <class T, std::enable_if_t<types_space::is_noncpyable_linear_container_v<types_space::base_type_t<T>> || types_space::is_nonlinear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;

            types::dgstd_unsigned_serialization_header_t expected_container_header  = {};
            this->put(buf, buf_sz, expected_container_header);
            types::dgstd_unsigned_serialization_header_t container_header           = get_dgstd_serialization_header<base_type>();

            if (container_header != expected_container_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            auto sz         = types::size_type{};
            auto isrter     = network_compact_serializer::utility::get_inserter<base_type>();
            this->put(buf, buf_sz, sz); 

            if constexpr(dg::network_compact_serializer::constants::DESERIALIZATION_HAS_CLEAR_CONTAINER_RIGHT){
                data.clear();
            }

            // data.reserve(sz);
            dg::network_compact_serializer::utility::reserve_if_possible(data, sz);

            for (size_t i = 0; i < sz; ++i){
                elem_type e;
                this->put(buf, buf_sz, e);
                isrter(data, std::move(e));
            }
        }

        template <class T, std::enable_if_t<types_space::is_cpyable_linear_container_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;
            using elem_type = types_space::containee_t<base_type>;

            types::dgstd_unsigned_serialization_header_t expected_container_header  = {};
            this->put(buf, buf_sz, expected_container_header);
            types::dgstd_unsigned_serialization_header_t container_header           = get_dgstd_serialization_header<base_type>();

            if (container_header != expected_container_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            types::dgstd_unsigned_serialization_header_t expected_elem_header       = {};
            this->put(buf, buf_sz, expected_elem_header);
            types::dgstd_unsigned_serialization_header_t elem_header                = get_dgstd_serialization_header<elem_type>();

            if (elem_header != expected_elem_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            auto sz = types::size_type{};
            this->put(buf, buf_sz, sz);

            size_t cpy_sz       = sz * sizeof(elem_type); 

            if (buf_sz < cpy_sz){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            data.resize(sz);

            void * dst          = data.data();
            const void * src    = buf;

            std::memcpy(dst, src, cpy_sz);
            std::advance(buf, cpy_sz);
            buf_sz -= cpy_sz;
        }

        template <class T, std::enable_if_t<types_space::is_container_wrapper_argument_v<T&&>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type             = types_space::base_type_t<T>;
            using container_type        = typename base_type::containee_type;
            using container_size_type   = typename base_type::size_type;
            using container_base_type   = types_space::base_type_t<container_type>;
            using containee_type        = types_space::containee_t<container_base_type>; 
            using elem_type             = containee_type;

            container_type container    = data.get();

            types::dgstd_unsigned_serialization_header_t expected_container_header  = {};
            this->put(buf, buf_sz, expected_container_header);
            types::dgstd_unsigned_serialization_header_t container_header           = get_dgstd_serialization_header<base_type>();

            if (container_header != expected_container_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            auto sz     = container_size_type{};
            auto isrter = network_compact_serializer::utility::get_inserter<container_base_type>();
            this->put(buf, buf_sz, sz);

            if constexpr(dg::network_compact_serializer::constants::DESERIALIZATION_HAS_CLEAR_CONTAINER_RIGHT){
                container.clear();
            }

            dg::network_compact_serializer::utility::reserve_if_possible(container, sz);

            for (size_t i = 0u; i < static_cast<size_t>(sz); ++i){
                elem_type e;
                this->put(buf, buf_sz, e);
                isrter(container, std::move(e));
            }
        }

        template <class T, std::enable_if_t<types_space::is_reflectible_v<types_space::base_type_t<T>>, bool> = true>
        constexpr void put(const char *& buf, size_t& buf_sz, T&& data) const{

            using base_type = types_space::base_type_t<T>;

            types::dgstd_unsigned_serialization_header_t expected_header    = {};
            this->put(buf, buf_sz, expected_header);
            types::dgstd_unsigned_serialization_header_t container_header   = get_dgstd_serialization_header<base_type>();

            if (container_header != expected_header){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }

            auto archiver = [&buf, &buf_sz]<class ...Args>(Args&& ...args){
                (Self().put(buf, buf_sz, std::forward<Args>(args)), ...);
            };

            data.dg_reflect(archiver);
        }
    };
}

namespace dg::network_compact_serializer{

    constexpr auto get_dgstd_serialization_identifier() noexcept -> std::string_view
    {
        return "";
    }

    template <class SizeType, class T>
    constexpr auto wrap_container(T&& container) noexcept -> dg::network_compact_serializer::types_space::ContainerWrapper<T, SizeType>{

        return dg::network_compact_serializer::types_space::ContainerWrapper<T, SizeType>(std::forward<T>(container));
    }

    template <class T>
    constexpr auto size(const T& obj) -> size_t{

        return dg::network_compact_serializer::archive::Counter{}.count(obj);
    }

    template <class T>
    constexpr auto serialize_into(char * buf, const T& obj) -> char *{

        dg::network_compact_serializer::archive::Forward{}.put(buf, obj);
        return buf;
    } 

    template <class T>
    constexpr auto deserialize_into(T& obj, const char * buf) -> const char *{

        dg::network_compact_serializer::archive::Backward{}.put(buf, obj);
        return buf;
    }

    template <class T>
    constexpr auto integrity_size(const T& obj) -> size_t{

        return network_compact_serializer::size(obj) + network_compact_serializer::size(types::hash_type{});
    }

    template <class T>
    constexpr auto integrity_serialize_into(char * buf, const T& obj, uint32_t secret = 0u) -> char *{ 

        char * first                = buf;
        char * last                 = dg::network_compact_serializer::serialize_into(first, obj);
        types::hash_type hashed     = dg::network_compact_serializer::utility::hash(first, std::distance(first, last), secret);
        char * llast                = dg::network_compact_serializer::serialize_into(last, hashed);

        return llast;
    }

    template <class T>
    constexpr void integrity_deserialize_into(T& obj, const char * buf, size_t sz, uint32_t secret = 0u){

        if (sz < dg::network_compact_serializer::size(types::hash_type{})){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        size_t content_sz           = sz - dg::network_compact_serializer::size(types::hash_type{});
        const char * first          = buf;
        const char * last           = std::next(first, content_sz);
        types::hash_type expecting  = {};
        types::hash_type reality    = dg::network_compact_serializer::utility::hash(first, content_sz, secret);

        dg::network_compact_serializer::deserialize_into(expecting, last);

        if (expecting != reality){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        {
            const char * iterating_ptr  = first;
            size_t deserializing_sz     = content_sz;
            dg::network_compact_serializer::archive::SafeBackward{}.put(iterating_ptr, deserializing_sz, obj);

            if (deserializing_sz != 0u){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }
        }
    }

    template <class T>
    constexpr auto capintegrity_size(const T& obj) -> size_t{

        return dg::network_compact_serializer::size(uint64_t{}) + dg::network_compact_serializer::integrity_size(obj);
    }

    template <class T>
    constexpr auto capintegrity_serialize_into(char * buf, const T& obj, uint32_t secret = 0u) -> char *{

        char * first    = std::next(buf, dg::network_compact_serializer::size(uint64_t{}));
        char * last     = dg::network_compact_serializer::integrity_serialize_into(first, obj, secret);
        uint64_t sz     = std::distance(first, last);
        dg::network_compact_serializer::serialize_into(buf, sz); 

        return last;
    }

    template <class T>
    constexpr auto capintegrity_deserialize_into(T& obj, const char * buf, size_t sz, uint32_t secret = 0u) -> const char *{

        if (sz < dg::network_compact_serializer::size(uint64_t{})){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        uint64_t obj_sz     = {};
        const char * first  = dg::network_compact_serializer::deserialize_into(obj_sz, buf);
        size_t remaining_sz = std::distance(first, std::next(buf, sz));
        
        if (obj_sz > remaining_sz){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        dg::network_compact_serializer::integrity_deserialize_into(obj, first, obj_sz, secret);

        return std::next(first, obj_sz);
    }

    template <class T>
    constexpr auto dgstd_size(const T& obj) -> size_t{

        return dg::network_compact_serializer::archive::DgStdCounter{}.count(obj) + network_compact_serializer::size(types::hash_type{}) + network_compact_serializer::size(types::version_control_t{});
    }

    template <class T>
    constexpr auto dgstd_serialize_into(char * buf, const T& obj, uint32_t secret = 0u) -> char *{

        char * first                = buf;
        char * last                 = first;
        dg::network_compact_serializer::archive::DgStdForward{}.put(last, obj);
        types::hash_type hashed     = dg::network_compact_serializer::utility::hash(first, std::distance(first, last), secret);
        char * llast                = dg::network_compact_serializer::serialize_into(last, hashed);
        char * lllast               = dg::network_compact_serializer::serialize_into(llast, dg::network_compact_serializer::constants::SERIALIZATION_VERSION_CONTROL); 

        return lllast;
    }

    template <class T>
    constexpr void dgstd_deserialize_into(T& obj, const char * buf, size_t sz, uint32_t secret = 0u){

        if (sz < dg::network_compact_serializer::size(types::hash_type{}) + dg::network_compact_serializer::size(types::version_control_t{})){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        size_t content_sz                       = sz - (dg::network_compact_serializer::size(types::hash_type{}) + dg::network_compact_serializer::size(types::version_control_t{}));

        const char * content_first              = buf;
        const char * content_last               = std::next(content_first, content_sz);

        const char * hash_first                 = content_last;
        types::hash_type expecting_value        = {};
        types::hash_type reality_value          = dg::network_compact_serializer::utility::hash(content_first, content_sz, secret);

        const char * version_control_first      = dg::network_compact_serializer::deserialize_into(expecting_value, hash_first);
        types::version_control_t cur_ver        = {};
        types::version_control_t expected_ver   = constants::SERIALIZATION_VERSION_CONTROL;

        dg::network_compact_serializer::deserialize_into(cur_ver, version_control_first);

        if (cur_ver != expected_ver){
            throw dg::network_compact_serializer::exception_space::bad_version_control();
        }

        if (expecting_value != reality_value){
            throw dg::network_compact_serializer::exception_space::corrupted_format();
        }

        {
            const char * iterating_ptr  = content_first;
            size_t deserializing_sz     = content_sz;
            dg::network_compact_serializer::archive::DgStdBackward{}.put(iterating_ptr, deserializing_sz, obj);

            if (deserializing_sz != 0u){
                throw dg::network_compact_serializer::exception_space::corrupted_format();
            }
        }
    }

    template <class Stream, class T, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto serialize(const T& obj) -> Stream{

        Stream stream{};
        stream.resize(dg::network_compact_serializer::size(obj));
        dg::network_compact_serializer::serialize_into(stream.data(), obj);

        return stream;
    }

    template <class T, class Stream, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto deserialize(const Stream& stream) -> T{

        T rs;
        dg::network_compact_serializer::deserialize_into(rs, stream.data());

        return rs;
    }

    template <class Stream, class T, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto integrity_serialize(const T& obj, uint32_t secret = 0u) -> Stream{

        Stream stream{};
        stream.resize(dg::network_compact_serializer::integrity_size(obj));
        dg::network_compact_serializer::integrity_serialize_into(stream.data(), obj, secret);
        
        return stream;
    }

    template <class T, class Stream, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto integrity_deserialize(const Stream& stream, uint32_t secret = 0u) -> T{

        T rs;
        dg::network_compact_serializer::integrity_deserialize_into(rs, stream.data(), stream.size(), secret);

        return rs;
    }

    template <class Stream, class T, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto dgstd_serialize(const T& obj, uint32_t secret = 0u) -> Stream{

        Stream stream{};
        stream.resize(dg::network_compact_serializer::dgstd_size(obj));
        dg::network_compact_serializer::dgstd_serialize_into(stream.data(), obj, secret);

        return stream;
    }

    template <class T, class Stream, std::enable_if_t<types_space::is_streamable_implemented_v<Stream>, bool> = true>
    constexpr auto dgstd_deserialize(const Stream& stream, uint32_t secret = 0u) -> T{

        T rs;
        dg::network_compact_serializer::dgstd_deserialize_into(rs, stream.data(), stream.size(), secret);

        return rs;
    }
}

#endif