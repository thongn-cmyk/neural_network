#ifndef __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_GENERIC_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_H__
#define __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_GENERIC_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include "host_wrapper/host_wrapper.h"
#include "taylor_cuda_wrapper/taylor_cuda_wrapper.h"
#include <variant>
#include <exception>
#include <immutable_memory/immutable_memory.h>

namespace deviation_projector::matrix_resource_as_deviation_projector
{
    struct GenericMatrixResourceAsDeviationCalculatorResource
    {
        std::variant<stdx::reflectible_monostate,
                     host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource,
                     taylor_cuda_wrapper::ExternalTaylorCudaMatrixDeviationCalculatorResource> resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(resource);
        }
    };

    struct ExternalGenericMatrixResourceAsDeviationCalculatorResource
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_generic_matrix_resource_as_deviation_calculator_resource(const GenericMatrixResourceAsDeviationCalculatorResource& resource) -> ExternalGenericMatrixResourceAsDeviationCalculatorResource
    {
        return ExternalGenericMatrixResourceAsDeviationCalculatorResource
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(resource)
        };
    }

    auto to_internal_generic_matrix_resource_as_deviation_calculator_resource(const ExternalGenericMatrixResourceAsDeviationCalculatorResource& resource) -> GenericMatrixResourceAsDeviationCalculatorResource
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericMatrixResourceAsDeviationCalculatorResource>(resource.config_bytestream);
    }

    class GenericMatrixResourceAsDeviationCalculator: public virtual GenericMatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<GenericMatrixDeviationCalculatorInterface> base;

        public:

            GenericMatrixResourceAsDeviationCalculator(const GenericMatrixResourceAsDeviationCalculatorResource& arg)
            {
                if (std::holds_alternative<host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource>(arg.resource))
                {
                    this->base  = std::make_unique<host_wrapper::GenericHostMatrixDeviationCalculator>(std::get<host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource>(arg.resource));
                }
                else if (std::holds_alternative<taylor_cuda_wrapper::ExternalTaylorCudaMatrixDeviationCalculatorResource>(arg.resource))
                {
                    this->base  = std::make_unique<taylor_cuda_wrapper::TaylorCudaMatrixDeviationCalculator>(std::get<taylor_cuda_wrapper::ExternalTaylorCudaMatrixDeviationCalculatorResource>(arg.resource));
                }
                else
                {
                    throw std::invalid_argument("bad deviation calculator resource, dispatch code not found");
                }
            }

            GenericMatrixResourceAsDeviationCalculator(const ExternalGenericMatrixResourceAsDeviationCalculatorResource& arg): GenericMatrixResourceAsDeviationCalculator(to_internal_generic_matrix_resource_as_deviation_calculator_resource(arg)){}

            auto get_deviation(const std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>>& training_token_vec) -> mdc_float_t
            {
                return this->base->get_deviation(training_token_vec);
            }
    };
}

#endif