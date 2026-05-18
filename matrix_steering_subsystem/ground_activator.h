#ifndef __GROUND_ACTIVATOR_H__
#define __GROUND_ACTIVATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <chrono>
#include <optional>
#include <algorithm>
#include <utility>

// #include "compact_serializer.h"
// #include "stl_extension.h"

namespace ground_activator
{
    //in this tree
    //we want to extract all the associated interval that the argument interval is a subset of
    //assume that we register 2 interval [1, 10] -> [1, 2], [1, 20] -> [1, 3]

    //we want to query interval [1, 4]
    //[1, 4] is subset of [1, 10] and [1, 20], so we'd get [1,2], [1, 3]
    //this is tricky

    //the proof of this algorithm is actually a c b, b c a => a == b

    //assume two random intervals arg = [a, b) and registered [c, d)
    //assume that the two intervals intersected at a random interval [x1, x2)

    //we'd need to prove that associated_set_of([c, d)) c associated_set_of([a, b))

    //let's inspect [x1, x2)

    //we know that both of the segments intervals (the log representation of the original interval) span the tree [x1, x2)
    //so we can prove that there exists a pair of node (node1, node2) such that node1 is parent of node2 or child of node2

    //without loss of generality assume that node1 is parent of node2, then we'd get the associated intervals via our parent trace
    //assume that node1 is child of node2, then we'd get the assoociated intervals via the total trace

    //therefore, associated_set_of([c, d)) c associated_set_of([a, b))

    //now let's inspect the result of associated_set_of([a, b))
    //the result is either the parent trace or the child total

    //parent trace => ok => the included set is logically correct
    //child total => ok => the included set is logically correct

    //=> the associated_set_of([a, b)) only includes the stated logically correct sets of intersection

    //therefore, associated_set_of([a, b)) are all the grounds of all the intersected intervals

    class GroundTree
    {
        private:

            struct TreeNode
            {
                std::optional<std::vector<std::pair<size_t, size_t>>> ground_segment_set;
                std::optional<std::vector<std::pair<size_t, size_t>>> total_segment_set;
            };

            std::vector<TreeNode> interval_tree;
            size_t sz;

        public:

            GroundTree(size_t range)
            {
                size_t ceil2_range  = this->ceil2(range);
                size_t tree_sz      = this->base_size_to_tree_size(ceil2_range);
                this->interval_tree = std::vector<TreeNode>(tree_sz);
                this->sz            = range;
            }

            auto get_ground(const std::pair<size_t, size_t>& interval) -> std::vector<std::pair<size_t, size_t>>
            {
                size_t first    = interval.first;
                size_t last     = interval.first + interval.second;

                if (last > this->sz)
                {
                    throw std::invalid_argument("out of bound access");
                }

                if (first == last)
                {
                    return {};
                }

                return this->tree_get_ground(this->interval_tree.data(), this->tree_size_to_base_size(this->interval_tree.size()),
                                             first, last);
            }

            template <class Visitor>
            void visit(Visitor&& visitor)
            {
                this->visit(this->interval_tree.data(), this->tree_size_to_base_size(this->interval_tree.size()),
                            visitor);
            }

            void add_ground(const std::pair<size_t, size_t>& interval, const std::pair<size_t, size_t>& ground)
            {
                size_t first    = interval.first;
                size_t last     = interval.first + interval.second;

                if (last > this->sz)
                {
                    throw std::invalid_argument("out of bound access");
                }

                if (first == last)
                {
                    return;
                }

                this->tree_add_ground(this->interval_tree.data(), this->tree_size_to_base_size(this->interval_tree.size()),
                                      first, last,
                                      ground);
            }

            void optimize()
            {
                this->tree_optimize(this->interval_tree.data(), this->tree_size_to_base_size(this->interval_tree.size()));
            }

            auto size() -> size_t
            {
                return this->sz;
            }

        private:

            auto ceil2(size_t sz) -> size_t
            {
                for (size_t i = 0u; i < std::numeric_limits<size_t>::digits; ++i)
                {
                    size_t cand = size_t{1} << i;

                    if (cand >= sz)
                    {
                        return cand;
                    }
                }

                throw std::invalid_argument("bad ceil2 size, upper numeric limits reached");
            }

            auto base_size_to_tree_size(size_t base_sz) -> size_t
            {
                return base_sz * 2 - 1;
            }

            auto tree_size_to_base_size(size_t tree_sz) -> size_t
            {
                return (tree_sz + 1) / 2;
            }

            void tree_get_ground_unsorted_helper(TreeNode * tree_arr,
                                                 size_t idx,
                                                 size_t node_interval_first, size_t node_interval_last,
                                                 size_t key_interval_first, size_t key_interval_last,
                                                 std::vector<std::pair<size_t, size_t>>& result)
            {
                if (tree_arr[idx].ground_segment_set.has_value())
                {
                    std::copy(tree_arr[idx].ground_segment_set->begin(), tree_arr[idx].ground_segment_set->end(), std::back_inserter(result));                            
                }

                if (node_interval_first == key_interval_first && node_interval_last == key_interval_last)
                {
                    if (tree_arr[idx].total_segment_set.has_value())
                    {
                        std::copy(tree_arr[idx].total_segment_set->begin(), tree_arr[idx].total_segment_set->end(), std::back_inserter(result));
                    }

                    return;
                }

                size_t interval_sz              = node_interval_last - node_interval_first;
                size_t node_next_interval_first = node_interval_first + interval_sz / 2;

                if (key_interval_last <= node_next_interval_first)
                {
                    this->tree_get_ground_unsorted_helper(tree_arr, idx * 2 + 1, node_interval_first, node_next_interval_first, key_interval_first, key_interval_last, result);
                    return;
                }

                if (key_interval_first >= node_next_interval_first)
                {
                    this->tree_get_ground_unsorted_helper(tree_arr, idx * 2 + 2, node_next_interval_first, node_interval_last, key_interval_first, key_interval_last, result);
                    return;
                }

                this->tree_get_ground_unsorted_helper(tree_arr, idx * 2 + 1, node_interval_first, node_next_interval_first, key_interval_first, node_next_interval_first, result);
                this->tree_get_ground_unsorted_helper(tree_arr, idx * 2 + 2, node_next_interval_first, node_interval_last, node_next_interval_first, key_interval_last, result);
            }

            auto interval_sort_and_adjecent_join(const std::vector<std::pair<size_t, size_t>>& arg_vec) -> std::vector<std::pair<size_t, size_t>>
            {
                auto less = [](const auto& lhs, const auto& rhs)
                {
                    if (lhs.first < rhs.first)
                    {
                        return true;
                    }

                    if (lhs.first > rhs.first)
                    {
                        return false;
                    }

                    if (lhs.second < rhs.second)
                    {
                        return true;
                    }

                    if (lhs.second > rhs.second)
                    {
                        return false;
                    }

                    return false;
                };

                auto tmp_vec    = arg_vec;
                auto result_vec = std::vector<std::pair<size_t, size_t>>();

                std::sort(tmp_vec.begin(), tmp_vec.end(), less);

                std::optional<std::pair<size_t, size_t>> aggregated_interval = std::nullopt;

                for (size_t i = 0u; i < tmp_vec.size(); ++i)
                {
                    if (!aggregated_interval.has_value())
                    {
                        aggregated_interval = tmp_vec[i];
                        continue;
                    }

                    size_t now_last     = aggregated_interval->first + aggregated_interval->second;
                    size_t nxt_first    = tmp_vec[i].first;
                    size_t nxt_last     = tmp_vec[i].first + tmp_vec[i].second;

                    if (now_last >= nxt_first)
                    {
                        if (nxt_last > now_last)
                        {
                            size_t new_sz = nxt_last - aggregated_interval->first;
                            aggregated_interval->second = new_sz;
                        }

                        continue;
                    }

                    result_vec.push_back(aggregated_interval.value());
                    aggregated_interval = tmp_vec[i];
                }

                if (aggregated_interval.has_value())
                {
                    result_vec.push_back(aggregated_interval.value());
                }

                return result_vec;
            }

            auto tree_get_ground(TreeNode * tree_arr, size_t base_sz,
                                 size_t interval_first, size_t interval_last) -> std::vector<std::pair<size_t, size_t>>
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                if (interval_first >= interval_last)
                {
                    throw std::invalid_argument("bad interval, interval size is 0 or negative");
                }

                if (interval_last > base_sz)
                {
                    throw std::invalid_argument("bad interval, out of bound access");
                }

                std::vector<std::pair<size_t, size_t>> unsorted_result{};

                size_t idx      = 0u;
                size_t first    = 0u;
                size_t last     = base_sz;

                this->tree_get_ground_unsorted_helper(tree_arr,
                                                      idx,
                                                      first, last,
                                                      interval_first, interval_last,
                                                      unsorted_result);

                this->interval_sort_and_adjecent_join(unsorted_result);

                return unsorted_result;
            }

            template <class Visitor>
            void visit_helper(TreeNode * tree_arr,
                              size_t idx,
                              size_t first, size_t last,
                              Visitor&& visitor)
            {
                if (tree_arr[idx].ground_segment_set.has_value())
                {
                    for (const auto& e: tree_arr[idx].ground_segment_set.value())
                    {
                        visitor(first, last, e);
                    }
                }

                if (first + 1 == last)
                {
                    return;
                }

                size_t interval_sz  = last - first;
                size_t next_first   = first + interval_sz / 2;

                this->visit_helper(tree_arr, idx * 2 + 1, first, next_first, visitor);
                this->visit_helper(tree_arr, idx * 2 + 2, next_first, last, visitor);
            }

            template <class Visitor>
            void visit(TreeNode * tree_arr, size_t base_sz,
                       Visitor&& visitor)
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                size_t idx      = 0u;
                size_t first    = 0u;
                size_t last     = base_sz;

                this->visit_helper(tree_arr,
                                   idx,
                                   first, last,
                                   visitor);
            }

            void tree_node_add_item(TreeNode& node,
                                    std::pair<size_t, size_t> item)
            {
                if (!node.ground_segment_set.has_value())
                {
                    node.ground_segment_set = std::vector<std::pair<size_t, size_t>>{};
                }

                node.ground_segment_set->push_back(item);
            }

            void tree_node_add_total(TreeNode& node,
                                     std::pair<size_t, size_t> item)
            {
                if (!node.total_segment_set.has_value())
                {
                    node.total_segment_set = std::vector<std::pair<size_t, size_t>>{};
                }

                node.total_segment_set->push_back(item);
            }

            void tree_add_ground_helper(TreeNode * tree_arr,
                                        size_t idx,
                                        size_t node_interval_first, size_t node_interval_last,
                                        size_t key_interval_first, size_t key_interval_last,
                                        std::pair<size_t, size_t> item)
            {
                this->tree_node_add_total(tree_arr[idx], item);

                if (node_interval_first == key_interval_first && node_interval_last == key_interval_last)
                {
                    this->tree_node_add_item(tree_arr[idx], item);
                    return;
                }

                size_t interval_sz              = node_interval_last - node_interval_first;
                size_t node_next_interval_first = node_interval_first + interval_sz / 2;

                if (key_interval_last <= node_next_interval_first)
                {
                    this->tree_add_ground_helper(tree_arr, idx * 2 + 1, node_interval_first, node_next_interval_first, key_interval_first, key_interval_last, item);
                    return;
                }

                if (key_interval_first >= node_next_interval_first)
                {
                    this->tree_add_ground_helper(tree_arr, idx * 2 + 2, node_next_interval_first, node_interval_last, key_interval_first, key_interval_last, item);
                    return;
                }

                this->tree_add_ground_helper(tree_arr, idx * 2 + 1, node_interval_first, node_next_interval_first, key_interval_first, node_next_interval_first, item);
                this->tree_add_ground_helper(tree_arr, idx * 2 + 2, node_next_interval_first, node_interval_last, node_next_interval_first, key_interval_last, item);
            }

            void tree_add_ground(TreeNode * tree_arr, size_t base_sz,
                                 size_t interval_first, size_t interval_last,
                                 const std::pair<size_t, size_t>& ground)
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                if (interval_first >= interval_last)
                {
                    throw std::invalid_argument("bad interval, interval size is 0 or negative");
                }

                if (interval_last > base_sz)
                {
                    throw std::invalid_argument("bad interval, out of bound access");
                }

                if (ground.second == 0u)
                {
                    return;
                }

                size_t idx      = 0u;
                size_t first    = 0u;
                size_t last     = base_sz;

                this->tree_add_ground_helper(tree_arr,
                                             idx,
                                             first, last,
                                             interval_first, interval_last,
                                             ground);
            }

            void tree_optimize_helper(TreeNode * tree_arr,
                                      size_t idx,
                                      size_t first, size_t last)
            {
                if (tree_arr[idx].ground_segment_set.has_value())
                {
                    tree_arr[idx].ground_segment_set    = this->interval_sort_and_adjecent_join(tree_arr[idx].ground_segment_set.value());
                }

                if (tree_arr[idx].total_segment_set.has_value())
                {
                    tree_arr[idx].total_segment_set     = this->interval_sort_and_adjecent_join(tree_arr[idx].total_segment_set.value());
                }

                if (first + 1 == last)
                {
                    return;
                }

                size_t interval_sz  = last - first;
                size_t next_first   = first + interval_sz / 2;

                this->tree_optimize_helper(tree_arr,
                                           idx * 2 + 1,
                                           first, next_first);

                this->tree_optimize_helper(tree_arr,
                                           idx * 2 + 2,
                                           next_first, last);
            }

            void tree_optimize(TreeNode * tree_arr, size_t base_sz)
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                size_t idx      = 0u;
                size_t first    = 0u;
                size_t last     = base_sz;

                this->tree_optimize_helper(tree_arr,
                                           idx,
                                           first, last);
            }
    };
}

#endif