#ifndef __NETWORK_ACK_EXPIRY_QUEUE_H__
#define __NETWORK_ACK_EXPIRY_QUEUE_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include "network_log.h"
#include "network_std_container.h"
#include "stdx.h"
#include <numeric>

namespace dg_sock::network_datastructure::expiry_queue
{
    template <class T, class StatelessIdExtractor, class ClockType = std::chrono::steady_clock>
    class temporal_ordered_item_map
    {
        public:

            using value_type    = T;
            using id_type       = decltype(std::declval<StatelessIdExtractor&>()(std::declval<const T&>()));
            using clock_type    = ClockType; 

            // static_assert();

        private:

            struct HeapNode
            {
                T item;
                std::chrono::time_point<ClockType> sched_time;
                size_t heap_idx;
            };

            dg_sock::unordered_unstable_map<id_type, HeapNode *> id_heap_map;
            dg_sock::vector<std::unique_ptr<HeapNode>> temporal_heap;
            size_t temporal_heap_sz;

        public:

            temporal_ordered_item_map(size_t cap): id_heap_map(),
                                                   temporal_heap(),
                                                   temporal_heap_sz(0u)
            {
                this->id_heap_map.reserve(cap);

                for (size_t i = 0u; i < cap; ++i)
                {
                    this->temporal_heap.push_back(std::make_unique<HeapNode>(HeapNode{}));
                }
            }

            template <class TypeLike>
            auto add(TypeLike&& item,
                     std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {   
                if (this->id_heap_map.contains(this->get_id(item)))
                {
                    return dg_sock::network_exception::DUPLICATED_ENTRY;
                }

                std::expected<HeapNode *, exception_t> reference_node = this->add_heap_node(std::forward<TypeLike>(item), expiry_time);

                if (!reference_node.has_value())
                {
                    return reference_node.error();
                }

                try
                {
                    auto [map_ptr, status] = this->id_heap_map.insert(std::make_pair(this->get_id(reference_node.value()->item), reference_node.value()));
                    dg_sock::network_exception_handler::dg_assert(status);
                }
                catch (...)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }

                return dg_sock::network_exception::SUCCESS;
            }

            auto update(id_type item_id,
                        std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {
                auto map_ptr = this->id_heap_map.find(item_id);

                if (map_ptr == this->id_heap_map.end())
                {
                    return dg_sock::network_exception::INVALID_DICTIONARY_KEY;
                }

                map_ptr->second->sched_time = expiry_time;
                this->correct_heap_node_at(map_ptr->second->heap_idx);

                return dg_sock::network_exception::SUCCESS;
            }

            template <class TypeLike>
            auto add_or_update(TypeLike&& item,
                               std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {   
                if (this->id_heap_map.contains(this->get_id(item)))
                {
                    return this->update(this->get_id(item), expiry_time);
                }

                return this->add(std::forward<TypeLike>(item), expiry_time);
            }

            void erase(id_type item_id) noexcept
            {
                auto map_ptr = this->id_heap_map.find(item_id);

                if (map_ptr == this->id_heap_map.end())
                {
                    return;
                }

                size_t idx = stdxx::safe_ptr_access(map_ptr->second)->heap_idx;
                this->id_heap_map.erase(map_ptr);
                this->erase_heap_node_at(idx);
            }

            auto get_expired_item(std::chrono::time_point<ClockType> time_bar) noexcept -> std::optional<T>
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return std::nullopt;
                }

                std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar)
                {
                    return std::nullopt;
                }

                T result = std::move(front_value->item);
                id_type associated_id = this->get_id(result);

                this->id_heap_map.erase(associated_id);
                this->pop_heap_node();

                return std::optional<T>(std::move(result));
            }

            auto has_expired_item(std::chrono::time_point<ClockType> time_bar) const noexcept -> bool
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return false;
                }

                const std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar)
                {
                    return false;
                }

                return true;
            }
            
            auto size() const noexcept -> size_t
            {
                return this->temporal_heap_sz;
            }

            auto capacity() const noexcept -> size_t
            {
                return this->temporal_heap.size();
            }

            auto empty() const noexcept -> bool
            {
                return this->size() == 0u;
            }

        private:

            auto get_id(const T& item) -> id_type
            {
                return StatelessIdExtractor{}(item);
            }

            static void nullify_heap_node(std::unique_ptr<HeapNode>& arg) noexcept
            {
                arg->item       = {};
                arg->sched_time = {};
                arg->heap_idx   = {};
            }

            static void swap_heap_node(std::unique_ptr<HeapNode>& lhs,
                                       std::unique_ptr<HeapNode>& rhs) noexcept
            {
                std::swap(lhs->heap_idx, rhs->heap_idx);
                std::swap(lhs, rhs);
            }

            static auto is_less_than(const std::unique_ptr<HeapNode>& lhs,
                                     const std::unique_ptr<HeapNode>& rhs) noexcept -> bool
            {
                return lhs->sched_time < rhs->sched_time;
            }

            void correct_heap_node_up_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = (idx - 1) >> 1;

                if (!is_less_than(this->temporal_heap[idx], this->temporal_heap[parent_idx]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[parent_idx]);
                this->correct_heap_node_up_at(parent_idx);
            }

            void correct_heap_node_down_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t cand_idx = idx * 2 + 1;

                if (cand_idx >= this->temporal_heap_sz)
                {
                    return;
                }

                if (cand_idx + 1 < this->temporal_heap_sz && is_less_than(this->temporal_heap[cand_idx + 1], this->temporal_heap[cand_idx]))
                {
                    cand_idx += 1;
                }

                if (!is_less_than(this->temporal_heap[cand_idx], this->temporal_heap[idx]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[cand_idx]);
                this->correct_heap_node_down_at(cand_idx);
            } 

            void correct_heap_node_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->correct_heap_node_up_at(idx);
                this->correct_heap_node_down_at(idx);
            }

            template <class TypeLike>
            auto add_heap_node(TypeLike&& item,
                               std::chrono::time_point<ClockType> sched_time) noexcept -> std::expected<HeapNode *, exception_t>
            {
                if (this->temporal_heap_sz == this->temporal_heap.size())
                {
                    return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                }

                HeapNode * operating_node   = stdxx::safe_ptr_access(this->temporal_heap[this->temporal_heap_sz].get());

                try
                {
                    operating_node->item = std::forward<TypeLike>(item);
                }
                catch (...)
                {
                    return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                }

                operating_node->sched_time  = sched_time;
                operating_node->heap_idx    = this->temporal_heap_sz;

                this->temporal_heap_sz      += 1;

                this->correct_heap_node_up_at(this->temporal_heap_sz - 1);

                return operating_node;
            }

            void erase_heap_node_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == idx)
                {
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                }
                else
                {
                    this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_at(idx);
                }
            }

            void pop_heap_node() noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (this->temporal_heap_sz == 0u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == 0u)
                {
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                }
                else
                {
                    this->swap_heap_node(this->temporal_heap.front(), this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_down_at(0u);
                }
            }
    };

    //I guess the problem with us std comittees is that we don't know what to write

    //you would try to account for every kind of Type with polymorphic resolver, but it's not recommended, and we dont use static_assert() because the std didn't write certain components noexcept
    //so its kind of stucked in the logics like that

    template <class T, class StatelessIdExtractor, class ClockType = std::chrono::steady_clock, class OutDegree = std::integral_constant<size_t, 8>>
    class temporal_ordered_item_map_2
    {
        public:

            using value_type    = T;
            using id_type       = decltype(std::declval<StatelessIdExtractor&>()(std::declval<const T&>()));
            using clock_type    = ClockType;

            static inline constexpr size_t OUTDEGREE_SZ    = OutDegree();
        
        private:

            std::unique_ptr<size_t[]> heap_idx_container;

            struct HeapNode
            {
                std::chrono::time_point<ClockType> sched_time;
                size_t * heap_idx;
                std::optional<T> item;
            };

            dg_sock::unordered_unstable_map<id_type, size_t *> id_heap_map;
            dg_sock::vector<HeapNode> temporal_heap;
            size_t temporal_heap_sz;

        public:

            temporal_ordered_item_map_2(size_t cap): id_heap_map(),
                                                     temporal_heap(),
                                                     temporal_heap_sz(0u)
            {
                this->heap_idx_container = std::make_unique<size_t[]>(cap);
                std::iota(this->heap_idx_container.get(), std::next(this->heap_idx_container.get(), cap), 0u);

                for (size_t i = 0u; i < cap; ++i)
                {
                    this->temporal_heap.push_back(HeapNode
                    {
                        .sched_time = {},
                        .heap_idx   = std::next(this->heap_idx_container.get(), i),
                        .item       = std::nullopt,
                    });
                }

                this->id_heap_map.reserve(cap);
            }

            template <class TypeLike>
            auto add(TypeLike&& item,
                     std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {
                id_type _id = this->get_id(item);

                if (this->id_heap_map.contains(_id))
                {
                    return dg_sock::network_exception::DUPLICATED_ENTRY;
                }

                if (exception_t err = this->enqueue_back_heap_node(std::forward<TypeLike>(item), expiry_time); dg_sock::network_exception::is_failed(err))
                {
                    return err;
                }

                size_t * addr_reference = this->temporal_heap[this->temporal_heap_sz - 1u].heap_idx;
                this->correct_heap_node_up_at(this->temporal_heap_sz - 1u);

                try
                {
                    auto [map_ptr, status] = this->id_heap_map.insert(std::make_pair(_id, addr_reference));
                    dg_sock::network_exception_handler::dg_assert(status);
                }
                catch (...)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }

                return dg_sock::network_exception::SUCCESS;
            }

            auto update(id_type item_id,
                        std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {
                auto map_ptr = this->id_heap_map.find(item_id);

                if (map_ptr == this->id_heap_map.end())
                {
                    return dg_sock::network_exception::INVALID_DICTIONARY_KEY;
                }

                HeapNode& heap_node_reference   = this->temporal_heap[*map_ptr->second];
                heap_node_reference.sched_time  = expiry_time;

                this->correct_heap_node_at(*map_ptr->second);

                return dg_sock::network_exception::SUCCESS;
            }

            template <class TypeLike>
            auto add_or_update(TypeLike&& item,
                               std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {
                if (this->id_heap_map.contains(this->get_id(item)))
                {
                    return this->update(this->get_id(item), expiry_time);
                }

                return this->add(std::forward<TypeLike>(item), expiry_time);
            }

            void erase(id_type item_id) noexcept
            {
                auto map_ptr = this->id_heap_map.find(item_id);

                if (map_ptr == this->id_heap_map.end())
                {
                    return;
                }

                size_t idx = *map_ptr->second;
                this->id_heap_map.erase(map_ptr);
                this->erase_heap_node_at(idx);
            }

            auto get_expired_item(std::chrono::time_point<ClockType> time_bar) noexcept -> std::optional<T>
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return std::nullopt;
                }

                HeapNode& front_value = this->temporal_heap.front();

                if (front_value.sched_time >= time_bar)
                {
                    return std::nullopt;
                }

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (!front_value.item.has_value())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                std::optional<T> result = std::move(front_value.item.value());
                id_type _id             = this->get_id(result.value());

                this->id_heap_map.erase(_id);
                this->pop_heap_node();

                return result;
            }

            auto has_expired_item(std::chrono::time_point<ClockType> time_bar) const noexcept -> bool
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return false;
                }

                const HeapNode& front_value = this->temporal_heap.front();

                if (front_value.sched_time >= time_bar)
                {
                    return false;
                }

                return true;
            }

            auto size() const noexcept -> size_t
            {
                return this->temporal_heap_sz;
            }

            auto capacity() const noexcept -> size_t
            {
                return this->temporal_heap.size();
            }

            auto empty() const noexcept -> bool
            {
                return this->size() == 0u;
            }

        private:

            void swap_heap_node(HeapNode& lhs, HeapNode& rhs) const noexcept
            {
                std::swap(*lhs.heap_idx, *rhs.heap_idx);
                std::swap(lhs, rhs);
            }

            template <class TypeLike>
            auto enqueue_back_heap_node(TypeLike&& item,
                                        std::chrono::time_point<ClockType> sched_time) noexcept -> exception_t
            {
                if (this->temporal_heap_sz == this->temporal_heap.size())
                {
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }

                try
                {
                    this->temporal_heap[this->temporal_heap_sz].item    = std::forward<TypeLike>(item);
                }
                catch (...)
                {
                    return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                }

                this->temporal_heap[this->temporal_heap_sz].sched_time  = sched_time;

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (*this->temporal_heap[this->temporal_heap_sz].heap_idx != this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->temporal_heap_sz += 1u;

                return dg_sock::network_exception::SUCCESS;
            }

            void decomission_back_heap_node() noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (this->temporal_heap_sz == 0u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }

                    if (*this->temporal_heap[this->temporal_heap_sz - 1u].heap_idx != this->temporal_heap_sz - 1u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->temporal_heap[this->temporal_heap_sz - 1u].item       = std::nullopt;
                this->temporal_heap[this->temporal_heap_sz - 1u].sched_time = {};
                this->temporal_heap_sz                                      -= 1u;
            }

            auto get_id(const T& item) const noexcept -> id_type
            {
                return StatelessIdExtractor{}(item);
            }

            static auto is_equal_better(const HeapNode& lhs, const HeapNode& rhs) noexcept -> bool
            {
                return lhs.sched_time <= rhs.sched_time;
            }

            void correct_heap_node_up_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = (idx - 1) / OUTDEGREE_SZ;

                if (this->is_equal_better(this->temporal_heap[parent_idx], this->temporal_heap[idx]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[parent_idx], this->temporal_heap[idx]);
                this->correct_heap_node_up_at(parent_idx);
            }

            void correct_heap_node_down_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                std::optional<size_t> cand_idx = std::nullopt;

                for (size_t i = 0u; i < OUTDEGREE_SZ; ++i)
                {
                    const size_t child_idx = idx * OUTDEGREE_SZ + (i + 1);

                    if (child_idx >= this->temporal_heap_sz)
                    {
                        break;
                    }

                    if (!cand_idx.has_value())
                    {
                        cand_idx = child_idx;
                        continue;
                    }

                    if (this->is_equal_better(this->temporal_heap[child_idx], this->temporal_heap[cand_idx.value()]))
                    {
                        cand_idx = child_idx;
                    }
                }

                if (!cand_idx.has_value())
                {
                    return;
                }

                if (this->is_equal_better(this->temporal_heap[idx], this->temporal_heap[cand_idx.value()]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[cand_idx.value()]);
                this->correct_heap_node_down_at(cand_idx.value());
            }

            void correct_heap_node_at(size_t idx) noexcept
            {
                this->correct_heap_node_up_at(idx);
                this->correct_heap_node_down_at(idx);
            }

            void erase_heap_node_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[back_node_idx]);
                this->decomission_back_heap_node();

                if (idx < this->temporal_heap_sz)
                {
                    this->correct_heap_node_at(idx);
                }               
            }

            void pop_heap_node() noexcept
            {
                this->erase_heap_node_at(0u);
            }
    };

}

#endif