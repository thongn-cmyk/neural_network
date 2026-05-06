#ifndef __DEVIATION_PROJECTOR_GENERIC_RESOURCE_H__
#define __DEVIATION_PROJECTOR_GENERIC_RESOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "generic_matrix_deviation_calculator_interface.h"
#include "host_wrapper/one_stop_wrapper.h"
#include "cuda_wrapper/cuda_wrapper.h"
#include <variant>
#include <exception>

namespace deviation_projector
{
    struct GenericMatrixDeviationCalculatorResource
    {
        std::variant<stdx::reflectible_monostate,
                     deviation_projector::host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource,
                     deviation_projector::cuda_wrapper::ExternalCudaMatrixDeviationCalculatorResource> resource;

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
    
    struct ExternalGenericMatrixDeviationCalculatorResource
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

    auto to_external_generic_matrix_deviation_calculator_resource(const GenericMatrixDeviationCalculatorResource& resource) -> ExternalGenericMatrixDeviationCalculatorResource
    {
        return ExternalGenericMatrixDeviationCalculatorResource
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(resource)
        };
    }

    auto to_internal_generic_matrix_deviation_calculator_resource(const ExternalGenericMatrixDeviationCalculatorResource& resource) -> GenericMatrixDeviationCalculatorResource
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericMatrixDeviationCalculatorResource>(resource.config_bytestream);
    }

    class GenericMatrixDeviationCalculator: public virtual GenericMatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<GenericMatrixDeviationCalculatorInterface> base;
        
        public:

            GenericMatrixDeviationCalculator(const GenericMatrixDeviationCalculatorResource& arg)
            {
                if (std::holds_alternative<deviation_projector::host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource>(arg.resource))
                {
                    this->base  = std::make_unique<deviation_projector::host_wrapper::GenericHostMatrixDeviationCalculator>(std::get<deviation_projector::host_wrapper::ExternalGenericHostMatrixDeviationCalculatorResource>(arg.resource));
                }
                else if (std::holds_alternative<deviation_projector::cuda_wrapper::ExternalCudaMatrixDeviationCalculatorResource>(arg.resource))
                {
                    this->base  = std::make_unique<deviation_projector::cuda_wrapper::CudaMatrixDeviationCalculator>(std::get<deviation_projector::cuda_wrapper::ExternalCudaMatrixDeviationCalculatorResource>(arg.resource));
                }
                else
                {
                    throw std::invalid_argument("bad deviation calculator resource, dispatch code not found");
                }
            }

            GenericMatrixDeviationCalculator(const ExternalGenericMatrixDeviationCalculatorResource& arg): GenericMatrixDeviationCalculator(to_internal_generic_matrix_deviation_calculator_resource(arg)){}

            auto get_deviation(const std::vector<std::shared_ptr<std::string>>& training_token_vec) -> mdc_float_t
            {
                return this->base->get_deviation(training_token_vec);
            }
    };
}

#endif