#ifndef __NETWORK_ERROR_HANDLER_H__
#define __NETWORK_ERROR_HANDLER_H__

//define HEADER_CONTROL 8

// #include "network_log.h"
#include <stdlib.h>
#include "network_exception.h" 

namespace dg_sock::network_exception_handler{
    
    static inline void dispatch(exception_t err_code, bool is_throw, bool has_log_write_on_err){

        if (network_exception::is_success(err_code)){
            return;
        }

        if (has_log_write_on_err){
            if (is_throw){
                // dg_sock::network_log_stackdump::error(network_exception::verbose(err_code));
                network_exception::throw_exception(err_code);
            } else{
                // dg_sock::network_log_stackdump::critical(network_exception::verbose(err_code));
                std::abort();
            }
        } else{
            if (is_throw){
                network_exception::throw_exception(err_code);
            } else{
                std::abort();
            }
        }
    }
    
    inline void err_log(exception_t) noexcept{

    }

    inline void err_log_optional(exception_t) noexcept{

    }

    template <class T>
    inline auto err_log(std::expected<T, exception_t>) noexcept{

    }

    template <class T>
    inline auto err_log_optional(std::expected<T, exception_t>) noexcept{

    }

    inline void nothrow_nolog(exception_t err_code) noexcept{

        dispatch(err_code, false, false);
    }

    inline void nothrow_log(exception_t err_code) noexcept{

        dispatch(err_code, false, true);
    }

    inline void throw_nolog(exception_t err_code){

        dispatch(err_code, true, false);
    }

    inline void throw_log(exception_t err_code){

        dispatch(err_code, true, true);
    }
    
    template <class T, std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool> = true>
    inline auto nothrow_nolog(std::expected<T, exception_t>) noexcept -> T{

    }

    template <class T, std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool> = true>
    inline auto nothrow_log(std::expected<T, exception_t> arg) noexcept -> T{

        if (!arg.has_value()){
            nothrow_log(arg.error());
        }

        return std::move(arg.value());
    }

    template <class T, std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool> = true>
    inline auto throw_nolog(std::expected<T, exception_t> arg) -> T{

        if (!arg.has_value()){
            dg_sock::network_exception::throw_exception(arg.error());
        }

        return std::move(arg.value());
    }

    template <class T, std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool> = true>
    inline auto throw_log(std::expected<T, exception_t>) -> T{

    }

    inline void dg_assert(bool){

    }
} 

#endif