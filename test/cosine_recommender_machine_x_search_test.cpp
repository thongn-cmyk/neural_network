#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <matrix_steering_subsystem/cosine_recommender_machine_x.h>
#include <random>
#include <functional>
#include <stl_extension/stdx.h>
#include <deque>

using namespace cosine_recommender_machine_x;

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

auto get_random_float_vector_of_size(size_t sz)  -> std::vector<crm_x_float_t>
{
    std::vector<crm_x_float_t> rs{};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(randomize_double(0, 1));
    }

    return rs;
}

class AverageWindowScoreCalculator
{
    private:

        std::deque<double> score_vec;
        size_t window_sz;
    
    public:

        AverageWindowScoreCalculator(size_t window_sz): score_vec(),
                                                        window_sz(std::max(size_t{1}, window_sz)){}

        void push(double score)
        {
            if (std::isnan(score))
            {
                throw std::invalid_argument("bad score, NaN");
            }

            if (this->score_vec.size() == this->window_sz)
            {
                this->score_vec.pop_front();
            }

            this->score_vec.push_back(score);
        }

        auto get_score() -> std::optional<double>
        {
            if (this->score_vec.empty())
            {
                return std::nullopt;
            }

            double rs = this->score_vec.front();

            for (double score: this->score_vec)
            {
                rs = std::min(rs, score);
            }

            return rs;
        }
};

template <class FloatType>
auto euclidean_distance(const std::vector<FloatType>& coor,
                        const std::vector<FloatType>& coor_1) -> FloatType
{
    if (coor.size() != coor_1.size())
    {
        throw std::invalid_argument("bad coor, mismatched size");
    }

    if (coor.size() == 0u)
    {
        throw std::invalid_argument("bad coor, out of dimension");
    }

    FloatType rs = 0;

    for (size_t i = 0u; i < coor.size(); ++i)
    {
        rs += std::pow(coor[i] - coor_1[i], 2);
    }

    return std::sqrt(rs);
}

auto get_random_unit_float_vector_of_size(size_t sz) -> std::vector<crm_x_float_t>
{
    std::vector<crm_x_float_t> coor = get_random_float_vector_of_size(sz);
    crm_x_float_t length            = euclidean_distance(std::vector<crm_x_float_t>(sz, 0.0), coor);

    for (auto& e: coor)
    {
        e /= length;
    }

    return coor;
}

void run_one_test()
{
    const size_t SPACE_SZ           = size_t{1} << 4;
    const size_t TEST_SZ            = size_t{1} << 20;
    const size_t COUT_SZ            = size_t{1} << 14;
    const size_t SCORE_WINDOW_SZ    = 1024u;

    std::unique_ptr<CosineRecommenderMachineInterface> machine  = MachineFactory::get_statistical_recommender_machine(SPACE_SZ);
    std::vector<crm_x_float_t> finding_coor                     = get_random_unit_float_vector_of_size(SPACE_SZ);

    AverageWindowScoreCalculator score_calculator(32);

    std::cout << "__BEGIN_SEARCH__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        std::unique_ptr<CosineRecommendationResultInterface> result = machine->next();

        std::vector<crm_x_float_t> coor = result->get();
        crm_x_float_t distance          = euclidean_distance(coor, finding_coor);

        result->feedback(1.0 / distance);
        score_calculator.push(distance);

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
            
            if (score_calculator.get_score().has_value())
            {
                std::cout << score_calculator.get_score().value() << "<score>\n";
            }
        }
    }

    std::cout << "__END_SEARCH__\n";

    // const size_t SPACE_SZ_RANGE         = size_t{1} << 4;
    // const size_t TEST_SZ_RANGE          = size_t{1} << 8;
    // const size_t WRONG_NUMERIC_CHANCE   = size_t{1} << 4;

    // static auto seed_randomizer         = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    // static auto uint_distributor        = std::uniform_int_distribution<size_t>();
    // static auto real_distributor        = std::uniform_real_distribution<double>(0, 1);

    // size_t space_sz                     = uint_distributor(seed_randomizer) % SPACE_SZ_RANGE;
    // size_t test_sz                      = uint_distributor(seed_randomizer) % TEST_SZ_RANGE;

    // std::unique_ptr<CosineRecommenderMachineInterface> machine = MachineFactory::get_trad_stat_recommender_machine(space_sz);

    // for (size_t i = 0u; i < test_sz; ++i)
    // {
    //     std::unique_ptr<CosineRecommendationResultInterface> result = machine->next();

    //     if (result == nullptr)
    //     {
    //         std::cout << "mayday, result is null" << std::endl;
    //         std::abort();
    //     }

    //     std::vector<crm_x_float_t> vec = result->get();
    //     stdx::xsafe_float_range_access(vec.data(), vec.size());

    //     double tentative_result = real_distributor(seed_randomizer);

    //     if (uint_distributor(seed_randomizer) % WRONG_NUMERIC_CHANCE == 0u)
    //     {
    //         if (uint_distributor(seed_randomizer) % 2 == 0u)
    //         {
    //             tentative_result = std::numeric_limits<double>::quiet_NaN();
    //         }
    //         else
    //         {
    //             tentative_result = std::numeric_limits<double>::infinity();
    //         }
    //     }

    //     result->feedback(tentative_result);
    // }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 1;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_COSINE_RECOMMENDER_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_COSINE_RECOMMENDER_TEST__" << std::endl;
}

int main()
{
    run_test();    
}