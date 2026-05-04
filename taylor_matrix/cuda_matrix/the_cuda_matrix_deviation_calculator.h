#ifndef __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_DEVIATION_CALCULATOR_H__
#define __TAYLOR_MATRIX_CUDA_MATRIX_THE_CUDA_MATRIX_DEVIATION_CALCULATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <memory_management/cu_immutable_memory.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <mutex_extension/fair_mutex.h>
#include <matrix/tensor_model.h>
#include "tensor_matrix_forward_to_deviation_header.h"
#include <cuda_management/host_service.h>
#include <funnel/funnel.h>

namespace taylor_matrix::cuda_matrix::the_cuda_matrix_deviation_calculator
{
    using mdc_float_t = float_def::mdc_float_t;

    class TheCudaMatrixDeviationCalculator: public virtual deviation_projector::GenericMatrixDeviationCalculatorInterface
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

            std::optional<size_t> deviation_operation_window;

            using self = TheCudaMatrixDeviationCalculator;

            static inline constexpr size_t CUDA_MEMFETCH_WINDOW = size_t{1} << 6; 

        public:

            TheCudaMatrixDeviationCalculator(std::vector<size_t> shape_vec,
                                             std::vector<size_t> focal_sz_vec,
                                             std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>> focal_suffix_map,
                                             std::vector<size_t> rotation_sz_vec,
                                             std::vector<double> parameter_bound_ratio_vec,
                                             bool has_process_unit_logit_reuse_tag,
                                             bool has_process_group_logit_reuse_tag,
                                             bool has_being_logit_reuse_tag,
                                             bool has_base_matrix_logit_reuse_tag,
                                             std::vector<tensor_std_float_t> shape_coeff_vec,
                                             size_t base_shape_coeff_sz,
                                             std::optional<size_t> deviation_operation_window): shape_vec(std::move(shape_vec)),
                                                                                                focal_sz_vec(std::move(focal_sz_vec)),
                                                                                                focal_suffix_map(std::move(focal_suffix_map)),
                                                                                                rotation_sz_vec(std::move(rotation_sz_vec)),
                                                                                                parameter_bound_ratio_vec(std::move(parameter_bound_ratio_vec)),
                                                                                                has_process_unit_logit_reuse_tag(has_process_unit_logit_reuse_tag),
                                                                                                has_process_group_logit_reuse_tag(has_process_group_logit_reuse_tag),
                                                                                                has_being_logit_reuse_tag(has_being_logit_reuse_tag),
                                                                                                has_base_matrix_logit_reuse_tag(has_base_matrix_logit_reuse_tag),
                                                                                                shape_coeff_vec(std::move(shape_coeff_vec)),
                                                                                                base_shape_coeff_sz(base_shape_coeff_sz),
                                                                                                deviation_operation_window(deviation_operation_window){}

            auto get_deviation(const std::vector<std::shared_ptr<std::string>>& token_vec) -> mdc_float_t
            {

            }
        };
}

#endif