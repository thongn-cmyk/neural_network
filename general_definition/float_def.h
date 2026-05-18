//HEADER_CONTROL 0 

#ifndef __FLOAT_DEF_H__
#define __FLOAT_DEF_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdexcept>
#include <type_traits>

namespace float_def
{
    using std_float_t               = double;
    using tm_float_t                = double;
    using eval_float_t              = double;
    using mdc_float_t               = float;
    using highest_precision_float_t = long double;

    static_assert(sizeof(double) == 8u);
    static inline constexpr size_t STD_FLOAT_TYPE_BYTE_WIDTH = 8u;

    template <class CallBack>
    auto get_float_type_by_byte_width(CallBack&& callback, size_t byte_width)
    {
        if (byte_width == 4u)
        {
            static_assert(sizeof(float) == 4u);
            callback(float{});
        }
        else if (byte_width == 8u)
        {
            static_assert(sizeof(double) == 8u);
            callback(double{});
        }
        else if (byte_width == 16u)
        {
            static_assert(sizeof(long double) == 16u);
            using type = long double;
            callback(type{});
        }
        else
        {
            throw std::runtime_error("bad byte width, unavailable float type for the specified byte width");
        }
    }

    void check_float_type_by_byte_width(size_t byte_width)
    {
        get_float_type_by_byte_width([](auto&& ...){}, byte_width);
    }

    template <class ...Args>
    class Tag{};

    template <class T>
    struct most_byte_width_float{};

    template <class First, class Second, class ...Args>
    struct most_byte_width_float<Tag<First, Second, Args...>>
    {
        static_assert(std::is_floating_point_v<First>);
        using Other = typename most_byte_width_float<Tag<Second, Args...>>::type;

        using type = std::conditional_t<(sizeof(First) > sizeof(Other)),
                                        First,
                                        Other>;
    };

    template <class First>
    struct most_byte_width_float<Tag<First>>
    {
        static_assert(std::is_floating_point_v<First>);
        using type = First;
    };

    template <class ...Args>
    using most_byte_width_float_t = typename most_byte_width_float<Tag<Args...>>::type;
}

#endif