#ifndef __CUDA_MANAGEMENT_HOST_SERVICE_DGBUF_H__
#define __CUDA_MANAGEMENT_HOST_SERVICE_DGBUF_H__

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include "assert.h"
#include "host_service.h"
#include <serializer/dg_buf.h>

namespace cuda_management::host_service
{
    template <class T>
    inline auto to_cuda_dgbuf(const T& obj) -> std::shared_ptr<decltype(dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::declval<const T&>(),
                                                                                                                        std::declval<std::string&>()))>
    {
        std::string host_buf                                    = {};
        auto result                                             = dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(obj, host_buf);
        std::shared_ptr<char[]> cuda_buf                        = make_cuda_buffer_from_host_view(host_buf);
        std::unique_ptr<decltype(cuda_buf)> immutable_wrapper   = std::make_unique<decltype(cuda_buf)>(std::move(cuda_buf));
        char * device_mem                                       = immutable_wrapper->get();

        auto deallocator = [mem_holder = std::move(immutable_wrapper)](decltype(result) * obj)
        {
            *mem_holder = nullptr; //this is to avoid undefined, mem_holder can be optimized away...
            delete obj;
        };

        result.set_buf(device_mem);

        return std::unique_ptr<decltype(result), decltype(deallocator)>(new decltype(result)(result), std::move(deallocator));
    }
}

#endif