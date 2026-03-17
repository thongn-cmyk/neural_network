#ifndef __DEVIATION_PROJECTOR_GENERIC_RESOURCE_H__
#define __DEVIATION_PROJECTOR_GENERIC_RESOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "generic_matrix_deviation_calculator_interface.h"
#include "host_deviation_projector_wrapper/one_stop_wrapper.h"
#include <variant>
#include <exception>

namespace deviation_projector
{
    struct GenericMatrixDeviationCalculatorResource
    {
        std::variant<stdx::reflectible_monostate, deviation_projector::host_wrapper::GenericResource> resource;

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

    class GenericMatrixDeviationCalculatorResourceLoader
    {
        public:

            auto load(const GenericMatrixDeviationCalculatorResource& resource) -> std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>
            {
                if (std::holds_alternative<deviation_projector::host_wrapper::GenericResource>(resource.resource))
                {
                    return deviation_projector::host_wrapper::GenericMatrixDeviationCalculatorResourceLoader{}.load(std::get<deviation_projector::host_wrapper::GenericResource>(resource.resource));
                }
                else
                {
                    throw std::invalid_argument("bad generic resource, monostate");
                }
            }
    };
}

#endif