#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include <random>
#include <functional>
#include <chrono>
#include <stl_extension/stdx.h>
#include <taylor_matrix/host_matrix/taylor_projection.h>

#include <stdint.h>
#include <stdlib.h>
#include <taylor_matrix/host_matrix/the_host_matrix.h>
#include <random>
#include <functional>
#include <algorithm>
#include <utility>
#include <numeric>
#include <type_traits>
#include <filesystem>

#include <iostream>

#include <taylor_matrix/host_matrix/the_host_matrix.h>
#include <matrix_optimizer_subsystem/coordinated_search_optimizer_engine.h>
#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include <matrix_steering_subsystem/taylor_projection.h>
#include <matrix_steering_subsystem/shape_projection.h>
#include <matrix_steering_subsystem/by_step_optimizer.h>
#include <matrix/tensor_model.h>
#include <matrix/tensor_factory.h>
#include <general_definition/float_def.h>
#include <math.h>
#include <type_traits>
#include <stl_extension/stdx.h>
#include <stl_extension/hasher.h>
#include <matrix_steering_subsystem/by_step_optimizer.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <general_definition/float_def.h>
#include <format>
#include <limits.h>
#include <sstream>
#include <taylor_matrix/host_matrix/dispatch_code_generator.h>
#include <taylor_matrix/host_matrix/generic_one_dimensional_cubic_interpolation.h>
#include <taylor_matrix/host_matrix/generic_two_dimensional_cubic_interpolation.h>


using namespace float_def;
using tensor_std_float_t = tensor_model::tensor_std_float_t;

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>
#include <utility>
#include <functional>
#include <assert.h>
#include <serializer/compact_serializer.h>
#include <stl_extension/hasher.h>

#include <cmath>
#include <algorithm>

constexpr float PI_F = 3.14159265358979323846f;

inline float smooth_unit(float t)
{
    return 1.f / (1.f + std::exp(-t));
}

// Chaotic logistic-map combine, 4 iterations.
// Smooth (C-infinity) everywhere, but only an intermediate rung on the
// uniformity ladder: chi2 ~ 239 (p ~= 0.0000, still rejected) with a
// mean gradient of ~2.97 -- about 10x rougher than lerp, but nowhere
// near enough mixing to pass a real uniformity test. See chaos-8 for
// the version that actually passes (chi2 ~ 20.8, p ~= 0.35).
inline float chaos4_combine(float lhs, float rhs, float a, float b, float c)
{
    // fold all five inputs into a seed strictly inside (0,1)
    float seed = smooth_unit(4.f * (a * lhs + b * rhs + c) - 2.f);

    // logistic map, r = 4, iterated 4 times
    float x = seed;
    for (int i = 0; i < 4; ++i)
    {
        x = 4.f * x * (1.f - x);
        x = std::clamp(x, 1e-6f, 1.f - 1e-6f); // keep interior for asin/sqrt below
    }

    // arcsine CDF: converts the logistic map's arcsine-distributed
    // output into a (still imperfectly, at only 4 iterations) uniform one
    return (2.f / PI_F) * std::asin(std::sqrt(x));
}

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

        float y     = chaos4_combine(x_arr[0], x_arr[1], a, b, c);
        float y1    = chaos4_combine(x_arr[0], x_arr[1], a1, b1, b1);

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

auto get_transform_coefficient_cap(size_t x_arr_sz,
                                    float x_first, float x_last, size_t discretization_sz,
                                    size_t rotation_sz) -> size_t
{
    std::vector<float> x_vec(x_arr_sz, x_first);
    size_t cur_cap = 1u;

    while (true)
    {
        std::vector<float> coeff_vec(cur_cap, 0.f);
        std::vector<float> work_vec  = x_vec;
        size_t cur_offset            = 0u;

        try
        {
            transform(work_vec.data(), work_vec.size(),
                      x_first, x_last, discretization_sz,
                      rotation_sz,
                      coeff_vec.data(), cur_offset, cur_cap);

            return cur_offset;
        }
        catch (const insufficient_logit_vector_size&)
        {
            cur_cap *= 2u;
        }
    }
}

auto get_loss(const float * transform_x_arr, size_t transformed_x_arr_sz,
              const float * x_arr, size_t x_arr_sz,
              float output) -> float
{
    std::vector<float> x_vec        = std::vector<float>(x_arr, std::next(x_arr, x_arr_sz));
    std::string serialized_x_vec    = dg::network_compact_serializer::serialize<std::string>(x_vec);
    size_t last_layer_idx           = hasher::hash_bytes(serialized_x_vec.data(), serialized_x_vec.size()) % transformed_x_arr_sz;

    return std::pow(transform_x_arr[last_layer_idx] - output, 2);
}

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

auto randomize_bool() -> bool
{
    return randomize_int(0u, 32) < 16;
}

template <class T>
auto randomize_optional_int(size_t first, size_t last) -> std::optional<T>
{
    static_assert(std::is_unsigned_v<T>);

    if (randomize_int(0u, 1) == 0u)
    {
        return std::nullopt;
    }

    return randomize_int(first, last);
}

auto randomize_float(float first, float last)
{
    static auto randomizer      = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    auto real_distributor       = std::uniform_real_distribution<float>(first, last);

    return real_distributor(randomizer);
}

struct Projection
{
    std::vector<float> x;
    float y;
};

auto is_valid_character(char c) -> bool
{
    if (c >= '0' && c <= '9')
    {
        return true;
    }

    return false;
}

auto character_set_size() -> size_t
{
    return 10;
}

auto to_character_index(char c) -> size_t
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }

    throw std::invalid_argument("bad character, not a valid character");
}

auto from_character_index(size_t x) -> char
{
    if (x < 10)
    {
        return '0' + x;
    }

    throw std::invalid_argument("bad character index, character index out of range");
}

auto interpolate_character(char c) -> float
{
    return static_cast<float>(to_character_index(c)) / static_cast<float>(character_set_size());
}

auto randomize_character() -> char
{
    return from_character_index(randomize_int(0u, character_set_size()));
}

auto make_random_projection_data(size_t projection_sz,
                                 size_t x_vec_sz) -> std::vector<Projection>
{
    std::vector<Projection> projection_vec{};

    auto get_x  = [](size_t sz)
    {
        std::vector<float> rs{};

        for (size_t i = 0u; i < sz; ++i)
        {
            rs.push_back(interpolate_character(randomize_character()));
        }

        return rs;
    };

    for (size_t i = 0u; i < projection_sz; ++i)
    {
        projection_vec.push_back
        (
            Projection
            {
                .x  = get_x(x_vec_sz),
                .y  = interpolate_character(randomize_character())
            }
        );
    }

    return projection_vec;
}

//

auto make_random_io_projection_data(size_t projection_sz,
                                    size_t x_vec_sz,
                                    char output_identifier) -> std::vector<Projection>
{
    std::vector<Projection> random_set = make_random_projection_data(projection_sz,
                                                                     x_vec_sz);

    for (auto& projection: random_set)
    {
        for (float& x_e: projection.x)
        {
            if (x_e == to_character_index(output_identifier))
            {
                x_e = from_character_index((to_character_index(output_identifier) + 1) % character_set_size());
            }
        }
    }

    for (auto& projection: random_set)
    {
        size_t half_sz                  = projection.x.size() / 2;
        size_t half_idx                 = randomize_int(0u, half_sz);
        size_t output_idx               = half_idx * 2;

        projection.x[output_idx]        = output_identifier;

        if (output_idx + 1 >= projection.x.size())
        {
            throw std::invalid_argument("bad access, out of bound access");
        }

        projection.x[output_idx + 1]    = projection.y;
    }

    return random_set;
}

auto stringify_projection(const Projection& projection) -> std::string
{
    std::string rs  = {};
    rs              += "input > ";

    for (float x: projection.x)
    {
        rs += std::to_string(x) + ", ";
    }

    rs              += "\n";
    rs              += "output > ";
    rs              += std::to_string(projection.y);

    return rs;
}

struct insufficient_coefficient_size: std::invalid_argument
{
    insufficient_coefficient_size(): std::invalid_argument("insufficient coefficient size"){}
};

auto two_dimensional_interpolated_project(float x0, float x1,
                                          float a, float b, float c) -> float
{
    return x0 * a + x1 * b + c;
}

auto binary_unf_interpolated_project(const float * x_arr, size_t x_arr_sz,
                                     float x_first, float x_last, size_t discretization_sz,
                                     const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap) -> float
{
    if (x_arr_sz == 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, 0");
    }

    if (x_arr_sz == 1u)
    {
        return x_arr[0];
    }

    if (std::isnan(x_first))
    {
        std::abort();
    }

    if (std::isnan(x_last))
    {
        std::abort();
    }

    if (x_first >= x_last)
    {
        std::abort();
    }

    if (x_arr_sz % 2u != 0u)
    {
        std::abort();
    }

    if (discretization_sz == 0u)
    {
        std::abort();
    }

    float global_interval               = x_last - x_first;
    float discretization_interval       = global_interval / discretization_sz;    
    size_t mid_sz                       = x_arr_sz / 2;

    const size_t saved_coeff_arr_offset = coeff_arr_offset;

    float lhs                           = binary_unf_interpolated_project(x_arr, mid_sz,
                                                                          x_first, x_last, discretization_sz,
                                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap);

    coeff_arr_offset                    = saved_coeff_arr_offset;
    float rhs                           = binary_unf_interpolated_project(std::next(x_arr, mid_sz), mid_sz,
                                                                          x_first, x_last, discretization_sz,
                                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap);

    if (std::isnan(lhs))
    {
        return lhs;
    }

    float _lhs                      = std::clamp(lhs, x_first, x_last);
    size_t tentative_lhs_slot       = (_lhs - x_first) / discretization_interval;
    size_t lhs_slot                 = std::min(tentative_lhs_slot, static_cast<size_t>(discretization_sz - 1u));

    if (std::isnan(rhs))
    {
        return rhs;
    }

    float _rhs                      = std::clamp(rhs, x_first, x_last);
    size_t tentative_rhs_slot       = (_rhs - x_first) / discretization_interval;
    size_t rhs_slot                 = std::min(tentative_rhs_slot, static_cast<size_t>(discretization_sz - 1u));

    size_t required_sz              = discretization_sz * discretization_sz * 3u;
    size_t nxt_offset               = coeff_arr_offset + required_sz;

    if (nxt_offset > coeff_arr_cap)
    {
        throw insufficient_coefficient_size();
    }

    size_t flat_slot                = lhs_slot * discretization_sz + rhs_slot;
    size_t relative_offset          = flat_slot * 3u; 
    size_t global_offset            = coeff_arr_offset + relative_offset;

    float a                         = coeff_arr[global_offset];
    float b                         = coeff_arr[global_offset + 1];
    float c                         = coeff_arr[global_offset + 2];

    float cand_y                    = two_dimensional_interpolated_project(lhs, rhs, a, b, c);

    coeff_arr_offset                = nxt_offset;

    return cand_y;
}

auto get_binary_unf_interpolated_projection_size(size_t x_arr_sz,
                                                 float x_first, float x_last, float discretization_sz)
{
    std::vector<float> x_vec(x_arr_sz, 0.f);
    size_t cur_cap   = 1;

    while (true)
    {
        size_t cur_sz   = 0u;
        std::vector<float> coeff_vec(cur_cap, 0.f);

        try
        {
            binary_unf_interpolated_project(x_vec.data(), x_arr_sz,
                                            x_first, x_last, discretization_sz,
                                            coeff_vec.data(), cur_sz, cur_cap);

            return cur_sz;
        }
        catch (const insufficient_coefficient_size& e)
        {
            cur_cap *= 2;
        }
    }
}


class SomeMatrix: public virtual the_matrix::MatrixInterface
{
    private:

        std::vector<tensor_std_float_t> coeff_vec;
    
    public:

        SomeMatrix(std::vector<tensor_std_float_t> coeff_vec): coeff_vec(std::move(coeff_vec)){}

        auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
        {
            return this->coeff_vec;
        }

        void set_coefficient_vector(const std::vector<tensor_std_float_t>& arg)
        {
            for (tensor_std_float_t e: arg)
            {
                if (std::isnan(e))
                {
                    throw std::invalid_argument("bad float, NaN");
                }
            }

            this->coeff_vec = arg;
        }

        auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>&) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
        {
            throw std::invalid_argument("project function not supported");
        }

        auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
        {
            return std::make_shared<SomeMatrix>(*this);
        }
};

class SomeProjector
{
    private:

        float  x_first;
        float  x_last;
        size_t discretization_sz;
        size_t rotation_sz;

    public:

        SomeProjector(float x_first,
                      float x_last,
                      size_t discretization_sz,
                      size_t rotation_sz): x_first(x_first),
                                           x_last(x_last),
                                           discretization_sz(discretization_sz),
                                           rotation_sz(rotation_sz){}

        auto project(const float * x_arr, size_t x_arr_sz,
                     const float * coeff_arr, size_t coeff_arr_cap) -> std::vector<float>
        {
            std::vector<float> work_vec(x_arr, std::next(x_arr, x_arr_sz));
            size_t coeff_arr_offset = 0u;

            transform(work_vec.data(), work_vec.size(),
                      this->x_first, this->x_last, this->discretization_sz,
                      this->rotation_sz,
                      coeff_arr, coeff_arr_offset, coeff_arr_cap);

            return work_vec;
        }
};

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<Projection> training_pair_vec;
        std::unique_ptr<SomeProjector> projector;

    public:

        PointPullMatrixEvaluator(std::vector<Projection> training_pair_vec,
                                 std::unique_ptr<SomeProjector> projector): training_pair_vec(std::move(training_pair_vec)),
                                                                            projector(std::move(projector)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<float> coeff_vec = stdx::to_castable_vector_initializer(matrix.get_coefficient_vector());
            double total_loss            = 0;

            for (const Projection& projection: this->training_pair_vec)
            {
                std::vector<float> transformed_x = this->projector->project(projection.x.data(), projection.x.size(),
                                                                            coeff_vec.data(), coeff_vec.size());

                total_loss += get_loss(transformed_x.data(), transformed_x.size(),
                                    projection.x.data(), projection.x.size(),
                                    projection.y);
            }

            if (!this->training_pair_vec.empty())
            {
                total_loss /= this->training_pair_vec.size();
            }

            return total_loss;
        }
    
    private:
        
        auto mean_sqrt(const std::vector<float>& lhs,
                       const std::vector<float>& rhs) -> double
        {
            if (lhs.size() != rhs.size())
            {
                throw std::invalid_argument("bad operation, mismatch evaluation dimension");
            }

            double total    = 0;

            for (size_t i = 0u; i < lhs.size(); ++i)
            {
                total += std::pow(lhs[i] - rhs[i], 2);
            }
            
            if (lhs.size() != 0u)
            {
                total /= lhs.size();
            }

            return total;
        }
};

auto get_optimizer() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = 1024ULL,
            .optimization_step_sz                       = 4ULL,
            .optimization_loop_sz                       = 8ULL
        }
    );
}

void run_test()
{
    const size_t SEMANTIC_SZ                        = character_set_size() + 2;
    const float DISCRETIZATION_VALUE                = float(1) / SEMANTIC_SZ;
    const size_t EPOCH_SZ                           = size_t{1} << 8;
    const size_t DATA_SET_SZ                        = size_t{1} << 12;
    const size_t X_SZ                               = 4;      // was 8 — must be in {2,4,16,256,65536}
    const size_t ROTATION_SZ                        = 2;       // new: number of row-pass/transpose layers
    const char Y_IDENTIFIER                         = '0';

    std::vector<Projection> projection_vec          = make_random_io_projection_data(DATA_SET_SZ, X_SZ, Y_IDENTIFIER);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();

    std::vector<tensor_std_float_t> tensor_vec = std::vector<tensor_std_float_t>(
        get_transform_coefficient_cap(X_SZ,
                                    0.f, DISCRETIZATION_VALUE * SEMANTIC_SZ, SEMANTIC_SZ,
                                    ROTATION_SZ),
        0.f);

    for (float& tensor: tensor_vec)
    {
        tensor = randomize_float(0, 1);
    }

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "coefficient vector size > " << tensor_vec.size() << "\n";

    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = std::make_unique<SomeMatrix>(std::move(tensor_vec));
    std::unique_ptr<SomeProjector> projector                                                = std::make_unique<SomeProjector>(0.f,
                                                                                                                              DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                              SEMANTIC_SZ,
                                                                                                                              ROTATION_SZ);

    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_vec, std::move(projector));
    common_exception::CancellationToken cancellation_token                                  = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << "\n";
    }

    for (size_t i = 0u; i < EPOCH_SZ; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
    }
}

int main()
{
    run_test();
}