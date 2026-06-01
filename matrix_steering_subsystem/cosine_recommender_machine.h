#ifndef __COSINE_RECOMMENDER_MACHINE_H__
#define __COSINE_RECOMMENDER_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <vector>
#include <memory>
#include <stdexcept>
#include "conventional_randomizer.h"
#include "coordinate_recommender_machine.h"
#include "activation.h"
#include "space_operation.h"
#include <stl_extension/stdx.h>

namespace cosine_recommender_machine
{
    using crm_float_t = double;
    using crm_promoted_float_t = long double;

    //this algorithm is not easy to implement, we'd have to be generic enough, and we'd have to be activative + deactivative enough in order for us to focus on where the meat is at
    //we'd have a L1 cache of best coordinates, it's up to the cosine recommender to move the focal into the right place

    //for the sake of simplicity, let's have this cosine recommender machine to only to do focal cosine sim or uniform cosine sim within a certain window, we'd have to implement another recommender machine to build the semantic space 
    //we'd use this cosine machine for literally everything

    //50% of coefficient randomization
    //50% of decision making
    //50% of ground saturation unhinge
    //50% of touching ground from transport
    //50% of etc.
    //50% of choosing the right optimizer
    //50% of choosing the right focal + etc.

    //what we have always said is cosine is never wrong, the semantic space is
    //so we'd try to be as round as possible

    //let's implement the implementables first

    class CosineRecommenderMachineInterface
    {
        public:

            virtual ~CosineRecommenderMachineInterface() = default;

            virtual void feedback(const std::vector<crm_float_t>& coordinate, crm_float_t rating) = 0;
            virtual auto next() -> std::optional<std::vector<crm_float_t>> = 0;
    };

    class RadianCoordinateCosineRandomizer
    {
        private:

            crm_float_t focal_angle_deviation;
            crm_float_t uniform_angle_deviation;
            conventional_randomizer::ApplicationRandomizerObject focal_randomizer;
            conventional_randomizer::RandomizerObject randomizer;

            static inline constexpr crm_float_t DEFAULT_FOCAL_ANGLE_DEVIATION   = 0.001;
            static inline constexpr crm_float_t DEFAULT_UNIFORM_ANGLE_DEVIATION = 0.001;

        public:

            RadianCoordinateCosineRandomizer(): focal_angle_deviation(DEFAULT_FOCAL_ANGLE_DEVIATION),
                                                uniform_angle_deviation(DEFAULT_UNIFORM_ANGLE_DEVIATION),
                                                focal_randomizer(),
                                                randomizer(){}

            RadianCoordinateCosineRandomizer(crm_float_t focal_angle_deviation,
                                             crm_float_t uniform_angle_deviation): focal_angle_deviation(focal_angle_deviation),
                                                                                   uniform_angle_deviation(uniform_angle_deviation),
                                                                                   focal_randomizer(),
                                                                                   randomizer()
            {
                if (std::isnan(this->focal_angle_deviation))
                {
                    throw std::invalid_argument("bad focal angle deviation, NaN");
                }

                if (this->focal_angle_deviation < 0)
                {
                    throw std::invalid_argument("bad focal angle deviation, < 0");
                }

                if (std::isnan(this->uniform_angle_deviation))
                {
                    throw std::invalid_argument("bad uniform angle deviation, NaN");
                }

                if (this->uniform_angle_deviation < 0)
                {
                    throw std::invalid_argument("bad uniform angle deviation, < 0");
                }
            }

            auto cosine_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                size_t dispatch_code = this->randomizer.randomize_uint(0, 7);

                switch (dispatch_code)
                {
                    case 0:
                    {
                        return this->uniform_cosine_like(vec);
                    }
                    case 1:
                    {
                        return this->focal_cosine_like(vec);
                    }
                    case 2:
                    {
                        return this->uniform_cosine_one_like(vec);
                    }
                    case 3:
                    {
                        return this->focal_cosine_one_like(vec);
                    }
                    case 4:
                    {
                        return this->uniform_cosine_two_like(vec);
                    }
                    case 5:
                    {
                        return this->focal_cosine_two_like(vec);
                    }
                    case 6:
                    {
                        return vec;
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

        private:

            auto uniform_cosine_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                const size_t DISCRETIZATION_SZ          = 1'000'000'000ULL;
                const crm_float_t individual_deviation  = std::max(static_cast<crm_float_t>(this->uniform_angle_deviation / vec.size()), std::numeric_limits<crm_float_t>::min());

                std::vector<crm_float_t> rs_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    crm_float_t deviation   = this->randomizer.randomize_fixed_point_float(-individual_deviation, individual_deviation, DISCRETIZATION_SZ);
                    rs_vec[i]               = space_operation::radian_normalize(vec[i] + deviation);
                }

                return rs_vec;
            }

            auto focal_cosine_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                const crm_float_t individual_deviation  = this->focal_angle_deviation / vec.size();

                std::vector<crm_float_t> rs_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    crm_float_t tentative_deviation = this->focal_randomizer.ld_randomize_focal(true);
                    crm_float_t deviation           = stdx::deviation_clamp(tentative_deviation, individual_deviation);
                    rs_vec[i]                       = space_operation::radian_normalize(vec[i] + deviation);
                }

                return rs_vec;
            }

            auto uniform_cosine_one_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                const size_t DISCRETIZATION_SZ  = 1'000'000'000ULL;
                size_t idx                      = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx]    = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-this->uniform_angle_deviation, this->uniform_angle_deviation, DISCRETIZATION_SZ) + tmp_vec[idx]);

                return tmp_vec;
            }

            auto focal_cosine_one_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                size_t idx      = this->randomizer.randomize_uint(0, vec.size());
                auto tmp_vec    = vec;
                tmp_vec[idx]    = space_operation::radian_normalize(stdx::deviation_clamp<crm_float_t>(this->focal_randomizer.ld_randomize_focal(true), this->focal_angle_deviation) + tmp_vec[idx]);

                return tmp_vec;
            }

            auto uniform_cosine_two_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                const size_t DISCRETIZATION_SZ  = 1'000'000'000ULL;
                size_t idx0                     = this->randomizer.randomize_uint(0, vec.size());
                size_t idx1                     = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx0]   = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-this->uniform_angle_deviation, this->uniform_angle_deviation, DISCRETIZATION_SZ) + tmp_vec[idx0]);
                tmp_vec[idx1]   = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-this->uniform_angle_deviation, this->uniform_angle_deviation, DISCRETIZATION_SZ) + tmp_vec[idx1]);

                return tmp_vec;
            }

            auto focal_cosine_two_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                size_t idx0     = this->randomizer.randomize_uint(0, vec.size());
                size_t idx1     = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx0]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_float_t>(this->focal_randomizer.ld_randomize_focal(true), this->focal_angle_deviation) + tmp_vec[idx0]);
                tmp_vec[idx1]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_float_t>(this->focal_randomizer.ld_randomize_focal(true), this->focal_angle_deviation) + tmp_vec[idx1]);

                return tmp_vec;
            }
    };

    class RadianCoordinateActivativeCosineRandomizer
    {
        private:

            RadianCoordinateCosineRandomizer base;
            conventional_randomizer::ApplicationRandomizerObject focal_randomizer;
            conventional_randomizer::RandomizerObject randomizer;

            static inline constexpr size_t MAX_ACTIVATION_SZ        = size_t{1} << 16;
            static inline constexpr size_t MAX_DISCRETIZATION_COUNT = size_t{1} << 16;

        public:

            RadianCoordinateActivativeCosineRandomizer(crm_float_t focal_angle_deviation,
                                                       crm_float_t uniform_angle_deviation): base(focal_angle_deviation, uniform_angle_deviation),
                                                                                             focal_randomizer(),
                                                                                             randomizer(){}

            auto cosine_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                if (vec.empty())
                {
                    return vec;
                }

                size_t tentative_max_discretization_sz  = vec.size() / MAX_DISCRETIZATION_COUNT + size_t{vec.size() % MAX_DISCRETIZATION_COUNT != 0u};
                size_t max_discretization_sz            = std::clamp(tentative_max_discretization_sz, size_t{1}, vec.size());

                size_t discretization_sz;

                if (this->randomizer.flip_a_coin())
                {
                    discretization_sz = this->randomizer.randomize_uint(1, vec.size() + 1);
                }
                else
                {
                    discretization_sz = std::clamp(static_cast<size_t>(this->focal_randomizer.randomize_percentage_focal() * vec.size()), size_t{1}, vec.size());
                }

                discretization_sz           = std::min(max_discretization_sz, discretization_sz);
                size_t discretization_count = vec.size() / discretization_sz + static_cast<size_t>(vec.size() % discretization_sz != 0u);
                size_t activation_count;

                if (this->randomizer.flip_a_coin())
                {
                    activation_count = std::min(this->randomizer.randomize_uint(0, discretization_count + 1), MAX_ACTIVATION_SZ);
                }
                else
                {
                    activation_count = std::min(std::min(static_cast<size_t>(this->focal_randomizer.randomize_percentage_focal() * discretization_count), discretization_count), MAX_ACTIVATION_SZ);

                    if (this->randomizer.flip_a_coin())
                    {
                        activation_count = std::max(size_t{1}, activation_count);
                    }
                }

                std::vector<std::vector<crm_float_t>> discretized_vec                       = stdx::split_vector(vec, discretization_sz);
                std::vector<std::pair<size_t, std::vector<crm_float_t>>> enumerated_vec     = stdx::enumerate_vector(discretized_vec);
                std::vector<activation::activation_codex_t> activation_codex_vec            = this->randomize_activation_codex_vec(activation_count);

                std::vector<std::pair<size_t, std::vector<crm_float_t>>> activated_vec      = activation::activate(enumerated_vec, activation_codex_vec);
                std::vector<std::pair<size_t, std::vector<crm_float_t>>> transformed_vec    = this->base_transform(activated_vec);

                std::vector<std::vector<crm_float_t>> discretized_vec_2                     = this->override_merge_enumeration(enumerated_vec, transformed_vec);
                std::vector<crm_float_t> ret_vec                                            = stdx::unsplit_vector(discretized_vec_2);

                return ret_vec;
            }

        private:
            
            auto randomize_activation_codex_vec(size_t sz) -> std::vector<activation::activation_codex_t>
            {
                auto generator = [&]
                {
                    return this->randomizer.randomize_uint(0, activation::ACTIVATION_CODEX_RANGE);
                };

                std::vector<activation::activation_codex_t> result(sz);
                std::generate(result.begin(), result.end(), generator);

                return result;
            }

            auto base_transform(const std::vector<std::pair<size_t, std::vector<crm_float_t>>>& vec) -> std::vector<std::pair<size_t, std::vector<crm_float_t>>>
            {
                std::vector<std::pair<size_t, std::vector<crm_float_t>>> result_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    result_vec[i] = {vec[i].first, this->base.cosine_like(vec[i].second)};
                }

                return result_vec;
            }

            template <class T>
            auto override_merge_enumeration(const std::vector<std::pair<size_t, T>>& dst, const std::vector<std::pair<size_t, T>>& src) -> std::vector<T>
            {
                if (dst.empty())
                {
                    return {};
                }

                size_t max_idx  = std::max_element(dst.begin(), dst.end(), [](const auto& lhs, const auto& rhs){return lhs.first < rhs.first;})->first;
                size_t count    = max_idx + 1;

                std::vector<T> result(count);

                for (size_t i = 0u; i < dst.size(); ++i)
                {
                    result[dst[i].first] = dst[i].second;
                }

                for (size_t i = 0u; i < src.size(); ++i)
                {
                    if (src[i].first < count)
                    {
                        result[src[i].first] = src[i].second;
                    }
                }

                return result;
            }
    };

    class EuclideanCoordinateCosineRandomizer
    {
        private:

            RadianCoordinateActivativeCosineRandomizer base;
            conventional_randomizer::ApplicationRandomizerObject focal_randomizer;
            conventional_randomizer::RandomizerObject randomizer;

        public:

            EuclideanCoordinateCosineRandomizer(crm_float_t focal_angle_deviation,
                                                crm_float_t uniform_angle_deviation): base(focal_angle_deviation, uniform_angle_deviation),
                                                                                      focal_randomizer(),
                                                                                      randomizer(){}

            auto coordinate_like(const std::vector<crm_float_t>& vec) -> std::vector<crm_float_t>
            {
                const size_t DEDUCT_RANGE_CHANCE = 10u;

                if (vec.empty())
                {
                    return vec;
                }

                crm_float_t distance_scale;

                if (this->randomizer.randomize_uint(0u, DEDUCT_RANGE_CHANCE) == 0u)
                {
                    distance_scale = 1 - this->focal_randomizer.randomize_percentage_focal();
                }
                else
                {
                    distance_scale = 1;
                }

                std::vector<crm_float_t> unit_vec           = space_operation::to_unit_vector(vec);
                crm_float_t new_coordinate_distance         = space_operation::coordinate_distance(vec.data(), vec.size()) * distance_scale;
                std::vector<crm_float_t> radian_vec         = this->euclidean_to_radian_vector(unit_vec);
                std::vector<crm_float_t> alike_radian_vec   = this->base.cosine_like(radian_vec);
                std::vector<crm_float_t> other_unit_vec     = this->radian_to_euclidean_vector(alike_radian_vec);

                return space_operation::mul_vector(other_unit_vec, new_coordinate_distance);
            }

        private:

            auto euclidean_to_radian_vector(const std::vector<crm_float_t>& unit_vec) -> std::vector<crm_float_t>
            {
                std::vector<crm_float_t> rs(unit_vec.size());
                space_operation::euclidean_to_radian_coordinate(unit_vec.data(), rs.size(), rs.data(), stdx::Tag<crm_promoted_float_t>{});

                return rs;
            }

            auto radian_to_euclidean_vector(const std::vector<crm_float_t>& radian_vec) -> std::vector<crm_float_t>
            {
                std::vector<crm_float_t> rs(radian_vec.size());
                space_operation::radian_to_euclidean_coordinate(radian_vec.data(), rs.size(), rs.data());

                return rs;
            }
    };

    class CosineRecommenderMachine: public virtual CosineRecommenderMachineInterface
    {
        private:

            std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base;
            std::unique_ptr<EuclideanCoordinateCosineRandomizer> randomizer;

        public:

            CosineRecommenderMachine(std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base,
                                     std::unique_ptr<EuclideanCoordinateCosineRandomizer> randomizer) noexcept: base(std::move(base)),
                                                                                                                randomizer(std::move(randomizer)){}

            void feedback(const std::vector<crm_float_t>& coordinate, crm_float_t rating)
            {
                try
                {
                    stdx::xsafe_float_range_access(coordinate.data(), coordinate.size());
                    stdx::xsafe_float_access(rating);
                }
                catch (...)
                {
                    return;
                }

                this->base->feedback(stdx::to_castable_vector_initializer(coordinate), rating);
            }

            auto next() -> std::optional<std::vector<crm_float_t>>
            {
                std::optional<std::vector<coordinate_recommender_machine::machine_float_t>> rs = this->base->next();

                if (!rs.has_value())
                {
                    return std::nullopt;
                }

                std::vector<crm_float_t> result_vec = this->randomizer->coordinate_like(stdx::to_castable_vector_initializer(rs.value())); //

                try
                {
                    stdx::xsafe_float_range_access(result_vec.data(), result_vec.size());
                }
                catch (...)
                {
                    return stdx::to_castable_vector_initializer(rs.value());
                }

                return result_vec;
            }
    };

    class CosineRecommenderMachineFactory
    {
        public:

            static auto get_temporal_cosine_recommender_machine(crm_float_t focal_angle_deviation = 1,
                                                                crm_float_t uniform_angle_deviation = 1,
                                                                size_t temporal_queue_sz = 16u) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                std::unique_ptr<EuclideanCoordinateCosineRandomizer> randomizer                             = std::make_unique<EuclideanCoordinateCosineRandomizer>(focal_angle_deviation, uniform_angle_deviation);
                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base = std::make_unique<coordinate_recommender_machine::MixedCoordinateRecommenderMachine>();

                base->set_window_size(temporal_queue_sz);

                return std::make_unique<CosineRecommenderMachine>(std::move(base), std::move(randomizer));
            }

            static auto get_echo_temporal_cosine_recommender_machine(crm_float_t focal_angle_deviation = 1,
                                                                     crm_float_t uniform_angle_deviation = 1,
                                                                     size_t temporal_queue_sz = 16u,
                                                                     size_t echo_sz = 16u) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                std::unique_ptr<EuclideanCoordinateCosineRandomizer> randomizer                                     = std::make_unique<EuclideanCoordinateCosineRandomizer>(focal_angle_deviation, uniform_angle_deviation);
                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base_base    = std::make_unique<coordinate_recommender_machine::MixedCoordinateRecommenderMachine>();
                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base         = std::make_unique<coordinate_recommender_machine::EchoCoordinateRecommenderMachine>(std::move(base_base), echo_sz);

                base->set_window_size(temporal_queue_sz);

                return std::make_unique<CosineRecommenderMachine>(std::move(base), std::move(randomizer));
            }
    };
}

#endif