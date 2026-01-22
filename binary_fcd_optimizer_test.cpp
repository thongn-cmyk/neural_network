#define STRONG_MEMORY_ORDERING_FLAG true

#include "graph_optimizer.h"
#include <chrono>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <functional>
#include <utility>
#include <algorithm>
#include <random>
#include <unordered_set>
// #include "stl_extension.h"
#include <unordered_map>

auto to_edge_vec(const std::unordered_map<size_t, std::unordered_map<size_t, double>>& edge_vec) -> std::vector<graph_optimizer::BinaryFCDEdgeInformation>
{
    std::vector<graph_optimizer::BinaryFCDEdgeInformation> rs{};

    for (const auto& [src, neighbors]: edge_vec)
    {
        for (const auto& [dst, score]: neighbors)
        {
            rs.push_back(graph_optimizer::BinaryFCDEdgeInformation
            {
                .src    = src,
                .dst    = dst,
                .score  = score
            });
        }
    }

    return rs;
}

auto randomize_graph_information() -> std::unordered_map<size_t, std::unordered_map<size_t, double>>
{
    const size_t EDGE_SZ_RANGE      = size_t{1} << 16;
    const size_t VERTEX_IDX_RANGE   = size_t{1} << 14;
    const double EDGE_FIRST_VALUE   = 0;
    const double EDGE_LAST_VALUE    = 1024;
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    static auto real_randomizer     = std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto real_dis            = std::uniform_real_distribution<double>(EDGE_FIRST_VALUE, EDGE_LAST_VALUE);

    std::unordered_map<size_t, std::unordered_map<size_t, double>> graph{};

    size_t edge_sz  = randomizer() % EDGE_SZ_RANGE;

    for (size_t i = 0u; i < edge_sz; ++i)
    {
        size_t src      = randomizer() % VERTEX_IDX_RANGE;
        size_t dst      = randomizer() % VERTEX_IDX_RANGE;
        double score    = real_dis(real_randomizer);

        graph[src][dst] = score;
        graph[dst][src] = score;
    }

    return graph;
}

auto calculate_random_relevant_score(const std::unordered_map<size_t, std::unordered_map<size_t, double>>& score_map,
                                     const std::vector<size_t>& community_vertices) -> double
{
    static size_t SAMPLE_SZ_RANGE           = 64u;
    static size_t SAMPLE_COMMUNITY_RANGE    = 32u;
    static auto randomizer                  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    size_t sample_community_sz              = std::max(size_t{1}, static_cast<size_t>(randomizer() % SAMPLE_SZ_RANGE));

    if (community_vertices.empty())
    {
        return 0;
    }

    size_t found_edge_count                 = 0u;
    double edge_score                       = 0;

    for (size_t i = 0u; i < sample_community_sz; ++i)
    {
        size_t random_offset        = randomizer() % community_vertices.size();
        size_t rem_sz               = community_vertices.size() - random_offset;
        size_t tentative_comm_sz    = std::max(size_t{1}, static_cast<size_t>(randomizer() % SAMPLE_COMMUNITY_RANGE));
        size_t actual_comm_sz       = std::min(tentative_comm_sz, rem_sz);
        size_t first                = random_offset;
        size_t last                 = first + actual_comm_sz;

        for (size_t j = first; j < last; ++j)
        {
            for (size_t k = j + 1; k < last; ++k)
            {
                size_t src_vtx  = community_vertices[j];
                size_t dst_vtx  = community_vertices[k];

                if (auto map_ptr = score_map.find(src_vtx); map_ptr != score_map.end())
                {
                    if (auto map_ptr2 = map_ptr->second.find(dst_vtx); map_ptr2 != map_ptr->second.end())
                    {
                        found_edge_count += 1;
                        edge_score  += map_ptr2->second;
                    }
                }
            }
        }
    }

    if (found_edge_count == 0u)
    {
        return 0;
    }

    return edge_score / found_edge_count;
}

auto to_vertex_set(const std::unordered_map<size_t, std::unordered_map<size_t, double>>& graph) -> std::vector<size_t>
{
    std::unordered_set<size_t> vtx_set{};

    for (const auto& [src, neighbor]: graph)
    {
        vtx_set.insert(src);

        for (const auto& [dst, score]: neighbor)
        {
            vtx_set.insert(dst);
        }
    }

    return {vtx_set.begin(), vtx_set.end()};
}

auto shuffle_vertex_set(const std::vector<size_t>& vec) -> std::vector<size_t>
{
    static auto randomizer = std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    auto tmp = vec;
    std::shuffle(tmp.begin(), tmp.end(), randomizer);

    return tmp;
}

auto is_same_vertex_set(const std::vector<size_t>& lhs, const std::vector<size_t>& rhs) -> bool
{
    return std::unordered_set<size_t>(lhs.begin(), lhs.end()) == std::unordered_set<size_t>(rhs.begin(), rhs.end());
}

auto is_set(const std::vector<size_t>& vec) -> bool
{
    return std::unordered_set<size_t>(vec.begin(), vec.end()).size() == vec.size();
}

auto run_one_test() -> double
{
    std::unordered_map<size_t, std::unordered_map<size_t, double>> graph = randomize_graph_information();

    std::vector<size_t> optimized_vec   = graph_optimizer::BinaryFCDOptimizer{}.optimize(to_edge_vec(graph)).vertices;
    std::vector<size_t> normal_vec      = to_vertex_set(graph);

    if (optimized_vec.size() != normal_vec.size())
    {
        std::cout << "mayday size " << optimized_vec.size() << "<>" << normal_vec.size() << std::endl; 
        std::abort();
    }

    if (!is_same_vertex_set(optimized_vec, normal_vec))
    {
        std::cout << "mayday set" << std::endl;
        std::abort();
    }

    double relevant_score_0 = calculate_random_relevant_score(graph, shuffle_vertex_set(normal_vec));
    double relevant_score_1 = calculate_random_relevant_score(graph, optimized_vec);

    return relevant_score_0 / relevant_score_1;
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 10;

    double total_score      = 0;
    size_t total_sz         = 0u;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        // std::cout << "test > " << i  << "/" << TEST_SZ << std::endl;
        // std::cout << "-------" << std::endl;
        double score = run_one_test();

        if (!std::isnan(score) && !std::isinf(score))
        {
            total_score += score;
            total_sz += 1;
        }

        if (i % 128 == 0u)
        {
            std::cout << "test > " << i  << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "score > " << (total_score / total_sz) << std::endl; 
}

int main()
{
    run_test();
}