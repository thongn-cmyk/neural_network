//HEADER_CONTROL 2

#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "temporal_coefficient_projector_interface.h"
#include <general_definition/float_def.h>
#include <memory>
#include <vector>
#include <stl_extension/stdx.h>
#include "conventional_randomizer.h"
#include "coefficient_randomizer.h"
#include "space_operation.h"
#include "shape_projection.h"
#include "taylor_projection.h"
#include "quantization_machine.h"

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
    class DomainScaledTemporalCoefficientProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::unique_ptr<TemporalCoefficientProjectorInterface> domain_scaler;
            std::unique_ptr<TemporalCoefficientProjectorInterface> rhs;

        public:

            DomainScaledTemporalCoefficientProjector(std::unique_ptr<TemporalCoefficientProjectorInterface> domain_scaler,
                                                     std::unique_ptr<TemporalCoefficientProjectorInterface> rhs): domain_scaler(std::move(domain_scaler)),
                                                                                                                  rhs(std::move(rhs)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<PromotedFloatType> lhs_vec  = stdx::to_castable_vector_initializer(this->domain_scaler->project(t));

                if (lhs_vec.size() != 1)
                {
                    throw std::runtime_error("internal corruption, operation size mismatched for domain scaled operation");
                }

                return this->rhs->project(lhs_vec.front() * t);
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

    template <class PromotedFloatType = std_float_t>
    class TaylorRadianSeriesProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec;

        public:

            TaylorRadianSeriesProjector(std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec) noexcept: coefficient_2d_vec(std::move(coefficient_2d_vec)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                PromotedFloatType promoted_time_lapsed  = static_cast<PromotedFloatType>(t);
                std::vector<std_float_t> result_vec     = {};

                for (const auto& coefficient_vec: this->coefficient_2d_vec)
                {
                    PromotedFloatType projection_result = shape_projection::taylor_shape_project(promoted_time_lapsed,
                                                                                                 coefficient_vec.data(), stdx::to_size_container(coefficient_vec.size()));

                    result_vec.push_back(projection_result);
                }

                return result_vec;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class LerpProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec;
            std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>> quantization_machine;

        public:

            LerpProjector(std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec,
                          std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>> quantization_machine) noexcept: coefficient_2d_vec(std::move(coefficient_2d_vec)),
                                                                                                                                                 quantization_machine(std::move(quantization_machine)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> result_vec = {};
                size_t quantization_idx             = this->quantization_machine->quantitize(t);

                for (const auto& d_vec: this->coefficient_2d_vec)
                {
                    result_vec.push_back
                    (
                        this->lerp_project(t, d_vec, quantization_idx)
                    );
                }

                return result_vec;
            }

        private:

            auto lerp_project(PromotedFloatType x,
                              const std::vector<PromotedFloatType>& coeff_vec,
                              size_t idx) -> PromotedFloatType
            {
                if (coeff_vec.empty())
                {
                    return 0;
                }

                if (coeff_vec.size() == 1u)
                {
                    return coeff_vec.front();
                }

                constexpr size_t LERP_COEFFICIENT_SZ    = 2u;
                size_t offset                           = idx * LERP_COEFFICIENT_SZ;
                PromotedFloatType result                = 0;

                {
                    size_t idx              = (offset + 0u) % coeff_vec.size();
                    PromotedFloatType coeff = coeff_vec[idx];
                    result                  += coeff;
                }

                {
                    size_t idx              = (offset + 1u) % coeff_vec.size();
                    PromotedFloatType coeff = coeff_vec[idx];
                    result                  += coeff * x;
                }

                return result;
            }
    };

    template <class PromotedFloatType = std_float_t>
    class CubicSplineProjector: public virtual TemporalCoefficientProjectorInterface
    {
        private:

            std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec;
            std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>> quantization_machine;

        public:

            CubicSplineProjector(std::vector<std::vector<PromotedFloatType>> coefficient_2d_vec,
                                 std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>> quantization_machine) noexcept: coefficient_2d_vec(std::move(coefficient_2d_vec)),
                                                                                                                                                        quantization_machine(std::move(quantization_machine)){}

            auto project(std_float_t t) -> std::vector<std_float_t>
            {
                std::vector<std_float_t> result_vec = {};
                size_t quantization_idx             = this->quantization_machine->quantitize(t);

                for (const auto& d_vec: this->coefficient_2d_vec)
                {
                    result_vec.push_back
                    (
                        this->cubic_project(t, d_vec, quantization_idx)
                    );
                }

                return result_vec;
            }

        private:

            auto cubic_project(PromotedFloatType x,
                               const std::vector<PromotedFloatType>& coeff_vec,
                               size_t idx) -> PromotedFloatType
            {
                if (coeff_vec.empty())
                {
                    return 0;
                }

                if (coeff_vec.size() == 1u)
                {
                    return coeff_vec[0];
                }

                if (coeff_vec.size() == 2u)
                {
                    return coeff_vec[0]
                        + coeff_vec[1] * x;
                }

                if (coeff_vec.size() == 3u)
                {
                    return coeff_vec[0]
                        + coeff_vec[1] * x
                        + coeff_vec[2] * x * x;
                }

                constexpr size_t CUBIC_COEFFICIENT_SZ   = 4u;
                size_t offset                           = idx * CUBIC_COEFFICIENT_SZ;

                PromotedFloatType result                = 0;
                PromotedFloatType multiplier            = 1;

                for (size_t i = 0u; i < CUBIC_COEFFICIENT_SZ; ++i)
                {
                    size_t idx              = (offset + i) % coeff_vec.size();
                    PromotedFloatType coeff = coeff_vec[idx];
                    result                  += coeff * multiplier;
                    multiplier              *= x;
                }

                return result;
            }
    };

    class CoefficientProjectorFactory
    {
        public:

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto get_line_projector(const std::vector<FloatType>& coefficient_vec,
                                           const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                stdx::safe_float_range_access(coefficient_vec.data(), coefficient_vec.size());

                return std::make_unique<LineTemporalCoefficientProjector<PromotedFloatType>>(stdx::to_castable_vector_initializer(coefficient_vec));
            }

            template <class FloatType0,
                      class FloatType1,
                      class FloatType2,
                      class PromotedFloatType = FloatType0>
            static auto get_oval_projector(const std::vector<FloatType0>& directional_vec,
                                           const std::vector<FloatType1>& radian_vec,
                                           const std::vector<FloatType2>& radius_vec,
                                           const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (directional_vec.size() != radian_vec.size())
                {
                    throw std::invalid_argument("bad directional vector size, size mismatched");
                }

                if (radian_vec.size() != radius_vec.size())
                {
                    throw std::invalid_argument("bad radian vector size, size mismatched");
                }

                stdx::safe_float_range_access(directional_vec.data(), directional_vec.size());
                stdx::safe_float_range_access(radian_vec.data(), radian_vec.size());
                stdx::safe_float_range_access(radius_vec.data(), radius_vec.size());

                return std::make_unique<OvalTemporalCoefficientProjector<PromotedFloatType>>
                (
                    LineTemporalCoefficientProjector<PromotedFloatType>(stdx::to_castable_vector_initializer(directional_vec)),
                    stdx::to_castable_vector_initializer(radian_vec),
                    stdx::to_castable_vector_initializer(radius_vec)
                );
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_multiplication_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& lhs,
                                                     std::unique_ptr<TemporalCoefficientProjectorInterface>&& rhs,
                                                     const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (lhs == nullptr)
                {
                    throw std::invalid_argument("bad lhs, null");
                }

                if (rhs == nullptr)
                {
                    throw std::invalid_argument("bad rhs, null");
                }

                return std::make_unique<MultiplicationTemporalCoefficientProjector<PromotedFloatType>>
                (
                    std::move(lhs),
                    std::move(rhs)
                );
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_domain_scaled_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& domain_scaler,
                                                    std::unique_ptr<TemporalCoefficientProjectorInterface>&& rhs,
                                                    const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (domain_scaler == nullptr)
                {
                    throw std::invalid_argument("bad domain scaler, null");
                }

                if (rhs == nullptr)
                {
                    throw std::invalid_argument("bad rhs, null");
                }

                return std::make_unique<DomainScaledTemporalCoefficientProjector<PromotedFloatType>>
                (
                    std::move(domain_scaler),
                    std::move(rhs)
                );
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_generic_oval_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& domain_projector,
                                                   std::unique_ptr<TemporalCoefficientProjectorInterface>&& radius_projector,
                                                   std::unique_ptr<TemporalCoefficientProjectorInterface>&& direction_projector,
                                                   const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (domain_projector == nullptr)
                {
                    throw std::invalid_argument("bad domain projector, null");
                }

                if (radius_projector == nullptr)
                {
                    throw std::invalid_argument("bad radius projector, null");
                }

                if (direction_projector == nullptr)
                {
                    throw std::invalid_argument("bad direction projector, null");
                }

                return std::make_unique<GenericOvalTemporalCoefficientProjector<PromotedFloatType>>
                (
                    std::move(domain_projector),
                    std::move(radius_projector),
                    std::move(direction_projector)
                );
            }

            static auto get_translation_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& domain_projector,
                                                  const std::vector<size_t>& translation_table,
                                                  size_t projection_sz) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (domain_projector == nullptr)
                {
                    throw std::invalid_argument("bad domain projector, null");
                }

                return std::make_unique<TranslationProjector>
                (
                    std::move(domain_projector),
                    translation_table,
                    projection_sz
                );
            }

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

            static auto get_scalar_scaled_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& base,
                                                    std_float_t scalar_value) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                if (std::isnan(scalar_value))
                {
                    throw std::invalid_argument("bad scalar value, NaN");
                }

                return std::make_unique<ScalarScaledProjector>
                (
                    std::move(base),
                    scalar_value
                );
            }

            static auto get_pairwise_scaled_projector(std::unique_ptr<TemporalCoefficientProjectorInterface>&& base,
                                                      const std::vector<std_float_t>& scaled_coeff_vec) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                stdx::safe_float_range_access(scaled_coeff_vec.data(), scaled_coeff_vec.size());

                return std::make_unique<PairWiseScaledProjector>
                (
                    std::move(base),
                    scaled_coeff_vec
                );
            }

            static auto get_point_projector(const std::vector<std_float_t>& coor) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                stdx::safe_float_range_access(coor.data(), coor.size());

                return std::make_unique<PointCoefficientProjector>(coor);
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto get_taylor_series_projector(const std::vector<std::vector<FloatType>>& coefficient_2d_vec,
                                                    const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std::vector<PromotedFloatType>> promoted_coefficient_2d_vec{};

                for (const auto& d_vec: coefficient_2d_vec)
                {
                    stdx::safe_float_range_access(d_vec.data(), d_vec.size());
                    promoted_coefficient_2d_vec.push_back(stdx::to_castable_vector_initializer(d_vec));
                }

                return std::make_unique<TaylorSeriesProjector<PromotedFloatType>>(std::move(promoted_coefficient_2d_vec));
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto get_taylor_radian_series_projector(const std::vector<std::vector<FloatType>>& coefficient_2d_vec,
                                                           const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std::vector<PromotedFloatType>> promoted_coefficient_2d_vec{};

                for (const auto& d_vec: coefficient_2d_vec)
                {
                    stdx::safe_float_range_access(d_vec.data(), d_vec.size());
                    promoted_coefficient_2d_vec.push_back(stdx::to_castable_vector_initializer(d_vec));
                }

                return std::make_unique<TaylorRadianSeriesProjector<PromotedFloatType>>(std::move(promoted_coefficient_2d_vec));
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto get_lerp_projector(const std::vector<std::vector<FloatType>>& coefficient_2d_vec,
                                           std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>>&& quantization_machine,
                                           const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (quantization_machine == nullptr)
                {
                    throw std::invalid_argument("bad quantization machine, null");
                }

                std::vector<std::vector<PromotedFloatType>> promoted_coefficient_2d_vec{};

                for (const auto& d_vec: coefficient_2d_vec)
                {
                    stdx::safe_float_range_access(d_vec.data(), d_vec.size());
                    promoted_coefficient_2d_vec.push_back(stdx::to_castable_vector_initializer(d_vec));
                }

                return std::make_unique<LerpProjector<PromotedFloatType>>(std::move(promoted_coefficient_2d_vec),
                                                                          std::move(quantization_machine));
            }

            template <class FloatType, class PromotedFloatType = FloatType>
            static auto get_cubic_spline_projector(const std::vector<std::vector<FloatType>>& coefficient_2d_vec,
                                                   std::unique_ptr<quantization_machine::QuantizationMachineInterface<PromotedFloatType>>&& quantization_machine,
                                                   const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                if (quantization_machine == nullptr)
                {
                    throw std::invalid_argument("bad quantization machine, null");
                }

                std::vector<std::vector<PromotedFloatType>> promoted_coefficient_2d_vec{};

                for (const auto& d_vec: coefficient_2d_vec)
                {
                    stdx::safe_float_range_access(d_vec.data(), d_vec.size());
                    promoted_coefficient_2d_vec.push_back(stdx::to_castable_vector_initializer(d_vec));
                }

                return std::make_unique<CubicSplineProjector<PromotedFloatType>>(std::move(promoted_coefficient_2d_vec),
                                                                                 std::move(quantization_machine));
            }

            static auto get_chained_projector(std::vector<std::unique_ptr<TemporalCoefficientProjectorInterface>>&& projector_vec) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
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
    };

    class BasicApplicableProjectorFactory
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

            template <class PromotedFloatType = std_float_t>
            static auto get_random_line_coefficient_projector(size_t coefficient_sz,
                                                              const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std_float_t> unit_vec = clamp_vector(get_random_unit_vector(coefficient_sz));

                return CoefficientProjectorFactory::get_line_projector(unit_vec, tag);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_oval_coefficient_projector(size_t coefficient_sz,
                                                              const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                std::vector<std_float_t> directional_vec  = clamp_vector(get_random_unit_vector(coefficient_sz));
                std::vector<std_float_t> radian_vec       = clamp_vector(get_random_radian_coordinate(coefficient_sz));
                std::vector<std_float_t> radius_vec       = clamp_vector(space_operation::mul_vector(get_random_unit_vector(coefficient_sz), static_cast<std_float_t>(FocalRandomizer::ld_randomize_focal())));

                return CoefficientProjectorFactory::get_oval_projector(directional_vec,
                                                                       radian_vec,
                                                                       radius_vec,
                                                                       tag);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_rotating_arm_coefficient_projector(size_t coefficient_sz,
                                                                      const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                return CoefficientProjectorFactory::get_chained_projector(stdx::to_variadic_vector_initializer(get_random_oval_coefficient_projector(coefficient_sz, tag),
                                                                                                               get_random_oval_coefficient_projector(coefficient_sz, tag)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_line_oval_coefficient_projector(size_t coefficient_sz,
                                                                   const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                return CoefficientProjectorFactory::get_chained_projector(stdx::to_variadic_vector_initializer(get_random_line_coefficient_projector(coefficient_sz, tag),
                                                                                                               get_random_oval_coefficient_projector(coefficient_sz, tag)));
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_random_coefficient_projector(size_t coefficient_sz,
                                                         const stdx::Tag<PromotedFloatType>& tag = stdx::Tag<PromotedFloatType>{}) -> std::unique_ptr<TemporalCoefficientProjectorInterface>
            {
                using projector_func_ptr = decltype(&get_random_line_coefficient_projector<PromotedFloatType>);

                static std::vector<projector_func_ptr> func_ptr_vec
                {
                    get_random_line_coefficient_projector<PromotedFloatType>,
                    get_random_oval_coefficient_projector<PromotedFloatType>,
                    get_random_rotating_arm_coefficient_projector<PromotedFloatType>,
                    get_random_line_oval_coefficient_projector<PromotedFloatType>
                };

                return func_ptr_vec[NumericRandomizer::randomize_uint(0u, func_ptr_vec.size())](coefficient_sz, tag);
            }
    };

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
            conventional_randomizer::RangeRandomizerObject range_randomizer;

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
                size_t tentative_sz             = this->range_randomizer.randomize_range(sz + 1u);

                return std::max(tentative_sz, MIN_ACTIVATION_SZ);
            }

            auto min_chunk_size_for_vector_size_of(size_t sz) -> size_t
            {
                return sz / MAX_VECTOR_SIZE + static_cast<size_t>(sz % MAX_VECTOR_SIZE != 0u);
            }

            auto randomize_vector_chunk_size(size_t sz) -> size_t
            {
                size_t tentative_chunk_size     = this->range_randomizer.randomize_range(sz + 1u);
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