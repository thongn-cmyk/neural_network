//HEADER_CONTROL 2

#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "temporal_coefficient_projector_interface.h"
#include "float_def.h"
#include <memory>
#include <vector>
#include "stdx.h"
#include "conventional_randomizer.h"
#include "coefficient_randomizer.h"
#include "space_operation.h"
#include "taylor_projection.h"

namespace temporal_coefficient_projector
{
    class SharedPointerProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::shared_ptr<TemporalCoefficientProjectorInterface> base;
        
        public:

            SharedPointerProjector(std::shared_ptr<TemporalCoefficientProjectorInterface> base) noexcept: base(std::move(base)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                return this->base->project(t);
            }
    };

    class ScalarScaledProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> base;
            std_float_t scalar_value;
        
        public:

            ScalarScaledProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> base,
                                  std_float_t scalar_value) noexcept: base(std::move(base)),
                                                                      scalar_value(scalar_value){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> tentative_projection = this->base->project(t);

                for (size_t i = 0u; i < tentative_projection.size(); ++i)
                {
                    tentative_projection[i] *= this->scalar_value;
                }

                return tentative_projection;
            }
    };

    class PairWiseScaledProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> base;
            std::vector<std_float_t> scaled_coeff_vec;

        public:

            PairWiseScaledProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> base,
                                    std::vector<std_float_t> scaled_coeff_vec) noexcept: base(std::move(base)),
                                                                                         scaled_coeff_vec(std::move(scaled_coeff_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> tentative_projection = this->base->project(t);

                if (this->scaled_coeff_vec.size() != tentative_projection.size())
                {
                    throw std::runtime_error("internal corruption");
                }

                std::vector<std_float_t> scaled_projection(tentative_projection.size());

                for (size_t i = 0u; i < tentative_projection.size(); ++i)
                {
                    scaled_projection[i] = tentative_projection[i] * this->scaled_coeff_vec[i];
                }

                return scaled_projection;
            }
    };

    class ActivationProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> base;
            std::vector<bool> activation_vec;

        public:

            ActivationProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> base,
                                std::vector<bool> activation_vec) noexcept: base(std::move(base)),
                                                                            activation_vec(std::move(activation_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> tentative_projection = this->base->project(t);

                if (activation_vec.size() != tentative_projection.size())
                {
                    throw std::runtime_error("internal corruption");
                }

                std::vector<std_float_t> activated_projection(tentative_projection.size());

                for (size_t i = 0u; i < tentative_projection.size(); ++i)
                {
                    if (this->activation_vec[i])
                    {
                        activated_projection[i] = tentative_projection[i];
                    }
                    else
                    {
                        activated_projection[i] = 0;
                    }
                }

                return activated_projection;
            }
    };

    class PointCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std_float_t> space;

        public:

            PointCoefficientProjector(std::vector<std_float_t> space) noexcept: space(std::move(space)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                return this->space;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class LineTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<PromotedFloatType> space;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            LineTemporalCoefficientProjector(std::vector<PromotedFloatType> space) noexcept: space(std::move(space)){}

            auto high_resolution_project(std_float_t t) -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(this->space.size());

                space_operation::restrict_scalar_mul_array<PromotedFloatType>(this->space.data(), this->space.size(),
                                                                              t,
                                                                              rs.data());

                return rs;
            }

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                return stdx::to_castable_vector_initializer(this->high_resolution_project(t));
            }
    };

    template <class PromotedFloatType = std_float_t>
    class OvalTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            LineTemporalCoefficientProjector<PromotedFloatType> base_projector;
            std::vector<PromotedFloatType> radian_space;
            std::vector<PromotedFloatType> radius_space;

        public:

            static_assert(std::is_floating_point_v<PromotedFloatType>);

            OvalTemporalCoefficientProjector(LineTemporalCoefficientProjector<PromotedFloatType> base_projector,
                                             std::vector<PromotedFloatType> radian_space,
                                             std::vector<PromotedFloatType> radius_space) noexcept: base_projector(std::move(base_projector)),
                                                                                                    radian_space(std::move(radian_space)),
                                                                                                    radius_space(std::move(radius_space)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<PromotedFloatType> radian_incremental_space = this->base_projector.high_resolution_project(t);
                std::vector<PromotedFloatType> current_radian_space     = std::vector<PromotedFloatType>(this->radian_space.size());

                if (this->radian_space.size() != radian_incremental_space.size())
                {
                    throw std::runtime_error("internal corruption, oval temporal coefficient projector");
                }

                space_operation::restrict_add_array<PromotedFloatType>(this->radian_space.data(), radian_incremental_space.data(),
                                                                       this->radian_space.size(),
                                                                       current_radian_space.data());

                std::vector<PromotedFloatType> rs(this->radian_space.size());

                space_operation::restrict_multidimensional_oval_to_euclidean_array<PromotedFloatType>(current_radian_space.data(), current_radian_space.size(),
                                                                                                      this->radius_space.data(),
                                                                                                      rs.data());

                return stdx::to_castable_vector_initializer(std::move(rs));
            }
    };

    template <class PromotedFloatType = std_float_t>
    class Oscillator
    {
        private:

            PromotedFloatType alpha;
            PromotedFloatType beta;
            PromotedFloatType x_c;
            PromotedFloatType y_c;

        public:

            Oscillator(PromotedFloatType alpha,
                       PromotedFloatType beta,
                       PromotedFloatType x_c,
                       PromotedFloatType y_c) noexcept: alpha(alpha),
                                                        beta(beta),
                                                        x_c(x_c),
                                                        y_c(y_c){}

            auto f(std_float_t x) -> PromotedFloatType
            {
                return this->alpha * std::sin(this->beta * x + this->x_c) + this->y_c;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class OscillatorTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::unique_ptr<Oscillator<PromotedFloatType>>> oscillator_vec;

        public:

            OscillatorTemporalCoefficientProjector(std::vector<std::unique_ptr<Oscillator<PromotedFloatType>>> oscillator_vec) noexcept: oscillator_vec(std::move(oscillator_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs(this->oscillator_vec.size());

                for (size_t i = 0u; i < this->oscillator_vec.size(); ++i)
                {
                    rs[i] = this->oscillator_vec[i]->f(t);
                }

                return rs;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class ExponentialTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<PromotedFloatType> space;

        public:

            ExponentialTemporalCoefficientProjector(std::vector<PromotedFloatType> space) noexcept: space(std::move(space)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs(this->space.size());

                for (size_t i = 0u; i < this->space.size(); ++i)
                {
                    rs[i] = std::exp(this->space[i]);
                }

                return rs;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class MultiplicationTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> lhs;
            std::unique_ptr<TemporalCoefficientProjectorInterface> rhs;
        
        public:

            MultiplicationTemporalCoefficientProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> lhs,
                                                       std::unique_ptr<TemporalCoefficientProjectorInterface> rhs) noexcept: lhs(std::move(lhs)),
                                                                                                                             rhs(std::move(rhs)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<PromotedFloatType> lhs_vec = stdx::to_castable_vector_initializer(this->lhs->project(t));
                std::vector<PromotedFloatType> rhs_vec = stdx::to_castable_vector_initializer(this->rhs->project(t));

                if (lhs_vec.size() != rhs_vec.size())
                {
                    throw std::runtime_error("internal corruption, operation size mismatched for multiplication projection");
                }

                std::vector<PromotedFloatType> rs_vec(lhs_vec.size());

                for (size_t i = 0u; i < lhs_vec.size(); ++i)
                {
                    rs_vec[i] = lhs_vec[i] * rhs_vec[i];
                }

                return stdx::to_castable_vector_initializer(std::move(rs_vec));
            }
    };

    template <class PromotedFloatType = std_float_t>
    class GenericOvalTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> domain_projector;
            std::unique_ptr<TemporalCoefficientProjectorInterface> radius_projector;
            std::unique_ptr<TemporalCoefficientProjectorInterface> direction_projector;

        public:

            GenericOvalTemporalCoefficientProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> domain_projector,
                                                    std::unique_ptr<TemporalCoefficientProjectorInterface> radius_projector,
                                                    std::unique_ptr<TemporalCoefficientProjectorInterface> direction_projector) noexcept: domain_projector(std::move(domain_projector)),
                                                                                                                                          radius_projector(std::move(radius_projector)),
                                                                                                                                          direction_projector(std::move(direction_projector)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<PromotedFloatType> domain_vec       = stdx::to_castable_vector_initializer(this->domain_projector->project(t));
                std::vector<PromotedFloatType> radius_vec       = stdx::to_castable_vector_initializer(this->radius_projector->project(t));
                std::vector<PromotedFloatType> direction_vec    = stdx::to_castable_vector_initializer(this->direction_projector->project(t));

                if (domain_vec.size() != radius_vec.size())
                {
                    throw std::runtime_error("internal corruption, operation size mismatched for generic oval projection");
                }

                if (domain_vec.size() != direction_vec.size())
                {
                    throw std::runtime_error("internal corruption, operation size mismatched for generic oval projection 2");
                }

                std::vector<PromotedFloatType> tmp_rs(domain_vec.size());
                std::vector<PromotedFloatType> rs(domain_vec.size());

                space_operation::restrict_multidimensional_oval_to_euclidean_array<PromotedFloatType>(domain_vec.data(), domain_vec.size(),
                                                                                                      radius_vec.data(),
                                                                                                      rs.data());

                // space_operation::rotate_euclidean_coordinate(tmp_rs.data(), tmp_rs.size(),
                                                            //  direction_vec.data(),
                                                            //  rs.data());

                return stdx::to_castable_vector_initializer(std::move(rs));
            }
    };

    class TranslationProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> domain_projector;
            std::vector<size_t> translation_table;
            size_t projection_sz;
        
        public:

            TranslationProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> domain_projector,
                                 std::vector<size_t> translation_table,
                                 size_t projection_sz) noexcept: domain_projector(std::move(domain_projector)),
                                                                 translation_table(std::move(translation_table)),
                                                                 projection_sz(projection_sz){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs(this->projection_sz, 0);
                std::vector<std_float_t> domain = this->domain_projector->project(t);

                if (domain.size() != this->translation_table.size())
                {
                    throw std::runtime_error("internal corruption, operation size mismatched for TranslationProjector");
                }

                for (size_t i = 0u; i < domain.size(); ++i)
                {
                    size_t src_idx  = i;
                    size_t dst_idx  = this->translation_table[i];

                    if (dst_idx >= this->projection_sz)
                    {
                        throw std::runtime_error("internal corruption, out of bound access of TranslationProjector's projection vector");
                    }

                    rs[dst_idx]     = domain[src_idx];
                }

                return rs;
            }
    };

    class ChainedTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>> projector_vec;

        public:

            ChainedTemporalCoefficientProjector(std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>> projector_vec) noexcept: projector_vec(std::move(projector_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::optional<std::vector<std_float_t>> result_vec{};

                for (const auto& projector: this->projector_vec)
                {
                    auto incremental_vec = projector->project(t);

                    if (!result_vec.has_value())
                    {
                        result_vec = std::move(incremental_vec);
                    }
                    else
                    {
                        if (result_vec->size() != incremental_vec.size())
                        {
                            throw std::runtime_error("internal_corruption, incompatible chained dimension size");
                        }

                        result_vec = space_operation::add_vector(*result_vec, incremental_vec);
                    }
                }

                if (!result_vec.has_value())
                {
                    throw std::runtime_error("internal corruption");
                }

                return std::move(result_vec.value());
            }
    };

    template <class PromotedFloatType = std_float_t>
    class TaylorSeriesProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec;
        
        public:

            TaylorSeriesProjector(std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec) noexcept: coefficient_2d_vec(std::move(coefficient_2d_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                PromotedFloatType promoted_time_lapsed  = static_cast<PromotedFloatType>(t);
                std::vector<std_float_t> result_vec     = {};

                for (const auto& coefficient_vec: this->coefficient_2d_vec)
                {
                    PromotedFloatType projection_result = taylor_projection::taylor_project(promoted_time_lapsed,
                                                                                            coefficient_vec.data(), stdx::to_size_container(coefficient_vec.size()));

                    result_vec.push_back(projection_result);
                }

                return result_vec;
            }
    };

    class CoefficientProjectorFactory
    {
        private:

            static inline const std_float_t MIN_LOGIT_VALUE   = std::numeric_limits<std_float_t>::min();
            static inline const std_float_t MAX_LOGIT_VALUE   = stdx::to_precise_float_conversion_initializer<double>(size_t{1} << 20);

            struct Signature{};

            using NumericRandomizer     = conventional_randomizer::RandomizerFacility<Signature>;
            using Randomizer            = stdx::thread_safe_singleton_container<coefficient_randomizer::CoefficientRandomizer, Signature>;
            using FocalRandomizer       = conventional_randomizer::ApplicationRandomizerFacility<Signature>;

            static auto get_random_radian_coordinate(size_t dimension_sz) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs;

                auto accessor = [&](auto& object)
                {
                    rs = object.template randomize_radian_vector<std_float_t>(dimension_sz);
                };

                Randomizer::access(accessor);

                return rs;
            }

            static auto clamp_vector(const std::vector<std_float_t>& vec) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs{};

                for (const auto& e: vec)
                {
                    rs.push_back(stdx::float_clamp(e, MIN_LOGIT_VALUE, MAX_LOGIT_VALUE));
                }

                return rs;
            }

            static auto get_random_unit_vector(size_t coefficient_sz) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> rs;

                auto accessor = [&](auto& object)
                {
                    rs = object.template randomize_unit_vector<std_float_t>(coefficient_sz);
                };

                Randomizer::access(accessor);

                return rs;
            }

        public:

            static auto get_activation_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& projector,
                                                 std::vector<bool> activation_vec) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (projector == nullptr)
                {
                    throw std::invalid_argument("bad projector, null");
                }

                std::vector<std_float_t> scaled_vec(activation_vec.size());

                for (size_t i = 0u; i < activation_vec.size(); ++i)
                {
                    if (activation_vec[i] == true)
                    {
                        scaled_vec[i] = 1;
                    }
                    else
                    {
                        scaled_vec[i] = 0;
                    }
                }

                return std::make_unique<PairWiseScaledProjector>(std::move(projector), std::move(scaled_vec));
            }

            static auto get_point_coefficient_projector(const std::vector<std_float_t>& coor) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                for (const auto& e: coor)
                {
                    if (std::isnan(e))
                    {
                        throw std::invalid_argument("bad numeric value, NaN");
                    }
                }

                return std::make_unique<PointCoefficientProjector>(coor);
            }

            static auto get_chained_coefficient_projector(std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>>&& projector_vec) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                for (const auto& e: projector_vec)
                {
                    if (e == nullptr)
                    {
                        throw std::invalid_argument("bad projector, null");
                    }
                }

                return std::make_unique<ChainedTemporalCoefficientProjector>(std::move(projector_vec));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_line_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std_float_t> unit_vec = clamp_vector(get_random_unit_vector(coefficient_sz));

                return std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(stdx::to_castable_vector_initializer<PromotedFloatType>(std::move(unit_vec)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_oval_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std_float_t> directional_vec  = clamp_vector(get_random_unit_vector(coefficient_sz));
                std::vector<std_float_t> radian_vec       = clamp_vector(get_random_radian_coordinate(coefficient_sz));
                std::vector<std_float_t> radius_vec       = clamp_vector(space_operation::mul_vector(get_random_unit_vector(coefficient_sz), static_cast<std_float_t>(FocalRandomizer::ld_randomize_focal())));

                return std::make_unique<OvalTemporalCoefficientProjector<PromotedFloatType>>(LineTemporalCoefficientProjector<PromotedFloatType>(stdx::to_castable_vector_initializer<PromotedFloatType>(std::move(directional_vec))),
                                                                                             stdx::to_castable_vector_initializer<PromotedFloatType>(std::move(radian_vec)),
                                                                                             stdx::to_castable_vector_initializer<PromotedFloatType>(std::move(radius_vec)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_rotating_arm_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                using namespace stdx;

                return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                            get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_line_oval_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                using namespace stdx;

                return std::make_unique<ChainedTemporalCoefficientProjector>(to_variadic_vector_initializer(get_random_line_coefficient_projector<PromotedFloatType>(coefficient_sz),
                                                                                                            get_random_oval_coefficient_projector<PromotedFloatType>(coefficient_sz)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_coefficient_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                using projector_func_ptr = std::unique_ptr<TemporalCoefficientProjectorInterface> (*) (size_t);

                static std::vector<projector_func_ptr> func_ptr_vec{
                    get_random_line_coefficient_projector<PromotedFloatType>,
                    get_random_oval_coefficient_projector<PromotedFloatType>,
                    get_random_rotating_arm_coefficient_projector<PromotedFloatType>,
                    get_random_line_oval_coefficient_projector<PromotedFloatType>
                };

                return func_ptr_vec[NumericRandomizer::randomize_uint(0u, func_ptr_vec.size())](coefficient_sz);
            }
    };

    //this is very super hard, but we'd have to translate the coefficient sz into an arbitrary space of activated tensor nodes, otherwise we'd be floating-wise screwed

    //in the FixedOvalProjectorFactory, we have done very well describing

    //the radian-cursor speed of the multidimensional sphere for each of the dimensions
    //relatively is important in the context

    //then we'd want to scale that relatively_system by a scalar value, which is exponential or uniformly distributed

    //so as soon as we finish iterating one sphere, we'd move another sphere cursor by an inch, essentially a nested for loop, in the extreme scenerio
    //or we'd just iterate one sphere in another extreme scenerio, so the value a is important in the context, such is that the derivative at the point might not be relevant in the extreme context, says 1/100*sin(10000x) - x

    //the reason I say this is hard because on one hand, we'd try our best to find the global minima, on the other hand, we'd want to increase that end to be so incredibly difficult that we would not mess up the dynamic-context layers 
    //if the lower layers are formed too quickly, we'd have a really hard time to adjust the lower layers, which would defeat the purpose of dynamic context building

    //but if it is too hard to bend the space for the lower and upper layers, we'd have slower decay of uniform distribution of possible shapes for the final output layer according to the former layers

    //yesterday I was proving that this optimization radix is different than that of the different ground projections, such is that one cannot say we'd have to do only this because this is sufficient
    //the different ground projections are to estimate the best possible hops to not over-abstractize the context

    //we'd try to work on the branch predicted version of these, because this is actually predictable in some of the scenerios

    //it seems to me that the question of whether a semantic skewed in what direction could be exploited in run-time and re-directed in run-time
    //because in the uniform-distribution scenerio of logit a being equivalently as important as logit b, there seems to be no-problem for the projection to be a sphere-shaped semantic
    //but we do have a problem otherwise

    //the reason that radius a == radius b being so important is that every point within the r + r would intersect with the closed space of the upper arm if we are to draw a closed space of the same size around it
    //this means that there exists a closed space of the radius at the intersected point that touches the point
    //alright, so we'd settle for this being the solution

    //so the question would be to map the searching semantic space into a uniform space where every point in the space is equivalently worth exploring
    //on top of this assumed semantic space

    template <class PromotedFloatType = std_float_t>
    class FixedOvalProjectorFactory
    {
        private:

            conventional_randomizer::RandomizerObject raw_randomizer;
            conventional_randomizer::ApplicationRandomizerObject app_randomizer;

        public:

            auto get_random_oval_projector(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(stdx::to_castable_vector_initializer(this->get_scaled_domain_vector_for_size_of(coefficient_sz))),
                                                                                                    std::make_unique<PointCoefficientProjector>(stdx::to_castable_vector_initializer(this->get_scaled_radius_vector_for_size_of(coefficient_sz))),
                                                                                                    std::make_unique<PointCoefficientProjector>(stdx::to_castable_vector_initializer(this->get_scaled_directional_vector_for_size_of(coefficient_sz))));
            }

            auto get_random_rotating_2_arm(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                PromotedFloatType r;

                if (this->raw_randomizer.flip_a_coin())
                {
                    r = this->app_randomizer.ld_randomize_focal();
                }
                else
                {
                    r = this->app_randomizer.ld_randomize_focal_2();
                }

                return std::make_unique<ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(this->get_random_circle_projector_for_radius_of(coefficient_sz, r),
                                                                                                                  this->get_random_circle_projector_for_radius_of(coefficient_sz, r)));
            }

            auto get_random_rotating_2_skewedarm(size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<ChainedTemporalCoefficientProjector>(stdx::to_variadic_vector_initializer(this->get_random_oval_projector(coefficient_sz),
                                                                                                                  this->get_random_oval_projector(coefficient_sz)));
            }

        private:

            auto get_random_circle_projector_for_radius_of(size_t coefficient_sz, PromotedFloatType r) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                return std::make_unique<GenericOvalTemporalCoefficientProjector<PromotedFloatType>>(std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(stdx::to_castable_vector_initializer(this->get_scaled_domain_vector_for_size_of(coefficient_sz))),
                                                                                                    std::make_unique<PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::vector<PromotedFloatType>(coefficient_sz, r))),
                                                                                                    std::make_unique<PointCoefficientProjector>(stdx::to_castable_vector_initializer(std::vector<PromotedFloatType>(coefficient_sz, 0))));
            }

            auto get_expdst2_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    rs[i] = this->app_randomizer.ld_randomize_focal_2();
                }

                return rs;
            }

            auto get_expdst_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                std::vector<PromotedFloatType> rs(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    rs[i] = this->app_randomizer.ld_randomize_focal();
                }

                return rs;
            }

            auto get_unfdst_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                const PromotedFloatType FIRST_VALUE = 0;
                const PromotedFloatType LAST_VALUE  = 100;
                const size_t DISCRETIZATION_SZ      = 1'000'000'000'000'000ULL;

                std::vector<PromotedFloatType> rs(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    rs[i] = this->raw_randomizer.template randomize_fixed_point_float<PromotedFloatType>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
                }

                return rs;
            }

            auto get_expdst_unsigned_scalar() -> PromotedFloatType
            {
                return this->app_randomizer.ld_randomize_focal();
            }

            auto get_unfdst_unsigned_scalar() -> PromotedFloatType
            {
                const PromotedFloatType FIRST_VALUE = 0;
                const PromotedFloatType LAST_VALUE  = 100;
                const size_t DISCRETIZATION_SZ      = 1'000'000'000'000'000ULL;

                return this->raw_randomizer.template randomize_fixed_point_float<PromotedFloatType>(FIRST_VALUE, LAST_VALUE, DISCRETIZATION_SZ);
            }

            auto get_expdst2_radius_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst2_vector_for_size_of(sz);
            }

            auto get_expdst_radius_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst_vector_for_size_of(sz);
            }

            auto get_unfdst_radius_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_unfdst_vector_for_size_of(sz);
            }

            auto get_radius_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                const size_t ENUMERATION_SZ = 3u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst2_radius_vector_for_size_of(sz);
                    }
                    case 1:
                    {
                        return this->get_expdst_radius_vector_for_size_of(sz);
                    }
                    case 2:
                    {
                        return this->get_unfdst_radius_vector_for_size_of(sz);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_expdst_radius_scalar() -> PromotedFloatType
            {
                return this->get_expdst_unsigned_scalar();
            }

            auto get_unfdst_radius_scalar() -> PromotedFloatType
            {
                return this->get_unfdst_unsigned_scalar();
            }

            auto get_radius_scalar() -> PromotedFloatType
            {
                const size_t ENUMERATION_SZ = 2u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst_radius_scalar();
                    }
                    case 1:
                    {
                        return this->get_unfdst_radius_scalar();
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_scaled_radius_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(this->get_radius_vector_for_size_of(sz), this->get_radius_scalar());
            }

            auto get_expdst2_domain_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst2_vector_for_size_of(sz);
            }

            auto get_expdst_domain_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst_vector_for_size_of(sz);
            }

            auto get_unfdst_domain_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_unfdst_vector_for_size_of(sz);
            }

            auto get_domain_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                const size_t ENUMERATION_SZ = 3u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst2_domain_vector_for_size_of(sz);
                    }
                    case 1:
                    {
                        return this->get_expdst_domain_vector_for_size_of(sz);
                    }
                    case 2:
                    {
                        return this->get_unfdst_domain_vector_for_size_of(sz);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_expdst_domain_scalar() -> PromotedFloatType
            {
                return this->get_expdst_unsigned_scalar();
            }

            auto get_unfdst_domain_scalar() -> PromotedFloatType
            {
                return this->get_unfdst_unsigned_scalar();
            }

            auto get_domain_scalar() -> PromotedFloatType
            {
                const size_t ENUMERATION_SZ = 2u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst_domain_scalar();
                    }
                    case 1:
                    {
                        return this->get_unfdst_domain_scalar();
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_scaled_domain_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(this->get_domain_vector_for_size_of(sz), this->get_domain_scalar());
            }

            auto get_expdst2_directional_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst2_vector_for_size_of(sz);
            }

            auto get_expdst_directional_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_expdst_vector_for_size_of(sz);
            }

            auto get_unfdst_directional_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return this->get_unfdst_vector_for_size_of(sz);
            }

            auto get_directional_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                const size_t ENUMERATION_SZ = 3u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst2_directional_vector_for_size_of(sz);
                    }
                    case 1:
                    {
                        return this->get_expdst_directional_vector_for_size_of(sz);
                    }
                    case 2:
                    {
                        return this->get_unfdst_directional_vector_for_size_of(sz);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_expdst_directional_vector_scalar() -> PromotedFloatType
            {
                return this->get_expdst_unsigned_scalar();
            }

            auto get_unfdst_directional_vector_scalar() -> PromotedFloatType
            {
                return this->get_unfdst_unsigned_scalar();
            }

            auto get_directional_vector_scalar() -> PromotedFloatType
            {
                const size_t ENUMERATION_SZ = 2u;
                size_t enumeration_idx      = this->raw_randomizer.randomize_uint(0u, ENUMERATION_SZ);

                switch (enumeration_idx)
                {
                    case 0:
                    {
                        return this->get_expdst_directional_vector_scalar();
                    }
                    case 1:
                    {
                        return this->get_unfdst_directional_vector_scalar();
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_scaled_directional_vector_for_size_of(size_t sz) -> std::vector<PromotedFloatType>
            {
                return space_operation::mul_vector(this->get_directional_vector_for_size_of(sz), this->get_directional_vector_scalar());
            }
    };

    class RedistributedFocalFactory
    {
        private:

            conventional_randomizer::RandomizerObject raw_randomizer;
            conventional_randomizer::ApplicationRandomizerObject app_randomizer;

            static inline constexpr size_t MAX_VECTOR_SIZE = size_t{1} << 16;

        public:

            template <class Generator>
            auto get(Generator&& generator, size_t coefficient_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                size_t chunk_sz         = this->randomize_vector_chunk_size(coefficient_sz);
                size_t segment_sz       = coefficient_sz / chunk_sz + static_cast<size_t>(coefficient_sz % chunk_sz != 0u);
                size_t activation_sz    = this->randomize_activation_size_within(segment_sz);
                size_t rem_sz           = segment_sz * chunk_sz - coefficient_sz;

                std::vector<activation::activation_codex_t> activation_codex_vec    = this->randomize_activation_vector(activation_sz);
                std::vector<size_t> suffix_array                                    = std::vector<size_t>(segment_sz);

                std::iota(suffix_array.begin(), suffix_array.end(), 0u);

                std::vector<size_t> activated_suffix_array  = activation::activate(suffix_array, activation_codex_vec);
                std::vector<size_t> translation_table       = this->make_translation_table(activated_suffix_array, chunk_sz, segment_sz, rem_sz);
                size_t projection_sz                        = this->count_activated_nodes(activated_suffix_array, chunk_sz, segment_sz, rem_sz);

                return std::make_unique<TranslationProjector>(generator(projection_sz),
                                                              std::move(translation_table),
                                                              coefficient_sz);
            }

        private:

            auto randomize_activation_vector(size_t sz) -> std::vector<activation::activation_codex_t>
            {
                std::vector<activation::activation_codex_t> rs{};

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t enumeration_idx = this->raw_randomizer.randomize_uint(0u, activation::ACTIVATION_CODEX_RANGE);
                    rs.push_back(enumeration_idx);
                }

                return rs;
            }

            auto randomize_activation_size_within(size_t sz) -> size_t
            {
                if (sz == 0u)
                {
                    return 0u;
                }

                const size_t MIN_ACTIVATION_SZ  = 1u;
                size_t tentative_sz             = this->app_randomizer.ld_randomize_percentage_focal() * sz;

                return std::max(tentative_sz, MIN_ACTIVATION_SZ);
            }

            auto min_chunk_size_for_vector_size_of(size_t sz) -> size_t
            {
                return sz / MAX_VECTOR_SIZE + static_cast<size_t>(sz % MAX_VECTOR_SIZE != 0u);
            }

            auto randomize_vector_chunk_size(size_t sz) -> size_t
            {
                size_t tentative_chunk_size     = this->app_randomizer.ld_randomize_percentage_focal() * sz;
                const size_t MIN_CHUNK_SIZE     = 1u;

                return std::max(std::max(MIN_CHUNK_SIZE, tentative_chunk_size), this->min_chunk_size_for_vector_size_of(sz));
            }

            auto make_translation_table(const std::vector<size_t>& chosen_suffix_table,
                                        size_t chunk_sz_per_suffix,
                                        size_t suffix_table_sz,
                                        size_t rem_sz) -> std::vector<size_t> 
            {
                std::vector<size_t> table{};

                for (size_t suffix: chosen_suffix_table)
                {
                    size_t offset = suffix * chunk_sz_per_suffix;
                    size_t suffix_sz;

                    if (suffix + 1u == suffix_table_sz)
                    {
                        suffix_sz = chunk_sz_per_suffix - rem_sz;
                    }
                    else
                    {
                        suffix_sz = chunk_sz_per_suffix;
                    }

                    for (size_t i = 0u; i < suffix_sz; ++i)
                    {
                        table.push_back(offset + i);
                    }
                }

                return table;
            }

            auto count_activated_nodes(const std::vector<size_t>& chosen_suffix_table,
                                       size_t chunk_sz_per_suffix,
                                       size_t suffix_table_sz,
                                       size_t rem_sz) -> size_t
            {
                size_t total = 0u;

                for (size_t suffix: chosen_suffix_table)
                {
                    if (suffix + 1u == suffix_table_sz)
                    {
                        total += chunk_sz_per_suffix - rem_sz;
                    }
                    else
                    {
                        total += chunk_sz_per_suffix;
                    }
                }

                return total;
            }
    };
}

#endif