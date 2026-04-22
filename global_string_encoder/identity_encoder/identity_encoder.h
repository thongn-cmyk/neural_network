#ifndef __GLOBAL_STRING_ENCODER_IDENTITY_ENCODER_H__
#define __GLOBAL_STRING_ENCODER_IDENTITY_ENCODER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <vector>

namespace global_string_encoder::identity_encoder
{
    template <class Streamable = std::string>
    auto identity_encode(std::string_view inp) -> Streamable
    {
        return Streamable(inp);
    }

    template <class Streamable = std::string>
    auto identity_decode(std::string_view inp) -> Streamable
    {
        return Streamable(inp);
    }
}

#endif