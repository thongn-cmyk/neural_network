#ifndef __INTERVAL_COEFFICIENT_OPTIMIZER_TREE_H__
#define __INTERVAL_COEFFICIENT_OPTIMIZER_TREE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include "coefficient_randomizer.h"
#include "cosine_recommender_machine_x.h"

namespace interval_coefficient_optimizer_tree
{
    using std_float_t = float_def::std_float_t;

    class CoefficientSpaceTensorInterface
    {
        public:

            virtual ~CoefficientSpaceTensorInterface() = default;
            virtual auto get_coefficient_space() -> std::vector<std_float_t> = 0;
            virtual void feedback(std_float_t rating) = 0;
    };

    class CoefficientOptimizerTreeInterface
    {
        public:

            virtual ~CoefficientOptimizerTreeInterface() = default;
            virtual auto get_coefficient_span(const std::pair<size_t, size_t>& range) -> std::unique_ptr<CoefficientSpaceTensorInterface> = 0;
            virtual void set_range(size_t range_sz) = 0;
            virtual auto size() -> size_t = 0;
            virtual void clear() = 0;
    };

    class IntervalConverter
    {
        private:

            size_t leaf_sz;

        public:

            IntervalConverter(size_t leaf_sz)
            {
                if (leaf_sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                this->leaf_sz = leaf_sz;
            }

            auto widen(const std::pair<size_t, size_t>& interval) -> std::pair<size_t, size_t>
            {
                size_t first        = interval.first;
                size_t last         = interval.first + interval.second;

                size_t first_idx    = first * this->leaf_sz;
                size_t last_idx     = last * this->leaf_sz;

                return {first_idx, last_idx - first_idx};
            }

            auto shorten(const std::pair<size_t, size_t>& interval) -> std::pair<size_t, size_t>
            {
                size_t first        = interval.first;
                size_t last         = interval.first + interval.second;
                size_t first_slot   = first / this->leaf_sz;

                if (first == last)
                {
                    return {first_slot, 0u};
                }

                size_t prev_slot    = (last - 1) / this->leaf_sz;
                size_t new_sz       = prev_slot - first_slot + 1;

                return {first_slot, new_sz};
            }

            auto get_shorten_offset(const std::pair<size_t, size_t>& interval) -> size_t
            {
                return interval.first - this->widen(this->shorten(interval)).first;
            }
    };

    class CompleteCoefficientOptimizerTree
    {
        private:

            struct TreeNode
            {
                std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface> recommender_machine;
            };

            struct TensorNode
            {
                std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface> result;
            };

            std::vector<TreeNode> tree_node_vec;
            size_t tree_leaf_sz;
            size_t tree_virtual_base_sz;

            static inline constexpr size_t RANDOMIZATION_CHANCE = 100u;

        public:

            CompleteCoefficientOptimizerTree(size_t range_sz, size_t leaf_sz)
            {
                if (leaf_sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                size_t ceil2_sz             = stdx::ceil2(range_sz);
                size_t tree_sz              = this->base_size_to_tree_size(ceil2_sz);

                this->tree_node_vec         = std::vector<TreeNode>(tree_sz);
                this->tree_leaf_sz          = leaf_sz;
                this->tree_virtual_base_sz  = range_sz;

                this->make_tree(this->tree_node_vec.data(), ceil2_sz, leaf_sz);
            }

            auto get_coefficient_span(const std::pair<size_t, size_t>& range) -> std::unique_ptr<CoefficientSpaceTensorInterface>
            {
                size_t first    = range.first;
                size_t last     = range.first + range.second;

                if (first >= last)
                {
                    throw std::invalid_argument("bad range, <= 0");
                }

                if (last > this->tree_virtual_base_sz)
                {
                    throw std::invalid_argument("bad range, out of bound access");
                }

                return std::make_unique<CoefficientSpaceTensor>(this->get_tensor_node(this->tree_node_vec.data(),
                                                                                      this->tree_size_to_base_size(this->tree_node_vec.size()),
                                                                                      first, last),
                                                                false);
            }

            auto size() const noexcept -> size_t
            {
                return this->tree_virtual_base_sz;
            }

            auto leaf_size() const noexcept -> size_t
            {
                return this->tree_leaf_sz;
            }

        private:

            auto base_size_to_tree_size(size_t base_sz) -> size_t
            {
                return base_sz * 2 - 1;
            }

            auto tree_size_to_base_size(size_t tree_sz) -> size_t
            {
                return (tree_sz + 1) / 2;
            }

            void make_tree_helper(TreeNode * tree_node_arr,
                                  size_t idx,
                                  size_t first, size_t last,
                                  size_t leaf_sz)
            {
                size_t interval_sz  = last - first;
                size_t space_sz     = interval_sz * leaf_sz;
                tree_node_arr[idx]  = TreeNode
                {
                    .recommender_machine = cosine_recommender_machine_x::MachineFactory::get_best_recommender_machine(space_sz)
                };

                if (first + 1 == last)
                {
                    return;
                }

                size_t mid_sz       = interval_sz / 2;
                size_t next_first   = first + mid_sz;

                this->make_tree_helper(tree_node_arr,
                                       idx * 2 + 1,
                                       first, next_first,
                                       leaf_sz);
                
                this->make_tree_helper(tree_node_arr,
                                       idx * 2 + 2,
                                       next_first, last,
                                       leaf_sz);
            }

            void make_tree(TreeNode * tree_node_arr, size_t tree_node_arr_sz, size_t leaf_sz)
            {
                if (!stdx::is_pow2(tree_node_arr_sz))
                {
                    throw std::invalid_argument("bad tree_base_sz, not base 2");
                }

                size_t idx      = 0u;
                size_t first    = 0u;
                size_t last     = tree_node_arr_sz;

                this->make_tree_helper(tree_node_arr,
                                       idx,
                                       first, last,
                                       leaf_sz);
            }

            class CoefficientSpaceTensor: public virtual CoefficientSpaceTensorInterface
            {
                private:

                    std::vector<TensorNode> tensor_node_vec;
                    bool was_feedback_received;

                public:

                    CoefficientSpaceTensor(std::vector<TensorNode> tensor_node_vec,
                                           bool was_feedback_received) noexcept: tensor_node_vec(std::move(tensor_node_vec)),
                                                                                 was_feedback_received(was_feedback_received){}

                    auto get_coefficient_space() -> std::vector<std_float_t>
                    {
                        std::vector<std_float_t> result{};

                        for (const auto& tensor_node: this->tensor_node_vec)
                        {
                            if (tensor_node.result == nullptr)
                            {
                                std::abort();
                            }

                            std::vector<std_float_t> app_vec = stdx::to_castable_vector_initializer(tensor_node.result->get());
                            std::copy(app_vec.begin(), app_vec.end(), std::back_inserter(result));
                        }

                        return result;
                    }

                    void feedback(std_float_t rating)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        for (const auto& tensor_node: this->tensor_node_vec)
                        {
                            if (tensor_node.result == nullptr)
                            {
                                std::abort();
                            }

                            tensor_node.result->feedback(rating);
                        }
                    }
            };

            void get_tensor_node_helper(TreeNode * tree_node_arr,
                                        size_t idx,
                                        size_t key_interval_first, size_t key_interval_last,
                                        size_t tree_interval_first, size_t tree_interval_last,
                                        std::vector<TensorNode>& result)
            {
                if (key_interval_first == tree_interval_first && key_interval_last == tree_interval_last)
                {
                    result.push_back(TensorNode{.result = tree_node_arr[idx].recommender_machine->next()});
                    return;
                }

                size_t tree_interval_sz         = tree_interval_last - tree_interval_first;
                size_t next_tree_interval_first = tree_interval_first + tree_interval_sz / 2;

                if (key_interval_last <= next_tree_interval_first)
                {
                    this->get_tensor_node_helper(tree_node_arr, idx * 2 + 1, key_interval_first, key_interval_last, tree_interval_first, next_tree_interval_first, result);
                    return;
                }

                if (key_interval_first >= next_tree_interval_first)
                {
                    this->get_tensor_node_helper(tree_node_arr, idx * 2 + 2, key_interval_first, key_interval_last, next_tree_interval_first, tree_interval_last, result);
                    return;
                }

                this->get_tensor_node_helper(tree_node_arr, idx * 2 + 1, key_interval_first, next_tree_interval_first, tree_interval_first, next_tree_interval_first, result);
                this->get_tensor_node_helper(tree_node_arr, idx * 2 + 2, next_tree_interval_first, key_interval_last, next_tree_interval_first, tree_interval_last, result);
            }

            auto get_tensor_node(TreeNode * tree_node_arr,
                                 size_t tree_base_sz,
                                 size_t key_interval_first, size_t key_interval_last) -> std::vector<TensorNode>
            {
                if (!stdx::is_pow2(tree_base_sz))
                {
                    throw std::invalid_argument("bad tree_base_sz, not base 2");
                }

                if (key_interval_first >= key_interval_last)
                {
                    throw std::invalid_argument("bad interval, size <= 0");
                }

                if (key_interval_last > tree_base_sz)
                {
                    throw std::invalid_argument("bad interval, out of bound");
                }

                size_t idx                  = 0u;
                size_t tree_interval_first  = 0u;
                size_t tree_interval_last   = tree_base_sz;

                std::vector<TensorNode> result{};

                this->get_tensor_node_helper(tree_node_arr,
                                             idx,
                                             key_interval_first, key_interval_last,
                                             tree_interval_first, tree_interval_last,
                                             result);

                return result;
            }
    };

    class CoefficientOptimizerTree
    {
        private:

            CompleteCoefficientOptimizerTree base;
            IntervalConverter interval_converter;
            size_t base_sz;

        public:

            using self = CoefficientOptimizerTree;

            CoefficientOptimizerTree(size_t range_sz, size_t leaf_sz): base(self::get_base_range_size(range_sz, leaf_sz), leaf_sz),
                                                                       interval_converter(leaf_sz),
                                                                       base_sz(range_sz){}

            auto get_coefficient_span(const std::pair<size_t, size_t>& range) -> std::unique_ptr<CoefficientSpaceTensorInterface>
            {
                size_t first    = range.first;
                size_t last     = range.first + range.second;

                if (first == last)
                {
                    throw std::invalid_argument("bad range, <= 0");
                }

                if (last > base_sz)
                {
                    throw std::invalid_argument("bad range, out of bound access");
                }

                return std::make_unique<InternalCoefficientSpaceTensor>(this->base.get_coefficient_span(this->interval_converter.shorten(range)),
                                                                        this->interval_converter.get_shorten_offset(range),
                                                                        range.second);
            }

            auto size() -> size_t
            {
                return this->base_sz;
            }
        
        private:

            static auto get_base_range_size(size_t range_sz, size_t leaf_sz) -> size_t
            {
                if (leaf_sz == 0u)
                {
                    throw std::invalid_argument("bad leaf size, 0");
                }

                return range_sz / leaf_sz + size_t{range_sz % leaf_sz != 0u};
            }

            class InternalCoefficientSpaceTensor: public virtual CoefficientSpaceTensorInterface
            {
                private:

                    std::unique_ptr<CoefficientSpaceTensorInterface> base;
                    size_t offset;
                    size_t sz;
                
                public:

                    InternalCoefficientSpaceTensor(std::unique_ptr<CoefficientSpaceTensorInterface> base,
                                                   size_t offset,
                                                   size_t sz) noexcept: base(std::move(base)),
                                                                        offset(offset),
                                                                        sz(sz){}

                    auto get_coefficient_space() -> std::vector<std_float_t>
                    {
                        if (this->base == nullptr)
                        {
                            std::abort();
                        }

                        auto base_vec   = this->base->get_coefficient_space();
                        size_t first    = this->offset;
                        size_t last     = this->offset + this->sz;

                        if (base_vec.size() < last)
                        {
                            std::abort();
                        }

                        return std::vector<std_float_t>(std::next(base_vec.begin(), first),
                                                        std::next(base_vec.begin(), last));
                    }

                    void feedback(std_float_t rating)
                    {
                        if (this->base == nullptr)
                        {
                            std::abort();
                        }

                        this->base->feedback(rating);
                    }
            };
    };

    class ExternalCoefficientOptimizerTree: public virtual CoefficientOptimizerTreeInterface
    {
        private:

            size_t range_sz;
            size_t leaf_sz;

            std::unique_ptr<CoefficientOptimizerTree> base;

        public:

            ExternalCoefficientOptimizerTree(size_t range_sz, size_t leaf_sz)
            {
                this->range_sz  = range_sz;
                this->leaf_sz   = leaf_sz;
                this->base      = std::make_unique<CoefficientOptimizerTree>(this->range_sz, this->leaf_sz);
            }

            auto get_coefficient_span(const std::pair<size_t, size_t>& range) -> std::unique_ptr<CoefficientSpaceTensorInterface>
            {
                return this->base->get_coefficient_span(range);
            }

            void set_range(size_t range_sz)
            {
                this->base      = std::make_unique<CoefficientOptimizerTree>(range_sz, this->leaf_sz);
                this->range_sz  = range_sz;
            }

            auto size() -> size_t
            {
                return this->base->size();
            }

            void clear()
            {
                this->base      = std::make_unique<CoefficientOptimizerTree>(this->range_sz, this->leaf_sz);
            }
    };
}

#endif