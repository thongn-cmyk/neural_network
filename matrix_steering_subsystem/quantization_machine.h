#ifndef __MATRIX_STEERING_SUBSYSTEM_QUANTIZATION_MACHINE_H__
#define __MATRIX_STEERING_SUBSYSTEM_QUANTIZATION_MACHINE_H__

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <stl_extension/stdx.h>

namespace quantization_machine
{
    template <class FloatType>
    class QuantizationMachineInterface
    {
        public:

            static_assert(std::is_floating_point_v<FloatType>);

            virtual ~QuantizationMachineInterface() noexcept = default;

            virtual auto quantitize(FloatType x) -> size_t = 0;
            virtual auto quantization_size() -> size_t = 0;
    };

    template <class FloatType>
    class SharedPointerQuantizationMachine: public virtual QuantizationMachineInterface<FloatType>
    {
        private:

            std::shared_ptr<QuantizationMachineInterface<FloatType>> base;

        public:

            SharedPointerQuantizationMachine(std::shared_ptr<QuantizationMachineInterface<FloatType>> base): base(std::move(base)){}

            auto quantitize(FloatType x) -> size_t
            {
                return this->base->quantitize(x);
            }

            auto quantization_size() -> size_t
            {
                return this->base->quantization_size();
            }
    };


    template <class FloatType = double>
    class ExponentialQuantizationMachine: public virtual QuantizationMachineInterface<FloatType>
    {
        private:

            size_t discretization_sz;
            FloatType exp_base;
            FloatType multiplier_base;

        public:

            ExponentialQuantizationMachine(size_t discretization_sz,
                                           FloatType exp_base,
                                           FloatType multiplier_base)
            {
                if (!stdx::is_pow2(discretization_sz))
                {
                    throw std::invalid_argument("bad discretization size, not pow 2");
                }

                if (discretization_sz < 4)
                {
                    throw std::invalid_argument("bad discretization size, < 4");
                }

                if (std::isnan(exp_base))
                {
                    throw std::invalid_argument("bad exp base, NaN");
                }

                if (exp_base <= 1)
                {
                    throw std::invalid_argument("bad exp base, <= 1");
                }

                if (std::isnan(multiplier_base))
                {
                    throw std::invalid_argument("bad multiplier base, NaN");
                }

                if (multiplier_base <= 0)
                {
                    throw std::invalid_argument("bad multiplier base, <= 0");
                }

                this->discretization_sz = discretization_sz;
                this->exp_base          = exp_base;
                this->multiplier_base   = multiplier_base;
            }

            auto quantitize(FloatType x_arg) -> size_t
            {
                const intmax_t FIRST_EXPONENT       = -static_cast<intmax_t>(this->discretization_sz / 4);
                const intmax_t LAST_EXPONENT        = static_cast<intmax_t>(this->discretization_sz / 4);
                const size_t HALF_SZ                = this->discretization_sz / 2;

                FloatType x                         = std::abs(x_arg);
                FloatType y                         = x / this->multiplier_base;
                FloatType exponent                  = std::log(y) / std::log(this->exp_base);
                FloatType upround                   = std::ceil(exponent);

                //it's fine if we treat 0 as a special bounded value, either in the positive or negative range
                //recall that this is a relative operation, we simply dont have the instrument to perfect all of these guys

                intmax_t upround_i                  = static_cast<intmax_t>(upround);
                intmax_t slot_i                     = upround_i - 1;
                intmax_t actual_slot_i              = std::min(std::max(slot_i, FIRST_EXPONENT), LAST_EXPONENT - 1);

                size_t signed_normalized_slot_i     = actual_slot_i - FIRST_EXPONENT;

                if (x_arg < 0)
                {
                    return HALF_SZ - signed_normalized_slot_i - 1;
                }
                else
                {
                    return HALF_SZ + signed_normalized_slot_i;
                }
            }

            auto quantization_size() -> size_t
            {
                return this->discretization_sz;
            }
    };
}

#endif