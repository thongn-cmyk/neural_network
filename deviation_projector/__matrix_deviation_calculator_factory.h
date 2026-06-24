#ifndef __DG_MATRIX_DEVIATION_CALCULATOR_FACTORY_H__
#define __DG_MATRIX_DEVIATION_CALCULATOR_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "matrix_deviation_calculator_interface.h"
#include "matrix_deviation_calculator.h"

namespace deviation_projector
{
    struct NoTransformDeviationCalculatorResource
    {
        std::string promoted_float_kind;
        uint8_t deviation_calculator_machine_kind;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(promoted_float_kind, deviation_calculator_machine_kind);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(promoted_float_kind, deviation_calculator_machine_kind);
        }
    };

    struct NoTransformDeviationCalculatorConstants
    {
        static inline constexpr std::string_view FLOAT32_IEC559                 = "float32_iec559";
        static inline constexpr std::string_view FLOAT64_IEC559                 = "float64_iec559";
        static inline constexpr std::string_view FLOAT128_IEC559                = "float128_iec559";

        static inline constexpr uint8_t SQUARE_DEVIATION_CALCULATOR             = 0u;
        static inline constexpr uint8_t DOUBLE_BAG_SQUARE_DEVIATION_CALCULATOR  = 1u;
    };

    class NoTransformDeviationCalculatorResourceBuilder
    {
        private:

            std::optional<std::string> promoted_float_kind;
            std::optional<uint8_t> deviation_calculator_machine_kind;

        public:

            NoTransformDeviationCalculatorResourceBuilder(): promoted_float_kind(std::string(NoTransformDeviationCalculatorConstants::FLOAT64_IEC559)),
                                                             deviation_calculator_machine_kind(NoTransformDeviationCalculatorConstants::SQUARE_DEVIATION_CALCULATOR){}


            auto float128_iec559() -> NoTransformDeviationCalculatorResourceBuilder&
            {
                this->promoted_float_kind = std::string(NoTransformDeviationCalculatorConstants::FLOAT128_IEC559);

                return *this;
            }

            auto float64_iec559() -> NoTransformDeviationCalculatorResourceBuilder&
            {
                this->promoted_float_kind = std::string(NoTransformDeviationCalculatorConstants::FLOAT64_IEC559);

                return *this;
            }

            auto float32_iec559() -> NoTransformDeviationCalculatorResourceBuilder&
            {
                this->promoted_float_kind = std::string(NoTransformDeviationCalculatorConstants::FLOAT32_IEC559);

                return *this;
            }

            auto sqr_deviation() -> NoTransformDeviationCalculatorResourceBuilder&
            {
                this->deviation_calculator_machine_kind = NoTransformDeviationCalculatorConstants::SQUARE_DEVIATION_CALCULATOR;

                return *this;
            }

            auto get() -> NoTransformDeviationCalculatorResource
            {
                if (!this->promoted_float_kind.has_value())
                {
                    throw std::invalid_argument("bad promoted float kind, nullopt");
                }

                if (!this->deviation_calculator_machine_kind.has_value())
                {
                    throw std::invalid_argument("bad deviation calculator machine kind, nullopt");
                }

                return
                {
                    .promoted_float_kind = this->promoted_float_kind.value(),
                    .deviation_calculator_machine_kind = this->deviation_calculator_machine_kind.value()
                };
            }
    };

    class NoTransformDeviationCalculatorLoader
    {
        public:

            auto load(const NoTransformDeviationCalculatorResource& resource) -> std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface>
            {
                if (std::string_view(resource.promoted_float_kind) == NoTransformDeviationCalculatorConstants::FLOAT32_IEC559)
                {
                    static_assert(sizeof(float) == 4u);
                    static_assert(std::numeric_limits<float>::is_iec559);

                    switch (resource.deviation_calculator_machine_kind)
                    {
                        case NoTransformDeviationCalculatorConstants::SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<float>{});
                        }
                        case NoTransformDeviationCalculatorConstants::DOUBLE_BAG_SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_double_bag_deviation_calculator(deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<float>{}));
                        }
                        default:
                        {
                            throw std::invalid_argument("bad resource format, deviation_calculator_machine_kind enumeration out of range");
                        }
                    }
                }
                else if (std::string_view(resource.promoted_float_kind) == NoTransformDeviationCalculatorConstants::FLOAT64_IEC559)
                {
                    static_assert(sizeof(double) == 8u);
                    static_assert(std::numeric_limits<double>::is_iec559);

                    switch (resource.deviation_calculator_machine_kind)
                    {
                        case NoTransformDeviationCalculatorConstants::SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<double>{});
                        }
                        case NoTransformDeviationCalculatorConstants::DOUBLE_BAG_SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_double_bag_deviation_calculator(deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<double>{}));
                        }
                        default:
                        {
                            throw std::invalid_argument("bad resource format, deviation_calculator_machine_kind enumeration out of range");
                        }
                    }
                }
                else if (std::string_view(resource.promoted_float_kind) == NoTransformDeviationCalculatorConstants::FLOAT128_IEC559)
                {
                    using promoted_float_t = long double;
                    static_assert(sizeof(promoted_float_t) == 16u);
                    static_assert(std::numeric_limits<promoted_float_t>::is_iec559);

                    switch (resource.deviation_calculator_machine_kind)
                    {
                        case NoTransformDeviationCalculatorConstants::SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<promoted_float_t>{});
                        }
                        case NoTransformDeviationCalculatorConstants::DOUBLE_BAG_SQUARE_DEVIATION_CALCULATOR:
                        {
                            return deviation_projector::DeviationCalculatorFactory::get_double_bag_deviation_calculator(deviation_projector::DeviationCalculatorFactory::get_mean_square_deviation_calculator(stdx::Tag<promoted_float_t>{}));
                        }
                        default:
                        {
                            throw std::invalid_argument("bad resource format, deviation_calculator_machine_kind enumeration out of range");
                        }
                    }
                }
                else
                {
                    throw std::invalid_argument("bad resource format, promoted_float_kind not found");
                }
            }
    };
}

#endif