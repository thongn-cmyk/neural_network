#define STRONG_MEMORY_ORDERING_FLAG true

#include <serializer/compressed_serializer.h>
#include <iostream>
#include <random>
#include <algorithm>
#include <utility>
#include <bit>
#include <type_traits>

template <class T, class = void>
struct is_std_fixed_size_container: std::false_type{};

template <class T>
struct is_std_fixed_size_container<T, std::void_t<decltype(std::tuple_size<T>::value)>>: std::true_type{};

template <class T>
struct is_std_optional: std::false_type{};

template <class ...Args>
struct is_std_optional<std::optional<Args...>>: std::true_type{}; 

template <class T>
struct is_std_basic_string: std::false_type{};

template <class ...Args>
struct is_std_basic_string<std::basic_string<char, std::char_traits<char>, Args...>>: std::true_type{};

template <class T>
struct is_std_vector: std::false_type{};

template <class ...Args>
struct is_std_vector<std::vector<Args...>>: std::true_type{};

template <class T>
struct is_std_unordered_map: std::false_type{};

template <class ...Args>
struct is_std_unordered_map<std::unordered_map<Args...>>: std::true_type{}; 

template <class T>
struct is_std_unordered_set: std::false_type{};

template <class ...Args>
struct is_std_unordered_set<std::unordered_set<Args...>>: std::true_type{};

template <class T>
struct is_std_map: std::false_type{};

template <class ...Args>
struct is_std_map<std::map<Args...>>: std::true_type{}; 

template <class T>
struct is_std_set: std::false_type{};

template <class ...Args>
struct is_std_set<std::set<Args...>>: std::true_type{};

template <class T>
static inline constexpr bool is_std_fixed_size_container_v      = is_std_fixed_size_container<T>::value;

template <class T>
static inline constexpr bool is_std_optional_v                  = is_std_optional<T>::value;

template <class T>
static inline constexpr bool is_std_basic_string_v              = is_std_basic_string<T>::value;

template <class T>
static inline constexpr bool is_std_vector_v                    = is_std_vector<T>::value;

template <class T>
static inline constexpr bool is_std_unordered_map_v             = is_std_unordered_map<T>::value;

template <class T>
static inline constexpr bool is_std_unordered_set_v             = is_std_unordered_set<T>::value;

template <class T>
static inline constexpr bool is_std_map_v                       = is_std_map<T>::value;

template <class T>
static inline constexpr bool is_std_set_v                       = is_std_set<T>::value;

template <size_t SZ, class Callback>
void randomize_type(const std::integral_constant<size_t, SZ>, Callback&& callback)
{
    static_assert(SZ != 0u);

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});

    if constexpr(SZ == 1u)
    {
        const size_t LEAF_ENUMERATION_SZ    = 5u;
        size_t leaf_enumeration_idx         = randomizer() % LEAF_ENUMERATION_SZ;

        switch (leaf_enumeration_idx)
        {
            case 0:
            {
                callback(int{});
                return;
            }
            case 1:
            {
                callback(size_t{});
                return;
            }
            case 2:
            {
                callback(char{});
                return;
            }
            case 3:
            {
                callback(uint32_t{});
                return;
            }
            case 4:
            {
                callback(std::string{});
                return;
            };
            default:
            {
                std::abort();
            }
        }
    }
    else
    {
        const size_t ENUMERATION_SZ = 8u;
        size_t enumeration_idx      = randomizer() % ENUMERATION_SZ;

        auto other_callback = [enumeration_idx, &callback]<class T>(T)
        {
            switch (enumeration_idx)
            {
                case 0:
                {
                    callback(std::vector<T>{});
                    return;
                }
                case 1:
                {
                    if (randomizer() % 2 == 0)
                    {
                        callback(std::unordered_map<size_t, T>{});
                    }
                    else
                    {
                        callback(std::unordered_map<std::string, T>{});
                    }

                    return;
                }
                case 2:
                {
                    if (randomizer() % 2 == 0)
                    {
                        callback(std::map<size_t, T>{});
                    }
                    else
                    {
                        callback(std::map<std::string, T>{});
                    }

                    return;
                }
                case 3:
                {
                    if (randomizer() % 2 == 0)
                    {
                        callback(std::unordered_set<size_t>{});
                    }
                    else
                    {
                        callback(std::unordered_set<std::string>{});
                    }

                    return;
                }
                case 4:
                {
                    if (randomizer() % 2 == 0)
                    {
                        callback(std::set<size_t>{});
                    }
                    else
                    {
                        callback(std::set<std::string>{});
                    }

                    return;
                }
                case 5:
                {
                    callback(std::optional<T>{});
                    return;
                }
                case 6:
                {
                    callback(std::pair<T, T>{});
                    return;
                }
                case 7:
                {
                    callback(std::tuple<T, T, T>{});
                    return;
                }
                default:
                {
                    std::abort();
                }
            }
        };

        randomize_type(std::integral_constant<size_t, SZ - 1u>{}, other_callback);
    }
}

template <class = void>
static inline constexpr bool FALSE_VAL = false;

template <class T>
auto recursive_random_populate() -> T
{
    using base_t = std::decay_t<T>;

    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});
    const size_t CONTAINER_SZ_RANGE = size_t{1} << 4;

    if constexpr(is_std_vector_v<T>)
    {
        size_t container_sz = randomizer() % CONTAINER_SZ_RANGE;
        using value_t = typename base_t::value_type;
        base_t result{};

        for (size_t i = 0u; i < container_sz; ++i)
        {
            result.push_back(recursive_random_populate<value_t>());
        }

        return result;
    }
    else if constexpr(is_std_unordered_set_v<T> || is_std_set_v<T>)
    {
        size_t container_sz = randomizer() % CONTAINER_SZ_RANGE;
        using value_t = typename base_t::value_type;
        base_t result{};

        for (size_t i = 0u; i < container_sz; ++i)
        {
            result.insert(recursive_random_populate<value_t>());
        }

        return result;
    }
    else if constexpr(is_std_unordered_map_v<T> || is_std_map_v<T>)
    {
        size_t container_sz = randomizer() % CONTAINER_SZ_RANGE;

        using key_t     = typename base_t::key_type;
        using mapped_t  = typename base_t::mapped_type;
        
        base_t result{};

        for (size_t i = 0u; i < container_sz; ++i)
        {
            result[recursive_random_populate<key_t>()] = recursive_random_populate<mapped_t>();
        }

        return result;
    }
    else if constexpr(is_std_optional_v<T>)
    {
        bool coin_flip = randomizer() % 2;

        if (coin_flip)
        {
            using value_t = typename base_t::value_type;

            return recursive_random_populate<value_t>();
        }
        else
        {
            return std::nullopt;
        }
    }
    else if constexpr(is_std_fixed_size_container_v<T>)
    {
        const auto idx_seq = std::make_index_sequence<std::tuple_size_v<base_t>>();
        base_t obj{};

        [&]<size_t ...IDX>(const std::index_sequence<IDX...>)
        {
            (
                [&]
                {
                    (void) IDX;
                    using value_t = std::decay_t<decltype(std::get<IDX>(obj))>;
                    std::get<IDX>(obj) = recursive_random_populate<value_t>();
                }(), ...
            );
        }(idx_seq);

        return obj;
    }
    else if constexpr(is_std_basic_string_v<T>)
    {
        size_t container_sz = randomizer() % CONTAINER_SZ_RANGE;
        base_t obj{};

        for (size_t i = 0u; i < container_sz; ++i)
        {
            obj.push_back(recursive_random_populate<char>());
        }

        return obj;
    }
    else if constexpr(std::is_arithmetic_v<T>)
    {
        if constexpr(sizeof(base_t) == 1u)
        {
            return std::bit_cast<base_t>(static_cast<uint8_t>(randomizer()));
        }
        else if constexpr(sizeof(base_t) == 2u)
        {
            return std::bit_cast<base_t>(static_cast<uint16_t>(randomizer()));
        }
        else if constexpr(sizeof(base_t) == 4u)
        {
            return std::bit_cast<base_t>(static_cast<uint32_t>(randomizer()));
        }
        else if constexpr(sizeof(base_t) == 8u)
        {
            return std::bit_cast<base_t>(static_cast<uint64_t>(randomizer()));
        }
        else
        {
            static_assert(FALSE_VAL<>);
        }
    }
    else
    {
        static_assert(FALSE_VAL<>);
    }
}

void test_one_dg_buf()
{
    auto test_sz    = std::integral_constant<size_t, 3u>{};
    auto callback   = []<class T>(T)
    {
        T obj = recursive_random_populate<T>();

        {
            auto serialized_obj     = compressed_serializer::to_str(compressed_serializer::best_serialize<std::string>(obj));
            auto deserialized_obj   = compressed_serializer::deserialize<T>(serialized_obj);

            if (deserialized_obj != obj)
            {
                std::cout << "mayday, mismatch representation 2" << std::endl;
                std::abort();
            }
        }

        {
            auto serialized_obj     = compressed_serializer::to_str(compressed_serializer::to_generic(compressed_serializer::normal_serialize<std::string>(obj)));
            auto deserialized_obj   = compressed_serializer::deserialize<T>(serialized_obj);

            if (deserialized_obj != obj)
            {
                std::cout << "mayday, mismatch representation 2" << std::endl;
                std::abort();
            }
        }

        {
            auto serialized_obj     = compressed_serializer::to_str(compressed_serializer::to_generic(compressed_serializer::huffman_serialize<std::string>(obj)));
            auto deserialized_obj   = compressed_serializer::deserialize<T>(serialized_obj);

            if (deserialized_obj != obj)
            {
                std::cout << "mayday, mismatch representation 2" << std::endl;
                std::abort();
            }
        }
    };

    randomize_type(test_sz, callback);
}

void test_dg_buf()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_dg_buf();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }
}

int main()
{
    //we'll try to get this inplace buffer done, because this is fairly important
    //essentially, this is the backbone of cuda or host memory access, we are not lacking computes
    //point is we are able to describe complex semantics without the conventional hand-unroll version of char * and friends
    //we are civil

    test_dg_buf();
}