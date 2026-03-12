#ifndef __TAYLOR_COEFFICIENT_RANDOMIZER_H__
#define __TAYLOR_COEFFICIENT_RANDOMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <vector>
#include <stdexcept>

namespace taylor_coefficient_randomizer
{
    //I guess that we'd need to support (1): one dimensional sin(x)/x coefficient seed (skew support)
    //                                  (2): multidimensional sin(x)/x coefficient seed (skew support)

    //                                  assume there are common curvy shapes to sort the projection space into places, we'd want to
    //                                  (3): common one dimensional projection seed, I dont know what common means (catch support, linear support)
    //                                  (4): common multidimensional projection seed (catch support, linear support)

    //                                  (5): a*sin(b*x + x0) + y0 coefficient seed (frequency support)
    //                                  (6): a*sin^k(b*x + x0) + y0 coefficient seed (frequency support)

    //I guess what we'd want to do apart from doing known curves is a cosine database of coefficient randomizer, and skew randomize in the direction, another flip a coin
    //I guess that our scientific work would involve that today, because these are theoretical curves that take too high of an order to be realistic
    //assume that we have a coefficient vector, we randomize in the direction, we'd get a signal of good or bad, we'd train our cosine space by using that, and skew randomize to mimic the statistics
    //we'd do a discretization of space, in the radian coordinate, we'd have 1 cube of 10 discrete unit, or 1 cube of 100 discrete unit, we'd return the boundaries of the polygon, and we'd continue the randomization in the space, let's do that

    auto factorial_base(size_t factorial_idx) -> __uint128_t 
    {
        if (factorial_idx == 0u)
        {
            return 1;
        }

        return factorial_base(factorial_idx - 1) * factorial_idx;
    }

    auto factorial(size_t factorial_idx) -> __uint128_t
    {
        const size_t MAX_FACTORIAL_IDX = 34;
        static std::vector<__uint128_t> factorial_table([]
        {
            std::vector<__uint128_t> rs(MAX_FACTORIAL_IDX + 1);

            for (size_t i = 0u; i < rs.size(); ++i)
            {
                rs[i] = factorial_base(i);
            }

            return rs;
        }());

        if (factorial_idx > MAX_FACTORIAL_IDX)
        {
            throw std::runtime_error("bad factorial idx, max numeric precision reached");
        }

        return factorial_table[factorial_idx];
    }

    template <class FloatType>
    void taylor_add(const FloatType * lhs, size_t lhs_sz,
                    const FloatType * rhs, size_t rhs_sz,
                    FloatType * __restrict__ rs, size_t& rs_sz, size_t rs_cap)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        size_t tentative_sz         = std::max(lhs_sz, rhs_sz);
        size_t actual_sz            = std::min(tentative_sz, rs_cap);
        size_t tentative_common_sz  = std::min(lhs_sz, rhs_sz);
        size_t actual_common_sz     = std::min(tentative_common_sz, actual_sz);

        rs_sz                       = actual_sz;

        for (size_t i = 0u; i < actual_common_sz; ++i)
        {
            rs[i] = lhs[i] + rhs[i];
        }

        for (size_t i = actual_common_sz; i < actual_sz; ++i)
        {
            rs[i] = 0;

            if (i < lhs_sz)
            {
                rs[i] += lhs[i];
            }

            if (i < rhs_sz)
            {
                rs[i] += rhs[i];
            }
        }
    }

    template <class FloatType>
    void taylor_sub(const FloatType * lhs, size_t lhs_sz,
                    const FloatType * rhs, size_t rhs_sz,
                    FloatType * __restrict__ rs, size_t& rs_sz, size_t rs_cap)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        size_t tentative_sz         = std::max(lhs_sz, rhs_sz);
        size_t actual_sz            = std::min(tentative_sz, rs_cap);
        size_t tentative_common_sz  = std::min(lhs_sz, rhs_sz);
        size_t actual_common_sz     = std::min(tentative_common_sz, actual_sz);

        rs_sz                       = actual_sz;

        for (size_t i = 0u; i < actual_common_sz; ++i)
        {
            rs[i] = lhs[i] - rhs[i];
        }

        for (size_t i = actual_common_sz; i < actual_sz; ++i)
        {
            rs[i] = 0;

            if (i < lhs_sz)
            {
                rs[i] += lhs[i];
            }

            if (i < rhs_sz)
            {
                rs[i] -= rhs[i];
            }
        }
    }

    template <class FloatType>
    void taylor_convolution(const FloatType * lhs, size_t lhs_sz,
                            const FloatType * rhs, size_t rhs_sz,
                            FloatType * __restrict__ rs, size_t& rs_sz, size_t rs_cap)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (lhs_sz == 0u)
        {
            throw std::invalid_argument("taylor coefficient lhs array cannot be empty");
        }

        if (rhs_sz == 0u)
        {
            throw std::invalid_argument("taylor coefficient rhs array cannot be empty");
        }

        size_t lhs_max_pow      = lhs_sz - 1u;
        size_t rhs_max_pow      = rhs_sz - 1u;
        size_t rs_max_pow       = lhs_max_pow + rhs_max_pow;
        size_t rs_tentative_sz  = rs_max_pow + 1u;
        rs_sz                   = std::min(rs_tentative_sz, rs_cap);

        std::fill(rs, std::next(rs, rs_sz), 0);

        for (size_t i = 0u; i < lhs_sz; ++i)
        {
            for (size_t j = 0u; j < rhs_sz; ++j)
            {
                size_t idx = i + j;

                if (idx >= rs_sz)
                {
                    continue;
                }

                FloatType lhs_coeff_value   = static_cast<FloatType>(1) / factorial(i);
                FloatType rhs_coeff_value   = static_cast<FloatType>(1) / factorial(j);
                FloatType slot_coeff_value  = static_cast<FloatType>(1) / factorial(idx);
                FloatType inc_value         = (static_cast<FloatType>(lhs[i]) * rhs[j]) * lhs_coeff_value * rhs_coeff_value / slot_coeff_value;

                rs[idx]                     += inc_value;
            }
        }
    }

    template <class FloatType>
    void offset_taylor_to_taylor_coefficient_sequence(const FloatType * offset_taylor_sequence, size_t offset_taylor_sequence_sz, size_t offset_sz,
                                                      FloatType * result_sequence)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < offset_taylor_sequence_sz; ++i)
        {
            size_t src_fact_idx         = i + offset_sz;
            size_t dst_fact_idx         = i;

            FloatType src_coeff_value   = static_cast<FloatType>(1) / factorial(src_fact_idx);
            FloatType dst_coeff_value   = static_cast<FloatType>(1) / factorial(dst_fact_idx);

            result_sequence[i]          = offset_taylor_sequence[i] * (src_coeff_value / dst_coeff_value);
        }        
    }

    template <class FloatType>
    void left_advance(FloatType * rs, size_t rs_sz, size_t advance_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        size_t actual_advance_offset = std::min(rs_sz, advance_sz);
        size_t first = 0u;

        for (size_t i = actual_advance_offset; i < rs_sz; ++i)
        {
            rs[first++] = rs[i];
        }

        std::fill(std::next(rs, first), std::next(rs, rs_sz), 0);
    }

    template <class FloatType>
    void get_sin_coefficient_sequence(FloatType * rs, size_t rs_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < rs_sz; ++i)
        {
            bool is_even            = i % 2 == 0;
            size_t signness_slot    = i / 2;
            bool is_even_signness   = signness_slot % 2 == 0;

            rs[i]                   = (is_even ? 0 : 1) * (is_even_signness ? 1: -1);
        }
    }

    template <class FloatType>
    void get_cos_coefficient_sequence(FloatType * rs, size_t rs_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < rs_sz; ++i)
        {
            bool is_even            = (i + 1) % 2 == 0;
            size_t signness_slot    = (i + 1) / 2;
            bool is_even_signness   = signness_slot % 2 == 0;

            rs[i]                   = (is_even ? 0 : 1) * (is_even_signness ? 1: -1);
        }
    }

    template <class FloatType>
    void get_exp_coefficient_sequence(FloatType * rs, size_t rs_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        for (size_t i = 0u; i < rs_sz; ++i)
        {
            rs[i] = 1;
        }
    }

    template <class FloatType>
    void get_sinx_x_coefficient_sequence(FloatType * rs, size_t rs_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        std::vector<FloatType> tmp(rs_sz);

        size_t actual_rs_sz;

        get_cos_coefficient_sequence(tmp.data(), rs_sz);
        offset_taylor_to_taylor_coefficient_sequence(tmp.data(), rs_sz, 1u,
                                                     rs);
    }

    template <class FloatType>
    void get_sinx_x_2_coefficient_sequence(FloatType * rs, size_t rs_sz)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (rs_sz <= 1)
        {
            throw std::invalid_argument("bad coefficient size, at least 2 is required for sinx_x_2");
        }

        size_t half_sz = rs_sz / 2;
        std::vector<FloatType> sinx_x_seq(half_sz);
        get_sinx_x_coefficient_sequence(sinx_x_seq.data(), half_sz);

        size_t actual_rs_sz = 0u;
        taylor_convolution(sinx_x_seq.data(), sinx_x_seq.size(),
                           sinx_x_seq.data(), sinx_x_seq.size(),
                           rs, actual_rs_sz, rs_sz);

        std::fill(std::next(rs, actual_rs_sz), std::next(rs, rs_sz), 0);
    }

    template <class FloatType>
    void get_constant_coefficient_sequence(FloatType * rs, size_t rs_sz, FloatType c)
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (rs_sz == 0u)
        {
            throw std::invalid_argument("bad coefficient size, at least 1 is required for constant");
        }

        rs[0] = c;
        std::fill(std::next(rs), std::next(rs, rs_sz), 0);
    }

    template <class FloatType>
    void get_x_pow_constant_coefficient_sequence(FloatType * rs, size_t rs_sz, size_t pow)
    {
        if (pow >= rs_sz)
        {
            throw std::invalid_argument("bad coefficient size, out of bound access");
        }

        std::fill(rs, std::next(rs, rs_sz), 0);
        rs[pow] = FloatType(1) / factorial(pow);
    }

    template <class FloatType>
    auto to_string(FloatType * rs, size_t rs_sz) -> std::string
    {
        static_assert(std::is_floating_point_v<FloatType>);
        std::string rs_str{};

        for (size_t i = 0u; i < rs_sz; ++i)
        {
            std::string factorial_str   = std::to_string(static_cast<size_t>(factorial(i)));
            std::string x_str           = "x^" + std::to_string(i);
            std::string coeff_str       = std::to_string(rs[i]);

            rs_str                      += "1/"+factorial_str + "*" + coeff_str + "*" + x_str;

            if (i + 1 != rs_sz)
            {
                rs += '+';
            }
        }

        return rs_str;
    }

    template <class FloatType>
    void get_identity_coefficient_sequence()
    {

    }
}

#endif