#ifndef __NETWORK_RANDOMIZER_H__
#define __NETWORK_RANDOMIZER_H__

//define HEADER_CONTROL 4

#include <stdlib.h>
#include <stdint.h>
#include <utility>
#include "network_concurrency.h" 
#include <limits.h>
#include <bit>
#include <random>
#include "stdx.h"
#include <type_traits>
#include "network_type_traits_x.h"

namespace dg_sock::network_randomizer{

    struct BitRandomizer{
        
        private:

            struct RandomizationUnit{
                uint64_t value;
                size_t bit_precision;
                std::mt19937_64 randomizer;
            };

            static inline std::vector<RandomizationUnit> table{};

            static inline void re_randomize(RandomizationUnit& random_unit) noexcept{

                random_unit.value           = static_cast<std::mt19937_64&>(random_unit.randomizer)();
                random_unit.bit_precision   = sizeof(uint64_t) * CHAR_BIT;
            }

        public: 

            static void init()
            {
                std::vector<RandomizationUnit> rs{};

                for (size_t i = 0u; i < dg_sock::network_concurrency::get_thread_count(); ++i){
                    size_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count(); 
                    std::mt19937_64 randomizer{seed};
                    rs.push_back(RandomizationUnit{randomizer(), sizeof(uint64_t) * CHAR_BIT, randomizer});
                }

                table = rs;
            }

            static void deinit() noexcept
            {
                table = {};
            }

            template <size_t BIT_SIZE>
            static inline auto randomize_bit(const std::integral_constant<size_t, BIT_SIZE>) noexcept -> uint64_t{

                static_assert(BIT_SIZE != 0);
                static_assert(BIT_SIZE <= sizeof(uint64_t) * CHAR_BIT);

                RandomizationUnit& unit = table[dg_sock::network_concurrency::this_thread_idx()];

                if (unit.bit_precision < BIT_SIZE){
                    re_randomize(unit);
                }

                uint64_t ret_value  = stdxx::low_bit<BIT_SIZE>(unit.value);
                unit.bit_precision  -= BIT_SIZE;
                unit.value          >>= BIT_SIZE;

                return ret_value;
            }
    };

    void init()
    {
        BitRandomizer::init();
    }

    void deinit() noexcept
    {
        BitRandomizer::deinit();
    }

    template <size_t RANGE_SZ>
    auto randomize_xrange(const std::integral_constant<size_t, RANGE_SZ>) noexcept -> size_t{

        static_assert(RANGE_SZ != 0u);

        constexpr size_t BIT_SIZE = stdxx::ulog2(RANGE_SZ) + 1u;
        uint64_t rs = BitRandomizer::randomize_bit(std::integral_constant<size_t, BIT_SIZE>{});

        if constexpr(stdxx::is_pow2(RANGE_SZ)){
            return rs;
        } else{
            return rs % RANGE_SZ;
        }
    }

    template <size_t FIRST, size_t LAST>
    auto randomize_range(const std::integral_constant<size_t, FIRST>, const std::integral_constant<size_t, LAST>) -> size_t{

        static_assert(LAST > FIRST);
        return FIRST + randomize_xrange(std::integral_constant<size_t, LAST - FIRST>{});
    }

    auto randomize_bool() noexcept -> bool{

        uint64_t rs = BitRandomizer::randomize_bit(std::integral_constant<size_t, 1u>{}); 
        return static_cast<bool>(rs);
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    auto randomize_int() noexcept -> T{

        using unsigned_ver_t = network_type_traits_x::unsigned_of_byte_t<sizeof(T)>;
        unsigned_ver_t rs = BitRandomizer::randomize_bit(std::integral_constant<size_t, sizeof(unsigned_ver_t) * CHAR_BIT>{});
        return std::bit_cast<T>(rs);
    }

    template <class T = std::string, std::enable_if_t<network_type_traits_x::is_basic_string_v<T>, bool> = true>
    auto randomize_string(size_t sz) -> T{

        T rs{};
        rs.resize(sz);
        std::generate(rs.begin(), rs.end(), []{return randomize_int<char>();});

        return rs;
    }
} 

#endif