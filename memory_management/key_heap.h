#ifndef __MEMORY_MANAGEMENT_BIDIRECITONAL_HEAP_H__
#define __MEMORY_MANAGEMENT_BIDIRECITONAL_HEAP_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <stl_extension/unordered_node_map.h>
#include <stl_extension/hasher.h>
#include <memory>
#include <type_traits>
#include <numeric>
#include <utility>

namespace immutable_multiplatform_memory_x::key_heap
{
    template <class Value, class IsBetterComparator, class ValueKeyExtractor, class OutDegree = std::integral_constant<size_t, 8u>>
    class KeyHeap
    {
        public:

            using key_t                 = decltype(std::declval<const ValueKeyExtractor&>()(std::declval<const Value&>()));
            using local_unordered_map   = unordered_map_variants::unordered_node_map<key_t, size_t *, size_t, std::integral_constant<bool, true>, hasher::default_hasher<key_t>>;

        private:

            struct HeapNode
            {
                Value value;
                std::unique_ptr<size_t> heap_idx;
            };

            std::vector<HeapNode> heap_vec;
            local_unordered_map reverse_lookup_map;

            IsBetterComparator comparator;
            ValueKeyExtractor key_extractor;

        public:

            using value_type                                = Value;
            using comparator_type                           = IsBetterComparator;
            using value_key_extractor_type                  = ValueKeyExtractor;

            static inline constexpr size_t OUT_DEGREE_SZ    = OutDegree{};

            template <class Tmp = IsBetterComparator,
                      class Tmp1 = ValueKeyExtractor,
                      std::enable_if_t<std::conjunction_v<std::is_default_constructible<Tmp>, std::is_default_constructible<Tmp1>>, bool> = true>
            KeyHeap(): heap_vec(),
                       reverse_lookup_map(),
                       comparator(),
                       key_extractor(){}

            template <class IsBetterComparatorLike, class ValueKeyExtractorLike>
            KeyHeap(IsBetterComparatorLike&& is_better_comparator,
                    ValueKeyExtractorLike&& value_key_extractor): heap_vec(),
                                                                  reverse_lookup_map(),
                                                                  comparator(std::forward<IsBetterComparatorLike>(is_better_comparator)),
                                                                  key_extractor(std::forward<ValueKeyExtractorLike>(value_key_extractor)){}

            template <class ValueLike>
            void push(ValueLike&& value)
            {
                key_t key       = this->key_extractor(value);
                auto map_ptr    = this->reverse_lookup_map.find(key);

                if (map_ptr != this->reverse_lookup_map.end())
                {
                    size_t heap_idx                 = *map_ptr->second;
                    this->heap_vec[heap_idx].value  = std::forward<ValueLike>(value);

                    this->correct_at(heap_idx);
                    return;
                }

                std::unique_ptr<size_t> heap_idx    = std::make_unique<size_t>(this->heap_vec.size()); //
                HeapNode heap_node                  = HeapNode
                {
                    .value      = std::forward<ValueLike>(value),
                    .heap_idx   = std::move(heap_idx)
                };

                try
                {
                    this->heap_vec.push_back(std::move(heap_node));
                    auto [_, status] = this->reverse_lookup_map.insert(std::make_pair(key, this->heap_vec.back().heap_idx.get()));

                    if (!status)
                    {
                        std::abort();
                    }

                    this->push_up_at(this->heap_vec.size() - 1u);
                }
                catch (...)
                {
                    std::abort();
                }
            }

            auto get_by_key(const key_t& key) const noexcept -> const Value *
            {
                auto map_ptr    = this->reverse_lookup_map.find(key);

                if (map_ptr == this->reverse_lookup_map.end())
                {
                    return nullptr;
                }

                size_t heap_idx = *map_ptr->second;

                return &this->heap_vec[heap_idx].value;
            }

            template <class Mutator>
            void update_by_key(const key_t& key, Mutator&& mutator_func)
            {
                auto map_ptr    = this->reverse_lookup_map.find(key);

                if (map_ptr == this->reverse_lookup_map.end())
                {
                    throw std::invalid_argument("bad key, key not found");
                }

                size_t heap_idx = *map_ptr->second;

                static_assert(noexcept(mutator_func(this->heap_vec[heap_idx].value)));
                mutator_func(this->heap_vec[heap_idx].value);

                this->correct_at(heap_idx);
            }

            void pop() noexcept
            {
                if (this->heap_vec.empty())
                {
                    std::abort();
                }

                this->swap_node(this->heap_vec.front(), this->heap_vec.back());
                key_t key = this->key_extractor(this->heap_vec.back().value);
                this->reverse_lookup_map.erase(key);
                this->heap_vec.pop_back();

                if (!this->heap_vec.empty())
                {
                    this->push_down_at(0u);
                }
            }

            auto peek() const noexcept -> const Value&
            {
                if (this->heap_vec.empty())
                {
                    std::abort();
                }

                return this->heap_vec.front().value;
            }

            void erase_by_key(const key_t& key) noexcept
            {
                auto map_ptr    = this->reverse_lookup_map.find(key);

                if (map_ptr == this->reverse_lookup_map.end())
                {
                    return;
                }

                size_t heap_idx = *map_ptr->second;

                this->swap_node(this->heap_vec[heap_idx], this->heap_vec.back());
                this->reverse_lookup_map.erase(key);
                this->heap_vec.pop_back();

                if (heap_idx < this->heap_vec.size())
                {
                    this->correct_at(heap_idx);
                }
            }
        
            auto size() const noexcept -> size_t
            {
                return this->heap_vec.size();
            }

            auto empty() const noexcept -> bool
            {
                return this->heap_vec.empty();
            }

        private:

            void swap_node(HeapNode& lhs, HeapNode& rhs) noexcept
            {
                std::swap(lhs, rhs);
                std::swap(*lhs.heap_idx, *rhs.heap_idx);
            }

            void push_up_at(size_t idx) noexcept
            {
                if (idx >= this->heap_vec.size())
                {
                    std::abort();
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = idx / OUT_DEGREE_SZ;

                if (this->comparator(this->heap_vec[parent_idx].value, this->heap_vec[idx].value))
                {
                    return;
                }

                this->swap_node(this->heap_vec[parent_idx], this->heap_vec[idx]);
                this->push_up_at(parent_idx);
            }

            void push_down_at(size_t idx) noexcept
            {
                if (idx >= this->heap_vec.size())
                {
                    std::abort();
                }

                std::optional<size_t> cand = std::nullopt;

                for (size_t i = 0u; i < OUT_DEGREE_SZ; ++i)
                {
                    size_t child_idx = idx * OUT_DEGREE_SZ + (i + 1);

                    if (child_idx >= this->heap_vec.size())
                    {
                        break;
                    }

                    if (!cand.has_value())
                    {
                        cand = child_idx;
                        continue;
                    }

                    if (this->comparator(this->heap_vec[child_idx].value, this->heap_vec[cand.value()].value))
                    {
                        cand = child_idx;
                    }
                }

                if (!cand.has_value())
                {
                    return;
                }

                if (this->comparator(this->heap_vec[idx].value, this->heap_vec[cand.value()].value))
                {
                    return;
                }

                this->swap_node(this->heap_vec[idx], this->heap_vec[cand.value()]);
                this->push_down_at(cand.value());
            }

            void correct_at(size_t idx) noexcept
            {
                this->push_up_at(idx);
                this->push_down_at(idx);
            }
    };
}

#endif