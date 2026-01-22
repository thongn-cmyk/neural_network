#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include <iomanip>
#include "cosine_recommender_machine_x.h"

void test_one_branch_predictor()
{   
    static auto seed_randomizer             = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto randomizer                  = std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    auto real_distributor                   = std::uniform_real_distribution<double>(0, 1);

    const size_t ENUMERATION_SZ_RANGE       = size_t{1} << 10;
    const size_t REEVALUATION_WINDOW_RANGE  = size_t{1} << 10;
    const size_t FACTORY_ID_RANGE           = 4u;
    const size_t TEST_SZ_RANGE              = size_t{1} << 10;
    const size_t WRONG_NUMERIC_CHANCE       = size_t{1} << 6;

    size_t enumeration_sz                   = randomizer() % ENUMERATION_SZ_RANGE + 1u;
    size_t reeval_window                    = randomizer() % REEVALUATION_WINDOW_RANGE;
    size_t factory_id                       = randomizer() % FACTORY_ID_RANGE;
    size_t test_sz                          = randomizer() % TEST_SZ_RANGE;

    std::unique_ptr<branch_optimizer::BranchPredictorInterface> predictor = branch_optimizer::BranchPredictorFactory::get_branch_predictor(factory_id, enumeration_sz, reeval_window);

    for (size_t i = 0u; i < test_sz; ++i)
    {
        auto branch_result = predictor->next();

        if (branch_result->get_enumeration() >= enumeration_sz)
        {
            std::cout << "mayday, branch result enumeration out of range" << std::endl;
            std::abort();
        }

        double tentative_result = real_distributor(seed_randomizer);

        if (randomizer() % WRONG_NUMERIC_CHANCE == 0u)
        {
            if (randomizer() % 2 == 0u)
            {
                tentative_result = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                tentative_result = std::numeric_limits<double>::infinity();
            }
        }

        branch_result->feedback(tentative_result);
    }
}

void test_branch_predictor()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_BRANCH_PREDICTOR_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_branch_predictor();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_BRANCH_PREDICTOR_TEST__" << std::endl;
}

struct EnumerationTree
{
    size_t enumeration_sz;
    std::vector<std::unique_ptr<EnumerationTree>> child_vec;
};

auto get_enumeration_tree(size_t enumeration_range,
                          size_t tree_height) -> std::unique_ptr<EnumerationTree>
{
    if (tree_height == 0u)
    {
        return nullptr;
    }

    if (enumeration_range == 0u)
    {
        throw std::invalid_argument("bad enumeration range, 0");
    }

    static auto randomizer  = std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t enumeration_sz   = randomizer() % enumeration_range;

    if (enumeration_sz <= 1u)
    {
        return nullptr;
    }

    std::unique_ptr<EnumerationTree> result = std::make_unique<EnumerationTree>(EnumerationTree
    {
        .enumeration_sz = enumeration_sz,
        .child_vec      = std::vector<std::unique_ptr<EnumerationTree>>(enumeration_sz)
    });

    for (size_t i = 0u; i < enumeration_sz; ++i)
    {
        result->child_vec[i] = get_enumeration_tree(enumeration_range, tree_height - 1u);
    }

    return result;
}

void get_prefix_helper(const std::unique_ptr<EnumerationTree>& root,
                       std::vector<size_t>& result)
{
    if (root == nullptr)
    {
        result.push_back(0u);
        return;
    }

    result.push_back(root->enumeration_sz);

    for (const auto& child: root->child_vec)
    {
        get_prefix_helper(child, result);
    }
}

auto get_prefix(const std::unique_ptr<EnumerationTree>& root) -> std::vector<size_t>
{
    std::vector<size_t> result{};
    get_prefix_helper(root, result);

    return result;
}

void enumerate(const std::unique_ptr<EnumerationTree>& root,
               const std::vector<size_t>& enumeration_vec)
{
    if (root == nullptr)
    {
        if (enumeration_vec.empty())
        {
            return;
        }

        throw std::runtime_error("bad enumeration");
    }

    if (root->enumeration_sz == 0u)
    {
        std::cout << "mayday, bad tree state" << std::endl;
        std::abort();
    }

    if (root->enumeration_sz != root->child_vec.size())
    {
        std::cout << "mayday, bad tree state" << std::endl;
        std::abort();
    }

    if (enumeration_vec.empty())
    {
        throw std::runtime_error("bad enumeration");
    }

    size_t enumeration_idx = enumeration_vec.front();

    if (enumeration_idx >= root->enumeration_sz)
    {
        throw std::runtime_error("bad enumeration");
    }

    enumerate(root->child_vec[enumeration_idx], {std::next(enumeration_vec.begin()), enumeration_vec.end()});
}

void test_one_hierarchical_branch_predictor()
{
    const size_t TREE_HEIGHT_RANGE          = size_t{1} << 2;
    const size_t ENUMERATION_RANGE_RANGE    = size_t{1} << 2;
    const size_t REEVALUATION_WINDOW_RANGE  = size_t{1} << 10;
    const size_t TEST_SZ_RANGE              = size_t{1} << 10;
    const size_t WRONG_NUMERIC_CHANCE       = size_t{1} << 6;

    static auto seed_randomizer             = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto randomizer                  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    auto real_distributor                   = std::uniform_real_distribution<double>(0, 1);

    size_t tree_height                      = randomizer() % TREE_HEIGHT_RANGE;
    size_t enumeration_range                = randomizer() % ENUMERATION_RANGE_RANGE + 1;
    size_t reeval_window                    = randomizer() % REEVALUATION_WINDOW_RANGE;
    size_t test_sz                          = randomizer() % TEST_SZ_RANGE;

    std::unique_ptr<EnumerationTree> root   = get_enumeration_tree(enumeration_range, tree_height);
    std::vector<size_t> prefix_vec          = get_prefix(root);

    std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor;
    size_t dispatch_code = randomizer() % 5;

    switch (dispatch_code)
    {
        case 0:
        {
            branch_predictor = branch_optimizer::HierarchicalBranchPredictorFactory::get_traditional_branch_predictor_from_preorder_tree(prefix_vec, reeval_window);
            break;
        }
        case 1:
        {
            branch_predictor = branch_optimizer::HierarchicalBranchPredictorFactory::get_aggressive_branch_predictor_from_preorder_tree(prefix_vec, reeval_window);
            break;
        }
        case 2:
        {
            branch_predictor = branch_optimizer::HierarchicalBranchPredictorFactory::get_traditional_adaptive_branch_predictor_from_preorder_tree(prefix_vec, reeval_window);
            break;
        }
        case 3:
        {
            branch_predictor = branch_optimizer::HierarchicalBranchPredictorFactory::get_aggressive_adaptive_branch_predictor_from_preorder_tree(prefix_vec, reeval_window);
            break;
        }
        case 4:
        {
            branch_predictor = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(prefix_vec);
            break;
        }
        default:
        {
            std::unreachable();
        }
    }

    for (size_t i = 0u; i < test_sz; ++i)
    {
        std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> result = branch_predictor->next();
        std::vector<size_t> enum_vec = result->get_enumeration();

        enumerate(root, enum_vec);

        double tentative_result = real_distributor(seed_randomizer);

        if (randomizer() % WRONG_NUMERIC_CHANCE == 0u)
        {
            if (randomizer() % 2 == 0u)
            {
                tentative_result = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                tentative_result = std::numeric_limits<double>::infinity();
            }
        }

        result->feedback(tentative_result);
    }
}

void test_hierarchical_branch_predictor()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_HIERARCHICAL_BRANCH_PREDICTOR_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_hierarchical_branch_predictor();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_HIERARCHICAL_BRANCH_PREDICTOR_TEST__" << std::endl;
}

void run_test()
{
    test_hierarchical_branch_predictor();
    test_branch_predictor();
}

int main()
{
    run_test();
}