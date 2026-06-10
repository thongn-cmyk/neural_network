#ifndef __MEMORY_SYNC_TAINT_INTERVAL_TREE_H__
#define __MEMORY_SYNC_TAINT_INTERVAL_TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>
#include <functional>
#include <vector>
#include <unordered_map>
#include <stl_extension/stdx.h>
#include <stdexcept>
#include <exception>

namespace memory_sync
{
    class TaintIntervalTree
    {
        private:

            struct TreeNode
            {
                bool is_visited;
                bool is_blocked;
            };

            std::vector<TreeNode> interval_tree;
            size_t actual_base_sz;
            size_t virtual_base_sz;

        public:

            TaintIntervalTree(size_t base_sz)
            {
                this->interval_tree     = this->make_interval_tree_of_base_size(stdx::ceil2(base_sz));
                this->actual_base_sz    = stdx::ceil2(base_sz);
                this->virtual_base_sz   = base_sz;
            }

            template <class Visitor>
            void get_taint_region_vector(Visitor&& visitor) const
            {
                this->visit_helper(visitor,
                                   0u,
                                   0u, this->actual_base_sz);
            }

            auto get_taint_region_vector() const -> std::vector<std::pair<size_t, size_t>>
            {
                std::vector<std::pair<size_t, size_t>> rs{};

                auto visitor = [&](const std::pair<size_t, size_t>& interval)
                {
                    rs.push_back(interval);
                };

                this->visit_helper(visitor,
                                   0u,
                                   0u, this->actual_base_sz);

                return stdx::shrunk_adjecent_interval(rs);
            }

            void taint(const std::pair<size_t, size_t>& interval)
            {
                size_t first    = interval.first;
                size_t last     = first + interval.second;

                if (last > this->virtual_base_sz)
                {
                    throw std::invalid_argument("bad interval, out of bound access");
                }

                if (first == last)
                {
                    return;
                }

                size_t root_idx     = 0u;
                size_t root_first   = 0u;
                size_t root_last    = this->actual_base_sz;
                
                this->dispatch_block(root_idx,
                                     root_first, root_last,
                                     first, last);
            }

            void reset()
            {
                std::fill(this->interval_tree.begin(),
                          this->interval_tree.end(),
                          TreeNode{.is_visited = false, .is_blocked = false});
            }

            auto size() const noexcept -> size_t
            {
                return this->virtual_base_sz;
            }

        private:

            auto make_interval_tree_of_base_size(size_t base_sz) -> std::vector<TreeNode>
            {
                if (!stdx::is_pow2(base_sz))
                {
                    std::abort();
                }

                size_t tree_sz  = base_sz * 2 - 1;
                std::vector<TreeNode> rs(tree_sz, TreeNode{.is_visited = false, .is_blocked = false});

                return rs;
            }

            void dispatch_block(size_t root_idx,
                                size_t root_first, size_t root_last,
                                size_t interval_first, size_t interval_last)
            {
                if (this->interval_tree[root_idx].is_blocked)
                {
                    return;
                }

                this->interval_tree[root_idx].is_visited = true;

                if (root_first == interval_first && root_last == interval_last)
                {
                    this->interval_tree[root_idx].is_blocked = true;
                    return;
                }

                size_t root_sz      = root_last - root_first;
                size_t mid_sz       = root_sz >> 1;
                size_t nxt_first    = root_first + mid_sz;

                if (interval_last <= nxt_first)
                {
                    this->dispatch_block(root_idx * 2 + 1,
                                         root_first, nxt_first,
                                         interval_first, interval_last);
                }
                else if (interval_first >= nxt_first)
                {
                    this->dispatch_block(root_idx * 2 + 2,
                                         nxt_first, root_last,
                                         interval_first, interval_last);
                }
                else
                {
                    this->dispatch_block(root_idx * 2 + 1,
                                         root_first, nxt_first,
                                         interval_first, nxt_first);

                    this->dispatch_block(root_idx * 2 + 2,
                                         nxt_first, root_last,
                                         nxt_first, interval_last);
                }
            }

            template <class Visitor>
            void visit_helper(Visitor&& visitor,
                              size_t root_idx,
                              size_t root_first, size_t root_last) const
            {
                //I feel like interval tree has special precond compared to my other implementations of tree
                //

                if (!this->interval_tree[root_idx].is_visited)
                {
                    return;
                }

                if (this->interval_tree[root_idx].is_blocked)
                {
                    visitor(std::make_pair(root_first, root_last - root_first));
                    return;
                }

                if (root_first + 1 == root_last)
                {
                    return;
                }

                size_t root_sz      = root_last - root_first;
                size_t mid_sz       = root_sz >> 1;
                size_t nxt_first    = root_first + mid_sz;

                this->visit_helper(visitor,
                                   root_idx * 2 + 1,
                                   root_first, nxt_first);

                this->visit_helper(visitor,
                                   root_idx * 2 + 2,
                                   nxt_first, root_last);
            }
    };
}

#endif