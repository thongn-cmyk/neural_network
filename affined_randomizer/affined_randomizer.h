#ifndef __DG_AFFINED_RANDOMIZER_H__
#define __DG_AFFINED_RANDOMIZER_H__

//define HEADER_CONTROL 4

#include <stdlib.h>
#include <stdint.h>
#include <utility>
#include <concurrency_base/concurrency_base.h>
#include <limits.h>
#include <bit>
#include <random>
#include <stl_extension/stdx.h>
#include <stl_extension/type_traits_x.h>
#include <stl_extension/hasher.h>

namespace affined_randomizer
{
    struct RandomizationSeed
    {
        uint64_t thr_idx;
        uint64_t time_seed;
        uint64_t stack_clue;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(thr_idx, time_seed, stack_clue);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(thr_idx, time_seed, stack_clue);
        }
    };

    struct BitRandomizer
    {
        private:

            struct RandomizationUnit
            {
                uint64_t value;
                size_t bit_precision;
                std::mt19937_64 randomizer;
            };

            static inline std::vector<RandomizationUnit> table{};

            static inline void re_randomize(RandomizationUnit& random_unit) noexcept
            {
                random_unit.value           = static_cast<std::mt19937_64&>(random_unit.randomizer)();
                random_unit.bit_precision   = std::numeric_limits<uint64_t>::digits;
            }

        public:

            static void init()
            {
                stdx::memtransaction_guard tx_grd;

                std::vector<RandomizationUnit> rs{};

                for (size_t i = 0u; i < concurrency_base::get_thread_count(); ++i)
                {
                    size_t seed = hasher::hash_reflectible(RandomizationSeed
                    {
                        .thr_idx    = i,
                        .time_seed  = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
                        .stack_clue = std::bit_cast<uint64_t>(static_cast<void *>(&rs))
                    });

                    std::mt19937_64 randomizer{seed};
                    rs.push_back(RandomizationUnit{randomizer(), sizeof(uint64_t) * CHAR_BIT, randomizer});
                }

                table = rs;
            }

            static void deinit() noexcept
            {
                stdx::memtransaction_guard tx_grd;

                table = {};
            }

            template <size_t BIT_SIZE>
            static inline auto randomize_bit(const std::integral_constant<size_t, BIT_SIZE>) noexcept -> uint64_t
            {
                static_assert(BIT_SIZE != 0);
                static_assert(BIT_SIZE <= sizeof(uint64_t) * CHAR_BIT);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (concurrency_base::this_thread_idx() >= table.size())
                    {
                        std::abort();
                    }
                }

                RandomizationUnit& unit = table[concurrency_base::this_thread_idx()];

                if (unit.bit_precision < BIT_SIZE)
                {
                    re_randomize(unit);
                }

                uint64_t ret_value  = stdx::low_bit<BIT_SIZE>(unit.value);
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
    auto randomize_xrange(const std::integral_constant<size_t, RANGE_SZ>) noexcept -> size_t
    {
        static_assert(RANGE_SZ != 0u);

        constexpr size_t BIT_SIZE   = (sizeof(size_t) * CHAR_BIT) - std::countl_zero(RANGE_SZ);
        uint64_t rs                 = BitRandomizer::randomize_bit(std::integral_constant<size_t, BIT_SIZE>{});

        return rs % RANGE_SZ;
    }

    template <size_t FIRST, size_t LAST>
    auto randomize_range(const std::integral_constant<size_t, FIRST>, const std::integral_constant<size_t, LAST>) -> size_t
    {
        static_assert(LAST > FIRST);
        return FIRST + randomize_xrange(std::integral_constant<size_t, LAST - FIRST>{});
    }

    auto randomize_bool() noexcept -> bool
    {
        uint64_t rs = BitRandomizer::randomize_bit(std::integral_constant<size_t, 1u>{}); 
        return static_cast<bool>(rs);
    }

    template <class T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
    auto randomize_int() noexcept -> T
    {
        using unsigned_ver_t = type_traits_x::unsigned_of_byte_t<sizeof(T)>;
        unsigned_ver_t rs = BitRandomizer::randomize_bit(std::integral_constant<size_t, static_cast<size_t>(sizeof(unsigned_ver_t) * CHAR_BIT)>{});

        return std::bit_cast<T>(rs);
    }

    template <class T = std::string, std::enable_if_t<type_traits_x::is_basic_string_v<T>, bool> = true>
    auto randomize_string(size_t sz) -> T
    {
        T rs{};

        rs.resize(sz);
        std::generate(rs.begin(), rs.end(), []{return randomize_int<char>();});

        return rs;
    }
} 

#endif