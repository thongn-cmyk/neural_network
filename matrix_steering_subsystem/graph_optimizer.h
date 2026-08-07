#ifndef __GRAPH_OPTIMIZER_H__
#define __GRAPH_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <utility>
#include <stl_extension/unordered_node_map.h>
#include <stl_extension/hasher.h>
#include <stl_extension/stdx.h>
#include <iostream>

namespace graph_optimizer
{
    struct PageRankEdgeInformation
    {
        size_t src;
        size_t dst;
        double score;
    };

    struct PageRankResult
    {
        std::vector<std::pair<size_t, double>> score_map;
    };

    class PageRankOptimizer
    {
        private:

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

            static inline const size_t SUB_ITERATION_SZ         = 8u;
            static inline const size_t MASTER_ITERATION_SZ      = 4u;
            static inline const double INITIAL_INFLUENCE_SCORE  = 0.02;
            static inline const double DECAY_RATE               = 0.1;
            static inline const double INITIAL_SCORE            = 1;

        public:

            auto optimize(const std::vector<PageRankEdgeInformation>& edge_vec) -> PageRankResult
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph  = this->to_percentage_graph(edge_vec);
                local_unordered_map<size_t, double> score_map                           = this->make_initial_score_map(this->to_vertex_set(edge_vec));

                double influence_score = INITIAL_INFLUENCE_SCORE;

                for (size_t i = 0u; i < MASTER_ITERATION_SZ; ++i)
                {
                    score_map       = this->internal_optimize(graph, score_map, influence_score, SUB_ITERATION_SZ);
                    influence_score *= DECAY_RATE;
                }

                std::vector<std::pair<size_t, double>> result = {};

                for (const auto& map_pair: score_map)
                {
                    result.push_back({map_pair.first, map_pair.second});
                }

                return PageRankResult
                {
                    .score_map = std::move(result)
                };
            }

        private:

            auto to_percentage_graph(const std::vector<PageRankEdgeInformation>& edge_vec) -> local_unordered_map<size_t, local_unordered_map<size_t, double>>
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph{};
                const double EPSILON = 0.001;

                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        throw std::invalid_argument("bad edge, NaN");
                    }

                    if (edge.score < 0)
                    {
                        throw std::invalid_argument("bad edge, negative");
                    }

                    graph[edge.src][edge.dst]   = edge.score;
                    graph[edge.dst][edge.src]   = edge.score;
                }

                for (auto& map_pair: graph)
                {
                    double normalization_value = 0;

                    for (const auto& other_map_pair: map_pair.second)
                    {
                        normalization_value += other_map_pair.second;
                    }

                    double total_percentage     = 0;

                    for (auto& other_map_pair: map_pair.second)
                    {
                        other_map_pair.second = other_map_pair.second / normalization_value;

                        if (std::isnan(other_map_pair.second))
                        {
                            throw std::invalid_argument("bad percentage edge, NaN");
                        }

                        if (std::isinf(other_map_pair.second))
                        {
                            throw std::invalid_argument("bad percentage edge, inf");
                        }

                        total_percentage += other_map_pair.second;
                    }

                    if (std::abs(total_percentage - 1) > EPSILON)
                    {
                        throw std::runtime_error("bad percentage calculation");
                    }
                }

                return graph;
            }

            auto to_vertex_set(const std::vector<PageRankEdgeInformation>& edge_vec) -> std::vector<size_t>
            {
                local_unordered_set<size_t> vertex_set{};

                for (const auto& edge: edge_vec)
                {
                    vertex_set.insert(edge.src);
                    vertex_set.insert(edge.dst);
                }

                return {vertex_set.begin(), vertex_set.end()};
            }

            auto make_initial_score_map(const std::vector<size_t>& vertex_set) -> local_unordered_map<size_t, double>
            {
                local_unordered_map<size_t, double> result{};

                for (size_t vertex: vertex_set)
                {
                    result[vertex] = INITIAL_SCORE;
                }

                return result;
            }

            auto internal_prop_neighbor_one(const local_unordered_map<size_t, local_unordered_map<size_t, double>>& percentage_graph,
                                            const local_unordered_map<size_t, double>& score_map,
                                            double influence_score) -> local_unordered_map<size_t, double>
            {
                local_unordered_map<size_t, double> result = score_map;

                for (auto& map_pair: result)
                {
                    result[map_pair.first] = map_pair.second * (1 - influence_score);
                }

                for (const auto& map_pair: percentage_graph)
                {
                    for (const auto& other_map_pair: map_pair.second)
                    {
                        size_t src      = map_pair.first;
                        size_t dst      = other_map_pair.first;
                        double perc     = other_map_pair.second;
                        double score    = influence_score * score_map.at(src) * perc;

                        result.at(dst)  += score;
                    }
                }

                return result;
            }

            auto internal_optimize(const local_unordered_map<size_t, local_unordered_map<size_t, double>>& percentage_graph,
                                   const local_unordered_map<size_t, double>& score_map,
                                   double influence_score,
                                   size_t iteration_sz) -> local_unordered_map<size_t, double>
            {
                local_unordered_map<size_t, double> result_map = score_map;

                for (size_t i = 0u; i < iteration_sz; ++i)
                {
                    result_map = this->internal_prop_neighbor_one(percentage_graph, result_map, influence_score);
                }

                return result_map;
            }
    };

    template <class Score, class Value, class IsBetter = std::less<Score>>
    class BiDirectionalHeap
    {
        private:

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key, class Value2>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value2, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

            struct HeapNode
            {
                Score score;
                Value value;
                size_t heap_idx;
            };

            local_unordered_map<Value, std::shared_ptr<HeapNode>> reverse_map;
            std::vector<std::shared_ptr<HeapNode>> heap;

        public:

            auto peek() const noexcept -> std::pair<const Score&, const Value&>
            {
                if (this->heap.empty())
                {
                    std::abort();
                }

                return std::tie(this->heap.front()->score, this->heap.front()->value);
            }

            template <class ScoreLike, class ValueLike, class ConditionalPredicate>
            void conditional_push(ScoreLike&& score, ValueLike&& value, ConditionalPredicate&& pred)
            {
                auto map_ptr = this->reverse_map.find(value);

                if (map_ptr == this->reverse_map.end())
                {
                    std::shared_ptr<HeapNode> new_node = std::make_shared<HeapNode>(HeapNode
                    {
                        .score      = Score(std::forward<ScoreLike>(score)),
                        .value      = Value(std::forward<ValueLike>(value)),
                        .heap_idx   = this->heap.size()
                    });

                    this->heap.push_back(new_node);

                    try
                    {
                        this->reverse_map.insert(std::make_pair(this->heap.back()->value, new_node));
                    }
                    catch (...)
                    {
                        this->heap.pop_back();
                        throw;
                    }

                    this->push_up_at(this->heap.size() - 1u);
                }
                else
                {
                    const std::shared_ptr<HeapNode>& node = map_ptr->second;

                    if (pred(score, node->score))
                    {
                        node->score = std::forward<ScoreLike>(score);
                        this->correct_at(node->heap_idx);
                    }
                }
            }

            template <class ScoreLike, class ValueLike>
            void push(ScoreLike&& score, ValueLike&& value)
            {
                this->conditional_push(std::forward<ScoreLike>(score),
                                       std::forward<ValueLike>(value),
                                       []<class ...Args>(Args&& ...) noexcept {return true;});
            }

            void pop() noexcept
            {
                if (this->heap.empty())
                {
                    std::abort();
                }

                static_assert(true);

                this->swap_node(this->heap.front(), this->heap.back());
                this->reverse_map.erase(this->heap.back()->value);
                this->heap.pop_back();

                if (this->heap.size() != 0u)
                {
                    this->push_down_at(0u);
                }
            }

            void pop_back() noexcept
            {
                if (this->heap.empty())
                {
                    std::abort();
                }

                static_assert(true);

                this->reverse_map.erase(this->heap.back()->value);
                this->heap.pop_back();
            }

            void clear() noexcept
            {
                static_assert(true);

                this->reverse_map.clear();
                this->heap.clear();
            }

            auto size() const noexcept -> size_t
            {
                return this->heap.size();
            }

            auto empty() const noexcept -> bool
            {
                return this->heap.empty();
            }

        private:

            void swap_node(std::shared_ptr<HeapNode>& lhs, std::shared_ptr<HeapNode>& rhs) const noexcept
            {
                std::swap(lhs, rhs);
                std::swap(lhs->heap_idx, rhs->heap_idx);
            }

            void push_up_at(size_t idx) noexcept
            {
                if (idx >= this->heap.size())
                {
                    std::abort();
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = (idx - 1) / 2;

                static_assert(true);

                if (IsBetter{}(this->heap[parent_idx]->score, this->heap[idx]->score))
                {
                    return;
                }

                this->swap_node(this->heap[parent_idx], this->heap[idx]);
                this->push_up_at(parent_idx);
            }

            void push_down_at(size_t idx) noexcept
            {
                if (idx >= this->heap.size())
                {
                    std::abort();
                }

                size_t c = idx * 2 + 1;

                if (c >= this->heap.size())
                {
                    return;
                }

                if (c + 1 < this->heap.size() && IsBetter{}(this->heap[c + 1]->score, this->heap[c]->score))
                {
                    c += 1;
                }

                static_assert(true);

                if (IsBetter{}(this->heap[idx]->score, this->heap[c]->score))
                {
                    return;
                }

                this->swap_node(this->heap[idx], this->heap[c]);
                this->push_down_at(c);
            }

            void correct_at(size_t idx) noexcept
            {
                this->push_up_at(idx);
                this->push_down_at(idx);
            }
    };

    struct DijkstraEdgeInformation
    {
        size_t src;
        size_t dst;
        double score;
        bool is_bidirectional;
    };

    struct DijkstraResult
    {
        std::vector<std::pair<size_t, double>> distance_vec;
    };

    class DijkstraOptimizer
    {
        private:

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

            local_unordered_map<size_t, local_unordered_map<size_t, double>> graph;

        public:

            DijkstraOptimizer(const std::vector<DijkstraEdgeInformation>& edge_vec): graph(this->to_non_negative_graph(edge_vec)){}

            auto optimize(size_t src) -> DijkstraResult
            {
                BiDirectionalHeap<double, size_t, std::less<double>> score_heap = {};
                local_unordered_map<size_t, double> best_distance_map           = {};

                score_heap.push(0, src);

                while (true)
                {
                    if (score_heap.empty())
                    {
                        break;
                    }

                    double score;
                    size_t hinge;

                    std::tie(score, hinge) = score_heap.peek();
                    score_heap.pop();
                    best_distance_map.insert({hinge, score});

                    auto neighbor = this->graph.find(hinge);

                    if (neighbor == this->graph.end())
                    {
                        continue;
                    }

                    const local_unordered_map<size_t, double>& neighbor_vec = neighbor->second;

                    for (const auto& map_pair: neighbor_vec)
                    {
                        const auto& [neighbor_vtx_id, cost] = std::tie(map_pair.first, map_pair.second);

                        if (best_distance_map.contains(neighbor_vtx_id))
                        {
                            continue;
                        }

                        double new_score = score + cost;

                        if (std::isnan(new_score))
                        {
                            throw std::runtime_error("bad score, NaN");
                        }

                        score_heap.conditional_push(new_score, neighbor_vtx_id, std::less<double>{});
                    }
                }

                std::vector<std::pair<size_t, double>> result{};

                for (const auto& map_pair: best_distance_map)
                {
                    result.push_back({map_pair.first, map_pair.second});
                }

                return DijkstraResult
                {
                    .distance_vec = std::move(result)
                };
            }
        
        private:
            
            auto to_non_negative_graph(const std::vector<DijkstraEdgeInformation>& edge_vec) -> local_unordered_map<size_t, local_unordered_map<size_t, double>>
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph{};

                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        throw std::invalid_argument("bad edge score, NaN");
                    }

                    if (edge.score < 0)
                    {
                        throw std::invalid_argument("bad edge score, < 0");
                    }

                    graph[edge.src][edge.dst]   = edge.score;

                    if (edge.is_bidirectional)
                    {
                        graph[edge.dst][edge.src]   = edge.score;
                    }
                }

                return graph;
            }
    };

    struct FloyedEdgeInformation
    {
        size_t src;
        size_t dst;
        double score;
        bool is_bidirectional;
    };

    struct FloyedResult
    {
        std::vector<std::tuple<size_t, size_t, double>> distance_vec;
    };

    //A -> D, A -> B -> C -> E -> D
    //

    class FloyedOptimizer
    {
        private:

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

        public:

            auto optimize(const std::vector<FloyedEdgeInformation>& edge_vec) -> FloyedResult
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph              = this->to_graph(edge_vec);
                local_unordered_map<size_t, local_unordered_map<size_t, double>> accumulating_map   = graph;

                std::vector<size_t> node_vec = this->to_node_set(edge_vec);

                for (size_t mid: node_vec)
                {
                    for (size_t src: node_vec)
                    {
                        if (mid == src)
                        {
                            continue;
                        }

                        std::optional<double> ab_distance = std::nullopt;

                        if (auto map_ptr = accumulating_map.find(src); map_ptr != accumulating_map.end())
                        {
                            if (auto other_map_ptr = map_ptr->second.find(mid); other_map_ptr != map_ptr->second.end())
                            {
                                ab_distance = other_map_ptr->second;                                    
                            }
                        }

                        if (!ab_distance.has_value())
                        {
                            continue;
                        }

                        for (size_t dst: node_vec)
                        {
                            if (mid == dst)
                            {
                                continue;
                            }

                            if (src == dst)
                            {
                                continue;
                            }

                            std::optional<double> bc_distance = std::nullopt;

                            if (auto map_ptr = accumulating_map.find(mid); map_ptr != accumulating_map.end())
                            {
                                if (auto other_map_ptr = map_ptr->second.find(dst); other_map_ptr != map_ptr->second.end())
                                {
                                    bc_distance = other_map_ptr->second;
                                }
                            }

                            if (!bc_distance.has_value())
                            {
                                continue;
                            }

                            double ac_distance = ab_distance.value() + bc_distance.value();

                            if (std::isnan(ac_distance))
                            {
                                throw std::runtime_error("bad virtual edge, NaN");
                            }

                            auto& map_reference = accumulating_map[src];
                            auto map_ptr        = map_reference.find(dst);

                            if (map_ptr == map_reference.end())
                            {
                                auto [new_map_ptr, status] = map_reference.insert({dst, ac_distance});
                                map_ptr = new_map_ptr;
                            }

                            map_ptr->second     = std::min(map_ptr->second, ac_distance);
                        }
                    }
                }

                std::vector<std::tuple<size_t, size_t, double>> result{};

                for (const auto& map_pair: accumulating_map)
                {
                    result.push_back({map_pair.first, map_pair.first, 0.0});

                    for (const auto& other_map_pair: map_pair.second)
                    {
                        result.push_back({map_pair.first, other_map_pair.first, other_map_pair.second});
                    }
                }

                return FloyedResult
                {
                    .distance_vec = std::move(result)
                };
            }

        private:

            auto to_graph(const std::vector<FloyedEdgeInformation>& edge_vec) -> local_unordered_map<size_t, local_unordered_map<size_t, double>>
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph{};

                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        throw std::invalid_argument("bad edge score, NaN");
                    }

                    if (edge.src == edge.dst)
                    {
                        if (edge.score < 0)
                        {
                            throw std::invalid_argument("bad edge, negative self edge");
                        }
                        else
                        {
                            continue;
                        }
                    }

                    graph[edge.src][edge.dst]   = edge.score;

                    if (edge.is_bidirectional)
                    {
                        graph[edge.dst][edge.src]   = edge.score;
                    }
                }

                return graph;
            }

            auto to_node_set(const std::vector<FloyedEdgeInformation>& edge_vec) -> std::vector<size_t>
            {
                local_unordered_set<size_t> node_set{};

                for (const auto& edge: edge_vec)
                {
                    node_set.insert(edge.src);
                    node_set.insert(edge.dst);
                }

                return {node_set.begin(), node_set.end()};
            }
    };

    struct AStarEdgeInformation
    {
        size_t src;
        size_t dst;
        double score;
        bool is_bidirectional;
    };

    struct AStarResult
    {
        double best_path_score;
    };

    template <class HeuristicFunction>
    class AStarResource
    {
        private:

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;  

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

        protected:

            local_unordered_map<size_t, local_unordered_map<size_t, double>> graph;
            HeuristicFunction func;

            AStarResource(const std::vector<AStarEdgeInformation>& edge_vec,
                          HeuristicFunction func): graph(to_graph(edge_vec)),
                                                   func(std::move(func)){}
        
        private:

            auto to_graph(const std::vector<AStarEdgeInformation>& edge_vec) -> local_unordered_map<size_t, local_unordered_map<size_t, double>>
            {
                local_unordered_map<size_t, local_unordered_map<size_t, double>> graph{};

                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        throw std::invalid_argument("bad edge score, NaN");
                    }

                    graph[edge.src][edge.dst]   = edge.score;

                    if (edge.is_bidirectional)
                    {
                        graph[edge.dst][edge.src]   = edge.score;
                    }
                }

                return graph;
            }
    };

    template <class HeuristicFunction>
    class ConsistentAStarOptimizer: private AStarResource<HeuristicFunction>
    {
        private:

            using Base                  = AStarResource<HeuristicFunction>;

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;  

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

        public:

            ConsistentAStarOptimizer(const std::vector<AStarEdgeInformation>& edge_vec,
                                     HeuristicFunction func): Base(edge_vec, std::move(func)){}

            auto optimize(size_t src, size_t dst) -> AStarResult
            {   
                BiDirectionalHeap<double, size_t, std::less<double>> score_heap = {};
                local_unordered_set<size_t> is_visited                          = {};

                double org_cost = this->func(src, dst);

                if (std::isnan(org_cost))
                {
                    throw std::runtime_error("bad heuristic score, NaN");
                }

                score_heap.push(org_cost, src);

                while (true)
                {
                    if (score_heap.empty())
                    {
                        return AStarResult
                        {
                            .best_path_score = std::numeric_limits<double>::quiet_NaN()
                        };
                    }

                    double score;
                    size_t hinge;

                    std::tie(score, hinge) = score_heap.peek();
                    score_heap.pop();
                    is_visited.insert(hinge);

                    if (hinge == dst)
                    {
                        return AStarResult
                        {
                            .best_path_score = score
                        };
                    }

                    auto neighbor = this->graph.find(hinge);

                    if (neighbor == this->graph.end())
                    {
                        continue;
                    }

                    const local_unordered_map<size_t, double>& neighbor_vec = neighbor->second;
                    double old_heuristic_cost = this->func(hinge, dst);

                    for (const auto& map_pair: neighbor_vec)
                    {
                        const auto& [neighbor_vtx_id, cost] = std::tie(map_pair.first, map_pair.second);

                        if (is_visited.contains(neighbor_vtx_id))
                        {
                            continue;
                        }

                        double new_heuristic_cost           = this->func(neighbor_vtx_id, dst);
                        double new_score                    = score - old_heuristic_cost + cost + new_heuristic_cost;

                        if (std::isnan(new_score))
                        {
                            throw std::runtime_error("bad heuristic score, NaN");
                        }

                        if (new_score < score)
                        {
                            throw std::runtime_error("inconsistent function, h(x) < h(neighbor)");
                        }

                        score_heap.conditional_push(new_score, neighbor_vtx_id, std::less<double>{});
                    }
                }
            }
    };

    template <class HeuristicFunction>
    class AdmissibleAStarOptimizer: private AStarResource<HeuristicFunction>
    {
        private:

            using Base                  = AStarResource<HeuristicFunction>;

            template <class T>
            using default_hasher        = hasher::default_hasher<T>;

            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;  

            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;

        public:

            AdmissibleAStarOptimizer(const std::vector<AStarEdgeInformation>& edge_vec,
                                     HeuristicFunction func): Base(edge_vec, std::move(func)){}

            auto optimize(size_t src, size_t dst,
                          size_t max_iteration_sz = 1024,
                          bool has_inferior_est = false) -> AStarResult
            {
                BiDirectionalHeap<double, size_t, std::less<double>> score_heap = {};
                local_unordered_map<size_t, double> best_score_map              = {};

                double org_cost = this->func(src, dst);

                if (std::isnan(org_cost))
                {
                    throw std::runtime_error("bad heuristic score, NaN");
                }

                score_heap.push(org_cost, src);
                best_score_map.insert({src, org_cost});

                for (size_t i = 0u; i < max_iteration_sz; ++i)
                {
                    if (score_heap.empty())
                    {
                        break;
                    }

                    double score;
                    size_t hinge;

                    std::tie(score, hinge) = score_heap.peek();
                    score_heap.pop();

                    if (hinge == dst)
                    {
                        return AStarResult
                        {
                            .best_path_score = score
                        };
                    }

                    auto neighbor = this->graph.find(hinge);

                    if (neighbor == this->graph.end())
                    {
                        continue;
                    }

                    const local_unordered_map<size_t, double>& neighbor_vec = neighbor->second;
                    double old_heuristic_cost = this->func(hinge, dst);

                    for (const auto& map_pair: neighbor_vec)
                    {
                        const auto& [neighbor_vtx_id, cost] = std::tie(map_pair.first, map_pair.second);
                        double new_heuristic_cost           = this->func(neighbor_vtx_id, dst);
                        double new_score                    = score - old_heuristic_cost + cost + new_heuristic_cost;

                        if (std::isnan(new_score))
                        {
                            throw std::runtime_error("bad heuristic score, NaN");
                        }

                        auto best_score_map_ptr             = best_score_map.find(neighbor_vtx_id);

                        if (best_score_map_ptr == best_score_map.end())
                        {
                            best_score_map.insert({neighbor_vtx_id, new_score});
                            score_heap.push(new_score, neighbor_vtx_id);
                        }
                        else
                        {
                            if (new_score < best_score_map_ptr->second)
                            {
                                best_score_map_ptr->second = new_score; 
                                score_heap.push(new_score, neighbor_vtx_id);
                            }
                        }
                    }
                }

                if (has_inferior_est)
                {
                    if (auto map_ptr = best_score_map.find(dst); map_ptr != best_score_map.end())
                    {
                        return AStarResult
                        {
                            .best_path_score = map_ptr->second
                        };
                    }
                }

                return AStarResult
                {
                    .best_path_score = std::numeric_limits<double>::quiet_NaN()
                };
            }
    };

   struct BinaryFCDEdgeInformation
    {
        size_t src;
        size_t dst;
        double score;
    };
 
    struct BinaryFCDResult
    {
        std::vector<size_t> vertices;
        std::vector<std::pair<size_t, size_t>> community_range;   // [first,last) into `vertices` per community
    };
    
    class BinaryFCDOptimizer
    {
        private:
    
            template <class T>
            using default_hasher        = hasher::default_hasher<T>;
    
            template <class Key>
            using local_unordered_set   = unordered_map_variants::unordered_node_set<Key, size_t, default_hasher<Key>>;
    
            template <class Key, class Value>
            using local_unordered_map   = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, default_hasher<Key>>;
    
            using compute_float_t       = long double;
    
            struct BinaryClassifyResult
            {
                std::vector<size_t> flat_community_vec;
                std::vector<std::pair<size_t, size_t>> flat_community_range_vec;
            };
    
        public:
    
            auto optimize(const std::vector<BinaryFCDEdgeInformation>& edge_vec, size_t optimization_step = 32, size_t best_decay_factor = 1) -> BinaryFCDResult
            {
                std::vector<BinaryFCDEdgeInformation> filtered_edge_vec = this->to_edge_vec(this->to_edge_graph(edge_vec));
                BinaryClassifyResult community_result = this->to_initial_group(this->to_initial_community(filtered_edge_vec));
    
                // Precompute static quantities needed for modularity gain: weighted
                // vertex degree and total edge weight. These don't change as
                // communities merge, only how they're grouped.
                local_unordered_map<size_t, compute_float_t> vertex_degree{};
                compute_float_t total_weight = 0.0L;
    
                for (const auto& edge : filtered_edge_vec)
                {
                    vertex_degree[edge.src] += static_cast<compute_float_t>(edge.score);
                    vertex_degree[edge.dst] += static_cast<compute_float_t>(edge.score);
                    total_weight            += static_cast<compute_float_t>(edge.score);
                }
    
                compute_float_t twom = 2.0L * total_weight;
    
                for (size_t i = 0u; i < optimization_step; ++i)
                {
                    if (community_result.flat_community_range_vec.size() <= 1)
                    {
                        break;
                    }
    
                    size_t old_sz                   = community_result.flat_community_range_vec.size();
                    BinaryClassifyResult nxt_bcr    = this->binary_classify(filtered_edge_vec, community_result.flat_community_vec, community_result.flat_community_range_vec, best_decay_factor, vertex_degree, twom);
                    size_t new_sz                   = nxt_bcr.flat_community_range_vec.size();
                    bool is_progressing             = old_sz != new_sz;
    
                    if (!is_progressing)
                    {
                        break;
                    }
    
                    community_result                = std::move(nxt_bcr);
                }
    
                return BinaryFCDResult
                {
                    .vertices        = std::move(community_result.flat_community_vec),
                    .community_range = std::move(community_result.flat_community_range_vec)
                };
            }
    
        private:
    
            auto to_edge_graph(const std::vector<BinaryFCDEdgeInformation>& edge_vec) -> local_unordered_map<std::pair<size_t, size_t>, double>
            {
                local_unordered_map<std::pair<size_t, size_t>, double> rs{};
    
                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        throw std::invalid_argument("bad edge score, not a number");
                    }
    
                    if (std::isinf(edge.score))
                    {
                        throw std::invalid_argument("bad edge score, inf");
                    }
    
                    size_t src_vtx  = std::min(edge.src, edge.dst);
                    size_t dst_vtx  = std::max(edge.src, edge.dst);
                    auto new_edge   = std::make_pair(src_vtx, dst_vtx);
    
                    rs[new_edge]    += edge.score;   // accumulate multi-edges
                }
    
                return rs;
            }
    
            auto to_edge_vec(const local_unordered_map<std::pair<size_t, size_t>, double>& graph) -> std::vector<BinaryFCDEdgeInformation>
            {
                std::vector<BinaryFCDEdgeInformation> rs(graph.size());
                size_t i = 0u;
    
                for (const auto& map_pair: graph)
                {
                    const auto& [src_vtx, dst_vtx] = map_pair.first;
                    rs[i++] = BinaryFCDEdgeInformation
                    {
                        .src    = src_vtx,
                        .dst    = dst_vtx,
                        .score  = map_pair.second
                    };
                }
    
                return rs;
            }
    
            auto to_initial_community(const std::vector<BinaryFCDEdgeInformation>& edge_vec) -> local_unordered_set<size_t>
            {
                local_unordered_set<size_t> rs{};
    
                for (const auto& edge: edge_vec)
                {
                    rs.insert(edge.src);
                    rs.insert(edge.dst);
                }
    
                return rs;
            }
    
            auto to_initial_group(const local_unordered_set<size_t>& vertex_set) -> BinaryClassifyResult
            {
                std::vector<size_t> rs(vertex_set.size());
                std::vector<std::pair<size_t, size_t>> rs_range(vertex_set.size());
    
                size_t i = 0u;
    
                for (size_t vertex: vertex_set)
                {
                    rs[i]       = vertex;
                    rs_range[i] = {i, i + 1};
                    i           += 1;
                }
    
                return BinaryClassifyResult
                {
                    .flat_community_vec         = std::move(rs),
                    .flat_community_range_vec   = std::move(rs_range)
                };
            }
    
            auto group_size(const std::pair<size_t, size_t>& first_last_range) -> size_t
            {
                return first_last_range.second - first_last_range.first;
            }
    
            auto binary_classify(const std::vector<BinaryFCDEdgeInformation>& edge_vec,
                                const std::vector<size_t>& flat_community_vec,
                                const std::vector<std::pair<size_t, size_t>>& flat_community_range_vec,
                                size_t best_decay_factor,
                                const local_unordered_map<size_t, compute_float_t>& vertex_degree,
                                compute_float_t twom) -> BinaryClassifyResult
            {
                local_unordered_map<size_t, std::pair<size_t, size_t>> reverse_map{};
                reverse_map.reserve(flat_community_vec.size());
    
                std::vector<compute_float_t> community_degree(flat_community_range_vec.size(), 0.0L);
    
                for (size_t i = 0u; i < flat_community_range_vec.size(); ++i)
                {
                    const auto& [first, last] = flat_community_range_vec[i];
    
                    for (size_t j = first; j < last; ++j)
                    {
                        size_t node_idx         = flat_community_vec[j];
                        size_t community_idx    = i;
                        reverse_map[node_idx]   = {community_idx, last - first};
                        community_degree[i]    += vertex_degree.at(node_idx);
                    }
                }
    
                local_unordered_map<std::pair<size_t, size_t>, compute_float_t> community_weight_map{};
    
                for (const auto& edge: edge_vec)
                {
                    size_t src_vtx;
                    size_t dst_vtx;
                    double score;
    
                    std::tie(src_vtx, dst_vtx, score)   = std::make_tuple(edge.src, edge.dst, edge.score);
    
                    auto [src_community_idx, src_community_sz]  = reverse_map.at(src_vtx);
                    auto [dst_community_idx, dst_community_sz]  = reverse_map.at(dst_vtx);
    
                    if (dst_community_idx == src_community_idx)
                    {
                        continue;   // intra-community edge: doesn't inform a merge decision
                    }
    
                    size_t lo_idx                   = std::min(src_community_idx, dst_community_idx);
                    size_t hi_idx                   = std::max(src_community_idx, dst_community_idx);
                    auto group_id                   = std::make_pair(lo_idx, hi_idx);
                    community_weight_map[group_id]  += static_cast<compute_float_t>(score);
                }
    
                std::vector<std::pair<std::pair<size_t, size_t>, compute_float_t>> community_vec{};
                community_vec.reserve(community_weight_map.size());
    
                for (const auto& map_pair: community_weight_map)
                {
                    size_t c1 = map_pair.first.first;
                    size_t c2 = map_pair.first.second;
    
                    compute_float_t e_c1c2 = map_pair.second;
                    compute_float_t a1     = community_degree[c1];
                    compute_float_t a2     = community_degree[c2];
    
                    // standard incremental modularity gain from merging c1,c2
                    compute_float_t delta_q = 2.0L * (e_c1c2 / twom - (a1 * a2) / (twom * twom));
    
                    if (delta_q > 0.0L)
                    {
                        community_vec.push_back({map_pair.first, delta_q});
                    }
                }
    
                auto less = [](const auto& lhs, const auto& rhs)
                {
                    bool lhs_valid = !std::isnan(lhs.second);
                    bool rhs_valid = !std::isnan(rhs.second);
    
                    if (lhs_valid != rhs_valid)
                    {
                        return rhs_valid;
                    }
    
                    if (!lhs_valid)
                    {
                        return false;
                    }
    
                    if (lhs.second != rhs.second)
                    {
                        return lhs.second < rhs.second;
                    }
    
                    return lhs.first > rhs.first;   // deterministic tie-break
                };
    
                std::make_heap(community_vec.begin(), community_vec.end(), less);
    
                local_unordered_set<size_t> visited_set                             = {};
                std::vector<size_t> new_flat_community_vec                          = {};
                std::vector<std::pair<size_t, size_t>> new_flat_community_range_vec = {};
                size_t iterable_community_vec_sz                                    = community_vec.empty() ? 0u : std::max<size_t>(community_vec.size() >> best_decay_factor, 1u);
    
                for (size_t i = 0u; i < iterable_community_vec_sz; ++i)
                {
                    size_t src_group_id = community_vec[0].first.first;
                    size_t dst_group_id = community_vec[0].first.second;
    
                    std::pop_heap(community_vec.begin(), community_vec.end(), less);
                    community_vec.pop_back();
    
                    if (visited_set.contains(src_group_id))
                    {
                        continue;
                    }
    
                    if (visited_set.contains(dst_group_id))
                    {
                        continue;
                    }
    
                    visited_set.insert(src_group_id);
                    visited_set.insert(dst_group_id);
    
                    size_t new_first                    = new_flat_community_vec.size();
                    size_t new_last                     = new_first + this->group_size(flat_community_range_vec[src_group_id]) + this->group_size(flat_community_range_vec[dst_group_id]);
    
                    const auto& [src_first, src_last]   = flat_community_range_vec[src_group_id];
                    const auto& [dst_first, dst_last]   = flat_community_range_vec[dst_group_id];
    
                    std::copy(std::next(flat_community_vec.begin(), src_first), std::next(flat_community_vec.begin(), src_last), std::back_inserter(new_flat_community_vec));
                    std::copy(std::next(flat_community_vec.begin(), dst_first), std::next(flat_community_vec.begin(), dst_last), std::back_inserter(new_flat_community_vec));
    
                    new_flat_community_range_vec.push_back({new_first, new_last});
                }
    
                for (size_t i = 0u; i < flat_community_range_vec.size(); ++i)
                {
                    if (visited_set.contains(i))
                    {
                        continue;
                    }
    
                    const auto& [group_first, group_last]   = flat_community_range_vec[i];
                    size_t new_first                        = new_flat_community_vec.size();
                    size_t new_last                         = new_first + (group_last - group_first);
    
                    std::copy(std::next(flat_community_vec.begin(), group_first), std::next(flat_community_vec.begin(), group_last), std::back_inserter(new_flat_community_vec));
                    new_flat_community_range_vec.push_back({new_first, new_last});
                }
    
                return BinaryClassifyResult
                {
                    .flat_community_vec         = std::move(new_flat_community_vec),
                    .flat_community_range_vec   = std::move(new_flat_community_range_vec)
                };
            }
    };
}

#endif