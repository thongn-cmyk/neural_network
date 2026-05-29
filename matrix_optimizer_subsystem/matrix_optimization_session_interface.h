#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_MATRIX_OPTIMIZATION_SESSION_INTERFACE_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_MATRIX_OPTIMIZATION_SESSION_INTERFACE_H__

#include <matrix/tensor_model.h>
#include <memory>
#include <common_exception/cancellation_token.h>
#include <matrix/generic_matrix_factory.h>
#include <concurrency_detachable_task/detachable_task_handle_interface.h>
#include <stl_extension/stdx.h>

namespace matrix_optimizer_subsystem
{
    template <class T>
    using Promise   = concurrency_detachable_task::DetachableTaskHandleInterface<T>; //

    class MatrixOptimizationSessionInterface
    {
        public:

            virtual ~MatrixOptimizationSessionInterface() noexcept = default;

            virtual auto add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp,
                                           const std::shared_ptr<tensor_model::Matrix>& out,
                                           const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> std::shared_ptr<Promise<stdx::fancy_void>> = 0;

            virtual auto optimize(const generic_matrix_factory::ExternalGenericMatrixResource& resource,
                                  const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> std::shared_ptr<Promise<generic_matrix_factory::ExternalGenericMatrixResource>> = 0;
    };
}

#endif