#define STRONG_MEMORY_ORDERING_FLAG true

#include <matrix_steering_subsystem/graph_optimizer.h>
#include <stdint.h>
#include <stdlib.h>
#include <random>
#include <chrono>
#include <utility>
#include <algorithm>
#include <functional>
#include <stl_extension/stl_extension.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cmath>
#include <bit>
#include <numbers>

auto randomize_non_negative_graph() -> std::unordered_map<size_t, std::unordered_map<size_t, double>>
{
    const double FIRST_EDGE_VALUE   = 0u;
    const double LAST_EDGE_VALUE    = size_t{1} << 4;

    const size_t GRAPH_SZ_COUNT     = size_t{1} << 4;
    const size_t VERTEX_ID_RANGE    = size_t{1} << 4;
    const size_t ZERO_CHANCE        = size_t{1} << 4;

    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>();
    static auto real_distributor    = std::uniform_real_distribution<double>(FIRST_EDGE_VALUE, LAST_EDGE_VALUE);

    size_t graph_sz                 = uint_distributor(randomizer) % GRAPH_SZ_COUNT;
    auto result                     = std::unordered_map<size_t, std::unordered_map<size_t, double>>{};

    for (size_t i = 0u; i < graph_sz; ++i)
    {
        size_t src          = uint_distributor(randomizer) % VERTEX_ID_RANGE;
        size_t dst          = uint_distributor(randomizer) % VERTEX_ID_RANGE;
        double weight       = real_distributor(randomizer) * ((uint_distributor(randomizer) % ZERO_CHANCE == 0u) ? 0: 1);

        result[src][dst]    = weight;
        result[dst][src]    = weight;
    }

    return result;
}

auto randomize_positive_graph() -> std::unordered_map<size_t, std::unordered_map<size_t, double>>
{
    const double FIRST_EDGE_VALUE   = 0.1;
    const double LAST_EDGE_VALUE    = size_t{1} << 4;

    const size_t GRAPH_SZ_COUNT     = size_t{1} << 4;
    const size_t VERTEX_ID_RANGE    = size_t{1} << 4;

    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>();
    static auto real_distributor    = std::uniform_real_distribution<double>(FIRST_EDGE_VALUE, LAST_EDGE_VALUE);

    size_t graph_sz                 = uint_distributor(randomizer) % GRAPH_SZ_COUNT;
    auto result                     = std::unordered_map<size_t, std::unordered_map<size_t, double>>{};

    for (size_t i = 0u; i < graph_sz; ++i)
    {
        size_t src          = uint_distributor(randomizer) % VERTEX_ID_RANGE;
        size_t dst          = uint_distributor(randomizer) % VERTEX_ID_RANGE;
        double weight       = real_distributor(randomizer);

        result[src][dst]    = weight;
        result[dst][src]    = weight;
    }

    return result;
}

template <class T>
using default_hasher        = stl_extension::default_hasher<T>;

template <class T>
using default_equal_to      = stl_extension::default_equal_to<T>;

template <class Key, class Value>
using unordered_map_x       = std::unordered_map<Key, Value, default_hasher<Key>, default_equal_to<Key>>;

template <class Key>
using unordered_set_x       = std::unordered_set<Key, default_hasher<Key>, default_equal_to<Key>>;

void test_one_dijkstra()
{
    using namespace graph_optimizer;

    //

    std::unordered_map<size_t, std::unordered_map<size_t, double>> graph = randomize_non_negative_graph();

    std::vector<DijkstraEdgeInformation> dijkstra_vec   = {};
    std::vector<FloyedEdgeInformation> floyed_vec       = {};

    for (const auto& [src, neighbor_map]: graph)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            if (src == dst)
            {
                continue;
            }

            dijkstra_vec.push_back(DijkstraEdgeInformation
            {
                .src                = src,
                .dst                = dst,
                .score              = score,
                .is_bidirectional   = true
            });

            floyed_vec.push_back(FloyedEdgeInformation
            {
                .src                = src,
                .dst                = dst,
                .score              = score,
                .is_bidirectional   = true
            });
        }
    }

    std::unordered_map<size_t, std::unordered_map<size_t, double>> path_info    = {};
    std::vector<std::tuple<size_t, size_t, double>> all_pair_result             = FloyedOptimizer{}.optimize(floyed_vec).distance_vec;
    const double EPSILON = 0.0001;

    for (const auto& [src, dst, score]: all_pair_result)
    {
        path_info[src][dst] = score;
    }

    for (const auto& [src, neighbor_map]: graph)
    {
        DijkstraOptimizer optimizer(dijkstra_vec);

        auto sssp_result    = optimizer.optimize(src).distance_vec;

        for (const auto& [dst, score]: sssp_result)
        {
            if (src == dst)
            {
                continue;
            }

            auto map_ptr = path_info.find(src);

            if (map_ptr == path_info.end())
            {
                std::cout << "mayday, path mismatch" << "<>" << src << "<>" << dst << std::endl;
                std::abort();
                continue;
            }

            auto other_map_ptr = map_ptr->second.find(dst);

            if (other_map_ptr == map_ptr->second.end())
            {
                std::cout << "mayday, path mismatch" << "<>" << src << "<>" << dst << std::endl;
                std::abort();
                continue;
            }

            if (std::isnan(score))
            {
                std::cout << "mayday, bad numeric" << std::endl;
                std::abort();
            }

            if (std::isnan(other_map_ptr->second))
            {
                std::cout << "mayday, bad numeric" << std::endl;
                std::abort();
            }

            if (std::abs(score - other_map_ptr->second) > EPSILON)
            {
                std::cout << "mayday, value mismatch" << "<> " << score << "<>" << other_map_ptr->second << "<>" << graph[src][dst] << std::endl;
                std::abort();
            }
        }
    }
}

void test_dijkstra()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 6;

    std::cout << "__BEGIN_DIJKSTRA_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_dijkstra();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "_END_DIJKSTRA_TEST__" << std::endl;
}

struct Point
{
    size_t x;
    size_t y;

    template <class Reflector>
    constexpr void dg_reflect(const Reflector& reflector) const
    {
        reflector(x, y);
    }

    template <class Reflector>
    constexpr void dg_reflect(const Reflector& reflector)
    {
        reflector(x, y);
    }
};

auto distance(const Point& a, const Point& b) -> double
{
    Point displacement = Point
    {
        .x  = std::max(a.x, b.x) - std::min(a.x, b.x),
        .y  = std::max(a.y, b.y) - std::min(a.y, b.y) 
    };

    double distance = std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y);

    return distance;
}

auto randomize_point_graph() -> unordered_map_x<Point, unordered_map_x<Point, double>>
{
    const size_t DOMAIN_RANGE       = size_t{1} << 4;
    const size_t POINT_SZ_RANGE     = size_t{1} << 4;
    const double SCALE_VALUE_FIRST  = 2.0;
    const double SCALE_VALUE_LAST   = 10.0;

    static auto randomizer          = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    static auto uint_distributor    = std::uniform_int_distribution<size_t>();
    static auto real_scaler         = std::uniform_real_distribution<double>(SCALE_VALUE_FIRST, SCALE_VALUE_LAST);

    size_t point_sz                 = uint_distributor(randomizer) % POINT_SZ_RANGE + 1u;
    auto result                     = unordered_map_x<Point, unordered_map_x<Point, double>>{};

    for (size_t i = 0u; i < point_sz; ++i)
    {
        size_t x_0  = uint_distributor(randomizer) % DOMAIN_RANGE;
        size_t y_0  = uint_distributor(randomizer) % DOMAIN_RANGE;
        size_t x_1  = uint_distributor(randomizer) % DOMAIN_RANGE;
        size_t y_1  = uint_distributor(randomizer) % DOMAIN_RANGE;

        Point p1    = Point
        {
            .x  = x_0,
            .y  = y_0
        };

        Point p2    = Point
        {
            .x  = x_1,
            .y  = y_1
        };

        if (std::tie(x_0, y_0) == std::tie(x_1, y_1))
        {
            continue;
        }

        double d        = distance(p1, p2);
        double d_1      = std::ceil(d) * static_cast<size_t>(real_scaler(randomizer));
        result[p1][p2]  = d_1;
    }

    return result;
}

auto enumerate_point_graph(const unordered_map_x<Point, unordered_map_x<Point, double>>& graph) -> std::vector<Point>
{
    unordered_set_x<Point> point_set{};

    for (const auto& [src, neighbor_map]: graph)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            point_set.insert(src);
            point_set.insert(dst);
        }
    }

    return {point_set.begin(), point_set.end()};
}

auto get_point_dictionary(const std::vector<Point>& point_vec) -> unordered_map_x<Point, size_t>
{
    unordered_map_x<Point, size_t> point_map{};

    for (const auto& point: point_vec)
    {
        if (point_map.contains(point))
        {
            throw std::invalid_argument("point vec is not a set");
        }

        point_map[point] = point_map.size();
    }

    return point_map;
}

void test_one_a_star_admissible()
{
    using namespace graph_optimizer;

    unordered_map_x<Point, unordered_map_x<Point, double>> point_graph = randomize_point_graph();

    std::vector<Point> point_set                    = enumerate_point_graph(point_graph);
    unordered_map_x<Point, size_t> point_dict       = get_point_dictionary(point_set);

    std::vector<AStarEdgeInformation> edge_vec      = {};
    std::vector<FloyedEdgeInformation> floyed_vec   = {};

    for (const auto& [src, neighbor_map]: point_graph)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            size_t src_id   = point_dict.at(src);
            size_t dst_id   = point_dict.at(dst);

            edge_vec.push_back(AStarEdgeInformation
            {
                .src                = src_id,
                .dst                = dst_id,
                .score              = score,
                .is_bidirectional   = false
            });

            floyed_vec.push_back(FloyedEdgeInformation
            {
                .src                = src_id,
                .dst                = dst_id,
                .score              = score,
                .is_bidirectional   = false
            });
        }
    }
    
    auto heuristic = [&](size_t lhs, size_t rhs) -> double
    {
        if (lhs >= point_set.size())
        {
            throw std::runtime_error("bad access, out of bound access");
        }

        if (rhs >= point_set.size())
        {
            throw std::runtime_error("bad access, out of bound access");
        }

        const Point& lhs_point  = point_set[lhs];
        const Point& rhs_point  = point_set[rhs];

        return distance(lhs_point, rhs_point);
    };


    std::unordered_map<size_t, std::unordered_map<size_t, double>> path_info    = {};
    std::vector<std::tuple<size_t, size_t, double>> all_pair_result             = FloyedOptimizer{}.optimize(floyed_vec).distance_vec;
    const double EPSILON = 0.0001;

    for (const auto& [src, dst, score]: all_pair_result)
    {
        path_info[src][dst] = score;
    }

    AdmissibleAStarOptimizer a_star_optimizer(edge_vec, heuristic);

    for (const auto& [src, neighbor_map]: path_info)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            double best_path_score  = a_star_optimizer.optimize(src, dst,
                                                                 std::numeric_limits<size_t>::max(),
                                                                 false).best_path_score;

            if (std::isnan(score))
            {
                std::cout << "mayday score, NaN" << std::endl;
                std::abort();
            }

            if (std::abs(best_path_score - score) > EPSILON)
            {
                std::cout << "mayday score, value mismatched" << "<>" << best_path_score << "<>" << score << "<>" << src  << "<>" << dst << std::endl;
                std::abort();
            }
        }
    }
}

void test_a_star_admissible()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 6;
    
    std::cout << "__BEGIN_A_STAR_ADMISSIBLE_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_a_star_admissible();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_A_STAR_ADMISSIBLE_TEST__" << std::endl;
}

void test_one_a_star_consistent()
{
    using namespace graph_optimizer;

    unordered_map_x<Point, unordered_map_x<Point, double>> point_graph = randomize_point_graph();

    std::vector<Point> point_set                    = enumerate_point_graph(point_graph);
    unordered_map_x<Point, size_t> point_dict       = get_point_dictionary(point_set);

    std::vector<AStarEdgeInformation> edge_vec      = {};
    std::vector<FloyedEdgeInformation> floyed_vec   = {};

    for (const auto& [src, neighbor_map]: point_graph)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            size_t src_id   = point_dict.at(src);
            size_t dst_id   = point_dict.at(dst);

            edge_vec.push_back(AStarEdgeInformation
            {
                .src                = src_id,
                .dst                = dst_id,
                .score              = score,
                .is_bidirectional   = true
            });

            floyed_vec.push_back(FloyedEdgeInformation
            {
                .src                = src_id,
                .dst                = dst_id,
                .score              = score,
                .is_bidirectional   = true
            });
        }
    }

    auto heuristic = [&](size_t lhs, size_t rhs) -> double
    {
        if (lhs >= point_set.size())
        {
            throw std::runtime_error("bad access, out of bound access");
        }

        if (rhs >= point_set.size())
        {
            throw std::runtime_error("bad access, out of bound access");
        }

        const Point& lhs_point  = point_set[lhs];
        const Point& rhs_point  = point_set[rhs];

        return distance(lhs_point, rhs_point);
    };


    std::unordered_map<size_t, std::unordered_map<size_t, double>> path_info    = {};
    std::vector<std::tuple<size_t, size_t, double>> all_pair_result             = FloyedOptimizer{}.optimize(floyed_vec).distance_vec;
    const double EPSILON = 0.0001;

    for (const auto& [src, dst, score]: all_pair_result)
    {
        path_info[src][dst] = score;
    }

    ConsistentAStarOptimizer a_star_optimizer(edge_vec, heuristic);

    for (const auto& [src, neighbor_map]: path_info)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            double best_path_score = a_star_optimizer.optimize(src, dst).best_path_score;

            if (std::isnan(score))
            {
                std::cout << "mayday score, NaN" << std::endl;
                std::abort();
            }

            if (std::abs(best_path_score - score) > EPSILON)
            {
                std::cout << "mayday score, value mismatched" << "<>" << best_path_score << "<>" << score << std::endl;
                std::abort();
            }
        }
    }
}

void test_a_star_consistent()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 6;

    std::cout << "__BEGIN_A_STAR_CONSISTENT_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_a_star_consistent();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_A_STAR_CONSISTENT_TEST__" << std::endl;
}

void test_one_pagerank()
{
    using namespace graph_optimizer;

    std::unordered_map<size_t, std::unordered_map<size_t, double>> graph    = {};
    std::vector<PageRankEdgeInformation> pagerank_edge                      = {};
    std::unordered_set<size_t> vtx_set                                      = {};
    std::unordered_set<size_t> other_vtx_set                                = {};

    for (const auto& [src, neighbor_map]: graph)
    {
        for (const auto& [dst, score]: neighbor_map)
        {
            pagerank_edge.push_back
            (
                PageRankEdgeInformation
                {
                    .src    = src,
                    .dst    = dst,
                    .score  = score
                }
            );

            vtx_set.insert(dst);
        }

        vtx_set.insert(src);
    }

    std::vector<std::pair<size_t, double>> score_map = PageRankOptimizer{}.optimize(pagerank_edge).score_map;

    for (const auto& [vtx_id, score]: score_map)
    {
        if (std::isnan(score))
        {
            std::cout << "mayday score, NaN" << std::endl;
            std::abort();
        }

        if (std::isinf(score))
        {
            std::cout << "mayday score, inf" << std::endl;
            std::abort();
        }

        if (other_vtx_set.contains(vtx_id))
        {
            std::cout << "mayday vertex id, duplicated" << std::endl;
            std::abort();
        }

        other_vtx_set.insert(vtx_id);
    }

    if (vtx_set != other_vtx_set)
    {
        std::cout << "mayday vertex, insufficient scores" << std::endl;
        std::abort();
    }
}

void test_pagerank()
{
    const size_t TEST_SZ    = size_t{1} << 30;
    const size_t COUT_SZ    = size_t{1} << 6;

    std::cout << "__BEGIN_PAGERANK_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        test_one_pagerank();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i  << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_PAGERANK_TEST__" << std::endl;
}

int main()
{
    test_pagerank();
    test_a_star_admissible();
    test_a_star_consistent();
    test_dijkstra();
}