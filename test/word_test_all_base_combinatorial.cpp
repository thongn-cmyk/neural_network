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

struct Projection
{
    std::vector<float> x;
    float y;
};

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

template <class T>
auto cat(const std::vector<T>& lhs, const std::vector<T>& rhs) -> std::vector<T>
{
    auto tmp = lhs;
    tmp.insert(tmp.end(), rhs.begin(), rhs.end());

    return tmp;
}

auto make_projection(size_t level,
                     float first, float last, size_t discretization_sz) -> std::vector<Projection>
{

    if (discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    if (level == 0u)
    {
        float discrete_interval     = (last - first) / discretization_sz;
        float val                   = first;
        std::vector<Projection> rs  = {};

        for (size_t i = 0u; i < discretization_sz; ++i)
        {
            rs.push_back
            (
                Projection
                {
                    .x  = {val},
                    .y  = val
                }
            );

            val += discrete_interval;
        }

        return rs;
    }

    size_t sub_discretization_sz    = std::sqrt(discretization_sz);

    if (sub_discretization_sz * sub_discretization_sz != discretization_sz)
    {
        throw std::invalid_argument("bad sub discretization, not even sqrt");
    }

    std::vector<Projection> lhs     = make_projection(level - 1, first, last, sub_discretization_sz);
    std::vector<Projection> rhs     = make_projection(level - 1, first, last, sub_discretization_sz);
    std::vector<Projection> rs      = {};

    float discrete_interval         = (last - first) / discretization_sz;
    float val                       = first;

    for (const Projection& e_lhs: lhs)
    {
        for (const Projection& e_rhs: rhs)
        {
            rs.push_back
            (
                Projection
                {
                    .x  = cat(e_lhs.x, e_rhs.x),
                    .y  = val
                }
            );

            val += discrete_interval;
        }
    }

    return rs;
}

struct insufficient_coefficient_size: std::invalid_argument
{
    insufficient_coefficient_size(): std::invalid_argument("insufficient coefficient size"){}
};

auto binary_unf_interpolated_project(const float * x_arr, size_t x_arr_sz,
                                     float x_first, float x_last, size_t top_discretization_sz,
                                     const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap) -> float
{
    if (x_arr_sz == 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, 0");
    }

    //right, this should be lhs =, rhs = but we'd cut some slack here, it's semantically different

    if (x_arr_sz == 1u)
    {
        return x_arr[0];
    }

    if (std::isnan(x_first))
    {
        throw std::invalid_argument("bad x_first, NaN");
    }

    if (std::isnan(x_last))
    {
        throw std::invalid_argument("bad x_last, NaN");
    }

    if (x_first >= x_last)
    {
        throw std::invalid_argument("bad interval x_first >= x_last");
    }

    if (x_arr_sz % 2u != 0u)
    {
        throw std::invalid_argument("bad x_arr_sz, not multiples of 2");
    }

    if (top_discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    size_t discretization_sz        = std::sqrt(top_discretization_sz);

    if (discretization_sz * discretization_sz != top_discretization_sz)
    {
        throw std::invalid_argument("bad sub discretization, not even sqrt");
    }

    float global_interval           = x_last - x_first;
    float discretization_interval   = global_interval / discretization_sz;

    float lhs                       = binary_unf_interpolated_project(x_arr, x_arr_sz / 2,
                                                                      x_first, x_last, discretization_sz,
                                                                      coeff_arr, coeff_arr_offset, coeff_arr_cap);

    float rhs                       = binary_unf_interpolated_project(std::next(x_arr, x_arr_sz / 2), x_arr_sz / 2,
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

    float a                         = coeff_arr[global_offset + 0];
    float b                         = coeff_arr[global_offset + 1];
    float c                         = coeff_arr[global_offset + 2];

    float cand_y                    = a * lhs + b * rhs + c;

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

        float x_first;
        float x_last;
        size_t discretization_sz;

    public:

        SomeProjector(float x_first,
                      float x_last,
                      size_t discretization_sz): x_first(x_first),
                                                 x_last(x_last),
                                                 discretization_sz(discretization_sz){}

        auto project(const float * x_arr, size_t x_arr_sz,
                     const float * coeff_arr, size_t coeff_arr_cap) -> float
        {
            size_t coeff_arr_offset = 0u;

            return binary_unf_interpolated_project(x_arr, x_arr_sz,
                                                   this->x_first, this->x_last, this->discretization_sz,
                                                   coeff_arr, coeff_arr_offset, coeff_arr_cap);
        }

        auto get_slot(float y) -> size_t
        {
            float _y                        = std::clamp(y, this->x_first, this->x_last);
            float discretization_interval   = (this->x_last - this->x_first) / this->discretization_sz;
            size_t tentative_slot           = (_y - this->x_first) / discretization_interval;

            return std::min(tentative_slot, static_cast<size_t>(this->discretization_sz - 1u));
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
            std::vector<float> coeff_vec        = stdx::to_castable_vector_initializer(matrix.get_coefficient_vector());
            std::vector<float> expected_y_vec   = {};
            std::vector<float> projected_y_vec  = {};

            for (const Projection& projection: this->training_pair_vec)
            {
                expected_y_vec.push_back(projection.y);

                float projected_y = this->projector->project(projection.x.data(), projection.x.size(),
                                                             coeff_vec.data(), coeff_vec.size());

                projected_y_vec.push_back(projected_y);
            }

            return this->mean_sqrt(projected_y_vec, expected_y_vec);
        }

        auto get_score(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<float> coeff_vec    = stdx::to_castable_vector_initializer(matrix.get_coefficient_vector());
            size_t match_cnt                = 0u;

            for (const Projection& projection: this->training_pair_vec)
            {
                float projected_y   = this->projector->project(projection.x.data(), projection.x.size(),
                                                                coeff_vec.data(), coeff_vec.size());

                size_t expected_slot  = this->projector->get_slot(projection.y);
                size_t projected_slot = this->projector->get_slot(projected_y);

                if (expected_slot == projected_slot)
                {
                    match_cnt += 1u;
                }
            }

            if (this->training_pair_vec.size() == 0u)
            {
                return 0.0;
            }

            return static_cast<double>(match_cnt) / this->training_pair_vec.size();
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
            .optimization_epoch_sz                      = 4096ULL,
            .optimization_step_sz                       = 1ULL,
            .optimization_loop_sz                       = 8ULL
        }
    );
}

//according to my knowledge, whatever the final layer output is, it follows the rule of next word prediction
//so either we can assign different letters to different logits, or we have all logits to represent a letter
//it is semantically the same, but assignning different letters to different logits have an additional memorization of where to zero it, and where to one it, which is a constant cost of transformation (a rule)
//so if we all mean A, then it means A, if we all mean B then it means B
//so either case, the letter A must be presented, but telling others to say A, or telling others to yell harder is a technical choice of which is more constantly costly

//what I do know for sure is that it is "trainable" at every "transform" checkpoints
//in the sense that we all yell A or B or C post the recursive 1 transform, or at the unit transform (which is a full one round transform)

//each checkpoint holds a weight, and this weight is moving to the right of the scale as we propagate forward, in the sense that the weight of the first transform feedback is not as important as the weight of the second transform feedback
//according to my understanding, we look far forward to make the next step right
//what do you want to do in the next 10 years? I don't have a faintest clue

//but it is worth checking if we can pull the scheme of correct next interpolation, if we run it parallel, and the best results are weighted towards some interpolation values, then we'd commit the interpolation choices, right?

//Am I saying that we'd run this in parallel, get to know the best of it (without loss of generality) at the end of the road, after exhausting some billion billion flops, just to get the next interpolation values?
//Yes
//and we'd have to do this again and again and again

//this in the sense is a greedy algorithm, sub-optimal
//because we can't thread 100 recursive stacks one shot, it's impossible
//so we'd thread 20 recursive stacks, then 21 recursive stacks then ...

//

void run_test()
{
    const size_t LEVEL                  = 3;
    const float X_FIRST                 = 0;
    const float X_LAST                  = 1;
    const size_t SEMANTIC_SZ            = 256;

    const size_t EPOCH_SZ               = size_t{1} << 8;

    std::vector<Projection> projection_vec          = make_projection(LEVEL,
                                                                      X_FIRST, X_LAST, SEMANTIC_SZ);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();
    std::vector<tensor_std_float_t> tensor_vec                                              = std::vector<tensor_std_float_t>(get_binary_unf_interpolated_projection_size(projection_vec.front().x.size(),
                                                                                                                                                                          X_FIRST, X_LAST, SEMANTIC_SZ),
                                                                                                                              0.f);

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "coefficient vector size > " << tensor_vec.size() << "\n";

    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = std::make_unique<SomeMatrix>(std::move(tensor_vec));
    std::unique_ptr<SomeProjector> projector                                                = std::make_unique<SomeProjector>(X_FIRST,
                                                                                                                              X_LAST,
                                                                                                                              SEMANTIC_SZ);

    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_vec, std::move(projector));
    common_exception::CancellationToken cancellation_token                                  = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);
        double optimized_score      = matrix_evaluator->get_score(*matrix);

        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << "\n";
        std::cout << "i > " << -1 << " score > " << optimized_score << "\n";
    }

    for (size_t i = 0u; i < EPOCH_SZ; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);
        double optimized_score      = matrix_evaluator->get_score(*matrix);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
        std::cout << "i > " << i << " score > " << optimized_score << "\n";
    }
}

int main()
{
    run_test();
}