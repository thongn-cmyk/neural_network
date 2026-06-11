//surprisingly, all we could do is to increase the hash table dispatch size to size_t{1} << 10 -> size_t{1} << 20
//so we'd have to definitely implement the interval tree memory management today - tomorrow to keep tracks of memory regions and sync 128 logits at a time to do "perfectch" training
//let's do a simple word test today

//I have spent so so so much time just to do square difference training with max bucket
//let's see if the strategy actually works or it's just a myth

#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

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

using tensor_std_float_t = tensor_model::tensor_std_float_t;

using namespace float_def;

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

template <class T>
auto randomize_optional_int(size_t first, size_t last) -> std::optional<T>
{
    static_assert(std::is_unsigned_v<T>);

    if (randomize_int(0, 1) == 0)
    {
        return std::nullopt;
    }

    return randomize_int(first, last);
}

auto randomize_double(double first, double last) -> double
{
    if (first >= last)
    {
        std::abort();
    }

    static auto distributor = std::uniform_real_distribution<double>(first, last);
    static auto randomizer  = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    return distributor(randomizer);
}

auto read_training_data(const size_t TRAINING_DATA_SZ) -> std::string
{
    const std::string FILE_PATH     = "/Users/megazone/Downloads/corpus-webis-tldr-17.json";

    std::ifstream f_stream(FILE_PATH, std::ios::in | std::ios::binary);

    std::string rs(TRAINING_DATA_SZ, ' ');
    f_stream.read(rs.data(), TRAINING_DATA_SZ);
    rs.resize(f_stream.gcount());

    return rs;
}

struct Token
{
    std::string context;
    char c;
};

auto window_tokenize(const std::string& s, const size_t WINDOW_SZ) -> std::vector<Token>
{
    std::vector<Token> rs   = {};

    for (size_t i = WINDOW_SZ; i < s.size(); ++i)
    {
        intmax_t cand   = static_cast<intmax_t>(i) - WINDOW_SZ;
        cand            = std::max(cand, intmax_t{0});

        std::string ctx(std::next(s.begin(), cand), std::next(s.begin(), i));

        rs.push_back
        (
            Token
            {
                .context    = std::move(ctx),
                .c          = s[i]
            }
        );
    }

    return rs;
}

auto hex_punch(char c) -> std::vector<bool>
{
    uint8_t uint_c  = std::bit_cast<uint8_t>(c);
    uint8_t lo_bit  = uint_c % 16;
    uint8_t hi_bit  = uint_c >> 4;

    static std::vector<std::vector<bool>> hex_punch_table = []
    {
        std::vector<std::vector<bool>> rs{};

        for (size_t i = 0u; i < 16u; ++i)
        {
            std::vector<bool> tmp(16, false);
            tmp[i] = true;

            rs.push_back(tmp);
        }

        return rs;
    }();

    std::vector<bool> rs{};

    rs.insert(rs.end(), hex_punch_table[lo_bit].begin(), hex_punch_table[lo_bit].end());
    rs.insert(rs.end(), hex_punch_table[hi_bit].begin(), hex_punch_table[hi_bit].end());

    return rs;
}

auto ff_punch(char c) -> std::vector<bool>
{
    std::vector<bool> rs(256, false);
    rs[std::bit_cast<uint8_t>(c)] = true;

    return rs;
}

// auto match_window(const std::vector<bool>& inp,
//                   size_t sz) -> std::vector<bool>
// {
//     if (inp.size() == 0u)
//     {
//         std::abort();
//     }

//     size_t multiplier       = sz / inp.size();
//     size_t full_sz          = inp.size() * multiplier;
//     size_t rem_sz           = sz - full_sz;

//     std::vector<bool> rs    = {};

//     for (size_t i = 0u; i < multiplier; ++i)
//     {
//         rs.insert(rs.end(), inp.begin(), inp.end());
//     }

//     for (size_t i = 0u; i < rem_sz; ++i)
//     {
//         rs.push_back(false);
//     }

//     return rs;
// }

auto token_to_input_binary(const Token& tok) -> std::vector<bool>
{
    std::vector<bool> rs{};

    for (char c: tok.context)
    {
        std::vector<bool> c_bitvec  = hex_punch(c);
        rs.insert(rs.end(), c_bitvec.begin(), c_bitvec.end());
    }

    return rs;
}

auto token_to_output_binary(const Token& tok) -> std::vector<bool>
{
    std::vector<bool> bit_vec = hex_punch(tok.c);
    std::vector<bool> bit_matrix(256);

    assert(bit_vec.size() == 32u);

    for (size_t i = 0u; i < 32u; ++i)
    {
        for (size_t j = 0u; j < 8u; ++j)
        {
            bit_matrix[i * 8 + j] = bit_vec[i];
        }
    }

    return bit_matrix;
}

auto guess_word(const std::shared_ptr<tensor_model::Matrix>& matrix) -> char
{
    std::vector<tensor_std_float_t> flat_tensor_vec{};

    tensor_factory::flatten(matrix, flat_tensor_vec);
    std::vector<tensor_std_float_t> accum_tensor_vec(32u);

    assert(flat_tensor_vec.size() > 256u);

    for (size_t i = 0u; i < 32u; ++i)
    {
        tensor_std_float_t total = 0;

        for (size_t j = 0u; j < 8u; ++j)
        {
            total += flat_tensor_vec[i * 8 + j];
        }

        accum_tensor_vec[i] = total;
    }

    size_t lo_bit_idx   = 0u;

    for (size_t i = 0u; i < 16u; ++i)
    {
        if (accum_tensor_vec[i] > accum_tensor_vec[lo_bit_idx])
        {
            lo_bit_idx = i;
        }
    }

    size_t hi_bit_idx   = 16u;

    for (size_t i = 16u; i < 32u; ++i)
    {
        if (accum_tensor_vec[i] > accum_tensor_vec[hi_bit_idx])
        {
            hi_bit_idx = i;
        }
    }
    
    uint8_t lo_bit  = lo_bit_idx;
    uint8_t hi_bit  = hi_bit_idx - 16;

    return (hi_bit << 4) | lo_bit;
}

auto binary_to_matrix_dispatchable(const std::vector<bool>& binary,
                                   const std::vector<size_t>& matrix_shape) -> std::shared_ptr<tensor_model::Matrix>
{
    std::vector<tensor_model::tensor_std_float_t> flat_matrix(tensor_factory::shape_size(matrix_shape), 0);

    if (binary.size() > flat_matrix.size())
    {
        std::abort();
    }

    for (size_t i = 0u; i < binary.size(); ++i)
    {
        if (binary[i])
        {
            flat_matrix[i] = 1;
        }
        else
        {
            flat_matrix[i] = 0;
        }
    }

    return tensor_factory::make_matrix_from_flat_vec(matrix_shape, flat_matrix);
}

auto get_random_coordinated_search_optimizer_engine() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = 16ULL,
            .optimization_step_sz                       = 4ULL,
            .optimization_loop_sz                       = 2ULL
        }
    );
}

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_pair_vec;

    public:

        PointPullMatrixEvaluator(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& training_pair_vec): training_pair_vec(training_pair_vec){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec      = {};
            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec_0    = {};

            for (const auto& [inp, out]: this->training_pair_vec)
            {
                inp_vec.push_back(inp);
                out_vec_0.push_back(out);
            }

            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec      = matrix.project(inp_vec);

            std::vector<std::vector<tensor_std_float_t>> flat_out_vec_0{};
            std::vector<std::vector<tensor_std_float_t>> flat_out_vec{};

            for (const auto [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                std::vector<tensor_std_float_t> flat_out    = {};
                std::vector<tensor_std_float_t> flat_out_0  = {};

                tensor_factory::flatten(out, flat_out);
                tensor_factory::flatten(expected_out, flat_out_0);

                flat_out_vec.push_back(std::move(flat_out));
                flat_out_vec_0.push_back(std::move(flat_out_0));
            }

            return mean_square_root(flat_out_vec, flat_out_vec_0);
        }

    private:

        auto mean_square_root(const std::vector<std::vector<tensor_std_float_t>>& lhs,
                              const std::vector<std::vector<tensor_std_float_t>>& rhs) -> eval_float_t
        {
            eval_float_t rs = 0;

            for (const auto& [e_lhs, e_rhs]: stdx::zip(lhs, rhs))
            {
                if (e_lhs.size() != e_rhs.size())
                {
                    std::abort();
                }

                eval_float_t tmp = 0;

                for (size_t i = 0u; i < e_lhs.size(); ++i)
                {
                    tmp += std::pow(e_lhs[i] - e_rhs[i], 8);
                }

                if (e_lhs.size() != 0)
                {
                    tmp /= e_lhs.size();
                }

                rs += tmp;
            }

            if (lhs.size() != 0u)
            {
                rs /= lhs.size();
            }

            return rs;
        }
};

auto get_matrix_evaluator(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& point_vec) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<PointPullMatrixEvaluator>(point_vec);
}

auto get_loss(the_matrix::MatrixInterface& matrix,
              const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, char>>& training_pair_vec) -> double
{
    std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec{};
    std::vector<char> char_vec{};

    for (const auto& [mtrx, c]: training_pair_vec)
    {
        inp_vec.push_back(mtrx);
        char_vec.push_back(c);
    }

    std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec = matrix.project(inp_vec);
    size_t correct_word = 0u;

    for (size_t i = 0u; i < out_vec.size(); ++i)
    {
        char predicted_word = guess_word(out_vec[i]);

        if (predicted_word == char_vec[i])
        {
            std::cout << "predicted word > " << std::bit_cast<uint8_t>(predicted_word) << "<> correct word > " << std::bit_cast<uint8_t>(char_vec[i]) << "\n";
            correct_word += 1;
        }
    }

    if (out_vec.size() == 0u)
    {
        std::abort();
    }

    return static_cast<float>(correct_word) / out_vec.size();
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

int main()
{
    initialize_concurrency_base();

    const size_t OPTIMIZATION_SZ                = size_t{1} << 10;
    const size_t TRAINING_DATA_SZ               = size_t{1} << 5;
    const size_t WINDOW_SZ                      = size_t{1} << 4;

    std::vector<Token> token_vec                = window_tokenize(read_training_data(TRAINING_DATA_SZ), WINDOW_SZ);

    std::vector<std::pair<std::vector<bool>, std::vector<bool>>> training_pair_vec{};

    for (const auto& token: token_vec)
    {
        training_pair_vec.push_back
        (
            std::make_pair
            (
                token_to_input_binary(token),
                token_to_output_binary(token)
            )
        );
    }
    
    std::shared_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> test_engine   = get_random_coordinated_search_optimizer_engine();

    size_t operable_sz                                                      = training_pair_vec.back().first.size();
    std::vector<size_t> matrix_shape                                        = taylor_matrix::host_matrix::the_host_matrix::TheHostMatrixFactory{}.set_vector_size(operable_sz).get_matrix_shape();
    std::shared_ptr<the_matrix::MatrixInterface> matrix                     = taylor_matrix::host_matrix::the_host_matrix::TheHostMatrixFactory{}.set_vector_size(operable_sz).get();

    std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_pair_matrix_vec{};

    for (const auto& [lhs, rhs]: training_pair_vec)
    {
        training_pair_matrix_vec.push_back
        (
            std::make_pair
            (
                binary_to_matrix_dispatchable(lhs, matrix_shape),
                binary_to_matrix_dispatchable(rhs, matrix_shape)
            )
        );
    }

    std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, char>> matrix_char_pair_vec{};

    for (size_t i = 0u; i < token_vec.size(); ++i)
    {
        matrix_char_pair_vec.push_back
        (
            std::make_pair
            (
                training_pair_matrix_vec[i].first,
                token_vec[i].c
            )
        );
    }

    std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface> evaluator   = get_matrix_evaluator(training_pair_matrix_vec);

    common_exception::CancellationToken cancellation_token{};

    std::cout << "__BEGIN_OPTIMIZATION_TEST__\n";

    double initial_deviation    = evaluator->get_deviation(*matrix);

    std::cout << "initial deviation > " << initial_deviation << "\n";

    // for (size_t i = 0u; i < matrix-)
    // {

    // }

    // matrix->set_coefficient_vector(std::vector<tensor_std_float_t>(random_vec));
    // std::cout << "post deviation > " << evaluator->get_deviation(*matrix) << "\n";

    std::cout << "loss > " << get_loss(*matrix, matrix_char_pair_vec) << "\n";

    // std::abort();

    for (size_t i = 0u; i < OPTIMIZATION_SZ; ++i)
    {
        matrix  = test_engine->optimize(*matrix, *evaluator, cancellation_token);
        double optimized_deviation  = evaluator->get_deviation(*matrix);

        std::string str_max = std::format("{:.{}g}", optimized_deviation, std::numeric_limits<double>::max_digits10);

        std::cout << "optimized deviation > " << str_max << "\n";
        std::cout << "loss > " << get_loss(*matrix, matrix_char_pair_vec) << "\n";
    }

    std::cout << "__END_OPTIMIZATION_TEST__\n";
}