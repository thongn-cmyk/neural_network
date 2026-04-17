#ifndef __MATRIX_PROJECTION_SERVER_LOCAL_EXCEPTION_H__
#define __MATRIX_PROJECTION_SERVER_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>

namespace matrix_projection_server
{
    using local_exception_t = uint8_t;

    struct client_box_not_found_error: std::invalid_argument
    {
        client_box_not_found_error(): std::invalid_argument("bad client box, client box not found"){}
    };

    struct other_invalid_argument: std::invalid_argument
    {
        other_invalid_argument(const char * msg): std::invalid_argument(msg){}
    };

    struct other_runtime_error: std::runtime_error
    {
        other_runtime_error(const char * msg): std::runtime_error(msg){}
    };

    static inline constexpr local_exception_t SUCCESS                           = 0u;
    static inline constexpr local_exception_t CLIENT_BOX_NOT_FOUND_ERROR_CODE   = 1u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE       = 2u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE          = 3u;

    auto to_local_exception_error_code(std::exception_ptr ptr) -> local_exception_t
    {
        try
        {
            std::rethrow_exception(ptr);
        }
        catch (client_box_not_found_error& e)
        {
            return CLIENT_BOX_NOT_FOUND_ERROR_CODE;
        }
        catch (other_invalid_argument& e)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }
        catch (other_runtime_error& e)
        {
            return OTHER_RUNTIME_ERROR_CODE;
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