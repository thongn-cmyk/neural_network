//HEADER_CONTROL 2

#ifndef __LOCAL_OPTIMALITY_APPROXIMATOR_H__
#define __LOCAL_OPTIMALITY_APPROXIMATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include <cmath>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <random>
#include <algorithm>
#include <chrono>
#include <array>
#include <type_traits>
#include <numbers>
#include "stdx.h"
#include "conventional_randomizer.h"
#include "local_optimality_approximator_interface.h"
#include "float_def.h"

namespace local_optimality_approximator
{
    using std_float_t = float_def::std_float_t;

    template <class FloatType>
    constexpr auto deviation_clamp(FloatType x, FloatType a) -> FloatType
    {
        return stdx::deviation_clamp(x, a);
    }

    template <class FloatType, class Function>
    constexpr auto get_derivative_at(Function&& f,
                                     FloatType x,
                                     size_t derivative_order,
                                     FloatType x_a = stdx::to_precise_float_conversion_initializer(0.001)) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (derivative_order == 0u)
        {
            return f(x);
        }

        FloatType y0    = get_derivative_at(f, x, derivative_order - 1u, x_a);

        if (std::isnan(y0))
        {
            return stdx::generic_nan();
        }

        FloatType y1    = get_derivative_at(f, x + x_a, derivative_order - 1u, x_a);

        if (std::isnan(y1))
        {
            return stdx::generic_nan();
        }

        FloatType slope = (y1 - y0) / x_a;

        return slope;
    }

    template <class T = std_float_t>
    static inline constexpr auto get_derivative_at_lambda = []<class ...Args>(Args&& ...args)
    {
        return get_derivative_at<T>(std::forward<Args>(args)...);
    };

    template <class Function, class FloatType, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto newton_first_order_approx(Function&& f,
                                             FloatType x,
                                             size_t iteration_sz,
                                             const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        FloatType cand                      = x;

        std::optional<FloatType> best_x     = std::nullopt;
        std::optional<FloatType> best_y     = std::nullopt;

        for (size_t i = 0u; i < iteration_sz; ++i)
        {
            if (std::isnan(cand))
            {
                break;
            }

            FloatType slope             = derivative_extractor(f, cand, 1u);

            if (std::isnan(slope))
            {
                break;
            }

            FloatType elevated_value    = f(cand);

            if (std::isnan(elevated_value))
            {
                break;
            }

            if (!best_y.has_value())
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            if (std::abs(best_y.value()) > std::abs(elevated_value))
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            FloatType previous_delta_x  = elevated_value / slope;
            cand                        -= previous_delta_x;
        }

        if (!best_x.has_value())
        {
            return x;
        }

        return best_x.value();
    }

    template <class Function, class FloatType, class XDeviationGenerator, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto dynamic_short_sight_newton_first_order_approx(Function&& f,
                                                                 FloatType x,
                                                                 size_t iteration_sz,
                                                                 XDeviationGenerator&& deviation_generator,
                                                                 const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        FloatType cand                      = x;

        std::optional<FloatType> best_x     = std::nullopt;
        std::optional<FloatType> best_y     = std::nullopt;

        for (size_t i = 0u; i < iteration_sz; ++i)
        {
            if (std::isnan(cand))
            {
                break;
            }

            FloatType slope             = derivative_extractor(f, cand, 1u);

            if (std::isnan(slope))
            {
                break;
            }

            FloatType elevated_value    = f(cand);

            if (std::isnan(elevated_value))
            {
                break;
            }

            if (!best_y.has_value())
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            if (std::abs(best_y.value()) > std::abs(elevated_value))
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            FloatType previous_delta_x  = elevated_value / slope;
            cand                        -= deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(i))>>(previous_delta_x, deviation_generator(i));
        }

        if (!best_x.has_value())
        {
            return x;
        }

        return best_x.value();
    }

    template <class Function, class FloatType, class XDeviationGenerator, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto dynamic_short_sight_and_slope_newton_first_order_approx(Function&& f,
                                                                           FloatType x,
                                                                           size_t iteration_sz,
                                                                           XDeviationGenerator&& deviation_generator,
                                                                           const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        FloatType cand                      = x;

        std::optional<FloatType> best_x     = std::nullopt;
        std::optional<FloatType> best_y     = std::nullopt;

        for (size_t i = 0u; i < iteration_sz; ++i)
        {
            if (std::isnan(cand))
            {
                break;
            }

            FloatType slope             = derivative_extractor(f, cand, 1u, i);

            if (std::isnan(slope))
            {
                break;
            }

            FloatType elevated_value    = f(cand);

            if (std::isnan(elevated_value))
            {
                break;
            }

            if (!best_y.has_value())
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            if (std::abs(best_y.value()) > std::abs(elevated_value))
            {
                best_y  = elevated_value;
                best_x  = cand;
            }

            FloatType previous_delta_x  = elevated_value / slope;
            cand                        -= deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(i))>>(previous_delta_x, deviation_generator(i));
        }

        if (!best_x.has_value())
        {
            return x;
        }

        return best_x.value();
    }

    template <class Function, class FloatType, class FloatType2, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto short_sight_newton_first_order_approx(Function&& f,
                                                         FloatType x,
                                                         size_t iteration_sz,
                                                         FloatType2 reliable_x_deviation,
                                                         const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<FloatType2>);

        return dynamic_short_sight_newton_first_order_approx(f,
                                                             x,
                                                             iteration_sz,
                                                             [=](size_t){return reliable_x_deviation;},
                                                             derivative_extractor);
    }

    template <class Function, class FloatType, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto newton_second_order_approx(Function&& f,
                                              FloatType x,
                                              size_t iteration_sz,
                                              const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>,
                                              const std::unique_ptr<std::unordered_map<FloatType, FloatType>>& cache_map = std::make_unique<std::unordered_map<FloatType, FloatType>>()) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (iteration_sz == 0u)
        {
            return x;
        }

        if (std::isnan(x))
        {
            return x;
        }

        if (auto cache_ptr = cache_map->find(x); cache_ptr != cache_map->end())
        {
            return cache_ptr->second;
        }

        FloatType a         = derivative_extractor(f, x, 2u) / static_cast<FloatType>(2);

        if (std::isnan(a))
        {
            return x;
        }

        FloatType b         = derivative_extractor(f, x, 1u);

        if (std::isnan(b))
        {
            return x;
        }

        FloatType c         = derivative_extractor(f, x, 0u);

        if (std::isnan(c))
        {
            return x;
        }

        FloatType d2        = b * b - 4 * a * c;

        if (std::isnan(d2))
        {
            return x;
        }

        FloatType d;

        if (d2 < 0)
        {
            c   = static_cast<FloatType>(1) / static_cast<FloatType>(4) * b * b / a;

            if (std::isnan(c))
            {
                return x;
            }

            d2  = 0;
            d   = 0;
        }
        else
        {
            d   = std::sqrt(d2);            
        }

        if (std::isnan(d))
        {
            return x;
        }

        FloatType rel_x1    = (-b + d) / (a * 2);
        FloatType rel_x2    = (-b - d) / (a * 2);

        FloatType x1        = x + rel_x1;
        FloatType x2        = x + rel_x2;

        FloatType xx1       = newton_second_order_approx(f, x1, iteration_sz - 1u, derivative_extractor, cache_map);
        FloatType xx2       = newton_second_order_approx(f, x2, iteration_sz - 1u, derivative_extractor, cache_map);

        auto is_less = [](const auto& lhs, const auto& rhs)
        {
            return stdx::nan_cmp(lhs.first, rhs.first) < 0;
        };

        std::array<std::pair<FloatType, FloatType>, 5u> cand_arr{std::make_pair(std::abs(f(x)), x),
                                                                 std::make_pair(std::abs(f(x1)), x1),
                                                                 std::make_pair(std::abs(f(x2)), x2),
                                                                 std::make_pair(std::abs(f(xx1)), xx1),
                                                                 std::make_pair(std::abs(f(xx2)), xx2)};

        auto rs = std::min_element(cand_arr.begin(), cand_arr.end(), is_less);

        if (std::isnan(rs->first))
        {
            cache_map->insert({x, x});
            return x;
        }

        cache_map->insert({x, rs->second});
        return rs->second;
    }

    template <class Function, class FloatType, class XDeviationGenerator, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto dynamic_short_sight_newton_second_order_approx(Function&& f,
                                                                  FloatType x,
                                                                  size_t iteration_sz,
                                                                  XDeviationGenerator&& deviation_generator,
                                                                  const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>,
                                                                  const std::unique_ptr<std::unordered_map<FloatType, FloatType>>& cache_map = std::make_unique<std::unordered_map<FloatType, FloatType>>()) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (iteration_sz == 0u)
        {
            return x;
        }

        if (std::isnan(x))
        {
            return x;
        }

        if (auto cache_ptr = cache_map->find(x); cache_ptr != cache_map->end())
        {
            return cache_ptr->second;
        }

        FloatType a         = derivative_extractor(f, x, 2u) / static_cast<FloatType>(2);

        if (std::isnan(a))
        {
            return x;
        }

        FloatType b         = derivative_extractor(f, x, 1u);

        if (std::isnan(b))
        {
            return x;
        }

        FloatType c         = derivative_extractor(f, x, 0u);

        if (std::isnan(c))
        {
            return x;
        }

        FloatType d2        = b * b - 4 * a * c;

        if (std::isnan(d2))
        {
            return x;
        }

        FloatType d;

        if (d2 < 0)
        {
            c   = static_cast<FloatType>(1) / static_cast<FloatType>(4) * b * b / a;

            if (std::isnan(c))
            {
                return x;
            }

            d2  = 0;
            d   = 0;
        }
        else
        {
            d   = std::sqrt(d2);            
        }

        if (std::isnan(d))
        {
            return x;
        }

        FloatType tentative_rel_x1  = (-b + d) / (a * 2);
        FloatType tentative_rel_x2  = (-b - d) / (a * 2);

        FloatType x1                = x + deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(iteration_sz))>>(tentative_rel_x1, deviation_generator(iteration_sz));
        FloatType x2                = x + deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(iteration_sz))>>(tentative_rel_x2, deviation_generator(iteration_sz));

        FloatType xx1               = dynamic_short_sight_newton_second_order_approx(f, x1, iteration_sz - 1u, deviation_generator, derivative_extractor, cache_map);
        FloatType xx2               = dynamic_short_sight_newton_second_order_approx(f, x2, iteration_sz - 1u, deviation_generator, derivative_extractor, cache_map);

        auto is_less = [](const auto& lhs, const auto& rhs)
        {
            return stdx::nan_cmp(lhs.first, rhs.first) < 0;
        };

        std::array<std::pair<FloatType, FloatType>, 5u> cand_arr{std::make_pair(std::abs(f(x)), x),
                                                                 std::make_pair(std::abs(f(x1)), x1),
                                                                 std::make_pair(std::abs(f(x2)), x2),
                                                                 std::make_pair(std::abs(f(xx1)), xx1),
                                                                 std::make_pair(std::abs(f(xx2)), xx2)};

        auto rs = std::min_element(cand_arr.begin(), cand_arr.end(), is_less);

        if (std::isnan(rs->first))
        {
            cache_map->insert({x, x});
            return x;
        }

        cache_map->insert({x, rs->second});
        return rs->second;
    }

    template <class Function, class FloatType, class XDeviationGenerator, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto dynamic_short_sight_and_slope_newton_second_order_approx(Function&& f,
                                                                            FloatType x,
                                                                            size_t iteration_sz,
                                                                            XDeviationGenerator&& deviation_generator,
                                                                            const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>,
                                                                            const std::unique_ptr<std::unordered_map<FloatType, FloatType>>& cache_map = std::make_unique<std::unordered_map<FloatType, FloatType>>()) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);

        if (iteration_sz == 0u)
        {
            return x;
        }

        if (std::isnan(x))
        {
            return x;
        }

        if (auto cache_ptr = cache_map->find(x); cache_ptr != cache_map->end())
        {
            return cache_ptr->second;
        }

        FloatType a         = derivative_extractor(f, x, 2u, iteration_sz) / static_cast<FloatType>(2);

        if (std::isnan(a))
        {
            return x;
        }

        FloatType b         = derivative_extractor(f, x, 1u, iteration_sz);

        if (std::isnan(b))
        {
            return x;
        }

        FloatType c         = derivative_extractor(f, x, 0u, iteration_sz);

        if (std::isnan(c))
        {
            return x;
        }

        FloatType d2        = b * b - 4 * a * c;

        if (std::isnan(d2))
        {
            return x;
        }

        FloatType d;

        if (d2 < 0)
        {
            c   = static_cast<FloatType>(1) / static_cast<FloatType>(4) * b * b / a;

            if (std::isnan(c))
            {
                return x;
            }

            d2  = 0;
            d   = 0;
        }
        else
        {
            d   = std::sqrt(d2);            
        }

        if (std::isnan(d))
        {
            return x;
        }

        FloatType tentative_rel_x1  = (-b + d) / (a * 2);
        FloatType tentative_rel_x2  = (-b - d) / (a * 2);

        FloatType x1                = x + deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(iteration_sz))>>(tentative_rel_x1, deviation_generator(iteration_sz));
        FloatType x2                = x + deviation_clamp<float_def::most_byte_width_float_t<FloatType, decltype(deviation_generator(iteration_sz))>>(tentative_rel_x2, deviation_generator(iteration_sz));

        FloatType xx1               = dynamic_short_sight_and_slope_newton_second_order_approx(f, x1, iteration_sz - 1u, deviation_generator, derivative_extractor, cache_map);
        FloatType xx2               = dynamic_short_sight_and_slope_newton_second_order_approx(f, x2, iteration_sz - 1u, deviation_generator, derivative_extractor, cache_map);

        auto is_less = [](const auto& lhs, const auto& rhs)
        {
            return stdx::nan_cmp(lhs.first, rhs.first) < 0;
        };

        std::array<std::pair<FloatType, FloatType>, 5u> cand_arr{std::make_pair(std::abs(f(x)), x),
                                                                 std::make_pair(std::abs(f(x1)), x1),
                                                                 std::make_pair(std::abs(f(x2)), x2),
                                                                 std::make_pair(std::abs(f(xx1)), xx1),
                                                                 std::make_pair(std::abs(f(xx2)), xx2)};

        auto rs = std::min_element(cand_arr.begin(), cand_arr.end(), is_less);

        if (std::isnan(rs->first))
        {
            cache_map->insert({x, x});
            return x;
        }

        cache_map->insert({x, rs->second});
        return rs->second;
    }

    template <class Function, class FloatType, class FloatType2, class DerivativeExtractor = decltype(get_derivative_at_lambda<FloatType>)>
    constexpr auto short_sight_newton_second_order_approx(Function&& f,
                                                          FloatType x,
                                                          size_t iteration_sz,
                                                          FloatType2 reliable_x_deviation,
                                                          const DerivativeExtractor& derivative_extractor = get_derivative_at_lambda<FloatType>,
                                                          const std::unique_ptr<std::unordered_map<FloatType, FloatType>>& cache_map = std::make_unique<std::unordered_map<FloatType, FloatType>>()) -> FloatType
    {
        static_assert(std::is_floating_point_v<FloatType>);
        static_assert(std::is_floating_point_v<FloatType2>);

        return dynamic_short_sight_newton_second_order_approx(f,
                                                              x,
                                                              iteration_sz,
                                                              [=](size_t){return reliable_x_deviation;},
                                                              derivative_extractor,
                                                              cache_map);
    }

    template <class PromotedFloatType = std_float_t>
    class FirstOrderNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            FirstOrderNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                        std_float_t x_a) noexcept: iteration_sz(iteration_sz),
                                                                                   x_a(x_a){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

                return newton_first_order_approx(lambda_wrapped_time_machine,
                                                 static_cast<PromotedFloatType>(x),
                                                 this->iteration_sz,
                                                 derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class FirstOrderShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            FirstOrderShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                  std_float_t x_a,
                                                                  std_float_t reliable_x_deviation) noexcept: iteration_sz(iteration_sz),
                                                                                                              x_a(x_a),
                                                                                                              reliable_x_deviation(reliable_x_deviation){}
            
            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

                return short_sight_newton_first_order_approx(lambda_wrapped_time_machine,
                                                             static_cast<PromotedFloatType>(x),
                                                             this->iteration_sz,
                                                             this->reliable_x_deviation,
                                                             derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class FirstOrderChaoticShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;
        
        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            FirstOrderChaoticShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                         std_float_t x_a,
                                                                         std_float_t reliable_x_deviation_range,
                                                                         conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                    x_a(x_a),
                                                                                                                                                    reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                    randomizer(std::move(randomizer)){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func        = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);
                auto x_deviation_generator  = [&](size_t) noexcept
                {
                    return static_cast<PromotedFloatType>(this->randomizer.ld_randomize_percentage_focal() * this->reliable_x_deviation_range);
                };

                return dynamic_short_sight_newton_first_order_approx(lambda_wrapped_time_machine,
                                                                     static_cast<PromotedFloatType>(x),
                                                                     this->iteration_sz,
                                                                     x_deviation_generator,
                                                                     derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class FirstOrderConvergingShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            FirstOrderConvergingShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                            std_float_t x_a,
                                                                            std_float_t reliable_x_deviation_range,
                                                                            conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                       x_a(x_a),
                                                                                                                                                       reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                       randomizer(std::move(randomizer)){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                std::vector<PromotedFloatType> deviation_range_vec  = this->get_deviation_range_vector();
                auto derivative_func                                = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);
                auto x_deviation_generator                          = [&](size_t idx) noexcept
                {
                    if (idx >= deviation_range_vec.size())
                    {
                        std::abort();
                    }

                    return deviation_range_vec[idx];
                };

                return dynamic_short_sight_newton_first_order_approx(lambda_wrapped_time_machine,
                                                                     static_cast<PromotedFloatType>(x),
                                                                     this->iteration_sz,
                                                                     x_deviation_generator,
                                                                     derivative_func);
            }

        private:

            auto get_deviation_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_deviation_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class FirstOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t reliable_x_a_range;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            FirstOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                                    std_float_t reliable_x_a_range,
                                                                                    std_float_t reliable_x_deviation_range,
                                                                                    conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                               reliable_x_a_range(reliable_x_a_range),
                                                                                                                                                               reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                               randomizer(std::move(randomizer)){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                std::vector<PromotedFloatType> x_a_range_vec        = this->get_a_range_vector();
                std::vector<PromotedFloatType> deviation_range_vec  = this->get_deviation_range_vector();
                auto derivative_func                                = [&]<class Function>(Function&& f, PromotedFloatType x, size_t derivative_order, size_t i)
                {
                    if (i >= x_a_range_vec.size())
                    {
                        std::abort();
                    }

                    PromotedFloatType x_a = x_a_range_vec[i];

                    return get_derivative_at(f, x, derivative_order, x_a);
                };

                auto x_deviation_generator                          = [&](size_t i) noexcept
                {
                    if (i >= deviation_range_vec.size())
                    {
                        std::abort();
                    }

                    return deviation_range_vec[i];
                };

                return dynamic_short_sight_and_slope_newton_first_order_approx(lambda_wrapped_time_machine,
                                                                               x,
                                                                               this->iteration_sz,
                                                                               x_deviation_generator,
                                                                               derivative_func);
            }
        
        private:

            auto get_a_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_a_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }

            auto get_deviation_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_deviation_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class SecondOrderNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            SecondOrderNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                         std_float_t x_a) noexcept: iteration_sz(iteration_sz),
                                                                                    x_a(x_a){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

                return newton_second_order_approx(lambda_wrapped_time_machine,
                                                  static_cast<PromotedFloatType>(x),
                                                  this->iteration_sz,
                                                  derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class SecondOrderShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            SecondOrderShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                   std_float_t x_a,
                                                                   std_float_t reliable_x_deviation) noexcept: iteration_sz(iteration_sz),
                                                                                                               x_a(x_a),
                                                                                                               reliable_x_deviation(reliable_x_deviation){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);

                return short_sight_newton_second_order_approx(lambda_wrapped_time_machine,
                                                              static_cast<PromotedFloatType>(x),
                                                              this->iteration_sz,
                                                              this->reliable_x_deviation,
                                                              derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class SecondOrderChaoticShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;
        
        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            SecondOrderChaoticShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                          std_float_t x_a,
                                                                          std_float_t reliable_x_deviation_range,
                                                                          conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                     x_a(x_a),
                                                                                                                                                     reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                     randomizer(std::move(randomizer)){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                auto derivative_func        = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);
                auto x_deviation_generator  = [&](size_t) noexcept
                {
                    return static_cast<PromotedFloatType>(this->randomizer.ld_randomize_percentage_focal() * this->reliable_x_deviation_range);
                };

                return dynamic_short_sight_newton_second_order_approx(lambda_wrapped_time_machine,
                                                                      static_cast<PromotedFloatType>(x),
                                                                      this->iteration_sz,
                                                                      x_deviation_generator,
                                                                      derivative_func);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class SecondOrderConvergingShortSightNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t x_a;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;
        
        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            SecondOrderConvergingShortSightNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                             std_float_t x_a,
                                                                             std_float_t reliable_x_deviation_range,
                                                                             conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                        x_a(x_a),
                                                                                                                                                        reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                        randomizer(std::move(randomizer)){}
            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };
                
                std::vector<PromotedFloatType> deviation_range_vec  = this->get_deviation_range_vector();
                auto derivative_func                                = std::bind_back(get_derivative_at_lambda<PromotedFloatType>, this->x_a);
                auto x_deviation_generator                          = [&](size_t sz_pointer) noexcept
                {
                    if (sz_pointer == 0u)
                    {
                        std::abort();
                    }

                    if (sz_pointer > this->iteration_sz)
                    {
                        std::abort();
                    }

                    size_t idx = this->iteration_sz - sz_pointer;

                    return deviation_range_vec[idx];
                };

                return dynamic_short_sight_newton_second_order_approx(lambda_wrapped_time_machine,
                                                                      static_cast<PromotedFloatType>(x),
                                                                      this->iteration_sz,
                                                                      x_deviation_generator,
                                                                      derivative_func);
            }

        private:

            auto get_deviation_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_deviation_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class SecondOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator: public virtual OptimalityApproximatorInterface
    {
        private:

            size_t iteration_sz;
            std_float_t reliable_x_a_range;
            std_float_t reliable_x_deviation_range;
            conventional_randomizer::ApplicationRandomizerObject randomizer;
        
        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            SecondOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator(size_t iteration_sz,
                                                                                     std_float_t reliable_x_a_range,
                                                                                     std_float_t reliable_x_deviation_range,
                                                                                     conventional_randomizer::ApplicationRandomizerObject randomizer) noexcept: iteration_sz(iteration_sz),
                                                                                                                                                                reliable_x_a_range(reliable_x_a_range),
                                                                                                                                                                reliable_x_deviation_range(reliable_x_deviation_range),
                                                                                                                                                                randomizer(std::move(randomizer)){}

            auto approx_x(time_machine::TimeMachineInterface& time_machine, std_float_t x) -> std_float_t
            {
                auto lambda_wrapped_time_machine = [&time_machine](std_float_t t)
                {
                    return time_machine.f(t);
                };

                std::vector<PromotedFloatType> x_a_range_vec        = this->get_x_a_range_vector();
                std::vector<PromotedFloatType> deviation_range_vec  = this->get_deviation_range_vector();
                auto derivative_func                                = [&]<class Function>(Function&& func, PromotedFloatType x, size_t derivative_order, size_t iteration_ptr)
                {
                    size_t i = this->iteration_sz - iteration_ptr;

                    if (i >= x_a_range_vec.size())
                    {
                        std::abort();
                    }

                    PromotedFloatType x_a = x_a_range_vec[i];

                    return get_derivative_at(func, x, derivative_order, x_a);
                };
                auto x_deviation_generator                          = [&](size_t iteration_ptr) noexcept
                {
                    size_t i = this->iteration_sz - iteration_ptr;

                    if (i >= deviation_range_vec.size())
                    {
                        std::abort();
                    }

                    return deviation_range_vec[i];
                };

                return dynamic_short_sight_and_slope_newton_second_order_approx(lambda_wrapped_time_machine,
                                                                                x,
                                                                                this->iteration_sz,
                                                                                x_deviation_generator,
                                                                                derivative_func);
            }
        
        private:

            auto get_x_a_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_a_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }

            auto get_deviation_range_vector() -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->iteration_sz);

                for (size_t i = 0u; i < this->iteration_sz; ++i)
                {
                    if (i == 0u)
                    {
                        rs[i] = this->reliable_x_deviation_range;
                    }
                    else
                    {
                        rs[i] = rs[i - 1] * static_cast<PromotedFloatType>(1 - this->randomizer.ld_randomize_percentage_focal());
                    }
                }

                return rs;
            }
    };

    class OptimalityApproximatorFactory
    {
        private:

            struct Signature{};
            using Randomizer = conventional_randomizer::RandomizerFacility<Signature>;

        public:

            template <class PromotedFloatType = std_float_t>
            static auto get_first_order_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST     = 0u;
                const size_t ITERATION_SZ_LAST      = 4u;

                const std_float_t MIN_X_A           = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A           = stdx::to_precise_float_conversion_initializer<double>(1);

                size_t iteration_sz                 = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, deviation out of range");
                }

                return std::make_unique<FirstOrderNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz, x_a);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_first_order_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                         std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation) )
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }
                
                return std::make_unique<FirstOrderShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                  x_a,
                                                                                                                  reliable_x_deviation);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_first_order_chaotic_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                 std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation) )
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<FirstOrderChaoticShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                         x_a,
                                                                                                                         reliable_x_deviation,
                                                                                                                         conventional_randomizer::ApplicationRandomizerObject());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_first_order_converging_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                    std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation) )
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<FirstOrderConvergingShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                            x_a,
                                                                                                                            reliable_x_deviation,
                                                                                                                            conventional_randomizer::ApplicationRandomizerObject());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_first_order_converging_short_sight_and_slope_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                              std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation) )
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<FirstOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                                    x_a,
                                                                                                                                    reliable_x_deviation,
                                                                                                                                    conventional_randomizer::ApplicationRandomizerObject());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_second_order_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST     = 0u;
                const size_t ITERATION_SZ_LAST      = 4u;

                const std_float_t MIN_X_A           = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A           = stdx::to_precise_float_conversion_initializer<double>(1);

                size_t iteration_sz                 = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                return std::make_unique<SecondOrderNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz, x_a);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_second_order_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                          std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation))
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<SecondOrderShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                   x_a,
                                                                                                                   reliable_x_deviation);

            }

            template <class PromotedFloatType = std_float_t>
            static auto get_second_order_chaotic_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                  std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation))
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<SecondOrderChaoticShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                          x_a,
                                                                                                                          reliable_x_deviation,
                                                                                                                          conventional_randomizer::ApplicationRandomizerObject());

            }

            template <class PromotedFloatType = std_float_t>
            static auto get_second_order_converging_short_sight_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                     std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation))
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<SecondOrderConvergingShortSightNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                             x_a,
                                                                                                                             reliable_x_deviation,
                                                                                                                             conventional_randomizer::ApplicationRandomizerObject());

            }

            template <class PromotedFloatType = std_float_t>
            static auto get_second_order_converging_short_sight_and_slope_newton_naive_optimality_approximator(std_float_t x_a = stdx::to_precise_float_conversion_initializer<double>(0.001),
                                                                                                               std_float_t reliable_x_deviation = stdx::to_precise_float_conversion_initializer<double>(10)) -> std::unique_ptr<OptimalityApproximatorInterface>
            {
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t ITERATION_SZ_FIRST             = 0u;
                const size_t ITERATION_SZ_LAST              = 4u;

                const std_float_t MIN_X_A                   = std::numeric_limits<std_float_t>::min();
                const std_float_t MAX_X_A                   = stdx::to_precise_float_conversion_initializer<double>(1);

                const std_float_t MIN_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(0);
                const std_float_t MAX_RELIABLE_X_DEVIATION  = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

                size_t iteration_sz                         = size_t{1} << Randomizer::randomize_uint(ITERATION_SZ_FIRST, ITERATION_SZ_LAST);

                if (std::isnan(x_a))
                {
                    throw std::invalid_argument("bad derivative deviation, NaN");
                }

                if (std::clamp(x_a, MIN_X_A, MAX_X_A) != x_a)
                {
                    throw std::invalid_argument("bad derivative deviation, value out of range");
                }

                if (std::isnan(reliable_x_deviation))
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, NaN");
                }

                if (std::clamp(reliable_x_deviation, MIN_RELIABLE_X_DEVIATION, MAX_RELIABLE_X_DEVIATION) != reliable_x_deviation)
                {
                    throw std::invalid_argument("bad local optimality reliable_x_deviation, value out of range");
                }

                return std::make_unique<SecondOrderConvergingShortSightAndSlopeNewtonNaiveOptimalityApproximator<PromotedFloatType>>(iteration_sz,
                                                                                                                                     x_a,
                                                                                                                                     reliable_x_deviation,
                                                                                                                                     conventional_randomizer::ApplicationRandomizerObject());

            }
    };
}

#endif