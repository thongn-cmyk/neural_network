#ifndef __CUDA_MANAGEMENT_CU_HOST_SERVICE_HEADER_H__
#define __CUDA_MANAGEMENT_CU_HOST_SERVICE_HEADER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string_view>
#include <bit>
#include <array>
#include <cstring>

namespace cuda_management::host_service
{
    extern auto make_cuda_buffer_from_size(size_t sz) -> std::shared_ptr<char[]>;

    extern void memcpy_host_to_device(void * dst, const void * src, size_t sz);

    extern void memcpy_device_to_host(void * dst, const void * src, size_t sz);

    extern auto make_cuda_buffer_from_host_view(std::string_view host_view) -> std::shared_ptr<char[]>;

    extern auto cuda_to_host_buffer(const std::shared_ptr<char[]>& cuda_buf, size_t cuda_buf_sz) -> std::shared_ptr<char[]>;

    template <class T, class ...Args, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true> //iec559 + compliances
    auto make_cuda_object(Args&& ...args) -> std::shared_ptr<T>
    {
        static_assert(sizeof(T) != 0u);
        static_assert(std::endian::native == std::endian::little);

        T obj                                   = T(std::forward<Args>(args...));
        std::array<char, sizeof(T)> byte_rep    = std::bit_cast<std::array<char, sizeof(T)>>(obj);

        return std::static_pointer_cast<T>(std::static_pointer_cast<void>(make_cuda_buffer_from_host_view(std::string_view(byte_rep.data(), byte_rep.size()))));
    }

    template <class T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true> //iec559 + compliances
    auto read_cuda_object(const std::shared_ptr<T>& obj) -> T
    {
        if (obj == nullptr)
        {
            throw std::invalid_argument("bad object, null");
        }

        std::array<char, sizeof(T)> byte_rep    = {};
        std::shared_ptr<char[]> host_buf        = cuda_to_host_buffer(std::static_pointer_cast<char[]>(std::static_pointer_cast<void>(obj)), sizeof(T));

        std::memcpy(byte_rep.data(), host_buf.get(), sizeof(T));

        return std::bit_cast<T>(byte_rep);
    }
}