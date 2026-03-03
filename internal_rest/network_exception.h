#ifndef __NETWORK_EXCEPTION_H__
#define __NETWORK_EXCEPTION_H__

//define HEADER_CONTROL 0

#include <exception>
#include <stdint.h>
#include <stdlib.h> 
#include <type_traits>
#include <expected>
#include <stacktrace>
#include <iostream>

using exception_t = uint16_t; 

namespace dg_sock::network_exception{

    #ifdef __DG_NETWORK_CUDA_FLAG__
    using cuda_exception_t = cudaError_t;
    #else 
    using cuda_exception_t = uint8_t;
    #endif

    using kernel_exception_t    = int; 
    using core_exception_t      = uint32_t;

    struct base_exception: std::exception{

        const char * c_msg;

        base_exception(const char * c_msg) noexcept: c_msg(c_msg){}

        inline auto what() const noexcept -> const char *{

            return this->c_msg;
        }
    };

    struct invalid_arg: base_exception{

        invalid_arg() noexcept: base_exception("invalid_arg"){}
    };

    struct ExceptionHandlerInterface{
        virtual ~ExceptionHandlerInterface() noexcept = default;
        virtual void update(exception_t) noexcept = 0;
    };

    static inline constexpr exception_t SUCCESS                             = 0u;
    static inline constexpr exception_t CRITICAL_FAILURE                    = 2u;
    static inline constexpr exception_t OCCUPIED_MEMREGION                  = 3u;
    static inline constexpr exception_t UNWAITABLE_EXCEPTION                = 4u;
    static inline constexpr exception_t SEGFAULT                            = 5u;
    static inline constexpr exception_t INTERNAL_CORRUPTION                 = 6u;
    static inline constexpr exception_t CUDA_DEVICE_NOT_FOUND               = 7u;
    static inline constexpr exception_t CUDA_DEVICE_NOT_SUPPORTED           = 8u;
    static inline constexpr exception_t UNMET_PRECOND                       = 9u; 
    static inline constexpr exception_t RECOVERABLE_OUT_OF_MEMORY           = 10u;
    static inline constexpr exception_t OUT_OF_MEMORY                       = 11u;  
    static inline constexpr exception_t INVALID_SERIALIZATION_FORMAT        = 12u;
    static inline constexpr exception_t INVALID_VMAPTR_FORMAT               = 13u;
    static inline constexpr exception_t INVALID_DICTIONARY_KEY              = 14u;
    static inline constexpr exception_t INVALID_TABLE_DISPATCH_CODE         = 15u;
    static inline constexpr exception_t INCOMPATIBLE_OPERATABLE_ID          = 16u;
    static inline constexpr exception_t BAD_ACCESS                          = 17u;
    static inline constexpr exception_t BAD_ALIGNMENT                       = 18u;
    static inline constexpr exception_t UNREGISTERED_CUFILE_PTR             = 19u;
    static inline constexpr exception_t RUNTIME_SOCKETIO_ERROR              = 20u;
    static inline constexpr exception_t BAD_SPIN                            = 21u;
    static inline constexpr exception_t BUFFER_OVERFLOW                     = 22u;
    static inline constexpr exception_t RUNTIME_FILEIO_ERROR                = 23u; 
    static inline constexpr exception_t LOST_RETRANSMISSION                 = 24u;
    static inline constexpr exception_t INVALID_INIT_ARG                    = 25u;
    static inline constexpr exception_t UNSUPPORTED_DAEMON_KIND             = 26u;
    static inline constexpr exception_t NO_DAEMON_RUNNER_AVAILABLE          = 27u;
    static inline constexpr exception_t INVALID_ARGUMENT                    = 28u;
    static inline constexpr exception_t UNIDENTIFIED_EXCEPTION              = 29u;
    static inline constexpr exception_t PTHREAD_EFAULT                      = 30u;
    static inline constexpr exception_t PTHREAD_EINVAL                      = 31u;
    static inline constexpr exception_t PTHREAD_ESRCH                       = 32u;
    static inline constexpr exception_t PTHREAD_CAUSA_SUI                   = 33u;
    static inline constexpr exception_t UNDEFINED_HARDWARE_CONCURRENCY      = 34u;
    static inline constexpr exception_t FILE_NOT_FOUND                      = 35u;
    static inline constexpr exception_t CORRUPTED_FILE                      = 36u;
    static inline constexpr exception_t BAD_CUDA_DEVICE_ACCESS              = 37u;
    static inline constexpr exception_t CUDA_LAUNCH_COMPLETED               = 38u; 
    static inline constexpr exception_t CUDA_EXECUTABLE_WAITING_DISPATCH    = 39u; 
    static inline constexpr exception_t CUDA_NOT_SUPPORTED                  = 40u;
    static inline constexpr exception_t UNMET_CLEARANCE                     = 41u;
    static inline constexpr exception_t POSTGRES_NOT_INITIALIZED            = 42u;
    static inline constexpr exception_t BAD_ENCODING_FORMAT                 = 43u;
    static inline constexpr exception_t POSTGRES_CORRUPTION                 = 44u;
    static inline constexpr exception_t POSTGRES_EXCEED_QUERY_LENGTH_LIMIT  = 45u;
    static inline constexpr exception_t EXPIRED_TOKEN                       = 46u;
    static inline constexpr exception_t BAD_AUTHENTICATION                  = 47u;
    static inline constexpr exception_t ENTRY_NOT_FOUND                     = 48u;
    static inline constexpr exception_t RESOURCE_EXHAUSTION                 = 49u;
    static inline constexpr exception_t TIMEOUT                             = 50u;
    static inline constexpr exception_t BAD_RETRANSMISSION                  = 51u;
    static inline constexpr exception_t INVALID_FORMAT                      = 52u;
    static inline constexpr exception_t SOCKET_BAD_IP                       = 53u;
    static inline constexpr exception_t UNSUPPORTED_FUNCTIONALITY           = 54u;
    static inline constexpr exception_t BAD_POLYMORPHIC_ACCESS              = 55u;
    static inline constexpr exception_t SOCKET_CORRUPTED_PACKET             = 56u;
    static inline constexpr exception_t SOCKET_MALFORMED_PACKET             = 57u;
    static inline constexpr exception_t SOCKET_ADD_ACKED_PACKET             = 00u;
    static inline constexpr exception_t SOCKET_BAD_RECEIPIENT               = 58u;
    static inline constexpr exception_t SOCKET_BAD_TRAFFIC                  = 59u;
    static inline constexpr exception_t SOCKET_BAD_IP_RULE                  = 60u;
    static inline constexpr exception_t SOCKET_BAD_BUFFER_LENGTH            = 61u;
    static inline constexpr exception_t SOCKET_MAX_RETRANSMISSION_REACHED   = 62u;
    static inline constexpr exception_t SOCKET_QUEUE_FULL                   = 63u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_SEGMENT           = 64u;
    static inline constexpr exception_t SOCKET_STREAM_DUPLICATE_SEGMENT     = 00u; 
    static inline constexpr exception_t SOCKET_STREAM_TIMEOUT               = 65u;
    static inline constexpr exception_t SOCKET_STREAM_MIGHT_BE_BLACKLISTED  = 66u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_BUFFER_LENGTH     = 67u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_SEGMENT_SIZE      = 68u;
    static inline constexpr exception_t SOCKET_STREAM_SEGMENT_FILLING       = 69u;
    static inline constexpr exception_t SOCKET_STREAM_LEAK                  = 70u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_OUTBOUND_RULE     = 71u;
    static inline constexpr exception_t SOCKET_STREAM_CORRUPTED_PACKET      = 00u;
    static inline constexpr exception_t SOCKET_CHANNEL_MAX_MSG_SIZE_REACHED = 00u;
    static inline constexpr exception_t ALOTTED_BUFFER_EXCEEDED             = 72u;
    static inline constexpr exception_t DUPLICATE_ENTRY                     = 00u;
    static inline constexpr exception_t EXPECTED_NOT_INITIALIZED            = 00u;
    static inline constexpr exception_t VARIANT_VBE                         = 00u;
    static inline constexpr exception_t QUEUE_FULL                          = 00u;
    static inline constexpr exception_t UNSPECIFIED_ERROR                   = 1024u;
    static inline constexpr exception_t BAD_OPERATION                       = 00u;
    static inline constexpr exception_t INDEX_OUT_OF_RANGE                  = 00u;
    static inline constexpr exception_t OUT_OF_BOUND_ACCESS                 = 00u;
    static inline constexpr exception_t BAD_STATE                               = 00u;

    static inline constexpr exception_t REST_CACHE_MAX_RESPONSE_SIZE_REACHED    = 00u;
    static inline constexpr exception_t REST_CACHE_POPULATION_LIMIT_REACHED     = 00u;
    static inline constexpr exception_t REST_CLIENTSIDE_TIMEOUT                 = 00u;
    static inline constexpr exception_t REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT      = 00u;
    static inline constexpr exception_t REST_INVALID_URL                        = 00u;
    static inline constexpr exception_t REST_INVALID_ARGUMENT                   = 00u;
    static inline constexpr exception_t REST_BAD_CACHE_UNIQUE_WRITE             = 00u;
    static inline constexpr exception_t REST_INTERNAL_SERVER_ERROR              = 00u;
    static inline constexpr exception_t REST_OBSERVER_SECOND_SET                = 00u;
    static inline constexpr exception_t REST_OBSERVER_BAD_NOTIFY                = 00u;
    static inline constexpr exception_t REST_OBSERVER_SECOND_NOTIFY             = 00u;
    static inline constexpr exception_t REST_RESPONSE_DOUBLE_INVOKE             = 00u;
    static inline constexpr exception_t REST_OTHER_ERROR                        = 00u;
    static inline constexpr exception_t REST_TICKET_NOT_FOUND                   = 00u;
    static inline constexpr exception_t REST_TICKET_OBSERVER_NOT_FOUND          = 00u;
    static inline constexpr exception_t REST_BAD_RESPONSE                       = 00u;
    static inline constexpr exception_t REST_INVALID_TIMEOUT                    = 00u;
    static inline constexpr exception_t MAX_CONSUME_SIZE_EXCEEDED               = 00u;

    inline auto wrap_cuda_exception(cuda_exception_t) noexcept -> exception_t{

        return UNSPECIFIED_ERROR;
    }

    inline auto wrap_kernel_error(kernel_exception_t) noexcept -> exception_t{

        return UNSPECIFIED_ERROR;
    }

    inline auto wrap_core_exception(core_exception_t) noexcept -> exception_t{

        return UNSPECIFIED_ERROR;
    }

    inline auto wrap_std_errcode(...) -> exception_t{ 

        return UNSPECIFIED_ERROR;
    }

    inline auto wrap_std_exception(std::exception_ptr) -> exception_t{

        return UNSPECIFIED_ERROR;
    }

    inline auto is_success(exception_t err) noexcept -> bool{

        return err == SUCCESS;
    }

    inline auto is_failed(exception_t err) noexcept -> bool{

        return err != SUCCESS;
    }

    inline auto verbose(exception_t) noexcept -> const char *{

        return "error";
    }

    inline void throw_exception(exception_t){
        
        throw base_exception("unspecified error");
    }

    template <class T>
    struct base_type{
        using type = T;
    };

    template <class T>
    struct base_type<T&>: base_type<T>{};

    template <class T>
    struct base_type<T&&>: base_type<T>{};

    template <class T>
    struct base_type<const T>: base_type<T>{};

    template <class T>
    struct base_type<volatile T>: base_type<T>{};

    template <class T>
    using base_type_t = typename base_type<T>::type; 

    template <class Functor>
    inline auto to_cstyle_function(Functor functor) noexcept{

        static_assert(std::is_nothrow_move_constructible_v<Functor>);

        auto rs = [f = std::move(functor)]<class ...Args>(Args&& ...args) noexcept(noexcept(functor(std::forward<Args>(args)...))){
            using ret_t = decltype(f(std::forward<Args>(args)...));

            if constexpr(std::is_same_v<ret_t, void>){
                try{
                    f(std::forward<Args>(args)...);
                    return SUCCESS;
                } catch (...){
                    return wrap_std_exception(std::current_exception());
                }
            } else{
                try{
                    static_assert(std::is_nothrow_move_constructible_v<ret_t>);
                    static_assert(std::is_same_v<ret_t, base_type_t<ret_t>>);
                    return std::expected<ret_t, exception_t>(f(std::forward<Args>(args)...));
                } catch (...){
                    return std::expected<ret_t, exception_t>(std::unexpected(wrap_std_exception(std::current_exception())));
                }
            }
        };

        return rs;
    } 

    template <class T>
    inline auto remove_expected(std::expected<T, exception_t> inp) noexcept -> T{
        
        static_assert(std::is_nothrow_move_constructible_v<T>);

        if (!inp.has_value()){
            std::abort();
        }

        return std::move(inp.value());
    } 

    inline void dg_noexcept(exception_t err) noexcept{

        if (dg_sock::network_exception::is_failed(err)){
            std::abort();
        }
    } 

    template <class T, class ...Args>
    inline auto cstyle_initialize(Args&& ...args) noexcept -> std::expected<T, exception_t>{

        if constexpr(std::is_nothrow_constructible_v<std::expected<T, exception_t>, std::in_place_t, Args&&...> && std::is_nothrow_move_constructible_v<std::expected<T, exception_t>>){
            return std::expected<T, exception_t>(std::in_place_t{}, std::forward<Args>(args)...);
        } else{
            try{
                return std::expected<T, exception_t>(std::in_place_t{}, std::forward<Args>(args)...);
            } catch (...){
                return std::unexpected(wrap_std_exception(std::current_exception()));
            }
        }
    }

    template <class ...Args, std::enable_if_t<std::conjunction_v<std::is_same<Args, exception_t>...>, bool> = true>
    inline auto disjunction(Args... args) noexcept -> exception_t{
        
        exception_t rs = SUCCESS; 

        ([&]{
            if (is_failed(args)){
                rs = args;
            }
        }(), ...);

        return rs;
    }

    template <class T, class T1>
    class expected_result_if_object{
        
        private:

            bool evaluator;
            T true_ret;
            T1 false_ret; 

        public:

            constexpr expected_result_if_object(bool evaluator, T true_ret, T1 false_ret) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<T1>): evaluator(evaluator),
                                                                                                                                                                                         true_ret(std::move(true_ret)),
                                                                                                                                                                                         false_ret(std::move(false_ret)){}
            
            template <class ...Args>
            constexpr operator std::expected<Args...>() noexcept(std::is_nothrow_constructible_v<std::expected<Args...>, T&&> && std::is_nothrow_constructible_v<std::expected<Args...>, T1&&>){

                if (this->evaluator){
                    return std::expected<Args...>(std::move(this->true_ret));
                } else{
                    return std::expected<Args...>(std::move(this->false_ret));
                }
            }
    };

    template <class T, class T1>
    constexpr auto expected_result_if(bool evaluator, T true_ret, T1 false_ret) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_constructible_v<T1>) -> expected_result_if_object<T, T1>{

        return expected_result_if_object<T, T1>(evaluator, std::move(true_ret), std::move(false_ret));
    }
}

#endif