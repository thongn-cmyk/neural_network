#ifndef __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__
#define __DG_DEVIATION_PROJECTOR_GENERIC_MATRIX_WRAPPER_RESOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "generic_matrix_deviation_calculator.h"
#include "taylor_cuda_wrapper/taylor_cuda_wrapper.h"
#include "host_wrapper/host_wrapper.h"

namespace deviation_projector
{
    class MatrixAsDeviationWrapperInterface
    {
        public:

            virtual ~MatrixAsDeviationWrapperInterface() noexcept = default;

            virtual auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& resource) -> deviation_projector::ExternalGenericMatrixDeviationCalculatorResource = 0;
    };

    struct HostMatrixAsDeviationWrapperConfig
    {
        global_string_encoder::StringTransformationRule str_transformation_rule;
        deviation_projector::NoTransformDeviationCalculatorResource deviation_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(str_transformation_rule, deviation_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(str_transformation_rule, deviation_resource);            
        }
    };

    struct ExternalHostMatrixAsDeviationWrapperConfig
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

    auto to_external_host_matrix_as_deviation_wrapper_config(const HostMatrixAsDeviationWrapperConfig& config) -> ExternalHostMatrixAsDeviationWrapperConfig
    {
        return ExternalHostMatrixAsDeviationWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_host_matrix_as_deviation_wrapper_config(const ExternalHostMatrixAsDeviationWrapperConfig& config) -> HostMatrixAsDeviationWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<HostMatrixAsDeviationWrapperConfig>(config.config_bytestream);
    }

    class HostMatrixAsDeviationWrapper: public virtual MatrixAsDeviationWrapperInterface
    {
        private:

            global_string_encoder::StringTransformationRule str_transformation_rule;
            deviation_projector::NoTransformDeviationCalculatorResource deviation_resource;
        
        public:

            HostMatrixAsDeviationWrapper(const HostMatrixAsDeviationWrapperConfig& config): str_transformation_rule(config.str_transformation_rule),
                                                                                            deviation_resource(config.deviation_resource){}

            HostMatrixAsDeviationWrapper(const ExternalHostMatrixAsDeviationWrapperConfig& config): HostMatrixAsDeviationWrapper(to_internal_host_matrix_as_deviation_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& arg) -> deviation_projector::ExternalGenericMatrixDeviationCalculatorResource
            {
                auto resource           = deviation_projector::host_wrapper::GenericHostMatrixDeviationCalculatorResource
                {
                    .str_transformation_rule    = this->str_transformation_rule,
                    .deviation_resource         = this->deviation_resource,
                    .matrix_resource            = arg
                };

                auto external_resource  = deviation_projector::host_wrapper::to_external_generic_host_matrix_deviation_calculator_resource(resource);
                auto generic_resource   = deviation_projector::GenericMatrixDeviationCalculatorResource
                {
                    .resource = external_resource
                };

                return deviation_projector::to_external_generic_matrix_deviation_calculator_resource(generic_resource);
            }
    };

    struct CudaMatrixAsDeviationWrapperConfig
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

    struct ExternalCudaMatrixAsDeviationWrapperConfig
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

    auto to_external_cuda_matrix_as_deviation_wrapper_config(const CudaMatrixAsDeviationWrapperConfig& config) -> ExternalCudaMatrixAsDeviationWrapperConfig
    {
        return ExternalCudaMatrixAsDeviationWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_cuda_matrix_as_deviation_wrapper_config(const ExternalCudaMatrixAsDeviationWrapperConfig& config) -> CudaMatrixAsDeviationWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<CudaMatrixAsDeviationWrapperConfig>(config.config_bytestream);
    }

    class CudaMatrixAsDeviationWrapper: public virtual MatrixAsDeviationWrapperInterface
    {
        private:

            global_string_encoder::StringTransformationRule str_transformation_rule;
            uint8_t cuda_deviation_calculator_device;

        public:

            CudaMatrixAsDeviationWrapper(const CudaMatrixAsDeviationWrapperConfig& config): str_transformation_rule(config.str_transformation_rule),
                                                                                            cuda_deviation_calculator_device(config.cuda_deviation_calculator_device){}

            CudaMatrixAsDeviationWrapper(const ExternalCudaMatrixAsDeviationWrapperConfig& config): CudaMatrixAsDeviationWrapper(to_internal_cuda_matrix_as_deviation_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& arg) -> deviation_projector::ExternalGenericMatrixDeviationCalculatorResource
            {
                auto resource           = deviation_projector::taylor_cuda_wrapper::TaylorCudaMatrixDeviationCalculatorResource
                {
                    .str_transformation_rule        = this->str_transformation_rule,
                    .deviation_calculator_device    = this->cuda_deviation_calculator_device,
                    .matrix_resource                = arg
                };

                auto external_resource  = deviation_projector::taylor_cuda_wrapper::to_external_taylor_cuda_matrix_deviation_calculator_resource(resource);
                auto generic_resource   = deviation_projector::GenericMatrixDeviationCalculatorResource
                {
                    .resource = external_resource
                };

                return deviation_projector::to_external_generic_matrix_deviation_calculator_resource(generic_resource);
            }
    };

    struct GenericMatrixAsDeviationWrapperConfig
    {
        std::variant<stdx::reflectible_monostate,
                     ExternalHostMatrixAsDeviationWrapperConfig,
                     ExternalCudaMatrixAsDeviationWrapperConfig> config;

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

    struct ExternalGenericMatrixAsDeviationWrapperConfig
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

    auto to_external_generic_matrix_as_deviation_wrapper_config(const GenericMatrixAsDeviationWrapperConfig& config) -> ExternalGenericMatrixAsDeviationWrapperConfig
    {
        return ExternalGenericMatrixAsDeviationWrapperConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_matrix_as_deviation_wrapper_config(const ExternalGenericMatrixAsDeviationWrapperConfig& config) -> GenericMatrixAsDeviationWrapperConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericMatrixAsDeviationWrapperConfig>(config.config_bytestream);
    }

    class GenericMatrixAsDeviationWrapper: public virtual MatrixAsDeviationWrapperInterface
    {
        private:

            std::unique_ptr<MatrixAsDeviationWrapperInterface> base;

        public:

            GenericMatrixAsDeviationWrapper(const GenericMatrixAsDeviationWrapperConfig& config)
            {
                if (std::holds_alternative<ExternalHostMatrixAsDeviationWrapperConfig>(config.config))
                {
                    this->base = std::make_unique<HostMatrixAsDeviationWrapper>(std::get<ExternalHostMatrixAsDeviationWrapperConfig>(config.config));
                }
                else if (std::holds_alternative<ExternalCudaMatrixAsDeviationWrapperConfig>(config.config))
                {
                    this->base = std::make_unique<CudaMatrixAsDeviationWrapper>(std::get<ExternalCudaMatrixAsDeviationWrapperConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad matrix as deviation wrapper config, dispatch code not found");
                }
            }

            GenericMatrixAsDeviationWrapper(const ExternalGenericMatrixAsDeviationWrapperConfig& config): GenericMatrixAsDeviationWrapper(to_internal_generic_matrix_as_deviation_wrapper_config(config)){}

            auto wrap(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> deviation_projector::ExternalGenericMatrixDeviationCalculatorResource
            {
                return this->base->wrap(matrix_resource);
            }
    };
}

#endif