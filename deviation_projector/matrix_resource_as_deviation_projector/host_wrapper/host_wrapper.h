#ifndef __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_HOST_WRAPPER_HOST_WRAPPER_H__
#define __DEVIATION_PROJECTOR_MATRIX_RESOURCE_AS_DEVIATION_PROJECTOR_HOST_WRAPPER_HOST_WRAPPER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include <matrix/generic_matrix_factory.h>
#include <matrix/tensor_model.h>
#include <deviation_projector/host_device/generic_device.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <stl_extension/stdx.h>
#include <global_string_encoder/generic_encoder.h>
#include <matrix/tensor_factory.h>
#include <immutable_memory/immutable_memory.h>
#include <deviation_projector/training_token_factory.h>

namespace deviation_projector::matrix_resource_as_deviation_projector::host_wrapper
{
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    struct GenericHostMatrixDeviationCalculatorResource
    {
        global_string_encoder::StringTransformationRule str_transformation_rule;
        deviation_projector::host_device::HostMatrixDeviationCalculatorResource deviation_resource;
        generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(str_transformation_rule,
                      deviation_resource,
                      matrix_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(str_transformation_rule,
                      deviation_resource,
                      matrix_resource);
        }
    };

    struct ExternalGenericHostMatrixDeviationCalculatorResource
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

    auto to_external_generic_host_matrix_deviation_calculator_resource(const GenericHostMatrixDeviationCalculatorResource& arg) -> ExternalGenericHostMatrixDeviationCalculatorResource
    {
        return ExternalGenericHostMatrixDeviationCalculatorResource
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(arg)
        };
    }

    auto to_internal_generic_host_matrix_deviation_calculator_resource(const ExternalGenericHostMatrixDeviationCalculatorResource& arg) -> GenericHostMatrixDeviationCalculatorResource
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericHostMatrixDeviationCalculatorResource>(arg.config_bytestream);
    }

    class GenericHostMatrixDeviationCalculator: public virtual deviation_projector::GenericMatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<global_string_encoder::EncoderInterface> string_encoder;
            std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface> deviation_calculator;
            std::unique_ptr<the_matrix::MatrixInterface> matrix;

            GenericHostMatrixDeviationCalculator(std::unique_ptr<global_string_encoder::EncoderInterface>&& string_encoder,
                                                 std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface>&& deviation_calculator,
                                                 std::unique_ptr<the_matrix::MatrixInterface>&& matrix)
            {
                if (string_encoder == nullptr)
                {
                    throw std::invalid_argument("bad string encoder, null");
                }

                if (deviation_calculator == nullptr)
                {
                    throw std::invalid_argument("bad deviation calculator, null");
                }

                if (matrix == nullptr)
                {
                    throw std::invalid_argument("bad matrix, null");
                }

                this->string_encoder        = std::move(string_encoder);
                this->deviation_calculator  = std::move(deviation_calculator);
                this->matrix                = std::move(matrix);
            }

        public:

            GenericHostMatrixDeviationCalculator(const GenericHostMatrixDeviationCalculatorResource& arg): GenericHostMatrixDeviationCalculator(std::make_unique<global_string_encoder::GenericEncoder>(arg.str_transformation_rule),
                                                                                                                                                deviation_projector::host_device::HostMatrixDeviationCalculatorLoader{}.load(arg.deviation_resource),
                                                                                                                                                generic_matrix_factory::GenericMatrixLoader{}.load_resource(generic_matrix_factory::GenericMatrixExternalizer{}.to_internal(arg.matrix_resource))){}

            GenericHostMatrixDeviationCalculator(const ExternalGenericHostMatrixDeviationCalculatorResource& arg): GenericHostMatrixDeviationCalculator(to_internal_generic_host_matrix_deviation_calculator_resource(arg)){}

            auto get_deviation(const std::vector<std::shared_ptr<immutable_memory::ImmutableMemoryInterface>>& training_token_vec) -> mdc_float_t
            {
                std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> tensor_pair_vec{};

                std::vector<std::shared_ptr<tensor_model::Matrix>> projecting_vec{};
                std::vector<std::shared_ptr<tensor_model::Matrix>> expected_vec{};
                std::vector<std::shared_ptr<tensor_model::Matrix>> projected_vec{};

                for (const auto& training_token: training_token_vec)
                {
                    if (training_token == nullptr)
                    {
                        throw std::invalid_argument("bad training token, null");
                    }

                    auto [lhs, rhs] = deviation_projector::training_token_factory::decode_training_token(this->string_encoder->encode(std::string(training_token->get())));

                    projecting_vec.push_back(lhs);
                    expected_vec.push_back(rhs);
                }

                projected_vec = this->matrix->project(projecting_vec);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (projected_vec.size() != expected_vec.size())
                    {
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < projected_vec.size(); ++i)
                {
                    tensor_pair_vec.push_back({projected_vec[i], expected_vec[i]});
                }

                return this->deviation_calculator->get_deviation(tensor_pair_vec);
            }
    };
}

#endif