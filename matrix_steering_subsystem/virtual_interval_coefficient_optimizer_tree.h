#ifndef __VIRTUAL_INTERVAL_COEFFICIENT_OPTIMIZER_TREE_H__
#define __VIRTUAL_INTERVAL_COEFFICIENT_OPTIMIZER_TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
#include "interval_coefficient_optimizer_tree.h"
#include "graph_optimizer.h"
#include <stl_extension/unordered_node_map.h>
#include <stl_extension/hasher.h>
#include <stl_extension/stdx.h>
#include "assert.h"
#include <algorithm_extension/short_heap.h>

namespace virtual_interval_coefficient_optimizer_tree
{
    using std_float_t = float_def::std_float_t;

    class BatchCoefficientSpaceTensorInterface
    {
        public:

            virtual ~BatchCoefficientSpaceTensorInterface() = default;

            virtual auto get_coefficient_space() -> std::vector<std::vector<std_float_t>> = 0;
            virtual void feedback(std_float_t rating) = 0;
    };

    class BatchCoefficientOptimizerTreeInterface
    {
        public:

            virtual ~BatchCoefficientOptimizerTreeInterface() = default;

            virtual auto get_coefficient_span(const std::vector<std::pair<size_t, size_t>>& range_vec) -> std::unique_ptr<BatchCoefficientSpaceTensorInterface> = 0;
            virtual void rearrange_focal() = 0;
            virtual auto size() -> size_t = 0;
    };

    class TranslationSpaceTensorInterface
    {
        public:

            virtual ~TranslationSpaceTensorInterface() = default;

            virtual auto get_translation_space() -> std::vector<std::vector<std::pair<size_t, size_t>>> = 0;
            virtual void feedback(std_float_t rating) = 0;
    };

    class TranslationOptimizerTreeInterface
    {
        public:

            virtual ~TranslationOptimizerTreeInterface() = default;

            virtual auto get_translation_tensor(const std::vector<std::pair<size_t, size_t>>& range_vec) -> std::unique_ptr<TranslationSpaceTensorInterface> = 0;
            virtual void rearrange_focal() = 0;
            virtual auto size() -> size_t = 0;
    };

    class SegmentMapperInterface
    {
        public:

            virtual ~SegmentMapperInterface() = default;

            virtual void hint(const std::vector<std::pair<size_t, size_t>>& segment_vec, std_float_t rating) = 0;
            virtual void clear() = 0;
            virtual void apply() = 0;
            virtual auto map(const std::pair<size_t, size_t>& segment) -> std::vector<std::pair<size_t, size_t>> = 0;
    };

    class SegmentMapper: public virtual SegmentMapperInterface
    {
        private:

            template <class Key, class Value>
            using local_unordered_map = unordered_map_variants::unordered_node_map<Key, Value, size_t, std::integral_constant<bool, true>, hasher::default_hasher<Key>, hasher::default_equal_to<Key>>;

            struct Edge
            {
                size_t src_vtx;
                size_t dst_vtx;
                std_float_t score;
            };

            std::vector<size_t> translation_table;
            std::vector<Edge> promotion_queue;

            size_t base_sz;
            size_t leaf_sz;
            size_t promotion_queue_cap;

        public:

            SegmentMapper(size_t base_sz,
                          size_t leaf_sz,
                          size_t promotion_queue_cap)
            {
                if (leaf_sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                if (base_sz % leaf_sz != 0u)
                {
                    throw std::invalid_argument("bad base size, not multipliers of leaf size");
                }

                size_t translation_table_sz = base_sz / leaf_sz + size_t{base_sz % leaf_sz != 0u};

                this->translation_table     = std::vector<size_t>(translation_table_sz);
                std::iota(translation_table.begin(), translation_table.end(), 0u);

                this->promotion_queue       = std::vector<Edge>();
                this->base_sz               = base_sz;
                this->leaf_sz               = leaf_sz;
                this->promotion_queue_cap   = promotion_queue_cap;
            }

            void hint(const std::vector<std::pair<size_t, size_t>>& segment_vec, std_float_t rating)
            {
                if (std::isnan(rating))
                {
                    return;
                }

                if (std::isinf(rating))
                {
                    return;
                }

                if (rating <= 0)
                {
                    return;
                }

                this->check_segment_vector_bound(segment_vec);

                std::unordered_set<size_t> cmb_idx_set{};

                for (size_t i = 0u; i < segment_vec.size(); ++i)
                {
                    std::vector<size_t> src_idx_vec = this->segment_to_table_idx_vec(segment_vec[i]);
                    cmb_idx_set.insert(src_idx_vec.begin(), src_idx_vec.end());
                }

                std::vector<size_t> cmb_idx_vec(cmb_idx_set.begin(), cmb_idx_set.end());

                for (size_t i = 0u; i < cmb_idx_vec.size(); ++i)
                {
                    for (size_t j = i + 1; j < cmb_idx_vec.size(); ++j)
                    {
                        this->push_promotion_queue(Edge
                        {
                            .src_vtx    = cmb_idx_vec[i],
                            .dst_vtx    = cmb_idx_vec[j],
                            .score      = rating
                        });
                    }
                }
            }

            void clear()
            {
                this->promotion_queue.clear();
            }

            void apply()
            {
                std::vector<graph_optimizer::BinaryFCDEdgeInformation> optimizing_edge_vec{};
                size_t self_vtx_sz = this->translation_table.size();

                for (size_t i = 0u; i < self_vtx_sz; ++i)
                {
                    optimizing_edge_vec.push_back(graph_optimizer::BinaryFCDEdgeInformation
                    {
                        .src    = i,
                        .dst    = i,
                        .score  = 1
                    });
                }

                for (const Edge& internal_edge: this->accum_and_filter_edges(this->promotion_queue))
                {
                    optimizing_edge_vec.push_back(graph_optimizer::BinaryFCDEdgeInformation
                    {
                        .src    = internal_edge.src_vtx,
                        .dst    = internal_edge.dst_vtx,
                        .score  = internal_edge.score
                    });
                }

                std::vector<size_t> suffix_vec = graph_optimizer::BinaryFCDOptimizer().optimize(optimizing_edge_vec).vertices;
                std::vector<size_t> new_translation_table(this->translation_table.size());

                this->verify_translation_suffix_table(suffix_vec);

                for (size_t i = 0u; i < suffix_vec.size(); ++i)
                {
                    new_translation_table[i] = this->translation_table[suffix_vec[i]];
                }

                this->translation_table = std::move(new_translation_table);
                this->promotion_queue.clear();
            }

            auto map(const std::pair<size_t, size_t>& segment) -> std::vector<std::pair<size_t, size_t>>
            {
                size_t first    = segment.first;
                size_t last     = segment.first + segment.second;

                if (first == last)
                {
                    return {};
                }

                size_t offset                                           = first % this->leaf_sz;
                std::vector<size_t> table_idx_vec                       = this->segment_to_table_idx_vec(segment);
                std::vector<std::pair<size_t, size_t>> tentative_result = this->translate_table_idx_vec(table_idx_vec);

                if (tentative_result.empty())
                {
                    std::abort();
                }

                tentative_result.front() = {tentative_result.front().first + offset, this->leaf_sz - offset};

                return this->interval_sort_and_adjecent_join(this->right_trim_interval(tentative_result, segment.second));
            }

        private:

            auto right_trim_interval(const std::vector<std::pair<size_t, size_t>>& interval_vec, size_t total_sz) -> std::vector<std::pair<size_t, size_t>>
            {
                size_t sum_sz = 0u;
                std::vector<std::pair<size_t, size_t>> rs{};

                for (const auto& [first, sz]: interval_vec)
                {
                    if (sum_sz == total_sz)
                    {
                        break;
                    }

                    size_t missing_sz   = total_sz - sum_sz;
                    size_t app_sz       = std::min(missing_sz, sz);

                    rs.push_back({first, app_sz});
                    sum_sz              += app_sz;
                }

                return rs;
            }

            auto accum_and_filter_edges(const std::vector<Edge>& edge_vec) -> std::vector<Edge>
            {
                local_unordered_map<std::pair<size_t, size_t>, std::pair<std_float_t, size_t>> edge_score_map{};

                for (const auto& edge: edge_vec)
                {
                    if (std::isnan(edge.score))
                    {
                        continue;
                    }

                    if (std::isinf(edge.score))
                    {
                        continue;
                    }

                    const auto key  = std::make_pair(std::min(edge.src_vtx, edge.dst_vtx), std::max(edge.src_vtx, edge.dst_vtx));
                    auto map_ptr    = edge_score_map.find(key);

                    if (map_ptr == edge_score_map.end())
                    {
                        auto [new_ptr, status]  = edge_score_map.insert({key, {0, 0}});
                        assert(status);
                        map_ptr                 = new_ptr;
                    }

                    auto& [score, score_count]  = map_ptr->second;
                    score                       += edge.score;
                    score_count                 += 1;
                }

                std::vector<Edge> result{};

                for (const auto& edge_pair: edge_score_map)
                {
                    const auto& [src_vtx, dst_vtx]      = edge_pair.first;
                    const auto& [score, score_count]    = edge_pair.second;

                    if (score_count == 0u)
                    {
                        continue;
                    }

                    std_float_t normalized_score        = score / score_count;

                    if (std::isnan(normalized_score))
                    {
                        continue;
                    }

                    if (std::isinf(normalized_score))
                    {
                        continue;
                    }

                    result.push_back(Edge
                    {
                        .src_vtx    = src_vtx,
                        .dst_vtx    = dst_vtx,
                        .score      = normalized_score
                    });
                }

                return result;
            }

            void verify_translation_suffix_table(const std::vector<size_t>& suffix_vec)
            {
                if (suffix_vec.empty())
                {
                    if (this->translation_table.empty())
                    {
                        return;
                    }

                    throw std::runtime_error("bad suffix array, size unmatched");
                }

                size_t set_sz       = std::unordered_set<size_t>(suffix_vec.begin(), suffix_vec.end()).size();
                size_t max_value    = *std::max_element(suffix_vec.begin(), suffix_vec.end());
                size_t max_count    = max_value + 1u;

                if (max_count != set_sz)
                {
                    throw std::runtime_error("bad suffix array, not a suffix array");
                }

                if (set_sz != this->translation_table.size())
                {
                    throw std::runtime_error("incompatible suffix array, size unmatched");
                }
            }

            auto translate_table_idx(size_t idx) -> std::pair<size_t, size_t>
            {
                if (idx >= this->translation_table.size())
                {
                    throw std::invalid_argument("bad access, out of bound access");
                }

                size_t actual_idx   = this->translation_table[idx];
                size_t offset       = actual_idx * this->leaf_sz;

                return {offset, this->leaf_sz};
            }

            auto translate_table_idx_vec(const std::vector<size_t>& table_idx_vec) -> std::vector<std::pair<size_t, size_t>>
            {
                std::vector<std::pair<size_t, size_t>> rs{};

                for (size_t table_idx: table_idx_vec)
                {
                    rs.push_back(this->translate_table_idx(table_idx));
                }

                return rs;
            }

            auto interval_sort_and_adjecent_join(const std::vector<std::pair<size_t, size_t>>& arg_vec) -> std::vector<std::pair<size_t, size_t>>
            {
                auto tmp_vec    = arg_vec;
                auto result_vec = std::vector<std::pair<size_t, size_t>>();

                std::sort(tmp_vec.begin(), tmp_vec.end());

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

            void check_segment_bound(const std::pair<size_t, size_t>& segment)
            {
                size_t first    = segment.first;
                size_t last     = segment.first + segment.second;

                if (last > this->base_sz)
                {
                    throw std::invalid_argument("bad access, out of bound access");
                }
            }

            void check_segment_vector_bound(const std::vector<std::pair<size_t, size_t>>& segment_vec)
            {
                for (const auto& segment: segment_vec)
                {
                    this->check_segment_bound(segment);
                }
            }

            auto segment_to_table_idx_vec(const std::pair<size_t, size_t>& range) -> std::vector<size_t>
            {
                size_t first    = range.first;
                size_t last     = range.first + range.second;

                if (last > this->base_sz)
                {
                    throw std::invalid_argument("bad access, out of bound access");
                }

                if (first == last)
                {
                    return {};
                }

                size_t first_slot   = first / this->leaf_sz;
                size_t prev_slot    = (last - 1u) / this->leaf_sz;
                size_t last_slot    = prev_slot + 1u;
                size_t slot_sz      = last_slot - first_slot;

                std::vector<size_t> rs(slot_sz);
                std::iota(rs.begin(), rs.end(), first_slot);

                return rs;
            }

            void push_promotion_queue(const Edge& edge)
            {
                if (this->promotion_queue_cap == 0u)
                {
                    return;
                }

                if (this->promotion_queue.size() == this->promotion_queue_cap)
                {
                    this->promotion_queue.pop_back();
                }

                this->promotion_queue.push_back(edge);

                auto is_greater_eq_cmp = [](const auto& lhs, const auto& rhs)
                {
                    return lhs.score >= rhs.score;
                };

                algorithm_extension::push_heap(this->promotion_queue.begin(),
                                               this->promotion_queue.end(),
                                               is_greater_eq_cmp);
            }
    };

    class OddRangeSegmentMapper: public virtual SegmentMapperInterface
    {
        private:

            std::unique_ptr<SegmentMapper> base;
            size_t odd_first;
            size_t odd_last;

        public:

            OddRangeSegmentMapper(size_t base_sz,
                                  size_t leaf_sz,
                                  size_t promotion_queue_cap)
            {
                size_t ceil_base_sz = stdx::mul_ceil(base_sz, leaf_sz);

                if (ceil_base_sz == base_sz)
                {
                    this->base      = std::make_unique<SegmentMapper>(base_sz, leaf_sz, promotion_queue_cap);
                    this->odd_first = base_sz;
                    this->odd_last  = base_sz;
                }
                else
                {
                    size_t floor_base_sz    = ceil_base_sz - leaf_sz;
                    this->base              = std::make_unique<SegmentMapper>(floor_base_sz, leaf_sz, promotion_queue_cap);
                    this->odd_first         = floor_base_sz;
                    this->odd_last          = base_sz;
                }
            }

            void hint(const std::vector<std::pair<size_t, size_t>>& segment_vec, std_float_t rating)
            {
                if (this->odd_first == this->odd_last)
                {
                    this->base->hint(segment_vec, rating);
                    return;
                }

                std::vector<std::pair<size_t, size_t>> transformed_segment_vec{};

                for (const auto& segment: segment_vec)
                {
                    auto new_segment    = this->trim_interval(segment);
                    transformed_segment_vec.push_back(new_segment);
                }

                this->base->hint(transformed_segment_vec, rating);
            }

            void clear()
            {
                this->base->clear();
            }

            void apply()
            {
                this->base->apply();
            }

            auto map(const std::pair<size_t, size_t>& segment) -> std::vector<std::pair<size_t, size_t>>
            {
                if (this->odd_first == this->odd_last)
                {
                    return this->base->map(segment);
                }

                size_t first    = segment.first;
                size_t last     = first + segment.second;

                if (first == last)
                {
                    return {};
                }

                if (last <= this->odd_first)
                {
                    return this->base->map(segment);
                }

                if (last > this->odd_last)
                {
                    throw std::invalid_argument("bad interval, out of range access");
                }

                if (first >= this->odd_first)
                {
                    return {segment};
                }

                std::vector<std::pair<size_t, size_t>> rs   = this->base->map({first, this->odd_first - first});
                std::pair<size_t, size_t> odd_interval      = std::make_pair(this->odd_first, last - this->odd_first);

                rs.push_back(odd_interval);

                return rs;
            }
        
        private:
            
            auto trim_interval(const std::pair<size_t, size_t>& interval) -> std::pair<size_t, size_t>
            {
                size_t first            = interval.first;
                size_t last             = first + interval.second;

                size_t trimmed_first    = std::min(first, this->odd_first);
                size_t trimmed_last     = std::min(last, this->odd_first);

                return std::make_pair(trimmed_first, trimmed_last - trimmed_first);
            }
    };

    class TranslationOptimizerTree: public virtual TranslationOptimizerTreeInterface
    {
        private:
            
            std::shared_ptr<SegmentMapperInterface> segment_mapper;
            size_t base_tree_sz;

        public:

            TranslationOptimizerTree(std::shared_ptr<SegmentMapperInterface> segment_mapper,
                                     size_t base_tree_sz) noexcept: segment_mapper(std::move(segment_mapper)),
                                                                    base_tree_sz(base_tree_sz){}

            auto get_translation_tensor(const std::vector<std::pair<size_t, size_t>>& range_vec) -> std::unique_ptr<TranslationSpaceTensorInterface>
            {
                return std::make_unique<InternalTranslationSpaceTensor>(range_vec,
                                                                        this->segment_mapper);
            }

            void rearrange_focal()
            {
                this->segment_mapper->apply();
            }

            auto size() -> size_t
            {
                return this->base_tree_sz;
            }
        
        private:
            
            class InternalTranslationSpaceTensor: public virtual TranslationSpaceTensorInterface
            {
                private:

                    std::vector<std::pair<size_t, size_t>> range_vec;
                    std::shared_ptr<SegmentMapperInterface> segment_mapper;
                    bool was_feedback_received;

                public:

                    InternalTranslationSpaceTensor(std::vector<std::pair<size_t, size_t>> range_vec,
                                                   std::shared_ptr<SegmentMapperInterface> segment_mapper): range_vec(std::move(range_vec)),
                                                                                                            segment_mapper(std::move(segment_mapper)),
                                                                                                            was_feedback_received(false){}

                    auto get_translation_space() -> std::vector<std::vector<std::pair<size_t, size_t>>>
                    {
                        std::vector<std::vector<std::pair<size_t, size_t>>> rs{};

                        for (const auto& e: this->range_vec)
                        {
                            rs.push_back(this->segment_mapper->map(e));
                        }

                        return rs;
                    }

                    void feedback(std_float_t rating)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        this->segment_mapper->hint(this->range_vec, rating);
                    }
            };
    };

    class BatchCoefficientOptimizerTree : public virtual BatchCoefficientOptimizerTreeInterface
    {
        private:

            std::shared_ptr<interval_coefficient_optimizer_tree::CoefficientOptimizerTreeInterface> base_tree;
            std::shared_ptr<SegmentMapperInterface> segment_mapper;

        public:

            BatchCoefficientOptimizerTree(std::unique_ptr<interval_coefficient_optimizer_tree::CoefficientOptimizerTreeInterface> base_tree,
                                          std::unique_ptr<SegmentMapperInterface> segment_mapper) noexcept: base_tree(std::move(base_tree)),
                                                                                                            segment_mapper(std::move(segment_mapper)){}

            auto get_coefficient_span(const std::vector<std::pair<size_t, size_t>>& range_vec) -> std::unique_ptr<BatchCoefficientSpaceTensorInterface>
            {
                std::vector<std::unique_ptr<interval_coefficient_optimizer_tree::CoefficientSpaceTensorInterface>> tensor_vec = {};

                for (const auto& range: range_vec)
                {
                    tensor_vec.push_back(this->base_tree->get_coefficient_span(range));
                }

                return std::make_unique<InternalBatchCoefficientSpaceTensor>(range_vec,
                                                                             std::move(tensor_vec),
                                                                             this->segment_mapper);
            }

            auto translate(const std::pair<size_t, size_t>& interval) -> std::vector<std::pair<size_t, size_t>>
            {
                return this->segment_mapper->map(interval);
            }

            void rearrange_focal()
            {
                this->segment_mapper->apply();
                this->base_tree->clear();
            }

            auto size() -> size_t
            {
                return this->base_tree->size();
            }

        private:

            class InternalBatchCoefficientSpaceTensor: public virtual BatchCoefficientSpaceTensorInterface
            {
                private:

                    std::vector<std::pair<size_t, size_t>> range_vec;
                    std::vector<std::unique_ptr<interval_coefficient_optimizer_tree::CoefficientSpaceTensorInterface>> tensor_vec;
                    std::shared_ptr<SegmentMapperInterface> segment_mapper;
                    bool was_feedback_received;

                public:

                    InternalBatchCoefficientSpaceTensor(std::vector<std::pair<size_t, size_t>> range_vec,
                                                        std::vector<std::unique_ptr<interval_coefficient_optimizer_tree::CoefficientSpaceTensorInterface>> tensor_vec,
                                                        std::shared_ptr<SegmentMapperInterface> segment_mapper) noexcept: range_vec(std::move(range_vec)),
                                                                                                                          tensor_vec(std::move(tensor_vec)),
                                                                                                                          segment_mapper(std::move(segment_mapper)),
                                                                                                                          was_feedback_received(false){}

                    auto get_coefficient_space() -> std::vector<std::vector<std_float_t>>
                    {
                        std::vector<std::vector<std_float_t>> rs{};

                        for (const auto& e: this->tensor_vec)
                        {
                            if (e == nullptr)
                            {
                                std::abort();
                            }

                            std::vector<std_float_t> tmp = e->get_coefficient_space();
                            rs.push_back(tmp);
                        }

                        return rs;
                    }

                    void feedback(std_float_t rating)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        for (const auto& e: this->tensor_vec)
                        {
                            if (e == nullptr)
                            {
                                std::abort();
                            }

                            e->feedback(rating);
                        }

                        if (this->segment_mapper == nullptr)
                        {
                            std::abort();
                        }

                        this->segment_mapper->hint(this->range_vec, rating);
                    }
            };
    };

    class TreeFactory
    {
        public:

            static auto get_mid_duty_dynamic_focal_tree(size_t space_sz,
                                                        size_t leaf_sz = 64u) -> std::unique_ptr<BatchCoefficientOptimizerTreeInterface>
            {
                using namespace interval_coefficient_optimizer_tree;

                const size_t MULTIPLIER             = leaf_sz;
                const size_t PROMOTION_QUEUE_CAP    = size_t{1} << 8;

                if (MULTIPLIER == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                if (space_sz % MULTIPLIER != 0u)
                {
                    throw std::invalid_argument("bad space size, space size is not a multiplication of leaf size");
                }

                size_t upceil_space_sz              = space_sz;

                std::unique_ptr<SegmentMapperInterface> segment_mapper          = std::make_unique<SegmentMapper>(upceil_space_sz,
                                                                                                                  MULTIPLIER,
                                                                                                                  PROMOTION_QUEUE_CAP);

                std::unique_ptr<CoefficientOptimizerTreeInterface> optimizer    = std::make_unique<ExternalCoefficientOptimizerTree>(space_sz, MULTIPLIER);

                return std::make_unique<BatchCoefficientOptimizerTree>(std::move(optimizer),
                                                                       std::move(segment_mapper));
            }

            static auto get_translation_focal_tree(size_t space_sz,
                                                   size_t leaf_sz = 64u) -> std::unique_ptr<TranslationOptimizerTreeInterface>
            {
                const size_t MULTIPLIER             = leaf_sz;
                const size_t PROMOTION_QUEUE_CAP    = size_t{1} << 8;

                if (MULTIPLIER == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                return std::make_unique<TranslationOptimizerTree>(std::make_unique<OddRangeSegmentMapper>(space_sz,
                                                                                                          MULTIPLIER,
                                                                                                          PROMOTION_QUEUE_CAP),
                                                                  space_sz);
            }
    };
}

#endif