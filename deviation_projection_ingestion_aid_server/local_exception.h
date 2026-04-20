#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_LOCAL_EXCEPTION_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>

namespace deviation_projection_ingestion_aid_server
{
    using local_exception_t = uint8_t;

    struct destroyed_client_box_error: std::runtime_error
    {
        destroyed_client_box_error(): std::runtime_error("bad client box operation, client box was destroyed"){}
    };

    struct second_run_error: std::invalid_argument
    {
        second_run_error(): std::invalid_argument("bad client box operation, second run invoked"){}
    };

    struct run_not_invoked_error: std::invalid_argument
    {
        run_not_invoked_error(): std::invalid_argument("bad client box operation, run was not invoked"){}
    };

    struct ingestion_in_progress_error: std::invalid_argument
    {
        ingestion_in_progress_error(): std::invalid_argument("bad client box operation, ingestion in progress"){}
    };

    struct client_box_not_found_error: std::invalid_argument
    {
        client_box_not_found_error(): std::invalid_argument("bad client box operation, client box id not found"){}
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
    static inline constexpr local_exception_t DESTROYED_CLIENT_BOX_ERROR_CODE   = 1u;
    static inline constexpr local_exception_t SECOND_RUN_ERROR_CODE             = 2u;
    static inline constexpr local_exception_t RUN_NOT_INVOKED_ERROR_CODE        = 3u;
    static inline constexpr local_exception_t INGESTION_IN_PROGRESS_ERROR_CODE  = 4u;
    static inline constexpr local_exception_t CLIENT_BOX_NOT_FOUND_ERROR_CODE   = 5u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE       = 6u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE          = 7u;

    auto to_local_exception_error_code(std::exception_ptr ptr) -> local_exception_t
    {
        try
        {
            std::rethrow_exception(ptr);
        }
        catch (destroyed_client_box_error& e)
        {
            return DESTROYED_CLIENT_BOX_ERROR_CODE;
        }
        catch (second_run_error& e)
        {
            return SECOND_RUN_ERROR_CODE;
        }
        catch (run_not_invoked_error& e)
        {
            return RUN_NOT_INVOKED_ERROR_CODE;
        }
        catch (ingestion_in_progress_error& e)
        {
            return INGESTION_IN_PROGRESS_ERROR_CODE;
        }
        catch (client_box_not_found_error& e)
        {
            return CLIENT_BOX_NOT_FOUND_ERROR_CODE;
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