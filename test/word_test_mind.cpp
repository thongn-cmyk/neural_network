#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>
#include <utility>
#include <functional>
#include <assert.h>
#include <serializer/compact_serializer.h>
#include <stl_extension/hasher.h>

void transpose(float * x_arr,
               size_t row_sz, size_t col_sz)
{
    assert(row_sz == col_sz);

    const size_t n = row_sz;

    for (size_t i = 0u; i < n; ++i)
    {
        for (size_t j = i + 1u; j < n; ++j)
        {
            std::swap(x_arr[i * n + j], x_arr[j * n + i]);
        }
    }
}

struct insufficient_logit_vector_size: std::invalid_argument
{
    insufficient_logit_vector_size(): std::invalid_argument("bad operation, insufficient logit vector size"){}
};

void transform(float * x_arr, size_t x_arr_sz,
               float x_first, float x_last, size_t discretization_sz,
               size_t rotation_sz,
               const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap)
{
    static std::unordered_set<size_t> accepted_sz_set
    {
        2,
        4,
        16,
        256,
        65536
    };

    if (!accepted_sz_set.contains(x_arr_sz))
    {
        throw std::invalid_argument("bad x array size, is not 2 or pow(c, 2)");
    }

    if (x_arr_sz == 2u)
    {
        const size_t interpolation_coeff_sz     = 6u;
        const size_t interpolation_slot_sz      = discretization_sz * discretization_sz;
        const size_t next_coeff_arr_offset      = coeff_arr_offset + interpolation_coeff_sz * interpolation_slot_sz;

        if (next_coeff_arr_offset > coeff_arr_cap)
        {
            throw insufficient_logit_vector_size();
        }

        float inv_discretization_multiplier     = float{1} / discretization_sz;

        size_t tentative_lhs_slot               = (x_arr[0] - x_first) * inv_discretization_multiplier;
        size_t lhs_slot                         = std::min(tentative_lhs_slot, static_cast<size_t>(discretization_sz - 1));

        size_t tentative_rhs_slot               = (x_arr[1] - x_first) * inv_discretization_multiplier;
        size_t rhs_slot                         = std::min(tentative_rhs_slot, static_cast<size_t>(discretization_sz - 1));

        size_t interpolation_idx                = lhs_slot * discretization_sz + rhs_slot;
        size_t coefficient_ptr                  = coeff_arr_offset + interpolation_idx * interpolation_coeff_sz;

        float a     = coeff_arr[coefficient_ptr + 0];
        float b     = coeff_arr[coefficient_ptr + 1];
        float c     = coeff_arr[coefficient_ptr + 2];

        float a1    = coeff_arr[coefficient_ptr + 3];
        float b1    = coeff_arr[coefficient_ptr + 4];
        float c1    = coeff_arr[coefficient_ptr + 5];

        float y     = (x_arr[0] * a + x_arr[1] * b + c) / 2;
        float y1    = (x_arr[0] * a1 + x_arr[1] * b1 + c1) / 2;

        x_arr[0]    = y;
        x_arr[1]    = y1;

        coeff_arr_offset    = next_coeff_arr_offset;

        return;
    }

    size_t row_sz   = std::sqrt(x_arr_sz);
    size_t col_sz   = row_sz;

    for (size_t i = 0u; i < rotation_sz; ++i)
    {
        for (size_t j = 0u; j < row_sz; ++j)
        {
            size_t first    = j * col_sz;
            size_t last     = first + col_sz;

            transform
            (
                std::next(x_arr, first), last - first,
                x_first, x_last, discretization_sz,
                rotation_sz,
                coeff_arr, coeff_arr_offset, coeff_arr_cap
            );
        }

        if (i + 1 != rotation_sz)
        {
            transpose(x_arr, row_sz, col_sz);
        }
    }
}

auto get_loss(const float * transform_x_arr, size_t transformed_x_arr_sz,
              float * x_arr, size_t x_arr_sz,
              float output) -> float
{
    std::vector<float> x_vec        = std::vector<float>(x_arr, std::next(x_arr, x_arr_sz));
    std::string serialized_x_vec    = dg::network_compact_serializer::serialize<std::string>(x_vec);
    size_t last_layer_idx           = hasher::hash_bytes(serialized_x_vec.data(), serialized_x_vec.size()) % transformed_x_arr_sz;

    return std::pow(transform_x_arr[last_layer_idx] - output, 2);
}

int main()
{

}