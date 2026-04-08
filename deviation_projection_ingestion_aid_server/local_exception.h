#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_LOCAL_EXCEPTION_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>

namespace deviation_projection_ingestion_aid_server
{
    using local_exception_t = uint8_t;

    struct client_box_not_found_error: std::invalid_argument
    {
        client_box_not_found_error(): std::invalid_argument("bad client box operation, client box id not found"){}
    };

    struct destroyed_client_box_error: std::runtime_error
    {
        destroyed_client_box_error(): std::runtime_error("bad client box operation, client box was destroyed"){}
    };

    struct run_not_invoked_error: std::invalid_argument
    {
        run_not_invoked_error(): std::invalid_argument("bad client box operation, wait invoked before run"){}
    };

    struct second_wait_error: std::invalid_argument
    {
        second_wait_error(): std::invalid_argument("bad client box operation, second wait invoked"){}
    };

    struct second_run_error: std::invalid_argument
    {
        second_run_error(): std::invalid_argument("bad client box operation, second run invoked"){}
    };

    struct interrupted_run_error: std::runtime_error
    {
        interrupted_run_error(): std::runtime_error("bad client box operation, run was interrupted by interruption signal"){}
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
    static inline constexpr local_exception_t DESTROYED_CLIENT_BOX_ERROR_CODE   = 2u;
    static inline constexpr local_exception_t RUN_NOT_INVOKE_ERROR_CODE         = 3u;
    static inline constexpr local_exception_t SECOND_WAIT_ERROR_CODE            = 4u;
    static inline constexpr local_exception_t SECOND_RUN_ERROR_CODE             = 5u;
    static inline constexpr local_exception_t INTERRUPTED_RUN_ERROR_CODE        = 6u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE       = 7u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE          = 8u;

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
        catch (destroyed_client_box_error& e)
        {
            return DESTROYED_CLIENT_BOX_ERROR_CODE;
        }
        catch (run_not_invoked_error& e)
        {
            return RUN_NOT_INVOKE_ERROR_CODE;
        }
        catch (second_wait_error& e)
        {
            return SECOND_WAIT_ERROR_CODE;
        }
        catch (second_run_error& e)
        {
            return SECOND_RUN_ERROR_CODE;
        }
        catch (interrupted_run_error& e)
        {
            return INTERRUPTED_RUN_ERROR_CODE;
        }
        catch (std::invalid_argument& e)
        {
            return OTHER_INVALID_ARGUMENT_CODE;
        }
        catch (std::runtime_error& e)
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