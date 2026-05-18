#ifndef __CONNECTION_HANDSHAKE_CLIENT_LOCAL_EXCEPTION_H__
#define __CONNECITON_HANDSHAKE_CLIENT_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>
#include <exception>
#include <stdexcept>

namespace connection_handshake_client
{
    using local_exception_t = uint8_t;

    struct bad_connect_error: std::invalid_argument
    {
        bad_connect_error(): std::invalid_argument("bad connect"){}
    };

    static inline constexpr local_exception_t SUCCESS                   = 0u;
    static inline constexpr local_exception_t BAD_CONNECTION_ERROR_CODE = 1u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE  = 2u;

    auto to_local_exception_error_code(std::exception_ptr ptr) -> local_exception_t
    {
        try
        {
            std::rethrow_exception(ptr);
        }
        catch (bad_connect_error& e)
        {
            return BAD_CONNECTION_ERROR_CODE;
        }
        catch (std::exception& e)
        {
            return OTHER_RUNTIME_ERROR_CODE;
        }

        return SUCCESS;
    }

    auto verbose_exception(std::exception_ptr ptr) -> std::string
    {
        try
        {
            std::rethrow_exception(ptr);
        }
        catch (std::invalid_argument& e)
        {
            return std::string(e.what());
        }
        catch (std::runtime_error& e)
        {
            return std::string(e.what());
        }
        catch (std::exception& e)
        {
            return std::string("something went wrong");
        }

        return "";
    }    
}

#endif