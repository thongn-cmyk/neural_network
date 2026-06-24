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
#include <limits.h>

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
    bool c;
};

auto str_to_bit_vector(const std::string& s) -> std::vector<bool>
{
    size_t bitvec_sz    = s.size() * CHAR_BIT;

    std::vector<bool> rs(bitvec_sz, false);

    for (size_t i = 0u; i < bitvec_sz; ++i)
    {
        size_t slot_idx = i / CHAR_BIT;
        size_t slot_off = i % CHAR_BIT;

        rs[i]           = (std::bit_cast<uint8_t>(s[slot_idx]) & (uint8_t{1} << slot_off)) != 0u;
    }

    return rs;
}

auto bit_vector_to_str(const std::vector<bool>& bitvec) -> std::string 
{
    size_t str_sz   = bitvec.size() / CHAR_BIT + size_t{bitvec.size() % CHAR_BIT != 0u};
    std::string rs  = {};

    rs.resize(str_sz);

    for (size_t i = 0u; i < bitvec.size(); ++i)
    {
        size_t slot_idx = i / CHAR_BIT;
        size_t slot_off = i % CHAR_BIT;

        char c          = rs[slot_idx];
        uint8_t uint_c  = std::bit_cast<uint8_t>(c);
        uint_c          |= static_cast<uint8_t>(bitvec[i]) << slot_off;

        rs[slot_idx]    = std::bit_cast<char>(uint_c);
    }

    return rs;
}

auto window_tokenize(const std::string& s, const size_t WINDOW_SZ) -> std::vector<Token>
{
    std::vector<Token> rs       = {};
    std::vector<bool> bitvec    = str_to_bit_vector(s);
    size_t bit_window_sz        = WINDOW_SZ * CHAR_BIT;

    for (size_t i = bit_window_sz; i < bitvec.size(); ++i)
    {
        size_t context_first                = i - bit_window_sz;
        size_t context_last                 = i;
        std::vector<bool> bitvec_context    = std::vector<bool>(std::next(bitvec.begin(), context_first), std::next(bitvec.begin(), context_last));
        std::string str_context             = bit_vector_to_str(bitvec_context);

        rs.push_back
        (
            Token
            {
                .context    = std::move(str_context),
                .c          = bitvec[i]
            }
        );
    }

    return rs;
}

auto mask_token_vector(const std::vector<Token>& token_vec) -> std::vector<Token>
{
    std::vector<Token> rs = token_vec;

    for (const auto& token: token_vec)
    {
        std::vector<bool> inp_bool_vec  = str_to_bit_vector(token.context);

        for (size_t i = 0u; i < inp_bool_vec.size(); ++i)
        {
            std::vector<bool> tmp_bool_vec  = inp_bool_vec;
            tmp_bool_vec[i]                 = !tmp_bool_vec[i];

            rs.push_back
            (
                Token
                {
                    .context    = bit_vector_to_str(tmp_bool_vec),
                    .c          = (randomize_int(0, 2) == 0) ? false : true
                }
            );
        }
    }

    return rs;
}

template <class T>
auto slice_vector(const std::vector<T>& vec, size_t first, size_t sz) -> std::vector<T>
{
    first       = std::min(first, vec.size());
    size_t last = std::min(first + sz, vec.size());

    return std::vector<T>(std::next(vec.begin(), first), std::next(vec.begin(), last));
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

auto binary_punch(char c) -> std::vector<bool>
{
    std::vector<bool> bitvec    = str_to_bit_vector({c});
    std::vector<bool> rs        = {};

    for (size_t i = 0u; i < bitvec.size(); ++i)
    {
        if (bitvec[i])
        {
            rs.push_back(true);
            rs.push_back(false);
        }
        else
        {
            rs.push_back(false);
            rs.push_back(true);
        }
    }

    return rs;
}

auto ff_punch(char c) -> std::vector<bool>
{
    std::vector<bool> rs(256, false);
    rs[std::bit_cast<uint8_t>(c)] = true;

    return rs;
}

auto token_to_input_binary(const Token& tok) -> std::vector<bool>
{
    std::vector<bool> rs{};

    //16 x 32
    //16 x 2
    //32

    for (char c: tok.context)
    {
        std::vector<bool> c_bitvec  = binary_punch(c);
        rs.insert(rs.end(), c_bitvec.begin(), c_bitvec.end());
    }

    return rs;
}

auto token_to_output_binary(const Token& tok) -> std::vector<bool>
{
    static std::vector<bool> true_vec   = []
    {
        std::vector<bool> rs{};

        for (size_t i = 0u; i < 32; ++i)
        {
            rs.push_back(false);
        }

        for (size_t i = 0u; i < 32; ++i)
        {
            rs.push_back(true);
        }

        return rs;
    }();

    static std::vector<bool> false_vec  = []
    {
        std::vector<bool> rs{};

        for (size_t i = 0u; i < 32; ++i)
        {
            rs.push_back(true);
        }

        for (size_t i = 0u; i < 32; ++i)
        {
            rs.push_back(false);
        }

        return rs;
    }();

    if (tok.c)
    {
        return true_vec;
    }

    return false_vec;
}

auto guess_word(const std::shared_ptr<tensor_model::Matrix>& matrix) -> bool //symmetry
{
    std::vector<tensor_std_float_t> flat_tensor_vec{};

    tensor_factory::flatten(matrix, flat_tensor_vec);
    std::vector<tensor_std_float_t> accum_tensor_vec(2u);

    assert(flat_tensor_vec.size() == 64);

    for (size_t i = 0u; i < 2u; ++i)
    {
        tensor_std_float_t total = 0;

        for (size_t j = 0u; j < 32; ++j)
        {
            total += flat_tensor_vec[i * 32 + j];
        }

        accum_tensor_vec[i] = total;
    }

    return accum_tensor_vec[1] > accum_tensor_vec[0];
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
            .optimization_epoch_sz                      = 128ULL,
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

            return parity_difference(flat_out_vec, flat_out_vec_0);
        }

    private:

        auto parity_difference(const std::vector<std::vector<tensor_std_float_t>>& result,
                               const std::vector<std::vector<tensor_std_float_t>>& expected) -> eval_float_t
        {
            eval_float_t rs = 0;

            for (const auto& [e_lhs, e_rhs]: stdx::zip(result, expected))
            {
                if (e_lhs.size() != e_rhs.size())
                {
                    std::abort();
                }

                assert(e_lhs.size() % 2 == 0u);

                size_t half_sz                      = e_lhs.size() / 2;

                eval_float_t expected_false_score   = 0;
                eval_float_t expected_true_score    = 0;

                for (size_t i = 0u; i < half_sz; ++i)
                {
                    expected_false_score    += e_rhs[i];
                    expected_true_score     += e_rhs[i + half_sz];
                }

                eval_float_t actual_false_score     = 0;
                eval_float_t actual_true_score      = 0;

                for (size_t i = 0u; i < half_sz; ++i)
                {
                    actual_false_score      += e_lhs[i];
                    actual_true_score       += e_lhs[i + half_sz];
                }

                eval_float_t expected_parity        = expected_false_score - expected_true_score;
                eval_float_t actual_parity          = actual_false_score - actual_true_score;

                rs                                  += std::pow(expected_parity - actual_parity, 2);
            }

            return rs;
        }

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
                    tmp += std::pow(e_lhs[i] - e_rhs[i], 4);
                }

                rs += tmp;
            }

            return rs;
        }
};

auto get_matrix_evaluator(const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>& point_vec) -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
{
    return std::make_unique<PointPullMatrixEvaluator>(point_vec);
}

auto get_loss(the_matrix::MatrixInterface& matrix,
              const std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, bool>>& training_pair_vec) -> double
{
    std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec{};
    std::vector<bool> char_vec{};

    for (const auto& [mtrx, c]: training_pair_vec)
    {
        inp_vec.push_back(mtrx);
        char_vec.push_back(c);
    }

    std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec = matrix.project(inp_vec);
    size_t correct_word = 0u;

    for (size_t i = 0u; i < out_vec.size(); ++i)
    {
        bool predicted_word = guess_word(out_vec[i]);

        if (predicted_word == char_vec[i])
        {
            std::cout << "predicted word > " << static_cast<size_t>(predicted_word) << "<> correct word > " << static_cast<size_t>(char_vec[i]) << "\n";
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

//today we'd work on compile-time optimizables (tomorrow)
//I strongly believe that models are compile-time optimizables, and there is a program to tune the parameters, we have successfully proved that these logit paths could be punched through

//what I want today tomorrow is we could run the Reddit PoC on cuda, on a hex vocabulary, or even a char vocabulary, but I doubt that would be an issue, for the reason being predicting the next bool is just as hard as predicting the next char
//what we'd need is random sampling, a context window, and an exponential prediction for the exponential property is the word count in the input field

//I have intel saying that 8192 hash_table_sz and <4, 2, 2, 2, ...> should suffice for super intelligent and super intelligence
//that would still be well under < 128MB, so I don't think there will be transportation issues (there would be)
//so I can say that memory sync is mandatory even though we break immutability

//we'd need to tune the static_fields by analyzing the convergence and the training rate
//we'd most likely focus on convergence

//today I'd focus on flops

//cuda memory allocations have to be on device, I have tried to think of different scenerios, but it's bad practice
//we'd go through the implementation one more time, optimize host flops -> x2, x3, x4
//we'll talk next steps

//7/8 seems to be "the edge" of context diffraction, in the sense of I mean A, you mean B, we mean C
//and 7/8 can drag the lowest "exponent" 10-12 transformation hops, the highest "exponent" 20 transformation hops

//I have concluded that 3/4 7/8 is optimal in this sense, and does not converge to "the one" too fast
//it means that we can train the initial layer logits without worrying about "nasal demons" or "disappearance" of final computation results
//I mean disappearance is a feature, but not in the sense of feature that 1/2 ** 10 hops == 1 means that x ** (1/2 ** 10) cannot be trained

//we chose 3/4 7/8 purely because it is computational efficient, it introduces entropy to the continuous projection, and it can drag to the final layer
//we have anticipated for "disappearance" being the feature, by using y = y + x

//we simply incorporated the implicit brain growth without ever blocking it, WOW!
//maybe not, we'd still need training blockages

//I have actually run some numbers about entropy / logit unit
//it seems like one of the solution to the continuous projection scheme is to make the operating matrix bigger (in conjunction with the interpolation)

//and if we can't "mean C," we'd have to increase the number of being_unit. Base LOGIT_SZ or PROCESS_GROUP_SZ are purely for crunching flops
//and we have crunched all the possible flops (we just need better allocation patterns)
//it's in the continuous equation term

//the interpolations are probably not "part of the continuous" equation, but works in conjunction with the continuous equation if correct configurations (and synchronization points)

int main()
{
    initialize_concurrency_base();

    const size_t OPTIMIZATION_SZ                = size_t{1} << 10;
    const size_t TRAINING_DATA_SZ               = 10;
    const size_t WINDOW_SZ                      = size_t{1} << 2;

    std::vector<Token> token_vec                = slice_vector(mask_token_vector(window_tokenize(read_training_data(TRAINING_DATA_SZ), WINDOW_SZ)), 0, 1000);

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

    std::cout << "<operable_sz> > " << operable_sz << "\n";
    std::cout << "<matrix_shape_size> > " << tensor_factory::shape_size(matrix_shape) << "\n";

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

    std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, bool>> matrix_char_pair_vec{};

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