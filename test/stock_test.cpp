#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <functional>
#include <utility>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <exception>
#include <unordered_set>
#include <stl_extension/stdx.h>
#include <filesystem>
#include <fstream>
#include "json.hpp"
#include "json_fwd.hpp"

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

using json = nlohmann::json;

auto suffix_lossless_compress(const std::vector<double>& vec) -> std::pair<std::vector<size_t>, std::vector<double>>
{
    stdx::safe_float_range_access(vec.data(), vec.size());

    std::vector<std::pair<size_t, double>> enumerated_vec   = stdx::enumerate_vector(vec);
    std::vector<double> successor_vec                       = {};
    std::vector<size_t> suffix_vec                          = {};
    auto less_cmp                                           = [](const auto& lhs, const auto& rhs)
    {
        return lhs.second < rhs.second;
    };

    std::stable_sort(enumerated_vec.begin(), enumerated_vec.end(), less_cmp);

    for (size_t i = 0u; i < enumerated_vec.size(); ++i)
    {
        if (i == 0u)
        {
            successor_vec.push_back(enumerated_vec[i].second);
        }
        else
        {
            double ratio = enumerated_vec[i].second / enumerated_vec[i - 1].second;

            if (std::isnan(ratio))
            {
                throw std::runtime_error("bad ratio, NaN");
            }

            successor_vec.push_back(ratio);
        }

        suffix_vec.push_back(enumerated_vec[i].first);
    }

    return std::make_pair(std::move(suffix_vec), std::move(successor_vec));
}

auto factorial(size_t x) -> uint64_t
{
    static const std::vector<uint64_t> FACTORIAL_TABLE    = []
    {
        std::vector<uint64_t> rs    = {};
        uint64_t total              = 1;
        const size_t ITERABLE_SZ    = 19u;

        for (size_t i = 0u; i < ITERABLE_SZ; ++i)
        {
            rs.push_back(total);
            total *= (i + 1);
        }

        return rs;
    }();

    if (x >= FACTORIAL_TABLE.size())
    {
        throw std::invalid_argument("bad factorial value, [0, 19) required");
    }

    return FACTORIAL_TABLE[x];
}

auto suffix_array_enumeration_size(size_t suffix_arr_sz)  -> size_t
{
    return factorial(suffix_arr_sz);
}

auto is_suffix_array(const std::vector<size_t>& suffix_arr) -> bool
{
    if (suffix_arr.size() == 0u)
    {
        return true;
    }

    size_t max_val          = *std::max_element(suffix_arr.begin(), suffix_arr.end());
    size_t suffix_arr_sz    = std::unordered_set<size_t>(suffix_arr.begin(), suffix_arr.end()).size();

    return max_val + 1 == suffix_arr_sz;
}

template <class T>
auto image_index(const std::vector<T>& permuted_image,
                 const std::vector<T>& image) -> size_t
{
    if (permuted_image.size() != image.size())
    {
        throw std::invalid_argument("bad image, mismatched size");
    }

    if (permuted_image.size() == 0u)
    {
        throw std::invalid_argument("bad image, size 0");
    }

    if (permuted_image.size() == 1u)
    {
        if (permuted_image[0] != image[0])
        {
            throw std::invalid_argument("bad image, not a permutation");
        }

        return 0u;
    }

    size_t idx      = std::distance(image.begin(), std::find(image.begin(), image.end(), permuted_image.front()));

    if (idx == image.size())
    {
        throw std::invalid_argument("bad image, not a permutation");
    }

    size_t nxt_sz                       = image.size() - 1;
    std::vector<T> nxt_permuted_image   = std::vector<T>(std::next(permuted_image.begin()), permuted_image.end());
    std::vector<T> nxt_image            = image;

    nxt_image.erase(std::next(nxt_image.begin(), idx));

    size_t rs       = idx * factorial(nxt_sz) + image_index(nxt_permuted_image, nxt_image);

    return rs;
}

auto suffix_array_to_suffix_index(const std::vector<size_t>& suffix_arr) -> size_t
{
    std::vector<size_t> image(suffix_arr.size());
    std::iota(image.begin(), image.end(), 0u);

    return image_index(suffix_arr, image);
}

auto embed_price_vector(const std::vector<double>& vec) -> double
{
    auto [suffix, _]    = suffix_lossless_compress(vec);
    size_t suffix_sz    = suffix_array_enumeration_size(suffix.size());
    size_t suffix_idx   = suffix_array_to_suffix_index(suffix);

    if (suffix_sz == 1u)
    {
        return 1u;
    }

    return static_cast<double>(suffix_idx) / (suffix_sz - 1u);
}

struct Ticker
{
    std::string ticker_id;

    double v;
    double o;
    double c;
    double h;
    double l;

    uint64_t seconds_since_epoch;
};

auto list_files_in_directory(const std::filesystem::path& dir) -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> rs{};

    for (const auto& entry: std::filesystem::directory_iterator(dir))
    {
        rs.push_back(entry.path());
    }

    return rs;
}

auto read_file(const std::filesystem::path& file_path) -> std::string
{
    std::ifstream f_stream(file_path, std::ios::in | std::ios::binary);

    f_stream.seekg(0, std::ios::end);
    size_t fsz  = f_stream.tellg();
    std::string stream(fsz, ' ');
    f_stream.seekg(0, std::ios::beg);
    f_stream.read(stream.data(), fsz);

    return stream;
}
//date == "YYYY-mm-dd"
auto load_ticker_data(const std::string& fr_date,
                      const std::string& to_date) -> std::unordered_map<std::string, std::vector<Ticker>>
{
    const std::filesystem::path DIR = "/Users/megazone/Downloads/dg_ballinger-main/src/data/gg_daily_parsed";

    std::vector<std::filesystem::path> file_path_vec = list_files_in_directory(DIR);
    std::sort(file_path_vec.begin(), file_path_vec.end());

    std::unordered_map<std::string, std::vector<Ticker>> rs{};

    for (const auto& file_path: file_path_vec)
    {
        std::string file_name = std::filesystem::path(file_path).replace_extension("").filename();

        if (file_name < fr_date)
        {
            continue;
        }

        if (file_name > to_date)
        {
            continue;
        }

        std::string file_data   = read_file(file_path);
        json data               = json::parse(file_data);

        for (const auto& e: data)
        {
            std::string ticker_id   = e["T"].template get<std::string>();
            double v                = e["v"].template get<double>();
            double o                = e["o"].template get<double>();
            double c                = e["c"].template get<double>();
            double h                = e["h"].template get<double>();
            double l                = e["l"].template get<double>();
            uint64_t epoch_seconds  = e["t"].template get<uint64_t>();

            rs[ticker_id].push_back
            (
                Ticker
                {
                    .ticker_id              = ticker_id,
                    .v                      = v,
                    .o                      = o,
                    .c                      = c,
                    .h                      = h,
                    .l                      = l,
                    .seconds_since_epoch    = epoch_seconds
                }
            );
        }
    }

    return rs;
}

struct Projection
{
    std::vector<float> x;
    float y;
};

auto get_projection(const std::vector<Ticker>& historical_data,
                    size_t focal_step_sz,
                    size_t focal_initial_sz,
                    size_t focal_exponential_factor,
                    size_t focal_interval_sz,
                    size_t back_offset) -> Projection
{
    if (focal_interval_sz == 0u)
    {
        throw std::invalid_argument("bad focal interval size, 0");
    }

    std::vector<float> x    = {};

    for (size_t i = 0u; i < focal_step_sz; ++i)
    {
        size_t focal_range          = focal_initial_sz * std::pow(focal_exponential_factor, i);
        size_t required_back_sz     = focal_range + back_offset + 1;

        if (required_back_sz > historical_data.size())
        {
            throw std::invalid_argument("bad focal range, out of range access");
        }

        size_t first                = historical_data.size() - required_back_sz;
        size_t last                 = first + focal_range;

        if (focal_range % focal_interval_sz != 0u)
        {
            throw std::invalid_argument("bad focal range, not multiples of focal interval size");
        }

        size_t focal_chunk_sz       = focal_range / focal_interval_sz;
        std::vector<double> low_vec = {};

        for (size_t j = 0u; j < focal_interval_sz; ++j)
        {
            size_t low_first    = first + focal_chunk_sz * j;
            size_t low_last     = low_first + focal_chunk_sz;

            if (low_first == low_last)
            {
                throw std::invalid_argument("bad interval, first == last");
            }

            double low          = std::min_element(std::next(historical_data.begin(), low_first),
                                                   std::next(historical_data.begin(), low_last),
                                                   [](const auto& lhs, const auto& rhs)
                                                    {
                                                        return lhs.l < rhs.l;
                                                    })->l;

            low_vec.push_back(low);
        }

        x.push_back(embed_price_vector(low_vec));
    }

    float y = {};

    {
        size_t required_sz  = back_offset + 2u;

        if (required_sz > historical_data.size())
        {
            throw std::invalid_argument("bad historical data size, bad access");
        }

        size_t first        = historical_data.size() - required_sz;
        size_t second       = first + 1;
        y                   = historical_data[second].l > historical_data[first].l;
    }

    return Projection
    {
        .x  = x,
        .y  = y
    };
}

//there is absolutely no compression if our training data already infers its past
//so we'd have to increase the randomness of the data, like training for next seconds
//if this works
//I could really say that it was lerp

auto extract_training_data() -> std::vector<Projection>
{
    const std::string FR_DATE                       = "2013-01-01";
    const std::string TO_DATE                       = "2023-01-01";

    const size_t FOCAL_STEP_SZ                      = 4;
    const size_t FOCAL_INITIAL_DAY_SZ               = 4;
    const size_t FOCAL_EXPONENTIAL_FACTOR           = 2;
    const size_t FOCAL_INTERVAL_SZ                  = 4;
    const size_t ITERABLE_DAY_SZ                    = 300;

    const std::vector<std::string> TICKER_VEC       = 
    {
        "MSFT",
        "AAPL",
        "AMZN",
        "LLY",
        "TSLA"
    };

    const std::vector<std::string> OTHER_TICKER_VEC =
    {
        "SPY",
        "QQQ",
        "VTI"
    };

    std::unordered_map<std::string, std::vector<Ticker>> ticker_data    = load_ticker_data(FR_DATE, TO_DATE); 
    std::vector<Projection> rs                                          = {};

    for (size_t i = 0u; i < ITERABLE_DAY_SZ; ++i)
    {
        std::vector<float> environment_data = {};

        for (const std::string& ticker: OTHER_TICKER_VEC)
        {
            Projection prediction   = get_projection
            (
                ticker_data.at(ticker),
                FOCAL_STEP_SZ,
                FOCAL_INITIAL_DAY_SZ,
                FOCAL_EXPONENTIAL_FACTOR,
                FOCAL_INTERVAL_SZ,
                i
            );

            environment_data.insert(environment_data.end(), prediction.x.begin(), prediction.x.end());
        }

        for (const std::string& ticker: TICKER_VEC)
        {
            Projection prediction   = get_projection
            (
                ticker_data.at(ticker),
                FOCAL_STEP_SZ,
                FOCAL_INITIAL_DAY_SZ,
                FOCAL_EXPONENTIAL_FACTOR,
                FOCAL_INTERVAL_SZ,
                i
            );

            prediction.x.insert(prediction.x.end(), environment_data.begin(), environment_data.end());

            rs.push_back(prediction);
        }
    }

    return rs;
}

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

    //right, this should be lhs =, rhs = but we'd cut some slack here, it's semantically different

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
    // const size_t saved_coeff_arr_offset = coeff_arr_offset;

    float lhs                           = binary_unf_interpolated_project(x_arr, mid_sz,
                                                                          x_first, x_last, discretization_sz,
                                                                          coeff_arr, coeff_arr_offset, coeff_arr_cap);

    // coeff_arr_offset                    = saved_coeff_arr_offset;
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
            .matrix_cache_map_cap                       = size_t{1} << 4,
            .time_machine_cache_map_cap                 = size_t{1} << 4,
            .optimization_epoch_sz                      = 128ULL,
            .optimization_step_sz                       = 32ULL,
            .optimization_loop_sz                       = 8ULL
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
    const float DISCRETIZATION_VALUE    = 0.08;
    const float ACCEPTANCE_WIDTH        = 0.04;
    const size_t SEMANTIC_SZ            = 16;
    const size_t EPOCH_SZ               = size_t{1} << 8;

    const double INITIAL_ROOT_WEIGHT    = 0.01;
    const double MAX_ROOT_WEIGHT        = 0.98;

    std::vector<Projection> projection_vec          = extract_training_data();

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
    std::unique_ptr<SomeProjector> projector                                                = std::make_unique<SomeProjector>(0,
                                                                                                                              DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                              SEMANTIC_SZ);

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
        current_root_weight         *= 2;
        current_root_weight         = std::min(current_root_weight, MAX_ROOT_WEIGHT);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
    }
}

int main()
{
    initialize_concurrency_base();
    run_test();
}