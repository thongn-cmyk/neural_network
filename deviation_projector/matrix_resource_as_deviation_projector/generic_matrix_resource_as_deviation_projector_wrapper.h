#ifndef __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_GENERIC_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_WRAPPER_H__
#define __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_GENERIC_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_WRAPPER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "generic_matrix_resource_as_deviation_projector.h"
#include "taylor_cuda_wrapper/taylor_cuda_wrapper.h"
#include "host_wrapper/host_wrapper.h"
#include <deviation_projector/host_device/generic_device.h>

//ok I admit that the naming conventions are too confusing

//we have decided to use a special representation for matrix (for ease of transformation) instead of compromising interfaces

//so essentially that we have two ways of doing this:
//one is to add mutability to deviation_projector + everything that is cross-region memory related
//second is to use a global unified memory, and actually notify mutability (only to host, then to worker nodes or to all the nodes)

//the <hinge> of all of these to happen is synchronizability

//right, so we have a <global_unified_memory> hub, we have "writer" and "reader"
//we have locks, we have memory orderings

//so we'd just do the exact same thing

//for the interface, we have:

//Segment = std::pair<size_t, size_t>

//read(Segment) -> std::unique_ptr<char[]>
//write(Segment, void *)
//size() -> size_t
//get_read_orders_for_subscriber(size_t) -> std::vector<Segment>
//clear_read_orders_for_subscriber(size_t)
//subscribe() -> size_t
//unsubscribe(size_t)

namespace deviation_projector::matrix_resource_as_deviation_projector
{
    class MatrixResourceAsDeviationCalculatorWrapperInterface
    {
        public:

            virtual ~MatrixResourceAsDeviationCalculatorWrapperInterface() noexcept = default;

            virtual auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& resource) -> ExternalGenericMatrixResourceAsDeviationCalculatorResource = 0;
    };

    struct HostMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        global_string_encoder::StringTransformationRule str_transformation_rule;
        deviation_projector::host_device::HostMatrixDeviationCalculatorResource deviation_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(str_transformation_rule,
                      deviation_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(str_transformation_rule,
                      deviation_resource);
        }
    };

    struct ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig
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

    auto to_external_host_matrix_resource_as_deviation_calculator_wrapper_config(const HostMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_host_matrix_resource_as_deviation_calculator_wrapper_config(const ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> HostMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<HostMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config_bytestream);
    }

    class HostMatrixResourceAsDeviationCalculatorWrapper: public virtual MatrixResourceAsDeviationCalculatorWrapperInterface
    {
        private:

            global_string_encoder::StringTransformationRule str_transformation_rule;
            deviation_projector::host_device::HostMatrixDeviationCalculatorResource deviation_resource;

        public:

            HostMatrixResourceAsDeviationCalculatorWrapper(const HostMatrixResourceAsDeviationCalculatorWrapperConfig& config): str_transformation_rule(config.str_transformation_rule),
                                                                                                                                deviation_resource(config.deviation_resource){}

            HostMatrixResourceAsDeviationCalculatorWrapper(const ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig& config): HostMatrixResourceAsDeviationCalculatorWrapper(to_internal_host_matrix_resource_as_deviation_calculator_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& arg) -> ExternalGenericMatrixResourceAsDeviationCalculatorResource
            {
                auto resource           = host_wrapper::GenericHostMatrixDeviationCalculatorResource
                {
                    .str_transformation_rule    = this->str_transformation_rule,
                    .deviation_resource         = this->deviation_resource,
                    .matrix_resource            = arg
                };

                auto external_resource  = host_wrapper::to_external_generic_host_matrix_deviation_calculator_resource(resource);
                auto generic_resource   = GenericMatrixResourceAsDeviationCalculatorResource
                {
                    .resource = external_resource
                };

                return to_external_generic_matrix_resource_as_deviation_calculator_resource(generic_resource);
            }
    };

    //refactor

    struct CudaMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        global_string_encoder::StringTransformationRule str_transformation_rule;
        uint8_t cuda_deviation_calculator_device;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(str_transformation_rule,
                      cuda_deviation_calculator_device);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(str_transformation_rule,
                      cuda_deviation_calculator_device);
        }
    };

    struct ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig
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

    auto to_external_cuda_matrix_resource_as_deviation_calculator_wrapper_config(const CudaMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_cuda_matrix_resource_as_deviation_calculator_wrapper_config(const ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> CudaMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<CudaMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config_bytestream);
    }

    class CudaMatrixResourceAsDeviationCalculatorWrapper: public virtual MatrixResourceAsDeviationCalculatorWrapperInterface
    {
        private:

            global_string_encoder::StringTransformationRule str_transformation_rule;
            uint8_t cuda_deviation_calculator_device;

        public:

            CudaMatrixResourceAsDeviationCalculatorWrapper(const CudaMatrixResourceAsDeviationCalculatorWrapperConfig& config): str_transformation_rule(config.str_transformation_rule),
                                                                                                                                cuda_deviation_calculator_device(config.cuda_deviation_calculator_device){}

            CudaMatrixResourceAsDeviationCalculatorWrapper(const ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig& config): CudaMatrixResourceAsDeviationCalculatorWrapper(to_internal_cuda_matrix_resource_as_deviation_calculator_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& arg) -> ExternalGenericMatrixResourceAsDeviationCalculatorResource
            {
                auto resource           = taylor_cuda_wrapper::TaylorCudaMatrixDeviationCalculatorResource
                {
                    .str_transformation_rule        = this->str_transformation_rule,
                    .deviation_calculator_device    = this->cuda_deviation_calculator_device,
                    .matrix_resource                = arg
                };

                auto external_resource  = taylor_cuda_wrapper::to_external_taylor_cuda_matrix_deviation_calculator_resource(resource);
                auto generic_resource   = GenericMatrixResourceAsDeviationCalculatorResource
                {
                    .resource = external_resource
                };

                return to_external_generic_matrix_resource_as_deviation_calculator_resource(generic_resource);
            }
    };

    struct GenericMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        std::variant<stdx::reflectible_monostate,
                     ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig,
                     ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig> config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config);
        }
    };

    struct ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig
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

    auto to_external_generic_matrix_resource_as_deviation_calculator_wrapper_config(const GenericMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_matrix_resource_as_deviation_calculator_wrapper_config(const ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig& config) -> GenericMatrixResourceAsDeviationCalculatorWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config_bytestream);
    }

    class GenericMatrixResourceAsDeviationCalculatorWrapper: public virtual MatrixResourceAsDeviationCalculatorWrapperInterface
    {
        private:

            std::unique_ptr<MatrixResourceAsDeviationCalculatorWrapperInterface> base;

        public:

            GenericMatrixResourceAsDeviationCalculatorWrapper(const GenericMatrixResourceAsDeviationCalculatorWrapperConfig& config)
            {
                if (std::holds_alternative<ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config))
                {
                    this->base = std::make_unique<HostMatrixResourceAsDeviationCalculatorWrapper>(std::get<ExternalHostMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config));
                }
                else if (std::holds_alternative<ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config))
                {
                    this->base = std::make_unique<CudaMatrixResourceAsDeviationCalculatorWrapper>(std::get<ExternalCudaMatrixResourceAsDeviationCalculatorWrapperConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad matrix as deviation wrapper config, dispatch code not found");
                }
            }

            GenericMatrixResourceAsDeviationCalculatorWrapper(const ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig& config): GenericMatrixResourceAsDeviationCalculatorWrapper(to_internal_generic_matrix_resource_as_deviation_calculator_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> ExternalGenericMatrixResourceAsDeviationCalculatorResource
            {
                return this->base->wrap(matrix_resource);
            }
    };
}

#endif