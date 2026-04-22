#ifndef __MATRIX_BROKER_SERVER_CONTROLLER_H__
#define __MATRIX_BROKER_SERVER_CONTROLLER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include "service.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <string>
#include <string_view>
#include "local_exception.h"

namespace matrix_broker_server
{
    class BrokeMatrixResolutor: public virtual request_extension::resolutor::TypeBasedResolutorInterface<BrokeMatrixRequest, BrokeMatrixResponse>
    {
        private:

            std::shared_ptr<MatrixBrokerageServiceInterface> matrix_brokerage_service;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_broker/broke_matrix";

            BrokeMatrixResolutor(std::shared_ptr<MatrixBrokerageServiceInterface> matrix_brokerage_service)
            {
                if (matrix_brokerage_service == nullptr)
                {
                    throw std::invalid_argument("bad matrix brokerage service, null");
                }

                this->matrix_brokerage_service = std::move(matrix_brokerage_service);
            }

            auto handle(const BrokeMatrixRequest& request) -> BrokeMatrixResponse
            {
                try
                {
                    return BrokeMatrixResponse
                    {
                        .result = this->matrix_brokerage_service->broke_matrix(request.generator_id, request.matrix_entropy, request.flat_matrix_sz),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return BrokeMatrixResponse
                    {
                        .result = std::unexpected(to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif