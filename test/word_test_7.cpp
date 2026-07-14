#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

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

//there isn't other optimizables (this point of code is so insanely tiny that I've spent the last 6 months to reach, I'm telling you that this is no easy job)
//I've been investigating the continuous optimization scheme longer than I have wanted

//the hinge is physical matrix distance of the relevant continuous optimizables
//our job as a logit-binding engineer is to figure out the way to evenly distribute the compressible_scale / logit unit
//I've come to the conclusion that parity + row-aligned hubs and row-aligned inputs are the answer

//now I want we to think in terms of non-continuous dispatch

//and synchronization string, remember that our problem of interpolation is 1000, 1000.1 are to two different strings, and that would be wasting because they are to project the same end-result (down the string line)
//but if we can guarantee that the two tokens are to be met again, so the string of meeting again would be synchronization point of the two tokens, so only identity transformation is required before the synchronization point, so we are projecting two for one (this is the gain)
//can we force identity transformation? (this is the topic that we will implement tomorrow)

//imagine that we have a bag of 1 million tokens -> 10000 points (dynamic programming)

//after the first interpolation, we'd nag 1000 points of synchronizables
//after the second interpolation, we'd nag 900 points of synchronizables (points that are waiting to be project to the same value in the same coordinate)

//can we project 10 for one? yes, without loss of generality. But this scheme is only possible for x^1 + x^0.9 + x^0.8 + ... + C

//Do i pro interpolation - yes, but that is not in the continuous scheme, I rather think that continuous is one business, interpolation is another
//how many rotation times? (very important) I have absolutely no clue, but we'd be there within a week

using namespace taylor_matrix::host_matrix::the_host_matrix;
using namespace taylor_matrix::host_matrix::tensor_matrix_operation;
using namespace float_def;

using DispatchCodeGenerator = taylor_matrix::host_matrix::dispatch_code_generator::DispatchCodeGenerator;

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

auto even_unsigned_div(size_t lhs, size_t rhs) -> size_t
{
    if (rhs == 0u)
    {
        throw std::invalid_argument("bad divisor, 0");
    }

    if (lhs % rhs != 0u)
    {
        throw std::invalid_argument("bad mod, != 0");
    }

    return lhs / rhs;
}

template <class Callback, size_t FIRST, size_t LAST>
void to_integral_constant(Callback&& callback,
                          size_t val,
                          const std::integral_constant<size_t, FIRST> first,
                          const std::integral_constant<size_t, LAST> last)
{
    static_assert(LAST > FIRST);


    if (val < first || val >= last)
    {
        throw std::invalid_argument("bad val, out of range");
    }

    constexpr size_t SZ = LAST - FIRST;

    [&]<size_t ...IDX>(const std::index_sequence<IDX...>)
    {
        (
            [&]
            {
                (void) IDX;
                constexpr size_t CAND = FIRST + IDX;

                if (CAND == val)
                {
                    callback(std::integral_constant<size_t, CAND>{});
                }
            }(), ...
        );
    }(std::make_index_sequence<SZ>{});
}

//we'd want to optimize maximum random weights/ string with minimal efforts
//maybe not minimal efforts
//but the minimal effort per random weight

//we'd try to get to 128 - 256 random tokens on a string
//the problem is that we get saturated too fast to even make sense of the single string

//this is concluded after reasoning that the only deliverable in the case is to increase single string saturation (but not breaking string properties by using sin(x)/x waves or non-continuous hacks)

//what we observed is that there are two distinct interferences:
    //first is the operating dimension and training strategy
    //second is the input distribution

//it seems that more operating dimension => better
//parity training strategy (a version of binary cross entropy) => better

//if 000...111 => better (000 or 111 => closer in the matrix distance => amplify the comtext of the training logits)
//the 0 and the 1 in 000...111 too far away => bad, because if I mean 1, 1 have to travel half the world to mean it
    //counter strategy 000...111, 000...111, ... => optimal 
    //number of regions becomes optimizable

//input distribution
//three representations:
    //row-aligned
    //or even-spaced
    //or row-aligned and even-spaced

//even-spaced => make room for other words to mean something first before combining them
//there is a threshold before that has destructive interference

//that is where row-aligned kicks in, to make sure that relevant inputs are stayed together

//I rather think that instead of wasting time figuring things out
//this can be run-time optimizable

//we'd stick to the goal of finding configuration that can withhold 1024 high entropy tokens on a single string (this is the point)
//single string optimization is important because we can apply dynamic programming on the string (whereas with hashing interpolation we can't)

//ok, one more continuous optimzation
//we are going to do cubic quantization + interpolation

static inline const size_t INPUT_DIMENSION_SZ   = 16;
static inline const size_t INPUT_SZ             = 64;

static inline const std::unordered_map<size_t, std::vector<size_t>> SHAPE_MAP = 
{
    {
        32,
        {
            size_t{1} << 1,
            1,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    },
    {
        128,
        {
            size_t{1} << 2,
            2,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    },
    {
        1024,
        {
            size_t{1} << 4,
            4,
            tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
            tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ
        }
    }
};

static inline const std::vector<size_t> DIMENSION_SZ_OPTIMIZABLE_VEC =
{
    128,
    1024
};

static inline const std::vector<size_t> BASE_SZ_OPTIMIZABLE_VEC   =
{
    2,
    2,
    2
};

static inline const uint8_t TRAINING_STRATEGY_GLOBAL_PARITY                 = 0;
static inline const uint8_t TRAINING_STRATEGY_TWO_ADJECENT_REGION_PARITY    = 1;
static inline const uint8_t TRAINING_STRATEGY_FOUR_ADJECENT_REGION_PARITY   = 2;

static inline const std::vector<uint8_t> TRAINING_STRATEGY_VEC  =
{
    // TRAINING_STRATEGY_GLOBAL_PARITY,
    TRAINING_STRATEGY_TWO_ADJECENT_REGION_PARITY,
    TRAINING_STRATEGY_FOUR_ADJECENT_REGION_PARITY
};

static inline const uint8_t INPUT_DISTRIBUTION_EVEN_SPACED                  = 0;
static inline const uint8_t INPUT_DISTRIBUTION_ON_ROW                       = 1;

static inline const std::vector<uint8_t> INPUT_DISTRIBUTION_VEC =
{
    INPUT_DISTRIBUTION_EVEN_SPACED,
    INPUT_DISTRIBUTION_ON_ROW
};

struct MatrixConfig
{
    size_t matrix_operable_sz;
    size_t base_projection_sz;
};

struct TokenConfig
{
    size_t input_dimension_sz;
    size_t input_sz;
};

struct TrainingTokenConfig
{
    uint8_t input_distribution;
    uint8_t training_strategy;

    size_t actual_dimension_sz;

    TokenConfig token_config;
};

struct TrainingConfig
{
    MatrixConfig matrix_config;
    TrainingTokenConfig token_config;
};

struct Score
{
    double score;
};

struct TrainingReport
{
    std::vector<Score> score_vec;
};

static inline const size_t TRAINING_OPTIMIZATION_SZ     = 20;
static inline const std::filesystem::path OUTPUT_FOLDER = "/Users/megazone/Downloads/SeriousBillionDollarProject/src/test/word_test_output";

auto get_matrix_config_vector() -> std::vector<MatrixConfig>
{
    std::vector<MatrixConfig> matrix_config_vec = {};

    for (const auto& dimension_sz: DIMENSION_SZ_OPTIMIZABLE_VEC)
    {
        for (const auto& base_sz: BASE_SZ_OPTIMIZABLE_VEC)
        {
            matrix_config_vec.push_back
            (
                MatrixConfig
                {
                    .matrix_operable_sz     = dimension_sz,
                    .base_projection_sz     = base_sz
                }
            );
        }
    }

    return matrix_config_vec;
}

auto get_token_config() -> TokenConfig
{
    return TokenConfig
    {
        .input_dimension_sz     = INPUT_DIMENSION_SZ,
        .input_sz               = INPUT_SZ
    };
}

auto get_training_token_config_vector(const MatrixConfig& matrix_config) -> std::vector<TrainingTokenConfig>
{
    std::vector<TrainingTokenConfig> training_token_config_vec  = {};

    for (const auto& input_distribution: INPUT_DISTRIBUTION_VEC)
    {
        for (const auto& training_strategy: TRAINING_STRATEGY_VEC)
        {
            training_token_config_vec.push_back
            (
                TrainingTokenConfig
                {
                    .input_distribution     = input_distribution,
                    .training_strategy      = training_strategy,
                    .actual_dimension_sz    = matrix_config.matrix_operable_sz,
                    .token_config           = get_token_config()
                }
            );
        }
    }

    return training_token_config_vec;
}

auto get_training_config_vector() -> std::vector<TrainingConfig>
{
    std::vector<MatrixConfig> matrix_config_vec     = get_matrix_config_vector();
    std::vector<TrainingConfig> training_config_vec = {};

    for (const auto& matrix_config: matrix_config_vec)
    {
        std::vector<TrainingTokenConfig> training_token_config_vec  = get_training_token_config_vector(matrix_config);

        for (const auto& training_token_config: training_token_config_vec)
        {
            training_config_vec.push_back
            (
                TrainingConfig
                {
                    .matrix_config  = matrix_config,
                    .token_config   = training_token_config
                }
            );
        }
    }

    return training_config_vec;
}


auto get_matrix(const MatrixConfig& matrix_config) -> std::unique_ptr<the_matrix::MatrixInterface>
{
    return TheHostMatrixFactory{}.set_vector_size(matrix_config.matrix_operable_sz)
                                 .get();
}

auto randomize_bit_vector(size_t sz) -> std::vector<bool>
{
    std::vector<bool> rs    = {};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(randomize_bool());
    }

    return rs;
}

auto to_one_bit_different_set(const std::vector<bool>& arg) -> std::vector<std::vector<bool>>
{
    std::vector<std::vector<bool>> rs{};

    for (size_t i = 0u; i < arg.size(); ++i)
    {
        std::vector<bool> tmp   = arg;
        tmp[i]                  = !tmp[i];

        rs.push_back(std::move(tmp));
    }

    return rs;
}

auto get_binary_parity_vector(size_t sz,
                              bool val) -> std::vector<bool>
{
    if (sz % 2 != 0)
    {
        throw std::invalid_argument("bad sz, not multiplies of 2");
    }

    size_t half_sz          = sz / 2;
    std::vector<bool> rs    = {};

    for (size_t i = 0u; i < half_sz; ++i)
    {
        rs.push_back(!val);
    }

    for (size_t i = 0u; i < half_sz; ++i)
    {
        rs.push_back(val);
    }

    return rs;
}

template <class T>
auto multiply_vector(const std::vector<T>& vec,
                     size_t sz) -> std::vector<T>
{
    if (sz == 0u)
    {
        return {};
    }

    size_t half_sz          = sz / 2;
    std::vector<T> half_vec = multiply_vector(vec, half_sz);
    std::vector<T> full_vec = half_vec;

    full_vec.insert(full_vec.end(), half_vec.begin(), half_vec.end());

    if (sz % 2 == 1)
    {
        full_vec.insert(full_vec.end(), vec.begin(), vec.end());
    }

    return full_vec;
}

template <class T>
auto multiply_fit(const std::vector<T>& vec,
                  size_t sz) -> std::vector<T>
{
    size_t multiplier   = even_unsigned_div(sz, vec.size());
    std::vector<T> rs   = {};

    for (size_t i = 0u; i < vec.size(); ++i)
    {
        for (size_t j = 0u; j < multiplier; ++j)
        {
            rs.push_back(vec[i]);
        }
    }

    return rs;
}

template <class T>
auto row_fit(const std::vector<T>& vec,
             size_t sz) -> std::vector<T>
{
    size_t multiplier   = even_unsigned_div(sz, vec.size());
    
    if (multiplier == 0u)
    {
        return vec;
    }

    std::vector<T> rs   = std::vector<T>(sz, T{});

    size_t row_idx      = randomize_int(0u, multiplier);
    size_t offset       = row_idx * vec.size();

    std::copy(vec.begin(),
              vec.end(),
              std::next(rs.begin(), offset));

    return rs;
}

auto get_matrix(const std::vector<bool>& arg) -> std::shared_ptr<tensor_model::Matrix>
{
    std::vector<tensor_std_float_t> tensor_vec{};

    for (bool e: arg)
    {
        tensor_vec.push_back(static_cast<tensor_std_float_t>(e));
    }

    return make_matrix_from_flat_vec(SHAPE_MAP.at(arg.size()), tensor_vec);
}

auto get_training_pair_vector(const TrainingTokenConfig& training_token_config) -> std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>,
                                                                                                         std::shared_ptr<tensor_model::Matrix>>>
{
    size_t token_unit_sz    = training_token_config.token_config.input_dimension_sz;
    size_t token_sz         = training_token_config.token_config.input_sz;

    if (token_unit_sz == 0u)
    {
        throw std::invalid_argument("bad token unit size, 0");
    }

    size_t replica_sz       = token_sz / 1;
    auto rs                 = std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>,
                                                    std::shared_ptr<tensor_model::Matrix>>>();

    for (size_t i = 0u; i < replica_sz; ++i)
    {
        std::vector<std::vector<bool>> token_vec    = {randomize_bit_vector(token_unit_sz)};

        for (const std::vector<bool>& token: token_vec)
        {
            std::vector<bool> inp_token = {};
            std::vector<bool> out_token = {};

            if (training_token_config.input_distribution == INPUT_DISTRIBUTION_EVEN_SPACED)
            {
                inp_token   = multiply_fit(token, training_token_config.actual_dimension_sz);
            }
            else if (training_token_config.input_distribution == INPUT_DISTRIBUTION_ON_ROW)
            {
                inp_token   = row_fit(token, training_token_config.actual_dimension_sz);
            }
            else
            {
                throw std::invalid_argument("bad input distribution, invalid dispatch code");
            }

            if (training_token_config.training_strategy == TRAINING_STRATEGY_GLOBAL_PARITY)
            {
                out_token   = get_binary_parity_vector(training_token_config.actual_dimension_sz, randomize_bool());
            }
            else if (training_token_config.training_strategy == TRAINING_STRATEGY_TWO_ADJECENT_REGION_PARITY)
            {
                out_token   = multiply_vector(get_binary_parity_vector(even_unsigned_div(training_token_config.actual_dimension_sz, 2), randomize_bool()), 2);
            }
            else if (training_token_config.training_strategy == TRAINING_STRATEGY_FOUR_ADJECENT_REGION_PARITY)
            {
                out_token   = multiply_vector(get_binary_parity_vector(even_unsigned_div(training_token_config.actual_dimension_sz, 4), randomize_bool()), 4);
            }
            else
            {
                throw std::invalid_argument("bad traininig strategy, invalid dispatch code");
            }

            rs.push_back(std::make_pair(get_matrix(inp_token), get_matrix(out_token)));
        }
    }

    return rs;
}

auto get_deviation(const std::vector<tensor_std_float_t>& arg) -> double
{
    if (arg.size() == 0u)
    {
        std::abort();
    }

    double total_value  = 0;

    for (const auto& e: arg)
    {
        total_value += e;
    }

    double mean_value   = total_value / arg.size();
    double deviation    = 0;

    for (const auto& e: arg)
    {
        deviation += std::pow(e - mean_value, 2);
    }

    deviation   /= arg.size();

    return deviation;
}

auto get_parity_distance(const std::shared_ptr<tensor_model::Matrix>& lhs,
                         const std::shared_ptr<tensor_model::Matrix>& rhs) -> double
{
    std::vector<tensor_std_float_t> lhs_flat_tensor_vec{};
    std::vector<tensor_std_float_t> rhs_flat_tensor_vec{};

    tensor_factory::flatten(lhs, lhs_flat_tensor_vec);
    tensor_factory::flatten(rhs, rhs_flat_tensor_vec);

    if (lhs_flat_tensor_vec.size() != rhs_flat_tensor_vec.size())
    {
        std::cout << "mayday, mismatched tensor logit vector\n";
        std::abort();
    }

    double lhs_false_sum    = 0;
    double lhs_true_sum     = 0;

    double rhs_false_sum    = 0;
    double rhs_true_sum     = 0;

    std::vector<tensor_std_float_t> true_vec{};
    std::vector<tensor_std_float_t> false_vec{};

    for (size_t i = 0u; i < rhs_flat_tensor_vec.size(); ++i)
    {
        if (rhs_flat_tensor_vec[i] == 1)
        {
            rhs_true_sum    += 1;
            lhs_true_sum    = std::max(lhs_true_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
        }
        else
        {
            rhs_false_sum   += 0;
            lhs_false_sum   = std::max(lhs_false_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
        }
    }

    double lhs_parity       = lhs_true_sum - lhs_false_sum;
    double rhs_parity       = rhs_true_sum - rhs_false_sum;

    if (lhs_parity > 0)
    {
        return 0;
    }

    return std::pow(lhs_parity - rhs_parity, 2); 
}

auto is_same_parity(const std::shared_ptr<tensor_model::Matrix>& lhs,
                    const std::shared_ptr<tensor_model::Matrix>& rhs) -> bool
{
    std::vector<tensor_std_float_t> lhs_flat_tensor_vec{};
    std::vector<tensor_std_float_t> rhs_flat_tensor_vec{};

    tensor_factory::flatten(lhs, lhs_flat_tensor_vec);
    tensor_factory::flatten(rhs, rhs_flat_tensor_vec);

    if (lhs_flat_tensor_vec.size() != rhs_flat_tensor_vec.size())
    {
        std::cout << "mayday, mismatched tensor logit vector\n";
        std::abort();
    }

    double lhs_false_sum    = 0;
    double lhs_true_sum     = 0;

    double rhs_false_sum    = 0;
    double rhs_true_sum     = 0;

    for (size_t i = 0u; i < rhs_flat_tensor_vec.size(); ++i)
    {
        if (rhs_flat_tensor_vec[i] == 1)
        {
            rhs_true_sum    += 1;
            lhs_true_sum    = std::max(lhs_true_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
        }
        else
        {
            rhs_false_sum   += 0;
            lhs_false_sum   = std::max(lhs_false_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
        }
    }

    double lhs_parity       = lhs_true_sum - lhs_false_sum;
    double rhs_parity       = rhs_true_sum - rhs_false_sum;
    
    return lhs_parity > 0;
}

//OK, it works fellas

//our thesis remains, all the heavy lifting you mean A, I mean B, we mean C are entirely at the ProcessUnit layer, and we'd have to do heavy interpolation there

//I want to see if certain techniques work, like punching through at saturation
//it works OK, let's pray that we can pass the word_test_8
//it's just a little difficult

auto get_optimizer() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
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

        PointPullMatrixEvaluator(std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_pair_vec): training_pair_vec(std::move(training_pair_vec)){}

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
            double rs = 0;

            for (const auto [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                rs += get_parity_distance(out, expected_out);
            }

            return rs / std::pow(get_score(matrix), 2);
        }

        auto get_score(the_matrix::MatrixInterface& matrix) -> double
        {
            std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec      = {};
            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec_0    = {};

            for (const auto& [inp, out]: this->training_pair_vec)
            {
                inp_vec.push_back(inp);
                out_vec_0.push_back(out);
            }

            std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec      = matrix.project(inp_vec);
            double hit      = 0;
            double total    = 0;

            for (const auto& [out, expected_out]: stdx::zip(out_vec, out_vec_0))
            {
                hit     += is_same_parity(out, expected_out);
                total   += 1;
            }

            if (total == 0)
            {
                return 0;
            }

            return hit / total;
        }
};

auto optimize(const TrainingConfig& config,
              size_t epoch_sz) -> TrainingReport
{
    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();
    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = get_matrix(config.matrix_config);

    std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> projection_pair_vec    = get_training_pair_vector(config.token_config);

    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_pair_vec);
    common_exception::CancellationToken cancellation_token                                  = {};
    TrainingReport training_report                                                          = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);
        double score                = matrix_evaluator->get_score(*matrix);
        
        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << " score > " << score << "\n";
    }

    for (size_t i = 0u; i < epoch_sz; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);
        double score                = matrix_evaluator->get_score(*matrix);
        
        std::cout << "i > " << i << " deviation > " << optimized_deviation << " score > " << score << "\n";

        training_report.score_vec.push_back(Score{score});
    }

    return training_report;
}

auto can_converge_to_one(const TrainingReport& training_report,
                         size_t expected_epoch_sz) -> bool
{
    return {};
}

auto get_string_report(const TrainingReport& training_report,
                       const TrainingConfig& training_config) -> std::string
{
    std::ostringstream oss;

    // Header
    oss << "=== TRAINING REPORT ===\n\n";
    
    // Configuration Details
    oss << "--- Configuration ---\n";
    oss << "Matrix Configuration:\n";
    oss << "  Operable Size: " << training_config.matrix_config.matrix_operable_sz << "\n";
    oss << "  Base Projection Size: " << training_config.matrix_config.base_projection_sz << "\n";

    oss << "\nToken Configuration:\n";
    oss << "  Input Dimension: " << training_config.token_config.token_config.input_dimension_sz << "\n";
    oss << "  Input Size: " << training_config.token_config.token_config.input_sz << "\n";
    oss << "  Actual Dimension: " << training_config.token_config.actual_dimension_sz << "\n";
    oss << "  Input Distribution: " << static_cast<int>(training_config.token_config.input_distribution) << "\n";
    oss << "  Training Strategy: " << static_cast<int>(training_config.token_config.training_strategy) << "\n";
    
    // Training Scores
    const auto& scores = training_report.score_vec;
    
    if (scores.empty()) {
        oss << "\n--- Training Scores ---\n";
        oss << "No scores recorded.\n";
        return oss.str();
    }

    // Calculate statistics
    double min_score = scores[0].score;
    double max_score = scores[0].score;
    double sum_score = 0.0;
    
    for (const auto& s : scores) {
        min_score = std::min(min_score, s.score);
        max_score = std::max(max_score, s.score);
        sum_score += s.score;
    }
    
    double avg_score = sum_score / scores.size();
    
    oss << "\n--- Training Scores ---\n";
    oss << "Total Scores: " << scores.size() << "\n";
    oss << "Minimum Score: " << std::fixed << std::setprecision(6) << min_score << "\n";
    oss << "Maximum Score: " << std::fixed << std::setprecision(6) << max_score << "\n";
    oss << "Average Score: " << std::fixed << std::setprecision(6) << avg_score << "\n";

    // Detailed score list (limit to first 10 to avoid clutter)
    oss << "\nDetailed Scores (first " << std::min(size_t(10), scores.size()) << " of " 
        << scores.size() << "):\n";
    
    for (size_t i = 0; i < std::min(size_t(10), scores.size()); ++i) {
        oss << "  [" << i << "]: " << std::fixed << std::setprecision(6) << scores[i].score << "\n";
    }
    
    if (scores.size() > 10) {
        oss << "  ... (" << (scores.size() - 10) << " more scores)\n";
    }
    
    oss << "\n=== END REPORT ===\n";
    
    return oss.str();
}

void run_optimization()
{
    std::vector<TrainingConfig> training_config_vec = get_training_config_vector();

    std::cout << "__BEGIN_CONTINUOUS_OPTIMIZATION__\n";

    std::cout << "optimizable_sz > " << training_config_vec.size() << "\n";

    for (size_t i = 0u; i < training_config_vec.size(); ++i)
    {
        std::filesystem::path output_path   = OUTPUT_FOLDER / (std::to_string(i) + ".csv");
        TrainingReport report               = optimize(training_config_vec[i], TRAINING_OPTIMIZATION_SZ);
        std::string str_report              = get_string_report(report, training_config_vec[i]);

        std::ofstream of(output_path);
        of.write(str_report.data(), str_report.size());
    }

    std::cout << "__END_CONTINUOUS_OPTIMIZATION__\n";
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
    run_optimization();
}