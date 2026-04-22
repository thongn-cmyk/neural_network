#ifndef __GLOBAL_STRING_ENCODER_HEX_ENCODER_H__
#define __GLOBAL_STRING_ENCODER_HEX_ENCODER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <vector>

namespace global_string_encoder::hex_encoder
{
    auto index_to_hex_index(uint8_t index) -> uint8_t
    {
        if (index < 10)
        {
            return std::bit_cast<uint8_t>('0') + index;
        }
        else if (index < 16)
        {
            return std::bit_cast<uint8_t>('a') + (index - 10);
        }
        else [[unlikely]]
        {
            throw std::invalid_argument("bad index, out of range");
        }
    }

    auto hex_index_to_index(uint8_t hex_index)
    {
        constexpr uint8_t ZERO_INDEX    = std::bit_cast<uint8_t>('0');
        constexpr uint8_t TEN_INDEX     = std::bit_cast<uint8_t>('0') + 10;
        constexpr uint8_t A_INDEX       = std::bit_cast<uint8_t>('a');
        constexpr uint8_t G_INDEX       = std::bit_cast<uint8_t>('a') + 6;

        if (hex_index >= ZERO_INDEX && hex_index < TEN_INDEX)
        {
            return hex_index - ZERO_INDEX;
        }
        else if (hex_index >= A_INDEX && hex_index < G_INDEX)
        {
            return hex_index - A_INDEX + 10;
        }
        else
        {
            throw std::invalid_argument("bad hex index, out of range");
        }
    }

    auto hex_encode_into(std::string_view inp, char * rs) noexcept -> char *
    {
        char * iter = rs;

        for (size_t i = 0u; i < inp.size(); ++i)
        {
            uint8_t numerical_rep   = std::bit_cast<uint8_t>(inp[i]);

            uint8_t lo              = numerical_rep % 16;
            uint8_t hi              = numerical_rep / 16;

            uint8_t lo_hex          = index_to_hex_index(lo);
            uint8_t hi_hex          = index_to_hex_index(hi);

            char lo_char            = std::bit_cast<char>(lo_hex);
            char hi_char            = std::bit_cast<char>(hi_hex);

            *(iter++)               = lo_char;
            *(iter++)               = hi_char;
        }

        return iter;
    }

    template <class Streamable = std::string>
    auto hex_encode(std::string_view inp) -> Streamable
    {
        Streamable rs{};
        rs.resize(inp.size() * 2);

        hex_encode_into(inp, rs.data());

        return rs;
    }

    auto hex_decode_into(std::string_view hex_str, char * rs) -> char *
    {
        if (hex_str.size() % 2 != 0u)
        {
            throw std::invalid_argument("invalid hex string, bad size");
        }

        size_t iterable_sz  = hex_str.size() / 2;
        char * iter         = rs;

        for (size_t i = 0u; i < iterable_sz; ++i)
        {
            size_t lo_idx           = i * 2;
            size_t hi_idx           = i * 2 + 1;

            char lo_char            = hex_str[lo_idx];
            char hi_char            = hex_str[hi_idx];

            uint8_t lo_hex          = std::bit_cast<uint8_t>(lo_char);
            uint8_t hi_hex          = std::bit_cast<uint8_t>(hi_char);

            uint8_t lo              = hex_index_to_index(lo_hex);
            uint8_t hi              = hex_index_to_index(hi_hex);

            uint8_t numerical_rep   = static_cast<uint8_t>(hi << 4) | lo;

            *(iter++)               = std::bit_cast<char>(numerical_rep);
        }

        return iter;
    }

    template <class Streamable = std::string>
    auto hex_decode(std::string_view inp) -> Streamable
    {
        Streamable rs{};
        rs.resize(inp.size() / 2);

        hex_decode_into(inp, rs.data());

        return rs;
    }
}

#endif