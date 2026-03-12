#ifndef __FUNCTION_CONTEXT_OPTIMIZER_H__
#define __FUNCTION_CONTEXT_OPTIMIZER_H__

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
#include <array>

//in this component, we'd try to extract the traits of the function, pick a sample point
//we'd have to have an operating window size, and a fixed discretization of the window size, just like stock

//think like this, we'd want to inspect the window 100, window 10, window 1, window 0.1, window 0.01
//discretization = 10 means that 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 1, 2, 3, ..., 10, ...

//how can we clue the context without exploding the space?
//such is that the context is fair and our optimization is reasonable

//we'd try to work on this today
//this is hard

//today we'd work on the theory of function context

//as we are well-awared, the function context in the sense of finding zeros is the up and down of a certain derivative value after a certain time
//let's say that a function has f(x), f'(x), f''(x), f'''(x), ...

//if f(x) is ever increasing, it's a radix of exponential function
//if f(x) is positive, then negative, then the acceleration pulled the string of the velocity which pulled the string of the position

//an easy way to radix a function is to punch anchor points for position, and sort the points to get a sense of the function
//we'd settle this being the solution before we are circling back for optimizations

namespace function_context_optimizer
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
            virtual auto get() -> std::unique_ptr<StatisticalMachineInterface> = 0;
    };

    class FunctionInterface
    {
        public:

            virtual ~FunctionInterface() = default;
            virtual auto f(ctx_float_t x) -> ctx_float_t = 0;
    };

    class FunctionRadixerInterface
    {
        public:

            virtual ~FunctionRadixerInterface() = default;
            virtual auto enumerate(FunctionInterface& f) -> size_t = 0;
            virtual auto enumeration_size() -> size_t = 0;
    };

    class FunctionContextOptimizerInterface
    {
        public:

            virtual ~FunctionContextOptimizerInterface() = default;
            virtual auto optimize_context(FunctionInterface& f) -> std::shared_ptr<StatisticalMachineInterface> = 0;
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

            auto space_size(const std::vector<size_t>& image) -> __uint128_t
            {
                return this->factorial(image.size());
            }
    };

    struct SuffixCompressionArgument
    {
        std::vector<size_t> suffix;
        std::vector<size_t> image;
    };

    class MultipleSuffixArrayHasher
    {
        public:

            auto hash(const std::vector<SuffixCompressionArgument>& argument_vec) -> __uint128_t
            {
                __uint128_t rs = 0u;

                for (const auto& arg: argument_vec)
                {
                    rs *= SuffixCompressor{}.space_size(arg.image);
                    rs += SuffixCompressor{}.compress(arg.suffix, arg.image);
                }

                return rs;
            }
    };

    class SuffixContextExtractor
    {
        public:

            auto extract(const std::vector<ctx_float_t>& ctx_vec) -> std::vector<size_t>
            {
                for (const ctx_float_t& e: ctx_vec)
                {
                    if (std::isnan(e))
                    {
                        throw std::invalid_argument("bad number, NaN");
                    }
                }

                std::vector<std::pair<size_t, ctx_float_t>> enumerated_vec = stdx::enumerate_vector(ctx_vec);
                auto less = [](const auto& lhs, const auto& rhs)
                {
                    return lhs.second < rhs.second;
                };

                std::sort(enumerated_vec.begin(), enumerated_vec.end(), less);
                std::vector<size_t> suffix_vec{};
                std::transform(enumerated_vec.begin(), enumerated_vec.end(), std::back_inserter(suffix_vec), [](const auto& e){return e.first;});

                return suffix_vec;
            }

            auto get_suffix_image_for_size_of(size_t sz) -> std::vector<size_t>
            {
                std::vector<size_t> image(sz);
                std::iota(image.begin(), image.end(), 0u);

                return image;
            }
    };

    class EasyPointExponentialRadixer: public virtual FunctionRadixerInterface
    {
        private:

            size_t enumeration_sz;

            static inline constexpr ctx_float_t LOW_POINT_BASE  = 1.1;
            static inline constexpr ctx_float_t HIGH_POINT_BASE = 10;

            static inline constexpr size_t LOW_POINT_SZ         = 3u;
            static inline constexpr size_t HIGH_POINT_SZ        = 3u;

            static inline constexpr std::array<ctx_float_t, LOW_POINT_SZ> LOW_POINT_DOMAIN_ARRAY = []
            {
                std::array<ctx_float_t, LOW_POINT_SZ> rs{};
                ctx_float_t e = 1;

                for (size_t i = 0u; i < rs.size(); ++i)
                {
                    rs[i]   = e;
                    e       *= LOW_POINT_BASE;
                }

                return rs;
            }();

            static inline constexpr std::array<ctx_float_t, HIGH_POINT_SZ> HIGH_POINT_DOMAIN_ARRAY = []
            {
                std::array<ctx_float_t, HIGH_POINT_SZ> rs{};
                ctx_float_t e = 1;

                for (size_t i = 0u; i < rs.size(); ++i)
                {
                    rs[i]   = e;
                    e       *= HIGH_POINT_BASE;
                }

                return rs;
            }();

        public:

            static inline constexpr size_t BEST_ENUMERATION_SZ = 36u;

            EasyPointExponentialRadixer(size_t enumeration_sz): enumeration_sz(stdx::safe_non_zero_access(enumeration_sz)){}

            auto enumerate(FunctionInterface& f) -> size_t
            {
                std::array<ctx_float_t, LOW_POINT_SZ> low_point_projection_array{};

                for (size_t i = 0u; i < LOW_POINT_SZ; ++i)
                {
                    low_point_projection_array[i] = f.f(LOW_POINT_DOMAIN_ARRAY[i]);
                    
                    if (std::isnan(low_point_projection_array[i]))
                    {
                        return 0u;
                    }
                }

                std::array<ctx_float_t, HIGH_POINT_SZ> high_point_projection_array{};

                for (size_t i = 0u; i < HIGH_POINT_SZ; ++i)
                {
                    high_point_projection_array[i] = f.f(HIGH_POINT_DOMAIN_ARRAY[i]);

                    if (std::isnan(high_point_projection_array[i]))
                    {
                        return 0u;
                    }
                }

                SuffixCompressionArgument low_suffix_argument
                {
                    .suffix = SuffixContextExtractor{}.extract({low_point_projection_array.begin(), low_point_projection_array.end()}),
                    .image  = SuffixContextExtractor{}.get_suffix_image_for_size_of(LOW_POINT_SZ)
                };

                SuffixCompressionArgument high_suffix_argument
                {
                    .suffix = SuffixContextExtractor{}.extract({high_point_projection_array.begin(), high_point_projection_array.end()}),
                    .image  = SuffixContextExtractor{}.get_suffix_image_for_size_of(HIGH_POINT_SZ)
                };

                return MultipleSuffixArrayHasher{}.hash({low_suffix_argument, high_suffix_argument}) % this->enumeration_sz;
            }

            auto enumeration_size() -> size_t
            {
                return this->enumeration_sz;
            }
    };

    class EnumeratableFunctionContextOptimizer: public virtual FunctionContextOptimizerInterface
    {
        private:

            std::unique_ptr<FunctionRadixerInterface> function_radixer;
            std::vector<std::shared_ptr<StatisticalMachineInterface>> statistical_machine_vec;
        
        public:

            EnumeratableFunctionContextOptimizer(std::unique_ptr<FunctionRadixerInterface> function_radixer,
                                                 std::vector<std::shared_ptr<StatisticalMachineInterface>> statistical_machine_vec) noexcept: function_radixer(std::move(function_radixer)),
                                                                                                                                              statistical_machine_vec(std::move(statistical_machine_vec)){}
            
            auto optimize_context(FunctionInterface& f) -> std::shared_ptr<StatisticalMachineInterface>
            {
                size_t slot = this->function_radixer->enumerate(f);

                if (slot >= this->statistical_machine_vec.size())
                {
                    std::abort();
                }

                return this->statistical_machine_vec[slot];
            }
    };

    class ContextOptimizerFactory
    {
        public:

            static auto get_low_exponential_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<FunctionContextOptimizerInterface>
            {
                if (generator == nullptr)
                {
                    throw std::invalid_argument("bad generator, null");
                }

                std::vector<std::shared_ptr<StatisticalMachineInterface>> statistical_machine_vec{};

                for (size_t i = 0u; i < EasyPointExponentialRadixer::BEST_ENUMERATION_SZ; ++i)
                {
                    statistical_machine_vec.push_back(generator->get());
                }

                return std::make_unique<EnumeratableFunctionContextOptimizer>(std::make_unique<EasyPointExponentialRadixer>(statistical_machine_vec.size()),
                                                                              std::move(statistical_machine_vec));
            }

            static auto get_dense_exponential_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<FunctionContextOptimizerInterface>
            {
                return get_low_exponential_context_optimizer(std::move(generator));
            }

            static auto get_best_context_optimizer(std::shared_ptr<StatisticalMachineGeneratorInterface> generator) -> std::unique_ptr<FunctionContextOptimizerInterface>
            {
                return get_low_exponential_context_optimizer(std::move(generator));
            }
    };
}

#endif