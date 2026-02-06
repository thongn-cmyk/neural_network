#ifndef __ACTIVATION_H__
#define __ACTIVATION_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include "conventional_randomizer.h"
#include <vector>

namespace activation
{
    using activation_codex_t  = uint8_t;

    static inline constexpr activation_codex_t ADJECENT_CODEX                       = 0u;
    static inline constexpr activation_codex_t EXPONENTIAL_ADJECENT_CODEX           = 1u;
    static inline constexpr activation_codex_t UNIFORM_CODEX                        = 2u;
    static inline constexpr activation_codex_t EXPONENTIAL_CODEX                    = 3u;
    static inline constexpr activation_codex_t BACKWARD_ADJECENT_CODEX              = 4u;
    static inline constexpr activation_codex_t BACKWARD_EXPONENTIAL_ADJECENT_CODEX  = 5u;

    static inline constexpr size_t ACTIVATION_CODEX_RANGE                           = 6u;

    class IndexTranslationTree
    {   
        private:

            std::vector<size_t> interval_tree;
            size_t cur_range;

        public:

            IndexTranslationTree(size_t range)
            {
                size_t pow2_range       = this->pow2_ceil(range);
                size_t interval_tree_sz = this->get_interval_tree_size_from_base_size(pow2_range);
                this->interval_tree     = std::vector<size_t>(interval_tree_sz);

                this->inplace_make_interval_tree(this->interval_tree.data(), pow2_range);
                this->trail_block_tree(this->interval_tree.data(), pow2_range, pow2_range - range);

                this->cur_range         = range;
            }

            auto get_index(size_t idx) -> size_t
            {
                if (idx >= this->cur_range)
                {
                    throw std::invalid_argument("index out of range"); //according to the language, it is invalid argument, not out of range, because it is in validation, and must be checked at the callee's end to make sure the proram is run properly, runtime_error is the unexpected, in range parameters that throws errors
                }

                return this->tree_find(this->interval_tree.data(),
                                       this->get_base_size_from_interval_tree_size(this->interval_tree.size()),
                                       idx);
            }

            void remove_index(size_t idx)
            {
                if (idx >= this->cur_range)
                {
                    throw std::invalid_argument("index out of range");
                }

                this->tree_remove(this->interval_tree.data(),
                                  this->get_base_size_from_interval_tree_size(this->interval_tree.size()),
                                  idx);

                this->cur_range -= 1;
            }

            auto size() -> size_t
            {
                return this->cur_range;
            }

        private:

            auto is_pow2(size_t val) -> bool
            {
                if (val == 0u)
                {
                    return false;
                }

                size_t val_1 = val - 1u;

                return (val & val_1) == 0u;
            }

            auto pow2_ceil(size_t range) -> size_t
            {
                for (size_t i = 0u; i < std::numeric_limits<size_t>::digits; ++i)
                {
                    size_t cand = size_t{1} << i;

                    if (cand >= range)
                    {
                        return cand;
                    }
                }

                throw std::invalid_argument("bad pow2_ceil range, numerical upper limits reached");
            }

            auto get_interval_tree_size_from_base_size(size_t base_sz) -> size_t
            {
                if (!this->is_pow2(base_sz))
                {
                    throw std::invalid_argument("bad base size, non pow2 base size");
                }

                size_t base_sz_threshold = size_t{1} << (std::numeric_limits<size_t>::digits - 1u);

                if (base_sz >= base_sz_threshold)
                {
                    throw std::invalid_argument("bad base size, numerical upper limits reached");
                }

                return base_sz * 2 - 1;
            }

            auto get_base_size_from_interval_tree_size(size_t interval_tree_sz) -> size_t
            {
                if (interval_tree_sz == 0u)
                {
                    throw std::invalid_argument("bad interval tree size, 0");
                }

                if (!this->is_pow2(interval_tree_sz + 1u))
                {
                    throw std::invalid_argument("bad interval tree size, non pow2 heap");
                }

                return (interval_tree_sz + 1) / 2;
            }

            auto inplace_make_interval_tree_helper(size_t * tree_arr,
                                                   size_t interval_first, size_t interval_last,
                                                   size_t root_idx) -> size_t
            {
                if (interval_first + 1 == interval_last)
                {
                    tree_arr[root_idx] = 1u;
                    return 1u;
                }

                size_t range_sz     = interval_last - interval_first;
                size_t mid_sz       = range_sz / 2;
                size_t nxt_first    = interval_first + mid_sz;

                tree_arr[root_idx]  = this->inplace_make_interval_tree_helper(tree_arr, interval_first, nxt_first, root_idx * 2 + 1)
                                        + this->inplace_make_interval_tree_helper(tree_arr, nxt_first, interval_last, root_idx * 2 + 2);

                return tree_arr[root_idx];
            }

            void inplace_make_interval_tree(size_t * tree_arr, size_t base_sz)
            {
                size_t first    = 0u;
                size_t last     = base_sz;
                size_t root_idx = 0u;

                this->inplace_make_interval_tree_helper(tree_arr, first, last, root_idx);
            }

            auto tree_find_helper(size_t * tree_arr,
                                  size_t interval_first, size_t interval_last,
                                  size_t root_idx,
                                  size_t rel_finding_idx) -> size_t
            {
                if (rel_finding_idx >= tree_arr[root_idx])
                {
                    throw std::runtime_error("out of range access");
                }

                if (interval_first + 1 == interval_last)
                {
                    return interval_first;
                }

                size_t range_sz     = interval_last - interval_first;
                size_t mid_sz       = range_sz / 2;
                size_t nxt_first    = interval_first + mid_sz;

                if (tree_arr[root_idx * 2 + 1] > rel_finding_idx)
                {
                    return this->tree_find_helper(tree_arr, interval_first, nxt_first, root_idx * 2 + 1, rel_finding_idx);
                }

                return this->tree_find_helper(tree_arr, nxt_first, interval_last, root_idx * 2 + 2, rel_finding_idx - tree_arr[root_idx * 2 + 1]);
            }

            auto tree_find(size_t * tree_arr, size_t base_sz, size_t idx) -> size_t
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                size_t first    = 0u;
                size_t last     = base_sz;
                size_t root_idx = 0u;

                return this->tree_find_helper(tree_arr, first, last, root_idx, idx);
            }

            auto tree_remove_helper(size_t * tree_arr,
                                    size_t interval_first, size_t interval_last,
                                    size_t root_idx,
                                    size_t rel_finding_idx) -> size_t
            {
                if (rel_finding_idx >= tree_arr[root_idx])
                {
                    throw std::runtime_error("out of range access");
                }

                if (interval_first + 1 == interval_last)
                {
                    tree_arr[root_idx] = 0u;
                    return 0u;
                }

                size_t range_sz     = interval_last - interval_first;
                size_t mid_sz       = range_sz / 2;
                size_t nxt_first    = interval_first + mid_sz;

                if (tree_arr[root_idx * 2 + 1] > rel_finding_idx)
                {
                    tree_arr[root_idx]  = this->tree_remove_helper(tree_arr, interval_first, nxt_first, root_idx * 2 + 1, rel_finding_idx)
                                            + tree_arr[root_idx * 2 + 2];
                }
                else
                {
                    tree_arr[root_idx]  = this->tree_remove_helper(tree_arr, nxt_first, interval_last, root_idx * 2 + 2, rel_finding_idx - tree_arr[root_idx * 2 + 1])
                                            + tree_arr[root_idx * 2 + 1];
                }

                return tree_arr[root_idx];
            }

            void tree_remove(size_t * tree_arr, size_t base_sz, size_t idx)
            {
                if (base_sz == 0u)
                {
                    throw std::runtime_error("internal corruption");
                }

                size_t first    = 0u;
                size_t last     = base_sz;
                size_t root_idx = 0u;

                this->tree_remove_helper(tree_arr, first, last, root_idx, idx);
            }

            auto tree_block_helper(size_t * tree_arr,
                                   size_t tree_interval_first, size_t tree_interval_last,
                                   size_t block_interval_first, size_t block_interval_last,
                                   size_t root_idx) -> size_t
            {
                if (tree_interval_first == block_interval_first && tree_interval_last == block_interval_last)
                {
                    tree_arr[root_idx] = 0u;
                    return 0u;
                }

                size_t range_sz     = tree_interval_last - tree_interval_first;
                size_t mid_sz       = range_sz / 2;
                size_t nxt_first    = tree_interval_first + mid_sz;

                if (block_interval_last <= nxt_first)
                {
                    tree_arr[root_idx] = this->tree_block_helper(tree_arr, tree_interval_first, nxt_first, block_interval_first, block_interval_last, root_idx * 2 + 1)
                                         + tree_arr[root_idx * 2 + 2];
                }
                else if (block_interval_first >= nxt_first)
                {
                    tree_arr[root_idx] = this->tree_block_helper(tree_arr, nxt_first, tree_interval_last, block_interval_first, block_interval_last, root_idx * 2 + 2)
                                         + tree_arr[root_idx * 2 + 1];
                }
                else
                {
                    tree_arr[root_idx] = this->tree_block_helper(tree_arr, tree_interval_first, nxt_first, block_interval_first, nxt_first, root_idx * 2 + 1)
                                         + this->tree_block_helper(tree_arr, nxt_first, tree_interval_last, nxt_first, block_interval_last, root_idx * 2 + 2);
                }

                return tree_arr[root_idx];
            }

            void trail_block_tree(size_t * tree_arr, size_t base_sz, size_t trail_sz)
            {
                if (trail_sz == 0u)
                {
                    return;
                }

                size_t tree_first   = 0u;
                size_t tree_last    = base_sz;
                size_t block_first  = tree_last - trail_sz;
                size_t block_last   = tree_last;
                size_t root_idx     = 0u;

                this->tree_block_helper(tree_arr,
                                        tree_first, tree_last,
                                        block_first, block_last,
                                        root_idx);
            }
    };

    template <class T, class ...Args, class ...Args1>
    auto activate(const std::vector<T, Args...>& vec,
                  const std::vector<activation_codex_t, Args1...>& activation_codex_vec) -> std::vector<T, Args...>
    {
        if (activation_codex_vec.size() > vec.size())
        {
            throw std::invalid_argument("bad activation codex vec size, value exceeded the max threhsold of vec size");
        }

        std::optional<size_t> last_idx      = std::nullopt;
        IndexTranslationTree idx_mapper(vec.size());
        std::vector<T, Args...> result_vec  = {};

        auto numeric_randomizer             = conventional_randomizer::RandomizerObject();
        auto focal_randomizer               = conventional_randomizer::ApplicationRandomizerObject();

        for (activation_codex_t codex: activation_codex_vec)
        {
            if (idx_mapper.size() == 0u)
            {
                std::abort();
            }

            if (last_idx.has_value())
            {
                if (last_idx.value() >= idx_mapper.size())
                {
                    last_idx = std::nullopt;
                }
            }

            switch (codex)
            {
                case ADJECENT_CODEX:
                {
                    size_t cand;

                    if (!last_idx.has_value())
                    {
                        cand = 0u;
                    }
                    else
                    {
                        cand = last_idx.value();
                    }

                    size_t actual_idx = idx_mapper.get_index(cand);
                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(cand);

                    last_idx = cand;
                    break;
                }
                case EXPONENTIAL_ADJECENT_CODEX:
                {
                    size_t cand;

                    if (!last_idx.has_value())
                    {
                        cand = 0u;
                    }
                    else
                    {
                        size_t rem_sz           = idx_mapper.size() - last_idx.value() - 1u;
                        size_t tentative_offset = rem_sz * focal_randomizer.randomize_percentage_focal();
                        size_t actual_offset    = std::clamp(tentative_offset, size_t{0u}, rem_sz);
                        cand                    = last_idx.value() + actual_offset;
                    }

                    size_t actual_idx = idx_mapper.get_index(cand);
                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(cand);

                    last_idx = cand;
                    break;
                }
                case UNIFORM_CODEX:
                {
                    size_t cand         = numeric_randomizer.randomize_uint(0u, idx_mapper.size());
                    size_t actual_idx   = idx_mapper.get_index(cand);

                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(cand);

                    last_idx            = cand;
                    break;
                }
                case EXPONENTIAL_CODEX:
                {
                    size_t tentative_cand   = focal_randomizer.ld_randomize_percentage_focal() * (idx_mapper.size() - 1u);
                    size_t actual_cand      = std::clamp(tentative_cand, static_cast<size_t>(0u), static_cast<size_t>(idx_mapper.size() - 1u));
                    size_t actual_idx       = idx_mapper.get_index(actual_cand);

                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(actual_cand);

                    last_idx                = actual_cand;
                    break;
                }
                case BACKWARD_ADJECENT_CODEX:
                {
                    size_t cand;

                    if (!last_idx.has_value())
                    {
                        cand = 0u;
                    }
                    else
                    {
                        if (last_idx.value() == 0u)
                        {
                            cand = 0u;
                        }
                        else
                        {
                            cand = last_idx.value() - 1u;
                        }
                    }

                    size_t actual_idx = idx_mapper.get_index(cand);
                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(cand);

                    last_idx = cand;
                    break;
                }
                case BACKWARD_EXPONENTIAL_ADJECENT_CODEX:
                {
                    size_t cand;

                    if (!last_idx.has_value())
                    {
                        cand = 0u;
                    }
                    else
                    {
                        size_t rem_sz           = last_idx.value();
                        size_t tentative_offset = rem_sz * focal_randomizer.randomize_percentage_focal();
                        size_t actual_offset    = std::clamp(tentative_offset, size_t{0u}, rem_sz);
                        cand                    = last_idx.value() - actual_offset;
                    }

                    size_t actual_idx = idx_mapper.get_index(cand);
                    result_vec.push_back(vec[actual_idx]);
                    idx_mapper.remove_index(cand);

                    last_idx = cand;
                    break;
                }
                default:
                {
                    throw std::invalid_argument("illegal activation codex");
                }
            }
        }

        return result_vec;
    }
}

#endif