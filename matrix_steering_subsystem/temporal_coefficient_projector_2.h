//HEADER_CONTROL 3

#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_2_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_2_H__

#include <stdint.h>
#include <stdlib.h>
#include <general_definition/float_def.h>
#include <memory>
#include <vector>
#include <stl_extension/stdx.h>
#include "conventional_randomizer.h"
#include "space_operation.h"
#include "temporal_coefficient_projector_2_interface.h"
#include "temporal_coefficient_projector.h"
#include "branch_optimizer.h"
#include "cosine_recommender_machine_x.h"

namespace temporal_coefficient_projector_2
{
    using std_float_t = float_def::std_float_t;

    class DecisiveFactoryInterface
    {
        public:

            virtual ~DecisiveFactoryInterface() = default;

            virtual auto get_optimizer(const std::vector<size_t>& enumeration_vec, size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> = 0;
            virtual auto get_enumeration_preorder_tree() -> std::vector<size_t> = 0;
    };

    class SpaceRadixerInterface
    {
        public:

            virtual ~SpaceRadixerInterface() = default;

            virtual auto enumerate(size_t coefficient_sz) -> size_t = 0;
            virtual auto enumeration_size() -> size_t = 0;
    };

    template <class PromotedFloatType = std_float_t>
    class DecisiveFactory: public virtual DecisiveFactoryInterface
    {
        private:

            static auto get_unfrange_scalar() -> PromotedFloatType
            {
                const double FIRST_VALUE        = 0;
                const double LAST_VALUE         = 10;
                const size_t DISCRETIZATION_SZ  = 1'000'000'000'000'000ULL;

                return conventional_randomizer::RandomizerObject{}.template randomize_fixed_point_float<PromotedFloatType>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
            }

            static auto get_unfdst_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                const double FIRST_VALUE        = 0;
                const double LAST_VALUE         = 10;
                const size_t DISCRETIZATION_SZ  = 1'000'000'000'000'000ULL;

                conventional_randomizer::RandomizerObject randomizer{};
                std::vector<PromotedFloatType> vec(space_sz);

                for (size_t i = 0u; i < space_sz; ++i)
                {
                    vec[i] = randomizer.template randomize_fixed_point_float<PromotedFloatType>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
                }

                return vec;
            }

            static auto get_expdst_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> vec(space_sz);

                conventional_randomizer::ApplicationRandomizerObject randomizer{};

                for (size_t i = 0u; i < space_sz; ++i)
                {
                    vec[i] = randomizer.ld_randomize_focal();
                }

                return vec;
            }

            static auto get_expdst2_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> vec(space_sz);

                conventional_randomizer::ApplicationRandomizerObject randomizer{};

                for (size_t i = 0u; i < space_sz; ++i)
                {
                    vec[i] = randomizer.ld_randomize_focal_2();
                }

                return vec;
            }

            static auto get_dcmrange_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return std::vector<PromotedFloatType>(space_sz, conventional_randomizer::ApplicationRandomizerObject{}.ld_randomize_focal_2());
            }

            static auto get_lowrange_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return std::vector<PromotedFloatType>(space_sz, get_unfrange_scalar());
            }

            static auto get_highrange_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return std::vector<PromotedFloatType>(space_sz, conventional_randomizer::ApplicationRandomizerObject{}.ld_randomize_focal());
            }

            static auto get_dcmrange_oval_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(get_expdst2_vector(space_sz), conventional_randomizer::ApplicationRandomizerObject{}.ld_randomize_focal_2());
            }

            static auto get_lowrange_oval_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(get_unfdst_vector(space_sz), get_unfrange_scalar());
            }

            static auto get_highrange_oval_radius_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(get_expdst_vector(space_sz), conventional_randomizer::ApplicationRandomizerObject{}.ld_randomize_focal());
            }

            static auto get_unfdst_frequency_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(get_unfdst_vector(space_sz), get_unfrange_scalar());
            }

            static auto get_expdst_frequency_vector(size_t space_sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(get_expdst_vector(space_sz), conventional_randomizer::ApplicationRandomizerObject{}.ld_randomize_focal());
            }

            static auto get_line_unfdst_direction(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(get_unfdst_vector(coefficient_sz));
            }

            static auto get_line_expdst_direction(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(get_expdst_vector(coefficient_sz));
            }

            static auto get_rotating_one_arm_circle_dcmrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_dcmrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_circle_dcmrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_dcmrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_circle_lowrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_lowrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_circle_lowrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_lowrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_circle_highrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_highrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_circle_highrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_highrange_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));

            }

            static auto get_rotating_one_arm_oval_dcmrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_dcmrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_oval_dcmrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_dcmrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_oval_lowrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_lowrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_oval_lowrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_lowrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));   
            }

            static auto get_rotating_one_arm_oval_highrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_highrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_one_arm_oval_highrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec   = get_highrange_oval_radius_vector(coefficient_sz);
                std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);

                return std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(radius_vec))),
                                                                                                                                    std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
            }

            static auto get_rotating_two_arm_circle_dcmrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_dcmrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));
            }

            static auto get_rotating_two_arm_circle_dcmrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_dcmrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));
            }

            static auto get_rotating_two_arm_circle_lowrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_lowrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));   
            }

            static auto get_rotating_two_arm_circle_lowrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_lowrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));
            }

            static auto get_rotating_two_arm_circle_highrange_unfdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_highrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_unfdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));
            }

            static auto get_rotating_two_arm_circle_highrange_expdst_frequency(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<PromotedFloatType> radius_vec       = get_highrange_radius_vector(coefficient_sz);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> inner_circle;
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> outer_circle;

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    inner_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                {
                    std::vector<PromotedFloatType> freq_vec     = get_expdst_frequency_vector(coefficient_sz);
                    std::vector<PromotedFloatType> rot_vec      = std::vector<PromotedFloatType>(coefficient_sz, 0);
                    outer_circle                                = std::make_unique<temporal_coefficient_projector::GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(freq_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(radius_vec)),
                                                                                                                                                                               std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::move(rot_vec))));
                }

                return std::make_unique<temporal_coefficient_projector::ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(std::move(inner_circle), std::move(outer_circle)));   
            }

            static auto get_preorder_tree_from_graph(const std::unordered_map<std::string, std::vector<std::string>>& graph,
                                                     const std::string& origin) -> std::vector<size_t>
            {
                auto map_ptr = graph.find(origin);

                if (map_ptr == graph.end())
                {
                    return {0u};
                }

                std::vector<size_t> result{map_ptr->second.size()};

                for (const std::string& other_origin: map_ptr->second)
                {
                    std::vector<size_t> other = self::get_preorder_tree_from_graph(graph, other_origin);
                    std::copy(other.begin(), other.end(), std::back_inserter(result));
                }

                return result;
            }

        private:

            using self = DecisiveFactory;

            static inline const std::unordered_map<std::string, std::vector<std::string>> DECISION_TREE =
            {
                {"origin", {"line", "rotating_one_arm", "rotating_two_arm_circle"}},

                {"line", {"line_unfdst_direction", "line_expdst_direction"}},
                {"rotating_one_arm", {"rotating_one_arm_circle", "rotating_one_arm_oval"}},

                {"rotating_one_arm_circle", {"rotating_one_arm_circle_dcmrange", "rotating_one_arm_circle_lowrange", "rotating_one_arm_circle_highrange"}},
                {"rotating_one_arm_oval", {"rotating_one_arm_oval_dcmrange", "rotating_one_arm_oval_lowrange", "rotating_one_arm_oval_highrange"}},
                {"rotating_two_arm_circle", {"rotating_two_arm_circle_dcmrange", "rotating_two_arm_circle_lowrange", "rotating_two_arm_circle_highrange"}},

                {"rotating_one_arm_circle_dcmrange", {"rotating_one_arm_circle_dcmrange_unfdst_frequency", "rotating_one_arm_circle_dcmrange_expdst_frequency"}},
                {"rotating_one_arm_circle_lowrange", {"rotating_one_arm_circle_lowrange_unfdst_frequency", "rotating_one_arm_circle_lowrange_expdst_frequency"}},
                {"rotating_one_arm_circle_highrange", {"rotating_one_arm_circle_highrange_unfdst_frequency", "rotating_one_arm_circle_highrange_expdst_frequency"}},

                {"rotating_one_arm_oval_dcmrange", {"rotating_one_arm_oval_dcmrange_unfdst_frequency", "rotating_one_arm_oval_dcmrange_expdst_frequency"}},
                {"rotating_one_arm_oval_lowrange", {"rotating_one_arm_oval_lowrange_unfdst_frequency", "rotating_one_arm_oval_lowrange_expdst_frequency"}},
                {"rotating_one_arm_oval_highrange", {"rotating_one_arm_oval_highrange_unfdst_frequency", "rotating_one_arm_oval_highrange_expdst_frequency"}},

                {"rotating_two_arm_circle_dcmrange", {"rotating_two_arm_circle_dcmrange_unfdst_frequency", "rotating_two_arm_circle_dcmrange_expdst_frequency"}},
                {"rotating_two_arm_circle_lowrange", {"rotating_two_arm_circle_lowrange_unfdst_frequency", "rotating_two_arm_circle_lowrange_expdst_frequency"}},
                {"rotating_two_arm_circle_highrange", {"rotating_two_arm_circle_highrange_unfdst_frequency", "rotating_two_arm_circle_highrange_expdst_frequency"}}
            };

            using factory_func_t = std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> (*) (size_t);

            static inline const std::unordered_map<std::string, factory_func_t> LEAF_GRAPH =
            {
                {"line_unfdst_direction", self::get_line_unfdst_direction},
                {"line_expdst_direction", self::get_line_expdst_direction},
                {"rotating_one_arm_circle_dcmrange_unfdst_frequency", self::get_rotating_one_arm_circle_dcmrange_unfdst_frequency},
                {"rotating_one_arm_circle_dcmrange_expdst_frequency", self::get_rotating_one_arm_circle_dcmrange_expdst_frequency},
                {"rotating_one_arm_circle_lowrange_unfdst_frequency", self::get_rotating_one_arm_circle_lowrange_unfdst_frequency},
                {"rotating_one_arm_circle_lowrange_expdst_frequency", self::get_rotating_one_arm_circle_lowrange_expdst_frequency},
                {"rotating_one_arm_circle_highrange_unfdst_frequency", self::get_rotating_one_arm_circle_highrange_unfdst_frequency},
                {"rotating_one_arm_circle_highrange_expdst_frequency", self::get_rotating_one_arm_circle_highrange_expdst_frequency},
                {"rotating_one_arm_oval_dcmrange_unfdst_frequency", self::get_rotating_one_arm_oval_dcmrange_unfdst_frequency},
                {"rotating_one_arm_oval_dcmrange_expdst_frequency", self::get_rotating_one_arm_oval_dcmrange_expdst_frequency},
                {"rotating_one_arm_oval_lowrange_unfdst_frequency", self::get_rotating_one_arm_oval_lowrange_unfdst_frequency},
                {"rotating_one_arm_oval_lowrange_expdst_frequency", self::get_rotating_one_arm_oval_lowrange_expdst_frequency},
                {"rotating_one_arm_oval_highrange_unfdst_frequency", self::get_rotating_one_arm_oval_highrange_unfdst_frequency},
                {"rotating_one_arm_oval_highrange_expdst_frequency", self::get_rotating_one_arm_oval_highrange_expdst_frequency},
                {"rotating_two_arm_circle_dcmrange_unfdst_frequency", self::get_rotating_two_arm_circle_dcmrange_unfdst_frequency},
                {"rotating_two_arm_circle_dcmrange_expdst_frequency", self::get_rotating_two_arm_circle_dcmrange_expdst_frequency},
                {"rotating_two_arm_circle_lowrange_unfdst_frequency", self::get_rotating_two_arm_circle_lowrange_unfdst_frequency},
                {"rotating_two_arm_circle_lowrange_expdst_frequency", self::get_rotating_two_arm_circle_lowrange_expdst_frequency},
                {"rotating_two_arm_circle_highrange_unfdst_frequency", self::get_rotating_two_arm_circle_highrange_unfdst_frequency},
                {"rotating_two_arm_circle_highrange_expdst_frequency", self::get_rotating_two_arm_circle_highrange_expdst_frequency}
            };

            static inline const std::string ORIGIN = "origin";

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            auto get_optimizer(const std::vector<size_t>& enumeration_vec, size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::string leaf = ORIGIN;

                for (size_t enumeration: enumeration_vec)
                {
                    auto map_ptr = self::DECISION_TREE.find(leaf);

                    if (map_ptr == self::DECISION_TREE.end())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration not found");
                    }

                    if (enumeration >= map_ptr->second.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of bound");
                    }

                    leaf = map_ptr->second[enumeration];
                }

                auto map_ptr = self::LEAF_GRAPH.find(leaf);

                if (map_ptr == self::LEAF_GRAPH.end())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration vec");
                }

                return (map_ptr->second)(coefficient_sz);
            }

            auto get_enumeration_preorder_tree() -> std::vector<size_t>
            {
                static const std::vector<size_t> result = self::get_preorder_tree_from_graph(self::DECISION_TREE, self::ORIGIN);

                return result;
            }
    };

    class ProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::unique_ptr<DecisiveFactoryInterface> decisive_factory;
            std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor;

        public:

            ProjectorGenerator(std::unique_ptr<DecisiveFactoryInterface> decisive_factory,
                               std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor) noexcept: decisive_factory(std::move(decisive_factory)),
                                                                                                                               branch_predictor(std::move(branch_predictor)){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result = this->branch_predictor->next();
                std::vector<size_t> enumeration_vec                                                                 = branch_prediction_result->get_enumeration();
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector    = this->decisive_factory->get_optimizer(enumeration_vec, coefficient_sz);

                return std::make_unique<InternalFactoryTensor>
                (
                    std::move(projector),
                    std::move(branch_prediction_result)
                );
            }
        
        private:
            
            class InternalFactoryTensor: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
                    std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result;

                public:

                    InternalFactoryTensor(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector,
                                          std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result) noexcept: projector(std::move(projector)),
                                                                                                                                                         branch_prediction_result(std::move(branch_prediction_result)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->projector;
                    }

                    void feedback(double rating)
                    {
                        this->branch_prediction_result->feedback(rating);
                    }
            };
    };

    template <class PromotedFloatType = std_float_t>
    class TaylorSeriesProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface>> cosine_recommender_vec;

        public:

            TaylorSeriesProjectorGenerator(std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface>> cosine_recommender_vec) noexcept: cosine_recommender_vec(std::move(cosine_recommender_vec)){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface>> recommendable_vec{};

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    size_t slot_idx = i % this->cosine_recommender_vec.size();
                    recommendable_vec.push_back(this->cosine_recommender_vec[slot_idx]->next());
                }

                std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec{};

                for (const auto& recommendable: recommendable_vec)
                {
                    coefficient_2d_vec.push_back(stdx::to_castable_vector_initializer(recommendable->get()));
                }

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector = std::make_unique<temporal_coefficient_projector::TaylorSeriesProjector<PromotedFloatType>>(std::move(coefficient_2d_vec));

                return std::make_unique<InternalFactoryTensor>
                (
                    std::move(projector),
                    std::move(recommendable_vec)
                );
            }

        private:

            class InternalFactoryTensor: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
                    std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface>> recommendable_vec;
                
                public:

                    InternalFactoryTensor(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector,
                                          std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface>> recommendable_vec) noexcept: projector(std::move(projector)),
                                                                                                                                                                       recommendable_vec(std::move(recommendable_vec)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->projector;
                    }

                    void feedback(double rating)
                    {
                        for (const auto& recommendable: this->recommendable_vec)
                        {
                            recommendable->feedback(rating);
                        }
                    }
            };
    };

    template <class PromotedFloatType = std_float_t>
    class MonoSpaceTaylorSeriesProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface> cosine_recommender;
            size_t dimension_chunk_sz;

        public:

            MonoSpaceTaylorSeriesProjectorGenerator(std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface> cosine_recommender,
                                                    size_t dimension_chunk_sz) noexcept: cosine_recommender(std::move(cosine_recommender)),
                                                                                         dimension_chunk_sz(dimension_chunk_sz){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface> recommendable = this->cosine_recommender->next();

                std::vector<PromotedFloatType> coefficient_vec                  = recommendable->get();
                std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec  = {};

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    size_t tentative_first  = i * this->dimension_chunk_sz;
                    size_t tentative_last   = (i + 1) * this->dimension_chunk_sz;
                    
                    size_t first            = std::min(tentative_first, static_cast<size_t>(coefficient_vec.size()));
                    size_t last             = std::min(tentative_last, static_cast<size_t>(coefficient_vec.size()));

                    coefficient_2d_vec.push_back(std::vector<PromotedFloatType>(std::next(coefficient_vec.begin(), first),
                                                                                std::next(coefficient_vec.begin(), last)));
                }

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector = std::make_unique<temporal_coefficient_projector::TaylorSeriesProjector<PromotedFloatType>>(std::move(coefficient_2d_vec));

                return std::make_unique<InternalFactoryTensor>
                (
                    std::move(projector),
                    std::move(recommendable)
                );
            }
        
        private:

            class InternalFactoryTensor: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
                    std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface> recommendable;
                
                public:

                    InternalFactoryTensor(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector,
                                          std::unique_ptr<cosine_recommender_machine_x::CosineRecommendationResultInterface> recommendable) noexcept: projector(std::move(projector)),
                                                                                                                                                      recommendable(std::move(recommendable)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->projector;
                    }

                    void feedback(double rating)
                    {
                        this->recommendable->feedback(rating);
                    }
            };
    };

    template <class PromotedFloatType = std_float_t>
    class DecisiveFocalFactory: public virtual DecisiveFactoryInterface
    {
        private:

            static auto get_const_scope_focal_xdcm_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                using operating_float_t = long double;

                conventional_randomizer::ApplicationRandomizerObject randomizer{};

                operating_float_t lense_1   = randomizer.ld_randomize_focal_2();
                operating_float_t lense_2   = randomizer.ld_randomize_focal_2();
                operating_float_t lense     = lense_1 * lense_2;

                return std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(std::vector<std_float_t>(coefficient_sz, lense));
            }

            static auto get_const_scope_focal_dcm_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                conventional_randomizer::ApplicationRandomizerObject randomizer{};
                std_float_t lense   = randomizer.ld_randomize_focal_2();

                return std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(std::vector<std_float_t>(coefficient_sz, lense));
            }

            static auto get_const_scope_focal_noaction(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<temporal_coefficient_projector::PointCoefficientProjector>(std::vector<std_float_t>(coefficient_sz, 1));
            }

            static auto get_sin_scope_focal_dcm_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {                
                conventional_randomizer::ApplicationRandomizerObject randomizer{};

                std_float_t amplitude   = randomizer.ld_randomize_focal_2();
                std_float_t frequency   = randomizer.ld_randomize_focal();

                std::vector<std::unique_ptr<temporal_coefficient_projector::Oscillator<PromotedFloatType>>> oscillator_vector{};

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    oscillator_vector.push_back(std::make_unique<temporal_coefficient_projector::Oscillator<PromotedFloatType>>(amplitude, frequency, 0, 0));
                }

                return std::make_unique<temporal_coefficient_projector::OscillatorTemporalCoefficientProjector<PromotedFloatType>>(std::move(oscillator_vector));
            }

            static auto get_sin_scope_focal_unfdst_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                using operating_float_t = long double;

                conventional_randomizer::RandomizerObject raw_randomizer{};
                conventional_randomizer::ApplicationRandomizerObject app_randomizer{};

                const operating_float_t FIRST_AMPLITUDE_VALUE   = 0;
                const operating_float_t LAST_AMPLITUDE_VALUE    = 1;
                const size_t DISCRETIZATION_SZ                  = 1'000'000'000'000'000ULL;

                operating_float_t amplitude                     = raw_randomizer.template randomize_fixed_point_float<operating_float_t>(FIRST_AMPLITUDE_VALUE, LAST_AMPLITUDE_VALUE, DISCRETIZATION_SZ);
                std_float_t frequency                           = app_randomizer.ld_randomize_focal();

                std::vector<std::unique_ptr<temporal_coefficient_projector::Oscillator<PromotedFloatType>>> oscillator_vector{};

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    oscillator_vector.push_back(std::make_unique<temporal_coefficient_projector::Oscillator<PromotedFloatType>>(amplitude, frequency, 0, 0));
                }

                return std::make_unique<temporal_coefficient_projector::OscillatorTemporalCoefficientProjector<PromotedFloatType>>(std::move(oscillator_vector));
            }

            static auto get_sin_scope_focal_expdst_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                conventional_randomizer::ApplicationRandomizerObject randomizer{};

                std_float_t amplitude   = randomizer.ld_randomize_focal();
                std_float_t frequency   = randomizer.ld_randomize_focal();

                std::vector<std::unique_ptr<temporal_coefficient_projector::Oscillator<PromotedFloatType>>> oscillator_vector{};

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    oscillator_vector.push_back(std::make_unique<temporal_coefficient_projector::Oscillator<PromotedFloatType>>(amplitude, frequency, 0, 0));
                }

                return std::make_unique<temporal_coefficient_projector::OscillatorTemporalCoefficientProjector<PromotedFloatType>>(std::move(oscillator_vector));
            }

            static auto get_line_scope_focal_unfdst_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                const std_float_t FIRST_VALUE   = 0;
                const std_float_t LAST_VALUE    = 1;
                const size_t DISCRETIZATION_SZ  = 1'000'000'000'000'000ULL;

                conventional_randomizer::RandomizerObject randomizer{};
                std::vector<std_float_t> line_vec(coefficient_sz);

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    line_vec[i] = randomizer.template randomize_fixed_point_float<std_float_t>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
                }

                return std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(line_vec));
            }

            static auto get_line_scope_focal_expdst_range(size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                const std_float_t FIRST_VALUE   = 0;
                const std_float_t LAST_VALUE    = 1;
                const size_t DISCRETIZATION_SZ  = 1'000'000'000'000'000ULL;

                conventional_randomizer::RandomizerObject randomizer{};
                std::vector<std_float_t> line_vec(coefficient_sz);

                for (size_t i = 0u; i < coefficient_sz; ++i)
                {
                    line_vec[i] = randomizer.template randomize_fixed_point_float<std_float_t>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
                }

                return std::make_unique<temporal_coefficient_projector::LineTemporalCoefficientProjector<PromotedFloatType>>(std::move(line_vec));
            }

            static auto get_preorder_tree_from_graph(const std::unordered_map<std::string, std::vector<std::string>>& graph,
                                                     const std::string& origin) -> std::vector<size_t>
            {
                auto map_ptr = graph.find(origin);

                if (map_ptr == graph.end())
                {
                    return {0u};
                }

                std::vector<size_t> result{map_ptr->second.size()};

                for (const std::string& other_origin: map_ptr->second)
                {
                    std::vector<size_t> other = self::get_preorder_tree_from_graph(graph, other_origin);
                    std::copy(other.begin(), other.end(), std::back_inserter(result));
                }

                return result;
            }

        private:

            using self = DecisiveFocalFactory;

            static inline const std::unordered_map<std::string, std::vector<std::string>> DECISION_TREE =
            {
                {"origin", {"const_scope_focal", "sin_scope_focal", "line_scope_focal"}},

                {"const_scope_focal", {"const_scope_focal_xdcm_range", "const_scope_focal_dcm_range", "const_scope_focal_noaction"}},
                {"sin_scope_focal", {"sin_scope_focal_dcm_range", "sin_scope_focal_unfdst_range", "sin_scope_focal_expdst_range"}},
                {"line_scope_focal", {"line_scope_focal_unfdst_range", "line_scope_focal_expdst_range"}}

            };

            using factory_func_t = std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> (*) (size_t);

            static inline const std::unordered_map<std::string, factory_func_t> LEAF_GRAPH = 
            {
                {"const_scope_focal_xdcm_range", self::get_const_scope_focal_xdcm_range},
                {"const_scope_focal_dcm_range", self::get_const_scope_focal_dcm_range},
                {"const_scope_focal_noaction", self::get_const_scope_focal_noaction},
                {"sin_scope_focal_dcm_range", self::get_sin_scope_focal_dcm_range},
                {"sin_scope_focal_unfdst_range", self::get_sin_scope_focal_unfdst_range},
                {"sin_scope_focal_expdst_range", self::get_sin_scope_focal_expdst_range},
                {"line_scope_focal_unfdst_range", self::get_line_scope_focal_unfdst_range},
                {"line_scope_focal_expdst_range", self::get_line_scope_focal_expdst_range}
            };

            static inline const std::string ORIGIN = "origin";

        public:

            auto get_optimizer(const std::vector<size_t>& enumeration_vec, size_t coefficient_sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::string leaf = ORIGIN;

                for (size_t enumeration: enumeration_vec)
                {
                    auto map_ptr = self::DECISION_TREE.find(leaf);

                    if (map_ptr == self::DECISION_TREE.end())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration not found");
                    }

                    if (enumeration >= map_ptr->second.size())
                    {
                        throw std::invalid_argument("bad enumeration, enumeration out of bound");
                    }

                    leaf = map_ptr->second[enumeration];
                }

                auto map_ptr = self::LEAF_GRAPH.find(leaf);

                if (map_ptr == self::LEAF_GRAPH.end())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration vec");
                }

                return (map_ptr->second)(coefficient_sz);
            }

            auto get_enumeration_preorder_tree() -> std::vector<size_t>
            {
                static const std::vector<size_t> result = self::get_preorder_tree_from_graph(self::DECISION_TREE, self::ORIGIN);

                return result;
            }
    };

    class FocalExtendedProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::unique_ptr<DecisiveFactoryInterface> focal_factory;
            std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> focal_branch_predictor;
            std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> base_generator;

        public:

            FocalExtendedProjectorGenerator(std::unique_ptr<DecisiveFactoryInterface> focal_factory,
                                            std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> focal_branch_predictor,
                                            std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> base_generator) noexcept: focal_factory(std::move(focal_factory)),
                                                                                                                                      focal_branch_predictor(std::move(focal_branch_predictor)),
                                                                                                                                      base_generator(std::move(base_generator)){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result     = this->focal_branch_predictor->next();
                std::vector<size_t> enumeration_vec                                                                     = branch_prediction_result->get_enumeration();
                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> origin_scaler    = this->focal_factory->get_optimizer(enumeration_vec, coefficient_sz);
                std::unique_ptr<TemporalCoefficientProjectorContainerInterface> base_container                          = this->base_generator->get(coefficient_sz);

                return std::make_unique<InternalFactoryTensor>
                (
                    std::move(origin_scaler),
                    std::move(base_container),
                    std::move(branch_prediction_result)
                );
            }

        private:

            class InternalFactoryTensor: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> origin_scaler;
                    std::unique_ptr<TemporalCoefficientProjectorContainerInterface> base_container;
                    std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result;

                public:

                    InternalFactoryTensor(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> origin_scaler,
                                          std::unique_ptr<TemporalCoefficientProjectorContainerInterface> base_container,
                                          std::unique_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch_prediction_result) noexcept: origin_scaler(std::move(origin_scaler)),
                                                                                                                                                         base_container(std::move(base_container)),
                                                                                                                                                         branch_prediction_result(std::move(branch_prediction_result)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return std::make_shared<temporal_coefficient_projector::MultiplicationTemporalCoefficientProjector<>>(std::make_unique<temporal_coefficient_projector::SharedPointerProjector>(this->origin_scaler),
                                                                                                                              std::make_unique<temporal_coefficient_projector::SharedPointerProjector>(this->base_container->get()));
                    }

                    void feedback(double rating)
                    {
                        this->branch_prediction_result->feedback(rating);
                        this->base_container->feedback(rating);
                    }
            };
    };

    class Base2ExponentialSpaceRadixer: public virtual SpaceRadixerInterface
    {
        private:

            static inline constexpr size_t ENUMERATION_SZ = 32u;
        
        public:

            auto enumerate(size_t coefficient_sz) -> size_t
            {
                size_t ceil_coefficient_sz  = stdx::ceil2(coefficient_sz);
                size_t tentative_slot_idx   = stdx::ulog2(ceil_coefficient_sz);
                size_t max_slot_idx         = ENUMERATION_SZ - 1u;

                return std::min(tentative_slot_idx, max_slot_idx);
            }

            auto enumeration_size() -> size_t
            {
                return ENUMERATION_SZ;
            }
    };

    class SmartProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec;
            std::unique_ptr<SpaceRadixerInterface> space_radixer;
        
        public:

            SmartProjectorGenerator(std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec,
                                    std::unique_ptr<SpaceRadixerInterface> space_radixer) noexcept: base_vec(std::move(base_vec)),
                                                                                                    space_radixer(std::move(space_radixer)){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                size_t enumeration_idx = this->space_radixer->enumerate(coefficient_sz);

                if (enumeration_idx >= this->base_vec.size())
                {
                    std::abort();
                }

                if (this->base_vec[enumeration_idx] == nullptr)
                {
                    std::abort();
                }

                return this->base_vec[enumeration_idx]->get(coefficient_sz);
            }
    };

    template <class PromotedFloatType = std_float_t>
    class TraditionalGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            conventional_randomizer::RandomizerObject randomizer;

        public:

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                const size_t ENUMERATION_SZ = 4u;
                size_t enumeration_idx      = randomizer.randomize_uint(0u, ENUMERATION_SZ);

                std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        projector = temporal_coefficient_projector::CoefficientProjectorFactory::template get_random_coefficient_projector<PromotedFloatType>(coefficient_sz);
                        break;
                    }
                    case 1:
                    {
                        projector = temporal_coefficient_projector::FixedOvalProjectorFactory<PromotedFloatType>{}.get_random_oval_projector(coefficient_sz);
                        break;
                    }
                    case 2:
                    {
                        projector = temporal_coefficient_projector::FixedOvalProjectorFactory<PromotedFloatType>{}.get_random_rotating_2_arm(coefficient_sz);
                        break;
                    }
                    case 3:
                    {
                        projector = temporal_coefficient_projector::FixedOvalProjectorFactory<PromotedFloatType>{}.get_random_rotating_2_skewedarm(coefficient_sz);
                        break;
                    }
                    default:
                    {
                        std::abort();
                    }
                }

                return std::make_unique<InternalFactoryTensor>(std::move(projector));
            }
        
        private:
            
            class InternalFactoryTensor: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
                
                public:

                    InternalFactoryTensor(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector) noexcept: projector(std::move(projector)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->projector;
                    }

                    void feedback(double score)
                    {
                        (void) score;
                    }
            };
    };

    class ChanceGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> lhs;
            std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> rhs;
            conventional_randomizer::ChanceMachine chance_machine;

        public:

            ChanceGenerator(std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> lhs,
                            std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> rhs,
                            conventional_randomizer::ChanceMachine chance_machine) noexcept: lhs(std::move(lhs)),
                                                                                             rhs(std::move(rhs)),
                                                                                             chance_machine(std::move(chance_machine)){}

            auto get(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                if (this->chance_machine.flip_a_coin())
                {
                    return this->lhs->get(coefficient_sz);
                }
                else
                {
                    return this->rhs->get(coefficient_sz);
                }
            }
    };

    class GeneratorFactory
    {
        private:

            template <class PromotedFloatType = std_float_t>
            static auto get_autolense_generator(std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>&& base) -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                std::unique_ptr<DecisiveFactoryInterface> lense_factory                                 = std::make_unique<DecisiveFocalFactory<PromotedFloatType>>();
                std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor    = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(lense_factory->get_enumeration_preorder_tree());

                return std::make_unique<FocalExtendedProjectorGenerator>(std::move(lense_factory),
                                                                         std::move(branch_predictor),
                                                                         std::move(base));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_multispace_shape_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                const size_t TAYLOR_COEFFICIENT_SZ  = 8u;
                const size_t RECOMMENDER_SZ         = 32u;

                std::vector<std::unique_ptr<cosine_recommender_machine_x::CosineRecommenderMachineInterface>> cosine_recommender_vec{};

                for (size_t i = 0u; i < RECOMMENDER_SZ; ++i)
                {
                    cosine_recommender_vec.push_back(cosine_recommender_machine_x::MachineFactory::get_best_recommender_machine(TAYLOR_COEFFICIENT_SZ));
                }

                return get_autolense_generator(std::make_unique<TaylorSeriesProjectorGenerator<PromotedFloatType>>(std::move(cosine_recommender_vec)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_monospace_shape_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                const size_t TAYLOR_COEFFICIENT_SZ  = 8u;
                const size_t EXPECTED_DIMENSION_SZ  = 64u;
                const size_t TOTAL_SPACE_SZ         = TAYLOR_COEFFICIENT_SZ * EXPECTED_DIMENSION_SZ;

                return get_autolense_generator(std::make_unique<MonoSpaceTaylorSeriesProjectorGenerator<PromotedFloatType>>(cosine_recommender_machine_x::MachineFactory::get_best_recommender_machine(TOTAL_SPACE_SZ),
                                                                                                                            TAYLOR_COEFFICIENT_SZ));
            }

        public:

            template <class PromotedFloatType = std_float_t>
            static auto get_traditional_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                return get_autolense_generator(std::make_unique<TraditionalGenerator<PromotedFloatType>>());
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_normal_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                std::unique_ptr<DecisiveFactoryInterface> factory                                       = std::make_unique<DecisiveFactory<PromotedFloatType>>();
                std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor    = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(factory->get_enumeration_preorder_tree());

                return get_autolense_generator(std::make_unique<ProjectorGenerator>(std::move(factory), std::move(branch_predictor)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_autoshape_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                return std::make_unique<ChanceGenerator>(get_multispace_shape_generator<PromotedFloatType>(),
                                                         get_monospace_shape_generator<PromotedFloatType>(),
                                                         conventional_randomizer::ChanceMachine(10, 5));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_representative_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> first   = std::make_unique<ChanceGenerator>(get_normal_generator<PromotedFloatType>(),
                                                                                                                            get_traditional_generator<PromotedFloatType>(),
                                                                                                                            conventional_randomizer::ChanceMachine(10, 7));

                std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface> second  = std::make_unique<ChanceGenerator>(get_autoshape_generator<PromotedFloatType>(),
                                                                                                                            std::move(first),
                                                                                                                            conventional_randomizer::ChanceMachine(10, 7));

                return second;
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_smart_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec   = {};
                std::unique_ptr<SpaceRadixerInterface> space_radixer                                    = std::make_unique<Base2ExponentialSpaceRadixer>();

                for (size_t i = 0u; i < space_radixer->enumeration_size(); ++i)
                {
                    base_vec.push_back(get_representative_generator<PromotedFloatType>());
                }

                return std::make_unique<SmartProjectorGenerator>(std::move(base_vec), std::move(space_radixer));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_best_generator() -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                return get_smart_generator<PromotedFloatType>();
            }
    };
}

#endif