#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_H__

#include <memory_management/cu_immutable_memory.h>
#include <serializer/dg_buf.h>
#include <matrix/the_matrix_interface.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <mutex_extension/fair_mutex.h>
#include <matrix/tensor_model.h>
#include <vector>
#include <unordered_map>
#include "tensor_matrix_forward_header.h"
#include <cuda_management/host_service.h>
#include <funnel/funnel.h>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix
{
    using namespace the_matrix;

    using tensor_std_float_t                = tensor_model::tensor_std_float_t;
    using cuda_matrix_kernel_exception_t    = uint8_t;

    class TheCudaMatrix: public virtual MatrixInterface
    {
        private:

            std::vector<size_t> shape_vec;
            std::vector<size_t> focal_sz_vec;
            std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map;
            std::vector<size_t> rotation_sz_vec;
            std::vector<double> parameter_bound_ratio_vec;
            bool has_process_unit_logit_reuse_tag;
            bool has_process_group_logit_reuse_tag;
            bool has_being_logit_reuse_tag;
            bool has_base_matrix_logit_reuse_tag;

            std::vector<tensor_std_float_t> shape_coeff_vec;
            size_t base_shape_coeff_sz;

            using self = TheCudaMatrix;

        public:

            TheCudaMatrix(std::vector<size_t> shape_vec,
                          std::vector<size_t> focal_sz_vec,
                          std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                          std::vector<size_t> rotation_sz_vec,
                          std::vector<double> parameter_bound_ratio_vec,
                          bool has_process_unit_logit_reuse_tag,
                          bool has_process_group_logit_reuse_tag,
                          bool has_being_logit_reuse_tag,
                          bool has_base_matrix_logit_reuse_tag,
                          std::vector<tensor_std_float_t> shape_coeff_vec,
                          size_t base_shape_coeff_sz): shape_vec(std::move(shape_vec)),
                                                       focal_sz_vec(std::move(focal_sz_vec)),
                                                       focal_suffix_map(std::move(focal_suffix_map)),
                                                       rotation_sz_vec(std::move(rotation_sz_vec)),
                                                       parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                       has_process_unit_logit_reuse_tag(has_process_unit_logit_reuse_tag),
                                                       has_process_group_logit_reuse_tag(has_process_group_logit_reuse_tag),
                                                       has_being_logit_reuse_tag(has_being_logit_reuse_tag),
                                                       has_base_matrix_logit_reuse_tag(has_base_matrix_logit_reuse_tag),
                                                       shape_coeff_vec(std::move(shape_coeff_vec)),
                                                       base_shape_coeff_sz(base_shape_coeff_sz){}

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {

            }

            auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
            {
                std::vector<tensor_std_float_t> rs{};

                std::copy(this->coeff_vec.begin(), this->coeff_vec.end(), std::back_inserter(rs));
                std::copy(this->shape_coeff_vec.begin(), this->shape_coeff_vec.end(), std::back_inserter(rs));

                return rs;
            }

            void set_coefficient_vector(const std::vector<tensor_std_float_t>& new_coeff_vec)
            {
                if (this->coeff_vec.size() + this->shape_coeff_vec.size() != new_coeff_vec.size())
                {
                    throw std::runtime_error("invalid new_coeff_vec shape");
                }

                std::vector<tensor_std_float_t> shadow_coeff_vec(this->coeff_vec.size());
                std::vector<tensor_std_float_t> shadow_shape_coeff_vec(this->shape_coeff_vec.size());

                for (size_t i = 0u; i < shadow_coeff_vec.size(); ++i)
                {
                    if (std::isnan(new_coeff_vec[i]))
                    {
                        throw std::runtime_error("invalid new_coeff_vec shape");
                    }

                    shadow_coeff_vec[i] = new_coeff_vec[i];
                }

                for (size_t i = 0u; i < shadow_shape_coeff_vec.size(); ++i)
                {
                    shadow_shape_coeff_vec[i] = shape_projection::radian_normalize(new_coeff_vec[i + shadow_coeff_vec.size()]);

                    if (std::isnan(shadow_shape_coeff_vec[i]))
                    {
                        throw std::runtime_error("invalid new_coeff_vec shape");
                    }
                }

                this->coeff_vec         = std::move(shadow_coeff_vec);
                this->shape_coeff_vec   = std::move(shadow_shape_coeff_vec);
            }

            auto clone() -> std::shared_ptr<MatrixInterface>
            {
                return std::make_shared<self>(*this);
            }

        private:


    };
}

#endif