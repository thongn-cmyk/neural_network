#ifndef __DEVIATION_PROJECTOR_CUDA_WRAPPER_TAYLOR_CUDA_WRAPPER_H__
#define __DEVIATION_PROJECTOR_CUDA_WRAPPER_TAYLOR_CUDA_WRAPPER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include <matrix/generic_matrix_factory.h>
#include <matrix/tensor_model.h>
#include <stl_extension/stdx.h>
#include <global_string_encoder/generic_encoder.h>
#include <matrix/tensor_factory.h>
#include <taylor_matrix/cuda_matrix/the_cuda_matrix_deviation_calculator.h>
#include <immutable_memory/immutable_memory.h>

namespace deviation_projector::matrix_resource_as_deviation_projector::taylor_cuda_wrapper
{
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    struct TaylorCudaMatrixDeviationCalculatorResource
    {
        global_string_encoder::StringTransformationRule str_transformation_rule;
        uint8_t deviation_calculator_device;
        generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(str_transformation_rule,
                      deviation_calculator_device,
                      matrix_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(str_transformation_rule,
                      deviation_calculator_device,
                      matrix_resource);
        }
    };

    struct ExternalTaylorCudaMatrixDeviationCalculatorResource
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

    auto to_external_taylor_cuda_matrix_deviation_calculator_resource(const TaylorCudaMatrixDeviationCalculatorResource& arg) -> ExternalTaylorCudaMatrixDeviationCalculatorResource
    {
        return ExternalTaylorCudaMatrixDeviationCalculatorResource
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(arg)
        };
    }

    auto to_internal_taylor_cuda_matrix_deviation_calculator_resource(const ExternalTaylorCudaMatrixDeviationCalculatorResource& arg) -> TaylorCudaMatrixDeviationCalculatorResource
    {
        return dg::network_compact_serializer::dgstd_deserialize<TaylorCudaMatrixDeviationCalculatorResource>(arg.config_bytestream);
    }

    class TaylorCudaMatrixDeviationCalculator: public virtual deviation_projector::GenericMatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface> base;

            using CudaMatrixIdentifiable = taylor_matrix::cuda_matrix::the_cuda_matrix_deviation_calculator::CudaMatrixIdentifiable;

            static auto get_cuda_matrix_identifiable(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> CudaMatrixIdentifiable
            {
                generic_matrix_factory::GenericMatrixResource internal_matrix_resource = generic_matrix_factory::GenericMatrixExternalizer{}.to_internal(matrix_resource);

                if (!std::holds_alternative<generic_matrix_factory::TheCudaMatrixResource>(internal_matrix_resource.resource))
                {
                    throw std::invalid_argument("bad matrix resource, not cuda matrix resource");
                }

                generic_matrix_factory::TheCudaMatrixResource& cuda_matrix_resource = std::get<generic_matrix_factory::TheCudaMatrixResource>(internal_matrix_resource.resource);

                return CudaMatrixIdentifiable
                {
                    .entropy_option = cuda_matrix_resource.entropy_option,
                    .compute_option = cuda_matrix_resource.compute_option,
                    .vector_sz      = cuda_matrix_resource.vector_sz
                };
            }

            static auto get_cuda_logit_vector(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> std::vector<tensor_std_float_t>
            {
                return generic_matrix_factory::GenericMatrixLoader{}.load_resource(generic_matrix_factory::GenericMatrixExternalizer{}.to_internal(matrix_resource))->get_coefficient_vector();
            }

        public:

            TaylorCudaMatrixDeviationCalculator(const TaylorCudaMatrixDeviationCalculatorResource& arg)
            {
                this->base = taylor_matrix::cuda_matrix::the_cuda_matrix_deviation_calculator::TheCudaMatrixDeviationCalculatorFactory{}
                                                                                               .set_matrix(get_cuda_matrix_identifiable(arg.matrix_resource))
                                                                                               .set_logit_vector(get_cuda_logit_vector(arg.matrix_resource))
                                                                                               .set_string_transformer_device(arg.str_transformation_rule)
                                                                                               .set_deviation_calculator_device(arg.deviation_calculator_device)
                                                                                               .get();
            }

            TaylorCudaMatrixDeviationCalculator(const ExternalTaylorCudaMatrixDeviationCalculatorResource& arg): TaylorCudaMatrixDeviationCalculator(to_internal_taylor_cuda_matrix_deviation_calculator_resource(arg)){}

            auto get_deviation(const std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>>& token_vec) -> mdc_float_t
            {
                return this->base->get_deviation(token_vec);                
            }
    };
}

#endif