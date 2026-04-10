#ifndef JG_POWER_OF_TWO_GROWTH_POLICY_HPP
#define JG_POWER_OF_TWO_GROWTH_POLICY_HPP

#include <cassert>
#include <cstddef>
#include <limits>
#include <bit>
#include <type_traits>

namespace jg::details
{

struct power_of_two_growth_policy
{

    private:

        template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
        static constexpr auto ulog2(T val) noexcept -> size_t
        {
            return static_cast<size_t>(sizeof(T) * CHAR_BIT - 1) - static_cast<size_t>(std::countl_zero(val));
        }

        template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
        static constexpr auto least_pow2_greater_equal_than(T val) noexcept -> T
        {
            if (val == 0u) [[unlikely]]{
                return 1u;
            }

            size_t max_log2     = ulog2(val);
            size_t min_log2     = std::countr_zero(val);
            size_t cand_log2    = max_log2 + ((max_log2 ^ min_log2) != 0u);

            return T{1u} << cand_log2;
        } 

    public:

        static constexpr auto compute_index(std::size_t hash, std::size_t capacity) -> std::size_t
        {
            return hash & (capacity - std::size_t{1});
        }

        static constexpr auto compute_closest_capacity(std::size_t min_capacity) -> std::size_t
        {
            return least_pow2_greater_equal_than(min_capacity);
        }

        static constexpr auto minimum_capacity() -> std::size_t { return 8u; }
};

} // namespace jg::details

#endif // JG_POWER_OF_TWO_GROWTH_POLICY_HPP
