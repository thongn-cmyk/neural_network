#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <random>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>
#include <stl_extension/stdx.h>


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

#include <seqpar_async/async_x.h>

using namespace float_def;
using tensor_std_float_t = tensor_model::tensor_std_float_t;

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        throw std::invalid_argument("bad interval, first >= last");
    }

    static auto randomizer  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t sz         = last - first;

    return first + randomizer() % sz;
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

auto randomize_range(size_t range_sz) -> size_t
{
    return randomize_int(0u, range_sz);
}

auto get_z_number_within_digits(size_t digit_sz) -> size_t
{
    const size_t first      = 0u;
    const size_t last       = std::pow(10, digit_sz);

    return randomize_int(first, last);
}

auto lpad_string(const std::string& s, char c, size_t pad_sz) -> std::string
{
    size_t new_str_sz   = std::max(s.size(), pad_sz);
    std::string pad     = {};

    for (size_t i = s.size(); i < new_str_sz; ++i)
    {
        pad.push_back(c);
    }

    return pad + s;
}

auto z_number_to_string(size_t sz) -> std::string
{
    return std::to_string(sz);
}

auto to_digit_vec(size_t val) -> std::vector<size_t>
{
    if (val == 0u)
    {
        return std::vector<size_t>{0u};
    }

    std::vector<size_t> rs  = {};

    while (val != 0u)
    {
        rs.push_back(val % 10);
        val /= 10;
    }

    return rs;
}

auto get_handroll_calculation_instruction(size_t lhs,
                                          size_t rhs) -> std::vector<std::string>
{
    if (lhs > rhs)
    {
        return get_handroll_calculation_instruction(rhs, lhs);
    }

    std::vector<size_t> lhs_digit_vec           = to_digit_vec(lhs);
    std::vector<size_t> rhs_digit_vec           = to_digit_vec(rhs);
    size_t tentative_rs_sz                      = std::max(lhs_digit_vec.size(), rhs_digit_vec.size()) + 1u;
    std::vector<size_t> rs_digit_vec            = std::vector<size_t>(tentative_rs_sz, 0u);
    size_t carry                                = 0u;
    std::vector<std::string> instruction_vec    = {};
    size_t common_sz                            = lhs_digit_vec.size();
    size_t upper_sz                             = rhs_digit_vec.size();

    instruction_vec.push_back("cbt");

    for (size_t i = 0u; i < common_sz; ++i)
    {
        size_t total    = carry + lhs_digit_vec[i] + rhs_digit_vec[i];

        std::string ins = z_number_to_string(carry)
                            + "+" + z_number_to_string(lhs_digit_vec[i])
                            + "+" + z_number_to_string(rhs_digit_vec[i]) 
                            + "=" + z_number_to_string(total);

        instruction_vec.push_back(ins);

        carry               = total;
        size_t write_val    = carry % 10;
        size_t rem_val      = carry / 10;

        instruction_vec.push_back("wr");
        instruction_vec.push_back(z_number_to_string(write_val));
        instruction_vec.push_back("rm");
        instruction_vec.push_back(z_number_to_string(rem_val));

        carry               = rem_val;
    }

    instruction_vec.push_back("lbt");

    for (size_t i = common_sz; i < upper_sz; ++i)
    {
        size_t total        = carry + rhs_digit_vec[i];

        std::string ins     = z_number_to_string(carry)
                                + "+" + z_number_to_string(rhs_digit_vec[i])
                                + "=" + z_number_to_string(total);

        instruction_vec.push_back(ins);

        carry               = total;
        size_t write_val    = carry % 10;
        size_t rem_val      = carry / 10;

        instruction_vec.push_back("wr");
        instruction_vec.push_back(z_number_to_string(write_val));
        instruction_vec.push_back("rm");
        instruction_vec.push_back(z_number_to_string(rem_val));

        carry               = rem_val;
    }

    instruction_vec.push_back("fl");

    if (carry != 0u)
    {
        instruction_vec.push_back(z_number_to_string(carry));
    }

    return instruction_vec;
}

struct Equation
{
    std::string eqn;
    std::string instruction;
};

auto join(const std::vector<std::string>& ins_vec, char c) -> std::string
{
    if (ins_vec.empty())
    {
        return {};
    }

    std::string rs = ins_vec.front();

    for (size_t i = 1u; i < ins_vec.size(); ++i)
    {
        rs += c;
        rs += ins_vec[i];
    }

    return rs;
}

auto encode_instruction(const std::vector<std::string>& ins_vec) -> std::string
{
    return join(ins_vec, ',');
}

auto encode_equation(const Equation& eqn) -> std::string
{
    return join({eqn.eqn, eqn.instruction}, '|');
}

auto encode_equation_vector(const std::vector<Equation>& eqn_vec) -> std::string
{
    std::vector<std::string> encoded_eqn_vec = {};

    for (const auto& eqn: eqn_vec)
    {
        encoded_eqn_vec.push_back(encode_equation(eqn));
    }

    return join(encoded_eqn_vec, '#');
}

auto get_addition_equation_string_vector(size_t digit_sz,
                                         size_t calculation_sz) -> std::vector<Equation>
{
    size_t operable_digit_sz    = digit_sz + 1;
    std::vector<Equation> rs    = {};

    for (size_t i = 0u; i < calculation_sz; ++i)
    {
        size_t lhs              = get_z_number_within_digits(digit_sz);
        size_t rhs              = get_z_number_within_digits(digit_sz);
        size_t total            = lhs + rhs;

        std::string lhs_str     = lpad_string(z_number_to_string(lhs), '0', operable_digit_sz);
        std::string rhs_str     = lpad_string(z_number_to_string(rhs), '0', operable_digit_sz);
        std::string total_str   = lpad_string(z_number_to_string(total), '0', operable_digit_sz);

        std::string eqn         = lhs_str + "+" + rhs_str + "=" + total_str;

        rs.push_back
        (
            Equation
            {
                .eqn            = std::move(eqn),
                .instruction    = encode_instruction(get_handroll_calculation_instruction(lhs, rhs))
            }
        );
    }

    return rs;
}

struct Token
{
    std::string context;
    char hex_code;
};

auto numeric_hex_code_to_char_hex_code(uint8_t hex_code)
{
    if (hex_code < 10)
    {
        return '0' + hex_code;
    }
    else if (hex_code < 16)
    {
        return 'a' + (hex_code - 10);
    }
    else
    {
        throw std::invalid_argument("bad hex numeric hex code, not within [0, 16) range");
    }
}

auto char_hex_code_to_numeric_hex_code(char c) -> uint8_t
{
    if (c >= 'A' && c <= 'F')
    {
        return (c - 'A') + 10;
    }
    else if (c >= 'a' && c <= 'f')
    {
        return (c - 'a') + 10;
    }
    else if (c >= '0' && c <= '9')
    {
        return (c - '0') + 0;
    }
    else
    {
        throw std::invalid_argument("invalid hex char, pattern mismatched");
    }
}

auto hex_decode(uint8_t c) -> std::pair<char, char>
{
    uint8_t lo  = c % 16;
    uint8_t hi  = c / 16;

    return
    {
        numeric_hex_code_to_char_hex_code(lo),
        numeric_hex_code_to_char_hex_code(hi)
    };
}

auto to_hex_vec(const std::string& s) -> std::string
{
    std::string rs{};

    for (char c: s)
    {
        auto [lo, hi] = hex_decode(std::bit_cast<uint8_t>(c));

        rs.push_back(lo);
        rs.push_back(hi);
    }

    return rs;
}

auto from_hex_vec(const std::string& s) -> std::string
{
    assert(s.size() % 2 == 0);

    std::string rs{};
    rs.reserve(s.size() / 2);

    auto nibble = [](char c) -> uint8_t
    {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        throw std::invalid_argument("invalid hex character");
    };

    for (std::size_t i = 0; i < s.size(); i += 2)
    {
        uint8_t lo = nibble(s[i]);
        uint8_t hi = nibble(s[i + 1]);
        uint8_t byte = static_cast<uint8_t>((hi << 4) | lo);

        rs.push_back(std::bit_cast<char>(byte));
    }

    return rs;
}

auto tokenize(const std::string& s,
              size_t hex_window_sz,
              size_t token_sz) -> std::vector<Token>
{
    std::vector<Token> tok_vec      = {};
    std::string hex_vec             = to_hex_vec(s);

    if (hex_vec.empty())
    {
        return {};
    }

    size_t prediction_last              = hex_vec.size();

    for (size_t i = 0u; i < token_sz; ++i)
    {
        Token tok                           = {};

        size_t random_first                 = std::min(hex_window_sz, prediction_last);
        size_t random_last                  = prediction_last;
        size_t random_sz                    = random_last - random_first;

        if (random_sz == 0u)
        {
            continue;
        }

        size_t idx                          = random_first + randomize_range(random_sz);
        size_t ctx_window_first             = std::max(static_cast<intmax_t>(idx) - static_cast<intmax_t>(hex_window_sz), intmax_t{0});
        std::string tmp_hex_vec             = {};

        for (size_t j = ctx_window_first; j < idx; ++j)
        {
            tmp_hex_vec.push_back(hex_vec[j]);
        }

        tok_vec.push_back
        (
            Token
            {
                .context    = tmp_hex_vec,
                .hex_code   = hex_vec[idx]
            }
        );
    }

    return tok_vec;
}

struct Projection
{
    std::vector<float> x;
    float y;
};

auto token_to_projection(const Token& token) -> Projection
{
    std::vector<float> x{};
    float y{};

    for (char c: token.context)
    {
        uint8_t numeric_hex_code    = char_hex_code_to_numeric_hex_code(c);
        float x_embed               = static_cast<float>(numeric_hex_code) / 16;

        x.push_back(x_embed);
    }

    y   = static_cast<float>(char_hex_code_to_numeric_hex_code(token.hex_code)) / 16;

    return Projection
    {
        .x  = x,
        .y  = y
    };
}

auto token_vector_to_projection_vector(const std::vector<Token>& token_vec) -> std::vector<Projection>
{
    std::vector<Projection> rs{};

    for (const auto& token: token_vec)
    {
        rs.push_back(token_to_projection(token));
    }

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

auto get_deviation(float expected, float actual, float acceptance_width) -> float
{
    if (std::isnan(expected))
    {
        return expected;
    }

    if (std::isnan(actual))
    {
        return actual;
    }

    return std::pow(expected - actual, 2);
}

auto binary_unf_interpolated_deviation_project(const float * x_arr, size_t x_arr_sz,
                                               float x_next, float acceptance_width,
                                               float x_first, float x_last, size_t discretization_sz,
                                               const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap,
                                               double root_weight) -> std::pair<float, float>
{
    if (x_arr_sz == 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, 0");
    }

    //right, this should be lhs =, rhs = but we'd cut some slack here, it's semantically different

    if (x_arr_sz == 1u)
    {
        return std::make_pair
        (
            x_arr[0],
            get_deviation(x_arr[0], x_next, acceptance_width)
        );
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
    // const size_t saved_coeff_arr_offset = coeff_arr_offset;

    auto [lhs, lhs_deviation]           = binary_unf_interpolated_deviation_project(x_arr, mid_sz,
                                                                                    x_next, acceptance_width,
                                                                                    x_first, x_last, discretization_sz,
                                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                    root_weight);

    // coeff_arr_offset                    = saved_coeff_arr_offset;
    auto [rhs, rhs_deviation]           = binary_unf_interpolated_deviation_project(std::next(x_arr, mid_sz), mid_sz,
                                                                                    x_next, acceptance_width,
                                                                                    x_first, x_last, discretization_sz,
                                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap,
                                                                                    root_weight);

    if (std::isnan(lhs))
    {
        return std::make_pair(lhs, lhs);
    }

    float _lhs                      = std::clamp(lhs, x_first, x_last);
    size_t tentative_lhs_slot       = (_lhs - x_first) / discretization_interval;
    size_t lhs_slot                 = std::min(tentative_lhs_slot, static_cast<size_t>(discretization_sz - 1u));

    if (std::isnan(rhs))
    {
        return std::make_pair(rhs, rhs);
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

    float root_deviation            = get_deviation(cand_y, x_next, acceptance_width);
    float total_deviation           = (lhs_deviation + rhs_deviation) * (1 - root_weight) + 2 * root_deviation * root_weight;

    coeff_arr_offset                = nxt_offset;

    return std::make_pair(cand_y, total_deviation);
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
            binary_unf_interpolated_deviation_project(x_vec.data(), x_arr_sz,
                                                      0, 0,
                                                      x_first, x_last, discretization_sz,
                                                      coeff_vec.data(), cur_sz, cur_cap,
                                                      0);

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


class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<Projection> training_pair_vec;
        float acceptance_width;
        float x_first;
        float x_last;
        size_t discretization_sz;
        double root_weight;

    public:

        PointPullMatrixEvaluator(std::vector<Projection> training_pair_vec,
                                 float acceptance_width,
                                 float x_first,
                                 float x_last,
                                 size_t discretization_sz,
                                 double root_weight): training_pair_vec(std::move(training_pair_vec)),
                                                      acceptance_width(acceptance_width),
                                                      x_first(x_first),
                                                      x_last(x_last),
                                                      discretization_sz(discretization_sz),
                                                      root_weight(root_weight){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<float> coeff_vec        = stdx::to_castable_vector_initializer(matrix.get_coefficient_vector());
            float rs                            = 0;

            for (const Projection& projection: this->training_pair_vec)
            {
                size_t coeff_vec_offset = 0u;

                auto [y, e] = binary_unf_interpolated_deviation_project(projection.x.data(), projection.x.size(),
                                                                        projection.y, this->acceptance_width,
                                                                        this->x_first, this->x_last, this->discretization_sz,
                                                                        coeff_vec.data(), coeff_vec_offset, coeff_vec.size(),
                                                                        this->root_weight);

                rs          += e;
            }

            return rs;
        }

        void set_root_weight(double root_weight)
        {
            this->root_weight = root_weight;
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
            .optimization_epoch_sz                      = 128ULL,
            .optimization_step_sz                       = 8ULL,
            .optimization_loop_sz                       = 4ULL
        }
    );
}

void initialize_concurrency_base()
{
    using namespace concurrency_base;

    std::cout << "initializing concurrency base\n";
    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < 8u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation
        {
            .cpu_id = std::nullopt,
            .daemon = ASYNC_SEQPAR_DAEMON
        });
    }

    init(Config{worker_info_vec});
    async_x::init(8u, 32u);
}

void run_test()
{
    const size_t DIGIT_SZ               = 8u;
    const size_t CALCULATION_SZ         = size_t{1} << 18;
    const size_t TEST_SZ                = size_t{1} << 11;
    const size_t WINDOW_SZ              = 128u;

    const float DISCRETIZATION_VALUE    = 0.10;
    const float ACCEPTANCE_WIDTH        = 0.04;
    const size_t SEMANTIC_SZ            = 10;
    const size_t EPOCH_SZ               = size_t{1} << 8;

    const double INITIAL_ROOT_WEIGHT    = 0.01;
    const double MAX_ROOT_WEIGHT        = 0.98;

    std::vector<Equation> eqn_vec                   = get_addition_equation_string_vector(DIGIT_SZ, CALCULATION_SZ);
    std::vector<Token> token_vec                    = tokenize(encode_equation_vector(eqn_vec),
                                                               WINDOW_SZ,
                                                               TEST_SZ);

    std::vector<Projection> projection_vec          = token_vector_to_projection_vector(token_vec);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();
    std::vector<tensor_std_float_t> tensor_vec                                              = std::vector<tensor_std_float_t>(get_binary_unf_interpolated_projection_size(projection_vec.front().x.size(),
                                                                                                                                                                          0,
                                                                                                                                                                          DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                                                                          SEMANTIC_SZ),
                                                                                                                              0.f);

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "coefficient vector size > " << tensor_vec.size() << "\n";

    double current_root_weight                                                              = INITIAL_ROOT_WEIGHT;
    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = std::make_unique<SomeMatrix>(std::move(tensor_vec));
    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_vec,
                                                                                                                                         ACCEPTANCE_WIDTH,
                                                                                                                                         0,
                                                                                                                                         DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                                         SEMANTIC_SZ,
                                                                                                                                         current_root_weight);
    common_exception::CancellationToken cancellation_token                                  = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << "\n";
    }

    for (size_t i = 0u; i < EPOCH_SZ; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);
        current_root_weight         *= 2;
        current_root_weight         = std::min(current_root_weight, MAX_ROOT_WEIGHT);

        matrix_evaluator->set_root_weight(current_root_weight);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
    }
}

int main()
{
    initialize_concurrency_base();
    run_test();
}