#ifndef __COEFFICIENT_RANDOMIZER_H__
#define __COEFFICIENT_RANDOMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <general_definition/float_def.h>
#include "conventional_randomizer.h"
#include "space_operation.h"
#include "activation.h"

namespace coefficient_randomizer
{
    class CoefficientRandomizer
    {
        private:

            conventional_randomizer::RandomizerObject randomizer;
            conventional_randomizer::ApplicationRandomizerObject focal_randomizer;
            conventional_randomizer::VectorRandomizerObject vector_randomizer;
            conventional_randomizer::RangeRandomizerObject range_randomizer;

        public:

            template <class FloatType>
            auto randomize_unit_vector(size_t coefficient_sz) -> std::vector<FloatType>
            {
                if (coefficient_sz == 0u)
                {
                    return {};
                }

                if (this->randomizer.flip_a_coin())
                {
                    return valid_unit_or_default(space_operation::to_unit_vector(activate_space(naive_randomize_vector<FloatType>(coefficient_sz)), stdx::Tag<float_def::highest_precision_float_t>())); //
                }
                else
                {
                    return valid_unit_or_default(space_operation::to_unit_vector(naive_randomize_vector<FloatType>(coefficient_sz), stdx::Tag<float_def::highest_precision_float_t>())); //
                }
            }

            template <class FloatType>
            auto randomize_radian_vector(size_t coefficient_sz) -> std::vector<FloatType>
            {
                if (coefficient_sz == 0u)
                {
                    return {};
                }

                if (this->randomizer.flip_a_coin())
                {
                    return activate_space(naive_randomize_radian_vector<FloatType>(coefficient_sz));
                }
                else
                {
                    return naive_randomize_radian_vector<FloatType>(coefficient_sz);
                }
            }

        private:

            template <class FloatType>
            auto valid_unit_or_default(const std::vector<FloatType>& unit_vec) -> std::vector<FloatType>
            {
                static_assert(std::is_floating_point_v<FloatType>);

                if (unit_vec.empty())
                {
                    throw std::invalid_argument("bad unit vector, 0 dimensional vector");
                }

                try
                {
                    const FloatType EPSILON     = 0.001;
                    stdx::xsafe_float_range_access(unit_vec.data(), unit_vec.size());
                    const FloatType coor_dist   = space_operation::coordinate_distance(unit_vec.data(), unit_vec.size());

                    if (std::isnan(coor_dist))
                    {
                        throw std::runtime_error("bad unit vector");
                    }

                    if (coor_dist > EPSILON)
                    {
                        throw std::runtime_error("bad unit vector");
                    }

                    return unit_vec;
                }
                catch (...)
                {
                    auto rs = std::vector<FloatType>(unit_vec.size(), 0);
                    rs[0]   = 1;

                    return rs;
                }
            }

            auto fit_discretization_unit(size_t tentative_discretization_unit,
                                         size_t space_sz,
                                         const size_t MAX_CHUNK_SZ = static_cast<size_t>(1) << 16) -> size_t
            {
                if (MAX_CHUNK_SZ == 0u)
                {
                    throw std::invalid_argument("bad discretization chunk size, 0");
                }

                size_t minimum_required_discretization_sz = space_sz / MAX_CHUNK_SZ + size_t{space_sz % MAX_CHUNK_SZ != 0u};
                return std::max(minimum_required_discretization_sz, tentative_discretization_unit);
            }

            auto uniform_dist_randomize_discretization_unit(size_t coefficient_sz) -> size_t
            {
                return this->randomizer.randomize_uint(1u, coefficient_sz + 1u);
            }

            auto exponential_dist_randomize_discretization_unit(size_t coefficient_sz) -> size_t
            {
                if (coefficient_sz == 0u)
                {
                    throw std::invalid_argument("bad exponential_dist_randomize_discretization_unit coefficient size, 0");
                }

                size_t tentative_discretization_sz  = this->range_randomizer.randomize_range(coefficient_sz + 1);

                return std::clamp(tentative_discretization_sz, size_t{1u}, coefficient_sz);
            }

            auto randomize_discretization_unit(size_t coefficient_sz) -> size_t
            {
                if (this->randomizer.flip_a_coin())
                {
                    return uniform_dist_randomize_discretization_unit(coefficient_sz);
                }
                else
                {
                    return exponential_dist_randomize_discretization_unit(coefficient_sz);
                }
            }

            auto uniform_dist_randomize_unit_count(size_t unit_count_sz) -> size_t
            {
                return this->randomizer.randomize_uint(0u, unit_count_sz);
            }

            auto exponential_dist_randomize_unit_count(size_t unit_count_sz) -> size_t
            {
                if (unit_count_sz == 0u)
                {
                    throw std::invalid_argument("bad exponential_dist_randomize_unit_count unit count sz, 0");
                }

                if (unit_count_sz == 1u)
                {
                    return 0u;
                }

                size_t unit_count           = unit_count_sz - 1u;
                size_t tentative_unit_count = this->range_randomizer.randomize_range(unit_count + 1);

                return std::clamp(tentative_unit_count, size_t{1u}, unit_count);
            }

            auto randomize_unit_count(size_t unit_count_sz) -> size_t
            {
                if (this->randomizer.flip_a_coin())
                {
                    return uniform_dist_randomize_unit_count(unit_count_sz);
                }
                else
                {
                    return exponential_dist_randomize_unit_count(unit_count_sz);
                }
            }

            auto randomize_distribution_codex_vec(size_t unit_count) -> std::vector<activation::activation_codex_t>
            {
                std::vector<activation::activation_codex_t> distribution_codex_vec{};

                for (size_t i = 0u; i < unit_count; ++i)
                {
                    activation::activation_codex_t distribution_codex = this->randomizer.randomize_uint(0u, activation::ACTIVATION_CODEX_RANGE);
                    distribution_codex_vec.push_back(distribution_codex);
                }

                return distribution_codex_vec;
            }

            template <class FloatType>
            auto naive_randomize_vector(size_t coefficient_sz) -> std::vector<FloatType>
            {
                return stdx::to_castable_vector_initializer(this->vector_randomizer.randomize_space(coefficient_sz));
            }

            template <class FloatType>
            auto naive_randomize_radian_vector(size_t coefficient_sz) -> std::vector<FloatType>
            {
                return stdx::to_castable_vector_initializer(this->vector_randomizer.randomize_radian_space(coefficient_sz));
            }

            template <class T>
            auto jagged_deenumerate_vector(const std::vector<std::pair<size_t, T>>& vec, size_t sz) -> std::vector<T>
            {
                auto rs = std::vector<T>(sz, T{});

                for (const auto& [idx, e]: vec)
                {
                    rs[idx] = e;
                }

                return rs;
            }

            template <class FloatType, std::enable_if_t<std::is_floating_point_v<FloatType>, bool> = true>
            auto empty_vec_as(const std::vector<FloatType>& vec) -> std::vector<FloatType>
            {
                static_assert(std::is_floating_point_v<FloatType>);

                std::vector<FloatType> result_vec(vec.size());
                std::fill(result_vec.begin(), result_vec.end(), 0);

                return result_vec;
            }

            template <class FloatType>
            auto activate_space(const std::vector<FloatType>& org_space) -> std::vector<FloatType>
            {
                size_t coefficient_sz                                                           = org_space.size();
                size_t discretization_unit_sz                                                   = fit_discretization_unit(randomize_discretization_unit(coefficient_sz), coefficient_sz);
                std::vector<std::vector<FloatType>> splitted_vec                                = stdx::split_vector(org_space, discretization_unit_sz);
                size_t unit_count                                                               = splitted_vec.size();
                size_t activated_unit_count                                                     = randomize_unit_count(unit_count + 1u);
                std::vector<std::pair<size_t, std::vector<FloatType>>> enumerated_splitted_vec  = stdx::enumerate_vector(splitted_vec);
                std::vector<activation::activation_codex_t> distribution_codex_vec              = randomize_distribution_codex_vec(activated_unit_count);
                std::vector<std::pair<size_t, std::vector<FloatType>>> activated_vec            = activation::activate(enumerated_splitted_vec, distribution_codex_vec);
                std::vector<std::vector<FloatType>> reverted_splitted_vec                       = jagged_deenumerate_vector(activated_vec, unit_count);

                for (size_t i = 0u; i < reverted_splitted_vec.size(); ++i)
                {
                    if (reverted_splitted_vec[i].empty())
                    {
                        reverted_splitted_vec[i] = empty_vec_as(splitted_vec[i]);
                    }
                }

                return stdx::unsplit_vector(reverted_splitted_vec);
            }
    };
}

#endif