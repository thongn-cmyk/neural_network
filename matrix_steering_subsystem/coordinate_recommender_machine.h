#ifndef __COORDINATE_RECOMMENDER_MACHINE_H__
#define __COORDINATE_RECOMMENDER_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <random>
#include <functional>
#include <numbers>
#include <memory>
#include <serializer/compact_serializer.h>
#include <deque>
#include <stl_extension/stdx.h>

namespace coordinate_recommender_machine
{
    using machine_float_t = double;

    class CoordinateRecommenderMachineInterface
    {
        public:

            virtual ~CoordinateRecommenderMachineInterface() noexcept = default;

            virtual void feedback(const std::vector<machine_float_t>& coordinate, machine_float_t rating) = 0;
            virtual auto next() -> std::optional<std::vector<machine_float_t>> = 0;
            virtual void set_window_size(size_t sz) = 0;
            virtual void clear() noexcept = 0;
    };

    class MultidimensionalRadianDiscretizer
    {
        private:

            machine_float_t discrete_unit;

            static inline constexpr machine_float_t MIN_DISCRETIZATION_SZ    = 0.00001;
            static inline constexpr machine_float_t MAX_DISCRETIZATION_SZ    = 2 * std::numbers::pi_v<machine_float_t>;

        public:

            MultidimensionalRadianDiscretizer(machine_float_t discrete_unit)
            {
                if (std::clamp(discrete_unit, MIN_DISCRETIZATION_SZ, MAX_DISCRETIZATION_SZ) != discrete_unit)
                {
                    throw std::invalid_argument("invalid discretization size");
                }

                this->discrete_unit = discrete_unit;
            }

            auto discretize(const std::vector<machine_float_t>& coordinate) -> std::vector<size_t>
            {
                return this->radian_discretize(this->normalize(coordinate), this->discrete_unit);
            }

            auto dimensional_size() -> size_t
            {
                machine_float_t worst_coordinate    = std::numbers::pi_v<machine_float_t> * 2;
                size_t worst_idx                    = std::round(worst_coordinate / this->discrete_unit);
                size_t worst_sz                     = worst_idx + 1;

                return worst_sz;
            }

        private:

            auto normalize(const std::vector<machine_float_t>& coordinate) -> std::vector<machine_float_t>
            {
                std::vector<machine_float_t> rs(coordinate.size());

                for (size_t i = 0u; i < coordinate.size(); ++i)
                {
                    if (std::isnan(coordinate[i]))
                    {
                        throw std::invalid_argument("bad cooridnate, NaN");
                    }

                    machine_float_t divisor  = std::numbers::pi_v<machine_float_t> * 2;
                    machine_float_t new_x    = std::remainder(std::remainder(coordinate[i], divisor) + divisor, divisor);

                    if (std::isnan(new_x))
                    {
                        throw std::runtime_error("bad coordinate, normalization went wrong");
                    }

                    rs[i]   = new_x;
                }

                return rs;
            }

            auto radian_discretize(const std::vector<machine_float_t>& coordinate, machine_float_t discretization_sz) -> std::vector<size_t>
            {
                std::vector<size_t> rs(coordinate.size());

                for (size_t i = 0u; i < coordinate.size(); ++i)
                {
                    rs[i] = std::round(coordinate[i] / discretization_sz);
                }

                return rs;
            }
    };

    class BidirectionalFeedbackHeap
    {
        private:

            struct HeapNode
            {
                machine_float_t rating;
                std::vector<machine_float_t> coordinate;
                std::shared_ptr<size_t> coordinate_reference;
                std::chrono::time_point<std::chrono::steady_clock> last_updated;
            };

            std::vector<HeapNode> heap;
            std::unordered_map<std::string, std::shared_ptr<size_t>> coordinate_reference_map;

        public:

            BidirectionalFeedbackHeap(): heap(),
                                         coordinate_reference_map(){}

            BidirectionalFeedbackHeap(const BidirectionalFeedbackHeap&) = delete;
            BidirectionalFeedbackHeap(BidirectionalFeedbackHeap&&) = delete;
            BidirectionalFeedbackHeap& operator =(const BidirectionalFeedbackHeap&) = delete;
            BidirectionalFeedbackHeap& operator =(BidirectionalFeedbackHeap&&) = delete;

            void push(machine_float_t rating, const std::vector<machine_float_t>& coordinate)
            {
                stdx::safe_float_range_access(coordinate.data(), coordinate.size());
                stdx::safe_float_access(rating);

                std::string coordinate_str_representation = dg::network_compact_serializer::serialize<std::string>(coordinate);

                if (auto map_ptr = this->coordinate_reference_map.find(coordinate_str_representation); map_ptr != this->coordinate_reference_map.end())
                {
                    this->heap[*map_ptr->second].rating         = rating;
                    this->heap[*map_ptr->second].last_updated   = std::chrono::steady_clock::now();
                    this->correct_at(*map_ptr->second);

                    return;
                }

                std::shared_ptr<size_t> new_idx = std::make_shared<size_t>(this->heap.size());
                this->heap.push_back(HeapNode{.rating               = rating,
                                              .coordinate           = coordinate,
                                              .coordinate_reference = new_idx,
                                              .last_updated         = std::chrono::steady_clock::now()});

                try
                {
                    this->coordinate_reference_map.insert({coordinate_str_representation, new_idx});
                }
                catch (...)
                {
                    this->heap.pop_back();
                    throw;
                }

                this->push_up_at(this->heap.size() - 1);
            }

            auto peek() noexcept -> const std::vector<machine_float_t> *
            {
                if (this->heap.empty())
                {
                    return nullptr;
                }

                return &this->heap.front().coordinate;
            }

            void pop()
            {
                if (this->heap.empty())
                {
                    std::abort();
                }

                std::string coordinate_str_representation = dg::network_compact_serializer::serialize<std::string>(this->heap.front().coordinate);

                this->coordinate_reference_map.erase(coordinate_str_representation);
                this->swap_heap_node(this->heap.front(), this->heap.back());
                this->heap.pop_back();

                if (!this->heap.empty())
                {
                    this->push_down_at(0);
                }
            }

            void pop_back()
            {
                if (this->heap.empty())
                {
                    std::abort();
                }

                std::string coordinate_str_representation = dg::network_compact_serializer::serialize<std::string>(this->heap.back().coordinate);

                this->coordinate_reference_map.erase(coordinate_str_representation);
                this->heap.pop_back();
            }

            void remove_by_coordinate(const std::vector<machine_float_t>& coordinate)
            {
                std::string coordinate_str_representation = dg::network_compact_serializer::serialize<std::string>(coordinate);
                auto map_ptr = this->coordinate_reference_map.find(coordinate_str_representation);

                if (map_ptr == this->coordinate_reference_map.end())
                {
                    return;
                }

                size_t rm_idx   = *map_ptr->second;
                size_t back_idx = this->heap.size() - 1u;

                this->coordinate_reference_map.erase(map_ptr);
                this->swap_heap_node(this->heap[rm_idx], this->heap[back_idx]);
                this->heap.pop_back();

                if (rm_idx < this->heap.size())
                {
                    this->correct_at(rm_idx);
                }
            }

            auto size() const noexcept -> size_t
            {
                return this->heap.size();
            }

            void clear() noexcept
            {
                this->heap.clear();
                this->coordinate_reference_map.clear();
            }

        private:
        
            void swap_heap_node(HeapNode& lhs, HeapNode& rhs) const noexcept
            {
                std::swap(lhs.rating, rhs.rating);
                std::swap(lhs.coordinate, rhs.coordinate);
                std::swap(lhs.coordinate_reference, rhs.coordinate_reference);
                std::swap(*lhs.coordinate_reference, *rhs.coordinate_reference);
                std::swap(lhs.last_updated, rhs.last_updated);
            }

            auto is_better(const HeapNode& lhs, const HeapNode& rhs) const noexcept -> bool
            {
                if (lhs.rating > rhs.rating)
                {
                    return true;
                }

                if (lhs.rating < rhs.rating)
                {
                    return false;
                }

                if (lhs.last_updated > rhs.last_updated)
                {
                    return true;
                }

                if (lhs.last_updated < rhs.last_updated)
                {
                    return false;
                }

                return false;
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

                size_t c = (idx - 1) / 2;

                if (this->is_better(this->heap[c], this->heap[idx]))
                {
                    return;
                }

                this->swap_heap_node(this->heap[c], this->heap[idx]);
                this->push_up_at(c);
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

                if (c + 1 < this->heap.size() && this->is_better(this->heap[c + 1], this->heap[c]))
                {
                    c += 1;
                }

                if (this->is_better(this->heap[idx], this->heap[c]))
                {
                    return;
                }

                this->swap_heap_node(this->heap[idx], this->heap[c]);
                this->push_down_at(c);
            }

            void correct_at(size_t idx) noexcept
            {
                this->push_up_at(idx);
                this->push_down_at(idx);
            }
    };

    class HyperFocusCoordinateRecommenderMachine: public virtual CoordinateRecommenderMachineInterface
    {
        private:

            BidirectionalFeedbackHeap heap;
            std::deque<std::vector<machine_float_t>> coordinate_deque;
            size_t deque_capacity;
    
            static inline constexpr size_t DEFAULT_DEQUE_CAPACITY = 128u;
        
        public:

            HyperFocusCoordinateRecommenderMachine(): heap(),
                                                      coordinate_deque(),
                                                      deque_capacity(DEFAULT_DEQUE_CAPACITY){}

            void feedback(const std::vector<machine_float_t>& coordinate, machine_float_t rating)
            {
                if (this->coordinate_deque.size() == this->deque_capacity)
                {
                    this->heap.remove_by_coordinate(this->coordinate_deque.front());
                    this->coordinate_deque.pop_front();
                }

                this->coordinate_deque.push_back(coordinate);

                try
                {
                    this->heap.push(rating, coordinate);
                }
                catch (...)
                {
                    this->coordinate_deque.pop_back();
                    throw;
                }
            }

            auto next() -> std::optional<std::vector<machine_float_t>>
            {
                const std::vector<machine_float_t> * result = this->heap.peek();

                if (result == nullptr)
                {
                    return std::nullopt;
                }

                return *result;
            }

            void set_window_size(size_t sz)
            {
                if (sz == 0u)
                {
                    throw std::invalid_argument("bad window size, window size cannot be 0");
                }

                size_t old_sz       = this->coordinate_deque.size();
                size_t new_sz       = std::min(old_sz, sz);
                size_t difference   = old_sz - new_sz;

                for (size_t i = 0u; i < difference; ++i)
                {
                    this->heap.remove_by_coordinate(this->coordinate_deque.front());
                    this->coordinate_deque.pop_front();
                }

                this->deque_capacity = sz;
            }

            void clear() noexcept
            {
                this->heap.clear();
                this->coordinate_deque.clear();
            }
    };

    class SecondChanceCoordinateRecommenderMachine: public virtual CoordinateRecommenderMachineInterface
    {
        private:

            BidirectionalFeedbackHeap heap;
            size_t capacity;

            static inline constexpr size_t DEFAULT_HEAP_CAPACITY = 128u;

        public:

            SecondChanceCoordinateRecommenderMachine(): heap(),
                                                        capacity(DEFAULT_HEAP_CAPACITY){}

            void feedback(const std::vector<machine_float_t>& coordinate, machine_float_t rating)
            {
                if (this->heap.size() == this->capacity)
                {
                    this->heap.pop_back();
                }

                this->heap.push(rating, coordinate);
            }

            auto next() -> std::optional<std::vector<machine_float_t>>
            {
                const std::vector<machine_float_t> * result = this->heap.peek();

                if (result == nullptr)
                {
                    return std::nullopt;
                }

                std::optional<std::vector<machine_float_t>> return_result = *result;
                this->heap.pop();

                return return_result;
            }

            void set_window_size(size_t sz)
            {
                if (sz == 0u)
                {
                    throw std::invalid_argument("bad window size, window size cannot be 0");
                }

                size_t old_sz       = this->heap.size();
                size_t new_sz       = std::min(old_sz, sz);
                size_t difference   = old_sz - new_sz;

                for (size_t i = 0u; i < difference; ++i)
                {
                    this->heap.pop_back();
                }

                this->capacity      = sz;
            }

            void clear() noexcept
            {
                this->heap.clear();
            }
    };

    class MixedCoordinateRecommenderMachine: public virtual CoordinateRecommenderMachineInterface
    {
        private:

            using randomizer_t = decltype(std::bind(std::uniform_int_distribution<uint8_t>(), std::mt19937{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())}));

            HyperFocusCoordinateRecommenderMachine machine1;
            SecondChanceCoordinateRecommenderMachine machine2;
            randomizer_t randomizer_machine;
       
        public:

            MixedCoordinateRecommenderMachine(): machine1(),
                                                 machine2(),
                                                 randomizer_machine(std::bind(std::uniform_int_distribution<uint8_t>(), std::mt19937{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())})){}

            void feedback(const std::vector<machine_float_t>& coordinate, machine_float_t rating)
            {
                this->machine1.feedback(coordinate, rating);
                this->machine2.feedback(coordinate, rating);
            }

            auto next() -> std::optional<std::vector<machine_float_t>>
            {
                bool coin_flip = this->randomizer_machine() % 2;

                if (coin_flip)
                {
                    std::optional<std::vector<machine_float_t>> cand = this->machine1.next();

                    if (cand.has_value())
                    {
                        return cand;
                    }

                    return this->machine2.next();
                }
                else
                {
                    std::optional<std::vector<machine_float_t>> cand = this->machine2.next();

                    if (cand.has_value())
                    {
                        return cand;
                    }

                    return this->machine1.next();
                }
            }

            void set_window_size(size_t sz)
            {
                this->machine1.set_window_size(sz);
                this->machine2.set_window_size(sz);
            }

            void clear() noexcept
            {
                this->machine1.clear();
                this->machine2.clear();
            }
    };

    class EchoCoordinateRecommenderMachine: public virtual CoordinateRecommenderMachineInterface
    {
        private:

            struct EchoCoordinate
            {
                std::vector<machine_float_t> item;
                size_t expiry_count;
            };

            size_t new_expiry_count;
            std::optional<EchoCoordinate> echo_coor;
            std::unique_ptr<CoordinateRecommenderMachineInterface> base;

        public:

            EchoCoordinateRecommenderMachine(std::unique_ptr<CoordinateRecommenderMachineInterface> base,
                                             size_t new_expiry_count)
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("invalid base, null base");
                }

                if (new_expiry_count == 0u)
                {
                    throw std::invalid_argument("invalid expiry count, 0");
                }

                this->new_expiry_count  = new_expiry_count;
                this->echo_coor         = std::nullopt;
                this->base              = std::move(base);
            }

            void feedback(const std::vector<machine_float_t>& coordinate, machine_float_t rating)
            {
                this->base->feedback(coordinate, rating);
            }

            auto next() -> std::optional<std::vector<machine_float_t>>
            {
                if (!this->echo_coor.has_value())
                {
                    std::optional<std::vector<machine_float_t>> base_result = this->base->next();

                    if (!base_result.has_value())
                    {
                        return std::nullopt;
                    }

                    this->echo_coor = EchoCoordinate
                    {
                        .item           = std::move(base_result.value()),
                        .expiry_count   = this->new_expiry_count
                    };
                }

                if (this->echo_coor->expiry_count == 0u)
                {
                    std::abort();
                }

                std::vector<machine_float_t> result = this->echo_coor->item;
                this->echo_coor->expiry_count -= 1;

                if (this->echo_coor->expiry_count == 0u)
                {
                    this->echo_coor = std::nullopt;
                }

                return result;
            }

            void set_window_size(size_t sz)
            {
                this->base->set_window_size(sz);
            }

            void clear() noexcept
            {
                this->base->clear();
            }
    };
}

#endif