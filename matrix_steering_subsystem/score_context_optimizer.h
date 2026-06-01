//what do we learn about this design patterns?
//it's called business engineering, where each namespace has their own virtues of languages to support one final interface, which is the business requirements

//do we care about if the interface is sufficient, NO
//do we care about what the interface means, probably

//the interface is, externally, the minimum sufficient logic for user to use the component
//the interface is, internally, an expectation declaration of a component to fulfill its logics
//the interface is, internally externally, an expectation declaration and minimum sufficient logic for user to use the component 

#ifndef __SCORE_CONTEXT_OPTIMIZER_H__
#define __SCORE_CONTEXT_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <vector>
#include <memory>
#include <stdexcept>
#include <deque>
#include <stl_extension/stdx.h>
#include "conventional_randomizer.h"

namespace score_context_optimizer
{
    using ctx_float_t = long double;

    class StatisticalMachineInterface
    {
        public:

            virtual ~StatisticalMachineInterface() = default;
    };

    class StatisticalMachineGeneratorInterface
    {
        public:

            virtual ~StatisticalMachineGeneratorInterface() = default;

            virtual auto get() -> std::shared_ptr<StatisticalMachineInterface> = 0;
    };

    class ActionableResultInterface
    {
        public:

            virtual ~ActionableResultInterface() = default;

            virtual void feedback(ctx_float_t score) = 0;
            virtual auto get_statistical_machine() -> std::shared_ptr<StatisticalMachineInterface> = 0;
    };

    class IterationContextInterface
    {
        public:

            virtual ~IterationContextInterface() = default;

            virtual auto next() -> std::unique_ptr<ActionableResultInterface> = 0;
    };

    class IterationContextGeneratorInterface
    {
        public:

            virtual ~IterationContextGeneratorInterface() = default;

            virtual auto get() -> std::unique_ptr<IterationContextInterface> = 0;
    };

    class WindowCalculatorInterface
    {
        public:

            virtual ~WindowCalculatorInterface() = default;

            virtual void push(ctx_float_t e) = 0;
            virtual auto get_enumeration() -> size_t = 0;
            virtual auto enumeration_size() -> size_t = 0;
    };

    class WindowCalculatorGeneratorInterface
    {
        public:

            virtual ~WindowCalculatorGeneratorInterface() = default;

            virtual auto get(size_t enumeration_sz) -> std::unique_ptr<WindowCalculatorInterface> = 0;
    };

    class AverageWindowCalculator
    {
        private:

            std::deque<ctx_float_t> result;
            size_t window_sz;

        public:

            AverageWindowCalculator(size_t window_sz): result(),
                                                       window_sz(stdx::safe_non_zero_access(window_sz)){}

            void push(ctx_float_t e)
            {
                if (std::isnan(e))
                {
                    throw std::invalid_argument("bad element, NaN");
                }

                if (std::isinf(e))
                {
                    throw std::invalid_argument("bad element, inf");
                }

                if (this->result.size() == this->window_sz)
                {
                    this->result.pop_front();
                }

                this->result.push_back(e);
            }

            auto get_score() -> ctx_float_t
            {
                ctx_float_t total = 0;

                for (ctx_float_t e: result)
                {
                    total += e;
                }

                return total / this->window_sz;
            }
    };

    class SuffixCompressor
    {
        private:

            static auto base_factorial(size_t e) -> __uint128_t
            {
                if (e == 0u)
                {
                    return 1;
                }

                return base_factorial(e - 1) * e;
            }

            static auto factorial(size_t e) -> __uint128_t
            {
                constexpr size_t MAX_FACTORIAL_SIZE = 35u;

                static std::vector<__uint128_t> table = [=]
                {
                    std::vector<__uint128_t> result{};

                    for (size_t i = 0u; i < MAX_FACTORIAL_SIZE; ++i)
                    {
                        result.push_back(base_factorial(i));
                    }

                    return result;
                }();

                if (e >= MAX_FACTORIAL_SIZE)
                {
                    throw std::invalid_argument("bad factorial, max size reached");
                }

                return table[e];
            }

        public:

            auto compress(const std::vector<size_t>& suffix, const std::vector<size_t>& image) -> __uint128_t
            {
                constexpr size_t MAX_IMAGE_SIZE = 30u;

                if (suffix.size() != image.size())
                {
                    throw std::invalid_argument("bad suffix sequence, incompatible image");
                }

                if (suffix.size() == 0u)
                {
                    return 0u;
                }

                if (suffix.size() == 1u)
                {
                    if (suffix.front() != image.front())
                    {
                        throw std::invalid_argument("bad image sequence, suffix not found");
                    }

                    return 0u;
                }

                if (image.size() > MAX_IMAGE_SIZE)
                {
                    throw std::invalid_argument("bad image, max size reached");
                }

                size_t offset = std::distance(image.begin(), std::find(image.begin(), image.end(), suffix.front()));

                if (offset == image.size())
                {
                    throw std::invalid_argument("bad image sequence, suffix not found");
                }

                std::vector<size_t> nxt_image   = image;
                nxt_image.erase(std::next(nxt_image.begin(), offset));
                std::vector<size_t> nxt_suffix  = std::vector<size_t>(std::next(suffix.begin()), suffix.end());

                __uint128_t nxt_value           = this->compress(nxt_suffix, nxt_image);
                __uint128_t result              = static_cast<__uint128_t>(offset) * this->factorial(nxt_suffix.size());

                return nxt_value + result;
            }
    };

    class SequenceCompressor
    {
        public:

            static consteval auto max_lossy_compress_size() -> size_t
            {
                return 30u;
            }

            auto lossy_compress(const std::vector<ctx_float_t>& vec) -> __uint128_t
            {
                for (const ctx_float_t& e: vec)
                {
                    if (std::isnan(e))
                    {
                        throw std::invalid_argument("bad element, NaN");
                    }
                }

                std::vector<std::pair<size_t, ctx_float_t>> enumerated_vec = stdx::enumerate_vector(vec);
                auto less = [](const auto& lhs, const auto& rhs)
                {
                    return lhs.second < rhs.second;
                };

                std::stable_sort(enumerated_vec.begin(), enumerated_vec.end(), less);
                std::vector<size_t> suffix_vec{};

                for (const auto& e: enumerated_vec)
                {
                    suffix_vec.push_back(e.first);
                }

                std::vector<size_t> image(suffix_vec.size());
                std::iota(image.begin(), image.end(), 0u);

                return SuffixCompressor{}.compress(suffix_vec, image);
            }
    };

    class MaxWindowSpitter
    {
        private:

            ctx_float_t max_value;
            size_t value_counter;
            size_t counter_sz;

        public:

            MaxWindowSpitter(size_t counter_sz): max_value(),
                                                 value_counter(0u),
                                                 counter_sz(stdx::safe_non_zero_access(counter_sz)){}

            auto insert(ctx_float_t value) -> std::optional<ctx_float_t>
            {
                if (std::isnan(value))
                {
                    throw std::invalid_argument("bad value, NaN");
                }

                std::optional<ctx_float_t> return_value = std::nullopt;

                if (this->value_counter == this->counter_sz)
                {
                    return_value        = this->max_value;
                    this->value_counter = 0u;
                }

                if (this->value_counter == 0u)
                {
                    this->max_value = value;
                }

                this->max_value     = std::max(this->max_value, value);
                this->value_counter += 1;

                return return_value;
            }
    };

    class SuffixWindowCalculator: public virtual WindowCalculatorInterface
    {
        private:

            std::deque<ctx_float_t> result;
            size_t window_sz;
            size_t enumeration_sz;

        public:

            SuffixWindowCalculator(size_t window_sz,
                                   size_t enumeration_sz)
            {
                if (window_sz == 0u)
                {
                    throw std::invalid_argument("bad window size, 0");
                }

                if (enumeration_sz == 0u)
                {
                    throw std::invalid_argument("bad enumeration size, 0");
                }

                this->result            = std::deque<ctx_float_t>();
                this->window_sz         = window_sz;
                this->enumeration_sz    = enumeration_sz;
            }

            void push(ctx_float_t score)
            {
                if (std::isnan(score))
                {
                    throw std::invalid_argument("bad score, NaN");
                }

                if (std::isinf(score))
                {
                    throw std::invalid_argument("bad score, inf");
                }

                if (this->result.size() == this->window_sz)
                {
                    this->result.pop_front();
                }

                this->result.push_back(score);
            }

            auto get_enumeration() -> size_t
            {
                if (this->result.size() == this->window_sz)
                {
                    return SequenceCompressor{}.lossy_compress({this->result.begin(), this->result.end()}) % this->enumeration_sz;
                }

                return this->enumeration_sz - 1u;
            }

            auto enumeration_size() -> size_t
            {
                return this->enumeration_sz;
            }
    };

    class BinaryBitsetWindowCalculator: public virtual WindowCalculatorInterface
    {
        private:

            __uint128_t bit_vec;

            size_t window_i;
            size_t window_sz;
            size_t enumeration_sz;

        public:

            BinaryBitsetWindowCalculator(size_t window_sz,
                                         size_t enumeration_sz)
            {
                if (window_sz == 0u)
                {
                    throw std::invalid_argument("bad window size 0");
                }

                if (window_sz > 128u)
                {
                    throw std::invalid_argument("bad window size, max value reached");
                }

                if (enumeration_sz == 0u)
                {
                    throw std::invalid_argument("bad enumeration size, 0");
                }

                this->bit_vec           = 0u;
                this->window_i          = 0u;
                this->window_sz         = window_sz;
                this->enumeration_sz    = enumeration_sz;
            }

            void push(ctx_float_t score)
            {
                if (std::isnan(score))
                {
                    throw std::invalid_argument("bad score, NaN");
                }

                if (std::isinf(score))
                {
                    throw std::invalid_argument("bad score, inf");
                }

                if (this->window_i == this->window_sz)
                {
                    this->window_i  -= 1;
                    this->bit_vec   &= (__uint128_t{1} << this->window_i) - 1u;
                }

                this->bit_vec   <<= 1;
                this->bit_vec   |= static_cast<__uint128_t>(score > 0);
                this->window_i  += 1;
            }

            auto get_enumeration() -> size_t
            {
                if (this->window_i == this->window_sz)
                {
                    return this->bit_vec % this->enumeration_sz;
                }

                return this->enumeration_sz - 1u;
            }

            auto enumeration_size() -> size_t
            {
                return this->enumeration_sz;
            }
    };

    class MaxWindowCalculatorExtension: public virtual WindowCalculatorInterface
    {
        private:

            MaxWindowSpitter spitter;
            std::unique_ptr<WindowCalculatorInterface> base;
        
        public:

            MaxWindowCalculatorExtension(size_t aggregation_sz,
                                         std::unique_ptr<WindowCalculatorInterface> base): spitter(aggregation_sz),
                                                                                           base(std::move(base))
            {
                if (this->base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }
            }

            void push(ctx_float_t e)
            {
                std::optional<ctx_float_t> new_e = this->spitter.insert(e);

                if (new_e.has_value())
                {
                    this->base->push(new_e.value());
                }
            }

            auto get_enumeration() -> size_t
            {
                return this->base->get_enumeration();
            }

            auto enumeration_size() -> size_t
            {
                return this->base->enumeration_size();
            }
    };

    class SuffixWindowCalculatorGenerator: public virtual WindowCalculatorGeneratorInterface
    {
        private:

            size_t window_sz;
            size_t aggregation_sz;

        public:

            static inline constexpr size_t DEFAULT_WINDOW_SZ        = 2u;
            static inline constexpr size_t DEFAULT_AGGREGATION_SZ   = 2u;

            SuffixWindowCalculatorGenerator(): window_sz(DEFAULT_WINDOW_SZ),
                                               aggregation_sz(DEFAULT_AGGREGATION_SZ){}

            auto set_window_size(size_t window_sz) -> SuffixWindowCalculatorGenerator&
            {
                this->window_sz = window_sz;
                return *this;
            }

            auto set_aggregation_size(size_t aggregation_sz) -> SuffixWindowCalculatorGenerator&
            {
                this->aggregation_sz = aggregation_sz;
                return *this;
            }

            auto get(size_t enumeration_sz) -> std::unique_ptr<WindowCalculatorInterface>
            {
                return std::make_unique<MaxWindowCalculatorExtension>(this->aggregation_sz,
                                                                      std::make_unique<SuffixWindowCalculator>(this->window_sz, enumeration_sz));
            }
    };

    class BinaryBitsetWindowCalculatorGenerator: public virtual WindowCalculatorGeneratorInterface
    {
        private:

            size_t window_sz;
            size_t aggregation_sz;

        public:

            static inline constexpr size_t DEFAULT_WINDOW_SZ        = 2u;
            static inline constexpr size_t DEFAULT_AGGREGATION_SZ   = 2u;

            BinaryBitsetWindowCalculatorGenerator(): window_sz(DEFAULT_WINDOW_SZ),
                                                     aggregation_sz(DEFAULT_AGGREGATION_SZ){}

            auto set_window_size(size_t window_sz) -> BinaryBitsetWindowCalculatorGenerator&
            {
                this->window_sz = window_sz;
                return *this;
            }

            auto set_aggregation_size(size_t aggregation_sz) -> BinaryBitsetWindowCalculatorGenerator&
            {
                this->aggregation_sz = aggregation_sz;
                return *this;
            }

            auto get(size_t enumeration_sz) -> std::unique_ptr<WindowCalculatorInterface>
            {
                return std::make_unique<MaxWindowCalculatorExtension>(this->aggregation_sz,
                                                                      std::make_unique<BinaryBitsetWindowCalculator>(this->window_sz, enumeration_sz));
            }
    };

    class WindowContextGenerator: public virtual IterationContextGeneratorInterface
    {
        private:

            std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr;
            std::unique_ptr<WindowCalculatorGeneratorInterface> context_window_generator;
            size_t machine_arr_sz;

        public:

            WindowContextGenerator(std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr,
                                   std::unique_ptr<WindowCalculatorGeneratorInterface> context_window_generator,
                                   size_t machine_arr_sz) noexcept: machine_arr(std::move(machine_arr)),
                                                                    context_window_generator(std::move(context_window_generator)),
                                                                    machine_arr_sz(machine_arr_sz){}

            auto get() -> std::unique_ptr<IterationContextInterface>
            {
                return std::make_unique<WindowContext>(this->context_window_generator->get(this->machine_arr_sz),
                                                       this->machine_arr);
            }

        private:

            class ActionableResult: public virtual ActionableResultInterface
            {
                private:

                    std::shared_ptr<WindowCalculatorInterface> window_calculator;
                    std::shared_ptr<StatisticalMachineInterface> machine;
                    bool was_feedback_received;

                public:

                    ActionableResult(std::shared_ptr<WindowCalculatorInterface> window_calculator,
                                     std::shared_ptr<StatisticalMachineInterface> machine,
                                     bool was_feedback_received) noexcept: window_calculator(std::move(window_calculator)),
                                                                           machine(std::move(machine)),
                                                                           was_feedback_received(was_feedback_received){}

                    void feedback(ctx_float_t score)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        if (std::isnan(score))
                        {
                            return;
                        }

                        if (std::isinf(score))
                        {
                            return;
                        }

                        this->window_calculator->push(score);
                    }

                    auto get_statistical_machine() -> std::shared_ptr<StatisticalMachineInterface>
                    {
                        return this->machine;
                    }
            };

            class WindowContext: public virtual IterationContextInterface
            {
                private:

                    std::shared_ptr<WindowCalculatorInterface> window_calculator;
                    std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr;

                public:

                    WindowContext(std::shared_ptr<WindowCalculatorInterface> window_calculator,
                                  std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr) noexcept: window_calculator(std::move(window_calculator)),
                                                                                                                         machine_arr(std::move(machine_arr)){}

                    auto next() -> std::unique_ptr<ActionableResultInterface>
                    {
                        size_t slot = this->window_calculator->get_enumeration();

                        return std::make_unique<ActionableResult>(this->window_calculator, this->machine_arr[slot], false);
                    }
            };
    };

    class WarmWindowContextGenerator: public virtual IterationContextGeneratorInterface
    {
        private:

            std::unique_ptr<IterationContextGeneratorInterface> lhs;
            std::unique_ptr<IterationContextGeneratorInterface> rhs;
            size_t warmup_window;

        public:

            WarmWindowContextGenerator(std::unique_ptr<IterationContextGeneratorInterface> lhs,
                                       std::unique_ptr<IterationContextGeneratorInterface> rhs,
                                       size_t warmup_window) noexcept: lhs(std::move(lhs)),
                                                                       rhs(std::move(rhs)),
                                                                       warmup_window(warmup_window){}

            auto get() -> std::unique_ptr<IterationContextInterface>
            {
                return std::make_unique<InternalIterationContext>
                (
                    this->lhs->get(),
                    this->rhs->get(),
                    this->warmup_window,
                    0u
                );
            }

        private:
            
            class InternalActionableResult: public virtual ActionableResultInterface
            {
                private:

                    std::unique_ptr<ActionableResultInterface> major_action;
                    std::unique_ptr<ActionableResultInterface> observe_action;
                
                public:

                    InternalActionableResult(std::unique_ptr<ActionableResultInterface> major_action,
                                             std::unique_ptr<ActionableResultInterface> observe_action) noexcept: major_action(std::move(major_action)),
                                                                                                                  observe_action(std::move(observe_action)){}

                    void feedback(ctx_float_t score)
                    {
                        this->major_action->feedback(score);
                        this->observe_action->feedback(score);
                    }

                    auto get_statistical_machine() -> std::shared_ptr<StatisticalMachineInterface>
                    {
                        return this->major_action->get_statistical_machine();
                    }
            };

            class InternalIterationContext: public virtual IterationContextInterface
            {
                private:

                    std::unique_ptr<IterationContextInterface> first_runner;
                    std::unique_ptr<IterationContextInterface> second_runner;
                    size_t warmup_window;
                    size_t warmup_counter;

                public:

                    InternalIterationContext(std::unique_ptr<IterationContextInterface> first_runner,
                                             std::unique_ptr<IterationContextInterface> second_runner,
                                             size_t warmup_window,
                                             size_t warmup_counter) noexcept: first_runner(std::move(first_runner)),
                                                                              second_runner(std::move(second_runner)),
                                                                              warmup_window(warmup_window),
                                                                              warmup_counter(warmup_counter){}

                    auto next() -> std::unique_ptr<ActionableResultInterface>
                    {
                        if (this->warmup_counter == this->warmup_window)
                        {
                            return std::make_unique<InternalActionableResult>
                            (
                                this->second_runner->next(),
                                this->first_runner->next()
                            );
                        }

                        this->warmup_counter += 1;

                        return std::make_unique<InternalActionableResult>
                        (
                            this->first_runner->next(),
                            this->second_runner->next()
                        );
                    }
            };
    };

    class ContextOptimizerFactory
    {
        public:

            static auto get_average_binary_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                const size_t WINDOW_SZ      = BinaryBitsetWindowCalculatorGenerator::DEFAULT_WINDOW_SZ;
                const size_t MACHINE_ARR_SZ = size_t{1} << 2;

                if (generator == nullptr)
                {
                    throw std::invalid_argument("bad generator, null");
                }

                std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr = std::make_shared<std::shared_ptr<StatisticalMachineInterface>[]>(MACHINE_ARR_SZ);
                
                for (size_t i = 0u; i < MACHINE_ARR_SZ; ++i)
                {
                    machine_arr[i] = generator->get();

                    if (machine_arr[i] == nullptr)
                    {
                        throw std::invalid_argument("bad machine, null");
                    }
                }

                return std::make_unique<WindowContextGenerator>(std::move(machine_arr),
                                                                std::make_unique<BinaryBitsetWindowCalculatorGenerator>(),
                                                                MACHINE_ARR_SZ);
            }

            static auto get_shape_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                const size_t WINDOW_SZ      = SuffixWindowCalculatorGenerator::DEFAULT_WINDOW_SZ;
                const size_t MACHINE_ARR_SZ = size_t{1} << 2;

                if (generator == nullptr)
                {
                    throw std::invalid_argument("bad generator, null");
                }

                std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr = std::make_shared<std::shared_ptr<StatisticalMachineInterface>[]>(MACHINE_ARR_SZ);

                for (size_t i = 0u; i < MACHINE_ARR_SZ; ++i)
                {
                    machine_arr[i] = generator->get();

                    if (machine_arr[i] == nullptr)
                    {
                        throw std::invalid_argument("bad machine, null");
                    }
                }

                return std::make_unique<WindowContextGenerator>(std::move(machine_arr),
                                                                std::make_unique<SuffixWindowCalculatorGenerator>(),
                                                                MACHINE_ARR_SZ);
            }

            static auto get_long_average_binary_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                const size_t WINDOW_SZ      = size_t{1} << 2;
                const size_t AGGREGATION_SZ = size_t{1} << 3;
                const size_t MACHINE_ARR_SZ = size_t{1} << 4;

                if (generator == nullptr)
                {
                    throw std::invalid_argument("bad generator, null");
                }

                std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr = std::make_shared<std::shared_ptr<StatisticalMachineInterface>[]>(MACHINE_ARR_SZ);

                for (size_t i = 0u; i < MACHINE_ARR_SZ; ++i)
                {
                    machine_arr[i] = generator->get();

                    if (machine_arr[i] == nullptr)
                    {
                        throw std::invalid_argument("bad machine, null");
                    }
                }

                std::unique_ptr<BinaryBitsetWindowCalculatorGenerator> gen = std::make_unique<BinaryBitsetWindowCalculatorGenerator>();
                gen->set_window_size(WINDOW_SZ);
                gen->set_aggregation_size(AGGREGATION_SZ);

                return std::make_unique<WindowContextGenerator>(std::move(machine_arr),
                                                                std::move(gen),
                                                                MACHINE_ARR_SZ);
            }

            static auto get_long_shape_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                const size_t WINDOW_SZ      = size_t{1} << 2;
                const size_t AGGREGATION_SZ = size_t{1} << 3;
                const size_t MACHINE_ARR_SZ = size_t{1} << 4;

                if (generator == nullptr)
                {
                    throw std::invalid_argument("bad generator, null");
                }

                std::shared_ptr<std::shared_ptr<StatisticalMachineInterface>[]> machine_arr = std::make_shared<std::shared_ptr<StatisticalMachineInterface>[]>(MACHINE_ARR_SZ);

                for (size_t i = 0u; i < MACHINE_ARR_SZ; ++i)
                {
                    machine_arr[i] = generator->get();

                    if (machine_arr[i] == nullptr)
                    {
                        throw std::invalid_argument("bad machine, null");
                    }
                }

                std::unique_ptr<SuffixWindowCalculatorGenerator> gen = std::make_unique<SuffixWindowCalculatorGenerator>();
                gen->set_window_size(WINDOW_SZ);
                gen->set_aggregation_size(AGGREGATION_SZ);

                return std::make_unique<WindowContextGenerator>(std::move(machine_arr),
                                                                std::move(gen),
                                                                MACHINE_ARR_SZ);
            }

            static auto get_best_binary_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                // return get_average_binary_progress_context_optimizer(generator);
                return std::make_unique<WarmWindowContextGenerator>(get_average_binary_progress_context_optimizer(generator),
                                                                    get_long_average_binary_progress_context_optimizer(generator),
                                                                    32u);
            }

            static auto get_best_shape_progress_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<IterationContextGeneratorInterface>
            {
                // return get_shape_progress_context_optimizer(generator);
                return std::make_unique<WarmWindowContextGenerator>(get_shape_progress_context_optimizer(generator),
                                                                    get_long_shape_progress_context_optimizer(generator),
                                                                    32u);
            }
    };
}

#endif