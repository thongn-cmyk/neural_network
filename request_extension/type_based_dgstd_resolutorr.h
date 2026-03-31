#ifndef __TYPE_BASED_DGSTD_RESOLUTOR_H__
#define __TYPE_BASED_DGSTD_RESOLUTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include <internal_rest/network_rest_frame.h>
#include "type_based_resolutor_interface.h"
#include <string_view>
#include <string>
#include <exception>

namespace request_extension::resolutor
{

    template <class T_In, class T_Out>
    class DgstdResolutor: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::unique_ptr<TypeBasedResolutorInterface<T_In, T_Out>> base;

        public:

            DgstdResolutor(std::unique_ptr<TypeBasedResolutorInterface<T_In, T_Out>> base) noexcept: base(std::move(base)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                T_In semantic_request       = dg::network_compact_serializer::dgstd_deserialize<T_In>(request.payload);
                T_Out semantic_response     = this->base->handle(semantic_request);

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                }
            }
    };
}

#endif
