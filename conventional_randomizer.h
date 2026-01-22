//HEADER_CONTROL 1

#ifndef __RANDOMIZER_H__
#define __RANDOMIZER_H__

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

namespace conventional_randomizer
{
    static inline constexpr bool IS_MULTI_THREADED = false;

    class RandomizerObject
    {
        private:

            using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())}));

            randomizer_t randomizer_machine;

        public:

            RandomizerObject(): randomizer_machine(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())})){}

            template <class FloatType, class PromotedFloatType = FloatType>
            auto randomize_fixed_point_float(FloatType first, FloatType last, size_t discretization_sz,
                                             const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> FloatType
            {

                static_assert(std::is_floating_point_v<FloatType>);
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                const size_t MIN_DISCRETIZATION_SZ  = 1u;
                const size_t MAX_DISCRETIZATION_SZ  = std::numeric_limits<size_t>::max();

                if (std::clamp(discretization_sz, MIN_DISCRETIZATION_SZ, MAX_DISCRETIZATION_SZ) != discretization_sz) [[unlikely]]
                {
                    throw std::invalid_argument("bad randomize_fixed_point_float discretization size");
                }

                if (std::isnan(first)) [[unlikely]]
                {
                    throw std::invalid_argument("bad randomize_fixed_point_float first");
                }

                if (std::isnan(last)) [[unlikely]]
                {
                    throw std::invalid_argument("bad randomize_fixed_point_float last");
                }

                if (first > last) [[unlikely]]
                {
                    throw std::invalid_argument("bad randomize_fixed_point_float [first, last)");
                }

                PromotedFloatType discrete_unit = (static_cast<PromotedFloatType>(last) - first) / discretization_sz;
                PromotedFloatType rs = discrete_unit * (this->randomizer_machine() % discretization_sz);

                return stdx::float_clamp<FloatType>(rs, first, last);
            }

            auto randomize_uint(size_t first, size_t last) -> size_t
            {
                if (first >= last)
                {
                    throw std::invalid_argument("bad randomize_uint [first, last)");
                }

                size_t range    = last - first;
                size_t rs       = first + (this->randomizer_machine() % range);

                return rs;
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            auto randomize_exponential_float(FloatType base,
                                             FloatType exponent_first, FloatType exponent_last, size_t discretization_sz,
                                             const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> FloatType
            {

                static_assert(std::is_floating_point_v<FloatType>);
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                if (std::isnan(base))
                {
                    throw std::invalid_argument("bad randomize_exponential_float base");
                }

                if (std::signbit(base))
                {
                    throw std::invalid_argument("bad randomize_exponential_float base sign");
                }

                return std::pow(base, this->randomize_fixed_point_float(exponent_first, exponent_last, discretization_sz, promotion_tag));
            }

            auto flip_a_coin() -> bool
            {
                return static_cast<bool>(this->randomizer_machine() % 2);
            }
    };

    class ChanceMachine
    {
        private:

            size_t dice_sz;
            size_t dice_chance;
            conventional_randomizer::RandomizerObject randomizer;

        public:

            ChanceMachine(size_t dice_sz,
                          size_t dice_chance): dice_sz(stdx::safe_non_zero_access(dice_sz)),
                                               dice_chance(dice_chance),
                                               randomizer(){}

            bool flip_a_coin()
            {
                size_t dice_result = this->randomizer.randomize_uint(0u, this->dice_sz);
                return dice_result < this->dice_chance;
            }
    };

    template <class Signature = void>
    class SingleThreadRandomizerFacility
    {
        private:

            using singleton_obj = stdx::singleton_container<RandomizerObject, SingleThreadRandomizerFacility>;

        public:

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto randomize_fixed_point_float(FloatType first, FloatType last, size_t discretization_sz,
                                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> FloatType
            {
                return singleton_obj::get().randomize_fixed_point_float(first, last, discretization_sz, promotion_tag);
            }

            static auto randomize_uint(size_t first, size_t last) -> size_t
            {
                return singleton_obj::get().randomize_uint(first, last);
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto randomize_exponential_float(FloatType base,
                                                    FloatType exponent_first, FloatType exponent_last, size_t discretization_sz,
                                                    const stdx::Tag<PromotedFloatType>& promotion_tag = stdx::Tag<PromotedFloatType>{}) -> FloatType
            {
                return singleton_obj::get().randomize_exponential_float(base,
                                                                        exponent_first, exponent_last, discretization_sz,
                                                                        promotion_tag);
            }

            static auto flip_a_coin() -> bool
            {
                return singleton_obj::get().flip_a_coin();
            }
    };

    template <class Signature = void>
    class ThreadSafeRandomizerFacility
    {
        public:

            using Base = SingleThreadRandomizerFacility<ThreadSafeRandomizerFacility<Signature>>;

            template <class ...Args>
            static auto randomize_fixed_point_float(Args&& ...args) -> decltype(auto)
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_fixed_point_float(std::forward<Args>(args)...);
            }

            template <class ...Args>
            static auto randomize_uint(Args&& ...args) -> decltype(auto)
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_uint(std::forward<Args>(args)...);
            }

            template <class ...Args>
            static auto randomize_exponenial_foat(Args&& ...args) -> decltype(auto)
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_exponential_float(std::forward<Args>(args)...);
            }

            template <class ...Args>
            static auto flip_a_coin(Args&& ...args) -> decltype(auto)
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::flip_a_coin(std::forward<Args>(args)...);
            }
    };

    template <class Signature = void>
    using RandomizerFacility = std::conditional_t<IS_MULTI_THREADED,
                                                  ThreadSafeRandomizerFacility<Signature>,
                                                  SingleThreadRandomizerFacility<Signature>>;

    class ApplicationRandomizerObject: private RandomizerObject
    {
        private:

            using Base = RandomizerObject;

        public:

            auto randomize_focal(bool has_sign = false) -> double
            {
                const double MIN_RESULT_VALUE           = std::numeric_limits<double>::min();
                const double MAX_RESULT_VALUE           = size_t{1} << 20;

                const double BASE_FIRST                 = 0.1f;
                const double BASE_LAST                  = 10.f;
                const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000ULL;

                const double EXPONENT_FIRST             = -6.f;
                const double EXPONENT_LAST              = 6.f;
                const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000ULL;

                const double tentative_result           = Base::randomize_exponential_float(Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ),
                                                                                            EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);

                if (has_sign)
                {
                    const double signness = Base::flip_a_coin() ? -1.0 : 1.0;
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE) * signness;
                }
                else
                {
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE);    
                }
            }

            auto ld_randomize_focal(bool has_sign = false) -> long double
            {
                using fl_t = long double;
            
                const fl_t MIN_RESULT_VALUE             = std::numeric_limits<fl_t>::min();
                const fl_t MAX_RESULT_VALUE             = size_t{1} << 40;

                const fl_t BASE_FIRST                   = 0.1f;
                const fl_t BASE_LAST                    = 10.f;
                const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000'000'000ULL;

                const fl_t EXPONENT_FIRST               = -20.f;
                const fl_t EXPONENT_LAST                = 20.f;
                const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000'000'000ULL;

                const fl_t tentative_result             = Base::randomize_exponential_float(Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ),
                                                                                            EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);

                if (has_sign)
                {
                    const fl_t signness = Base::flip_a_coin() ? -1.0 : 1.0;
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE) * signness;
                }
                else
                {
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE);    
                }
            }

            auto ld_randomize_focal_2(bool has_sign = false) -> long double
            {
                using fl_t = long double;
            
                const fl_t MIN_RESULT_VALUE             = std::numeric_limits<fl_t>::min();
                const fl_t MAX_RESULT_VALUE             = size_t{1} << 40;

                const fl_t BASE_FIRST                   = 1.f;
                const fl_t BASE_LAST                    = 10.f;
                const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000'000'000ULL;

                const fl_t EXPONENT_FIRST               = -20.f;
                const fl_t EXPONENT_LAST                = 0.f;

                const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000'000'000ULL;

                const fl_t tentative_result             = Base::randomize_exponential_float(Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ),
                                                                                            EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);

                if (has_sign)
                {
                    const fl_t signness = Base::flip_a_coin() ? -1.0 : 1.0;
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE) * signness;
                }
                else
                {
                    return stdx::float_clamp(tentative_result, MIN_RESULT_VALUE, MAX_RESULT_VALUE);    
                }
            }

            auto randomize_percentage_focal() -> double
            {
                const double MIN_RESULT_VALUE           = std::numeric_limits<double>::min();
                const double MAX_RESULT_VALUE           = size_t{1} << 20;

                const double BASE_FIRST                 = 1.0;
                const double BASE_LAST                  = 10.0;
                const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000ULL;

                const double EXPONENT_FIRST             = 0;
                const double EXPONENT_LAST              = 6;
                const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000ULL;

                const double base                       = Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ);
                const double norm_threshold             = std::pow(base, EXPONENT_LAST);
                const double pre_denormed_value         = Base::randomize_exponential_float(base, EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);
                const double denormed_value             = pre_denormed_value / norm_threshold;

                return stdx::float_clamp(denormed_value, MIN_RESULT_VALUE, MAX_RESULT_VALUE);
            }

            auto ld_randomize_percentage_focal() -> long double
            {
                using fl_t = long double;

                const fl_t MIN_RESULT_VALUE             = std::numeric_limits<double>::min();
                const fl_t MAX_RESULT_VALUE             = size_t{1} << 40;

                const fl_t BASE_FIRST                   = 1.0;
                const fl_t BASE_LAST                    = 10.0;
                const size_t BASE_DISCRETIZATION_SZ     = 1'000'000'000'000'000ULL;

                const fl_t EXPONENT_FIRST               = 0;
                const fl_t EXPONENT_LAST                = 20;
                const size_t EXPONENT_DISCRETIZATION_SZ = 1'000'000'000'000'000ULL;

                const fl_t base                         = Base::randomize_fixed_point_float(BASE_FIRST, BASE_LAST, BASE_DISCRETIZATION_SZ);
                const fl_t norm_threshold               = std::pow(base, EXPONENT_LAST);
                const fl_t pre_denormed_value           = Base::randomize_exponential_float(base, EXPONENT_FIRST, EXPONENT_LAST, EXPONENT_DISCRETIZATION_SZ);
                const fl_t denormed_value               = pre_denormed_value / norm_threshold;

                return stdx::float_clamp(denormed_value, MIN_RESULT_VALUE, MAX_RESULT_VALUE);
            }
    };

    class ExponentialSeededRangeFloatUniformDistributioner
    {
        public:

            using object_float_t = long double;

        private:

            RandomizerObject raw_randomizer;
            ApplicationRandomizerObject app_randomizer;

            object_float_t first;
            object_float_t last;

            static inline constexpr uint64_t DISCRETIZATION_SZ  = 1'000'000'000'000'000ULL;

        public:

            ExponentialSeededRangeFloatUniformDistributioner(): raw_randomizer(),
                                                                app_randomizer()
            {
                object_float_t tentative_range  = this->app_randomizer.ld_randomize_focal();
                object_float_t adjusted_range   = tentative_range + std::numeric_limits<object_float_t>::min();

                this->first                     = -adjusted_range;
                this->last                      = adjusted_range;
            }

            auto get() -> object_float_t
            {
                return this->raw_randomizer.template randomize_fixed_point_float<object_float_t>(this->first, this->last, DISCRETIZATION_SZ);
            }
    };

    template <class Signature = void>
    class SingleThreadApplicationRandomizerFacility
    {
        private:

            using singleton_obj = stdx::singleton_container<ApplicationRandomizerObject, SingleThreadApplicationRandomizerFacility<Signature>>;

        public:

            static auto randomize_focal(bool has_sign = false) -> double
            {
                return singleton_obj::get().randomize_focal(has_sign);
            }
            
            static auto ld_randomize_focal(bool has_sign = false) -> long double
            {
                return singleton_obj::get().ld_randomize_focal(has_sign);
            }

            static auto ld_randomize_focal_2(bool has_sign = false) -> long double
            {
                return singleton_obj::get().ld_randomize_focal_2(has_sign);
            }

            static auto randomize_percentage_focal() -> double
            {
                return singleton_obj::get().randomize_percentage_focal();
            }

            static auto ld_randomize_percentage_focal() -> long double
            {
                return singleton_obj::get().ld_randomize_percentage_focal();
            }
    };

    template <class Signature = void>
    class ThreadSafeApplicationRandomizerFacility
    {
        public:

            using Base = SingleThreadApplicationRandomizerFacility<ThreadSafeApplicationRandomizerFacility<Signature>>;

            static auto randomize_focal(bool has_sign = false) -> double
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_focal(has_sign);
            }

            static auto ld_randomize_focal(bool has_sign = false) -> long double
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::ld_randomize_focal(has_sign);
            }

            static auto ld_randomize_focal_2(bool has_sign = false) -> long double
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::ld_randomize_focal_2(has_sign);
            }

            static auto randomize_percentage_focal() -> double
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_percentage_focal();
            }

            static auto ld_randomize_percentage_focal() -> long double
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::ld_randomize_percentage_focal();
            }
    };

    template <class Signature = void>
    using ApplicationRandomizerFacility = std::conditional_t<IS_MULTI_THREADED,
                                                             ThreadSafeApplicationRandomizerFacility<Signature>,
                                                             SingleThreadApplicationRandomizerFacility<Signature>>;

    class VectorRandomizerObject: private RandomizerObject
    {
        private:

            using Base = RandomizerObject;

        public:

            auto randomize_space(size_t dimension_sz,
                                 const double value_first             = -10,
                                 const double value_last              = 10,
                                 const size_t value_discretization_sz = 1'000'000'000ULL) -> std::vector<double>
            {            
                auto gen = [&]{
                    return Base::randomize_fixed_point_float(value_first, value_last, value_discretization_sz);
                };

                std::vector<double> rs(dimension_sz);
                std::generate(rs.begin(), rs.end(), gen);

                return rs;
            }

            auto randomize_unit_space(size_t dimension_sz) -> std::vector<double>
            {
                const size_t LUCKY_ITERATION_SZ = 10u;

                for (size_t i = 0u; i < LUCKY_ITERATION_SZ; ++i)
                {
                    std::vector<double> random_space    = randomize_space(dimension_sz);
                    std::vector<double> tentative_rs    = div_vector(random_space, coordinate_distance<double>(random_space.data(), random_space.size()));
                    bool flag                           = false;

                    for (double e: tentative_rs)
                    {
                        if (std::isnan(e) || std::isinf(e))
                        {
                            flag = true;
                            break;
                        }
                    }

                    if (!flag)
                    {
                        return tentative_rs;
                    }
                }

                throw std::runtime_error("randomize_unit_space went wrong");
            }

            auto randomize_radian_space(size_t dimension_sz) -> std::vector<double>
            {
                constexpr double PI_FIRST                   = 0;
                constexpr double PI_LAST                    = std::numbers::pi_v<double>;
                constexpr size_t PI_DISCRETIZATION_SZ       = 1'000'000'000ULL;

                auto pi_random_gen = [&]
                {
                    return Base::randomize_fixed_point_float(PI_FIRST, PI_LAST, PI_DISCRETIZATION_SZ);
                };

                std::vector<double> rs(dimension_sz);
                std::generate(rs.begin(), rs.end(), pi_random_gen);

                return rs;
            }

        private:

            template <class FloatType, class PromotedFloatType = FloatType>
            auto coordinate_distance(FloatType * coor_arr, size_t coor_arr_sz) -> FloatType
            {
                static_assert(std::is_floating_point_v<FloatType>);
                static_assert(std::is_floating_point_v<PromotedFloatType>);

                PromotedFloatType rs = 0;

                for (size_t i = 0u; i < coor_arr_sz; ++i)
                {
                    rs += static_cast<PromotedFloatType>(coor_arr[i]) * coor_arr[i];
                }

                return std::sqrt(rs);
            }

            template <class T, class ...Args>
            auto div_vector(const std::vector<T, Args...>& vec, T c) -> std::vector<T, Args...>
            {
                std::vector<T, Args...> rs(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    rs[i] = vec[i] / c;
                }

                return rs;
            }
    };

    template <class Signature = void>
    class SingleThreadVectorRandomizerFacility
    {
        private:

            using singleton_obj = stdx::singleton_container<VectorRandomizerObject, SingleThreadVectorRandomizerFacility<Signature>>;

        public:

            static auto randomize_space(size_t dimension_sz,
                                        const double value_first             = -10,
                                        const double value_last              = 10,
                                        const size_t value_discretization_sz = 1'000'000'000ULL) -> std::vector<double>
            {            
                return singleton_obj::get().randomize_space(dimension_sz, value_first, value_last, value_discretization_sz);
            }

            static auto randomize_unit_space(size_t dimension_sz) -> std::vector<double>
            {
                return singleton_obj::get().randomize_unit_space(dimension_sz);
            }

            static auto randomize_radian_space(size_t dimension_sz) -> std::vector<double>
            {
                return singleton_obj::get().randomize_radian_space(dimension_sz);
            }
    };

    template <class Signature = void>
    class ThreadSafeVectorRandomizerFacility
    {
        private:

            using Base = SingleThreadVectorRandomizerFacility<ThreadSafeVectorRandomizerFacility<Signature>>;

        public:

            static auto randomize_space(size_t dimension_sz,
                                        const double value_first             = -10,
                                        const double value_last              = 10,
                                        const size_t value_discretization_sz = 1'000'000'000ULL) -> std::vector<double>
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_space(dimension_sz, value_first, value_last, value_discretization_sz);
            }

            static auto randomize_unit_space(size_t dimension_sz) -> std::vector<double>
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_unit_space(dimension_sz);
            }

            static auto randomize_radian_space(size_t dimension_sz) -> std::vector<double>
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);
                return Base::randomize_radian_space(dimension_sz);
            }
    };

    template <class Signature = void>
    using VectorRandomizerFacility = std::conditional_t<IS_MULTI_THREADED,
                                                        ThreadSafeVectorRandomizerFacility<Signature>,
                                                        SingleThreadVectorRandomizerFacility<Signature>>;
}

#endif