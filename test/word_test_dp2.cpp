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

struct Node
{
    std::unique_ptr<Node> lhs;
    std::unique_ptr<Node> rhs;
    std::vector<float> semantic_vec;
};

struct NodeContainer
{
    std::unique_ptr<Node> root;
};

auto randomize_float(float first, float last)
{
    static auto randomizer      = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    auto real_distributor       = std::uniform_real_distribution<float>(first, last);

    return real_distributor(randomizer);
}

auto make_tree(size_t height,
               float discretization_value,
               size_t semantic_sz) -> std::unique_ptr<Node>
{
    if (height == 0u)
    {
        return nullptr;
    }

    std::vector<float> semantic_vec = {};

    for (size_t i = 0u; i < semantic_sz; ++i)
    {
        float first = discretization_value * i;
        float last  = first + discretization_value;

        semantic_vec.push_back(randomize_float(first, last));
    }

    return std::make_unique<Node>
    (
        make_tree(height - 1, discretization_value, semantic_sz),
        make_tree(height - 1, discretization_value, semantic_sz),
        std::move(semantic_vec)
    );
}

auto make_node_container(std::unique_ptr<Node>&& root) -> std::unique_ptr<NodeContainer>
{
    return std::make_unique<NodeContainer>
    (
        std::move(root)
    );
}

struct Projection
{
    std::vector<float> x;
    float y;
};

auto make_projection(const std::unique_ptr<Node>& root) -> std::vector<Projection>
{
    if (root == nullptr)
    {
        throw std::invalid_argument("bad root, null");
    }

    bool has_lhs    = root->lhs != nullptr;
    bool has_rhs    = root->rhs != nullptr;

    if (has_lhs ^ has_rhs == true)
    {
        throw std::invalid_argument("bad root, not complete tree");
    }

    bool is_childless   = root->lhs == nullptr;

    std::vector<Projection> rs{};

    if (is_childless)
    {
        for (size_t i = 0u; i < root->semantic_vec.size(); ++i)
        {
            rs.push_back
            (
                Projection
                {
                    .x  = std::vector<float>{root->semantic_vec[i]},
                    .y  = root->semantic_vec[i]
                }
            );
        }

        return rs;
    }

    if (root->semantic_vec.empty())
    {
        throw std::invalid_argument("bad root, empty semantic vector");
    }

    std::unordered_map<float, std::unordered_map<float, float>> semantic_map{};
    size_t semantic_idx = 0u;

    for (float lhs_semantic: root->lhs->semantic_vec)
    {
        for (float rhs_semantic: root->rhs->semantic_vec)
        {
            semantic_map[lhs_semantic][rhs_semantic] = root->semantic_vec[semantic_idx % root->semantic_vec.size()];
            semantic_idx    += 1;
        }
    }

    std::vector<Projection> lhs_projection_vec  = make_projection(root->lhs);
    std::vector<Projection> rhs_projection_vec  = make_projection(root->rhs);

    for (const Projection& lhs_projection: lhs_projection_vec)
    {
        for (const Projection& rhs_projection: rhs_projection_vec)
        {
            std::vector<float> x    = lhs_projection.x;
            x.insert(x.end(), rhs_projection.x.begin(), rhs_projection.x.end());
            float y                 = semantic_map.at(lhs_projection.y).at(rhs_projection.y);

            rs.push_back
            (
                Projection
                {
                    .x  = std::move(x),
                    .y  = y
                }
            );
        }
    }

    return rs;
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

    float diff  = std::abs(expected - actual);

    if (diff < acceptance_width)
    {
        return 0;
    }

    return 1;
}

auto binary_unf_interpolated_deviation_project(const float * x_arr, size_t x_arr_sz,
                                               float x_next, float acceptance_width,
                                               float x_first, float x_last, size_t discretization_sz,
                                               const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap) -> std::pair<float, float>
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
                                                                                    x_arr[mid_sz], acceptance_width,
                                                                                    x_first, x_last, discretization_sz,
                                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap);

    // coeff_arr_offset                    = saved_coeff_arr_offset;
    auto [rhs, rhs_deviation]           = binary_unf_interpolated_deviation_project(std::next(x_arr, mid_sz), mid_sz,
                                                                                    x_next, acceptance_width,
                                                                                    x_first, x_last, discretization_sz,
                                                                                    coeff_arr, coeff_arr_offset, coeff_arr_cap);

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
    float total_deviation           = lhs_deviation / 4 + rhs_deviation / 4 + root_deviation;

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

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<Projection> training_pair_vec;
        float acceptance_width;
        float x_first;
        float x_last;
        size_t discretization_sz;

    public:

        PointPullMatrixEvaluator(std::vector<Projection> training_pair_vec,
                                 float acceptance_width,
                                 float x_first,
                                 float x_last,
                                 size_t discretization_sz): training_pair_vec(std::move(training_pair_vec)),
                                                            acceptance_width(acceptance_width),
                                                            x_first(x_first),
                                                            x_last(x_last),
                                                            discretization_sz(discretization_sz){}

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
                                                                        coeff_vec.data(), coeff_vec_offset, coeff_vec.size());

                rs          += e;
            }

            return rs;
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
            .optimization_epoch_sz                      = 512ULL,
            .optimization_step_sz                       = 8ULL,
            .optimization_loop_sz                       = 4ULL
        }
    );
}

void run_test()
{
    const size_t HEIGHT                 = 4;
    const float DISCRETIZATION_VALUE    = 0.2;
    const float ACCEPTANCE_WIDTH        = 0.01;
    const size_t SEMANTIC_SZ            = 2;
    const size_t EPOCH_SZ               = size_t{1} << 8;

    std::shared_ptr<NodeContainer> node_container   = make_node_container(make_tree(HEIGHT, DISCRETIZATION_VALUE, SEMANTIC_SZ));
    std::vector<Projection> projection_vec          = make_projection(node_container->root);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();
    std::vector<tensor_std_float_t> tensor_vec                                              = std::vector<tensor_std_float_t>(get_binary_unf_interpolated_projection_size(projection_vec.front().x.size(),
                                                                                                                                                                          0,
                                                                                                                                                                          DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                                                                          SEMANTIC_SZ),
                                                                                                                              0.f);

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "coefficient vector size > " << tensor_vec.size() << "\n";

    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = std::make_unique<SomeMatrix>(std::move(tensor_vec));
    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_vec,
                                                                                                                                         ACCEPTANCE_WIDTH,
                                                                                                                                         0,
                                                                                                                                         DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                                         SEMANTIC_SZ);
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