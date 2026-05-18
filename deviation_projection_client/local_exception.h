#ifndef __DEVIATION_PROJECTION_CLIENT_LOCAL_EXCEPTION_H__
#define __DEVIATION_PROJECTION_CLIENT_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <exception>
#include <stdexcept>

namespace deviation_projection_client
{
    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                               = 0u;
    static inline constexpr local_exception_t DESTROYED_CLIENT_BOX_ERROR_CODE       = 1u;
    static inline constexpr local_exception_t CLIENT_BOX_NOT_FOUND_ERROR_CODE       = 2u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE           = 3u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE              = 4u;
    static inline constexpr local_exception_t INOPERABLE_CLIENT_ERROR_CODE          = 5u;

    struct destroyed_client_box_error: std::invalid_argument
    {
        destroyed_client_box_error(): std::invalid_argument("bad client box operation, client box was destroyed"){}
    };

    struct client_box_not_found_error: std::invalid_argument
    {
        client_box_not_found_error(): std::invalid_argument("bad client box, client box not found"){}
    };

    struct other_invalid_argument: std::invalid_argument
    {
        private:

            std::string msg;
        
        public:

            other_invalid_argument(std::string_view msg_arg): std::invalid_argument(""),
                                                              msg(msg_arg){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct other_runtime_error: std::runtime_error
    {
        private:

            std::string msg;

        public:

            other_runtime_error(std::string_view msg_arg): std::runtime_error(""),
                                                           msg(msg_arg){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct inoperable_client_error: std::invalid_argument
    {
        inoperable_client_error(): std::invalid_argument("bad client operation, client in in inoperable state"){}
    };

    void throw_error_code(local_exception_t err_code, std::string_view msg_arg)
    {
        switch (err_code)
        {
            case SUCCESS:
            {
                break;
            }
            case DESTROYED_CLIENT_BOX_ERROR_CODE:
            {
                throw destroyed_client_box_error();
            }
            case CLIENT_BOX_NOT_FOUND_ERROR_CODE:
            {
                throw client_box_not_found_error();
            }
            case INOPERABLE_CLIENT_ERROR_CODE:
            {
                throw inoperable_client_error{};
            }
            case OTHER_INVALID_ARGUMENT_CODE:
            {
                throw other_invalid_argument(msg_arg);
            }
            case OTHER_RUNTIME_ERROR_CODE:
            {
                throw other_runtime_error(msg_arg);
            }
            default:
            {
                throw other_runtime_error(msg_arg);
            }
        }
    }
}

#endif