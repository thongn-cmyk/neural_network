#ifndef __DG_COMMON_EXCEPTION_H__
#define __DG_COMMON_EXCEPTION_H__

//define HEADER_CONTROL 0

#include <exception>
#include <stdint.h>
#include <stdlib.h> 
#include <type_traits>
#include <expected>
#include <stacktrace>
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <utility>

using exception_t = uint16_t; 

namespace common_exception
{
    using kernel_exception_t    = int;

    struct codex_base
    {
        exception_t codex;

        codex_base(exception_t codex): codex(codex){}
    };

    struct polymorphic_exception_interface
    {
        virtual ~polymorphic_exception_interface() noexcept = default;

        virtual void throw_me() = 0;
        virtual auto what() const noexcept -> std::add_pointer_t<const char> = 0;
    };

    template <class BaseException>
    struct stack_embedded_error: BaseException
    {
        private:

            decltype(std::stacktrace::current()) trace;
            std::unique_ptr<std::string> what_inquiry;

        public:

            stack_embedded_error(): trace(std::stacktrace::current()),
                                    what_inquiry(std::make_unique<std::string>()),
                                    BaseException(){}

            virtual const char * what() const noexcept(noexcept(std::declval<const BaseException&>().what()))
            {
                if (this->what_inquiry == nullptr)
                {
                    *this->what_inquiry = std::string(BaseException::what()) + "<trace>" + std::to_string(this->trace);
                }

                return this->what_inquiry->data();
            }
    };

    static inline constexpr exception_t SUCCESS                                 = 0u;
    static inline constexpr exception_t SEGFAULT                                = 1u;
    static inline constexpr exception_t INTERNAL_CORRUPTION                     = 2u;
    static inline constexpr exception_t OUT_OF_MEMORY                           = 3u;
    static inline constexpr exception_t INVALID_SERIALIZATION_FORMAT            = 4u;
    static inline constexpr exception_t INVALID_DICTIONARY_KEY                  = 5u;
    static inline constexpr exception_t BAD_ACCESS                              = 6u;
    static inline constexpr exception_t BAD_ALIGNMENT                           = 7u;
    static inline constexpr exception_t RUNTIME_SOCKETIO_ERROR                  = 8u;
    static inline constexpr exception_t BUFFER_OVERFLOW                         = 9u;
    static inline constexpr exception_t LOST_RETRANSMISSION                     = 10u;
    static inline constexpr exception_t UNSUPPORTED_DAEMON_KIND                 = 11u;
    static inline constexpr exception_t NO_DAEMON_RUNNER_AVAILABLE              = 12u;
    static inline constexpr exception_t INVALID_ARGUMENT                        = 13u;
    static inline constexpr exception_t UNIDENTIFIED_ERROR                      = 14u;
    static inline constexpr exception_t PTHREAD_EFAULT                          = 15u;
    static inline constexpr exception_t PTHREAD_EINVAL                          = 16u;
    static inline constexpr exception_t PTHREAD_ESRCH                           = 17u;
    static inline constexpr exception_t PTHREAD_CAUSA_SUI                       = 18u;
    static inline constexpr exception_t RESOURCE_EXHAUSTION                     = 19u;
    static inline constexpr exception_t TIMEOUT                                 = 20u;
    static inline constexpr exception_t INVALID_FORMAT                          = 21u;
    static inline constexpr exception_t BAD_POLYMORPHIC_ACCESS                  = 22u;
    static inline constexpr exception_t SOCKET_BAD_IP                           = 23u;
    static inline constexpr exception_t SOCKET_CORRUPTED_PACKET                 = 24u;
    static inline constexpr exception_t SOCKET_MALFORMED_PACKET                 = 25u;
    static inline constexpr exception_t SOCKET_ACKED_ENQUEUE                    = 26u;
    static inline constexpr exception_t SOCKET_BAD_RECEIPIENT                   = 27u;
    static inline constexpr exception_t SOCKET_BAD_TRAFFIC                      = 28u;
    static inline constexpr exception_t SOCKET_BAD_IP_RULE                      = 29u;
    static inline constexpr exception_t SOCKET_BAD_BUFFER_LENGTH                = 30u;
    static inline constexpr exception_t SOCKET_MAX_RETRANSMISSION_REACHED       = 31u;
    static inline constexpr exception_t SOCKET_QUEUE_FULL                       = 32u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_SEGMENT               = 33u;
    static inline constexpr exception_t SOCKET_STREAM_DUPLICATED_SEGMENT        = 34u; 
    static inline constexpr exception_t SOCKET_STREAM_TIMEOUT                   = 35u;
    static inline constexpr exception_t SOCKET_STREAM_BLACKLISTED               = 36u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_BUFFER_LENGTH         = 37u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_SEGMENT_SIZE          = 38u;
    static inline constexpr exception_t SOCKET_STREAM_SEGMENT_FILLING           = 39u;
    static inline constexpr exception_t SOCKET_STREAM_LEAK                      = 40u;
    static inline constexpr exception_t SOCKET_STREAM_BAD_OUTBOUND_RULE         = 41u;
    static inline constexpr exception_t SOCKET_STREAM_CORRUPTED_PACKET          = 42u;
    static inline constexpr exception_t SOCKET_CHANNEL_MAX_MSG_SIZE_REACHED     = 43u;
    static inline constexpr exception_t DUPLICATED_ENTRY                        = 44u;
    static inline constexpr exception_t EXPECTED_NOT_INITIALIZED                = 45u;
    static inline constexpr exception_t VARIANT_VBE                             = 46u;
    static inline constexpr exception_t QUEUE_FULL                              = 47u;
    static inline constexpr exception_t BAD_OPERATION                           = 48u;
    static inline constexpr exception_t INDEX_OUT_OF_RANGE                      = 49u;
    static inline constexpr exception_t OUT_OF_BOUND_ACCESS                     = 50u;
    static inline constexpr exception_t BAD_STATE                               = 51u;
    static inline constexpr exception_t REST_CACHE_MAX_RESPONSE_SIZE_REACHED    = 52u;
    static inline constexpr exception_t REST_CACHE_POPULATION_LIMIT_REACHED     = 53u;
    static inline constexpr exception_t REST_CLIENTSIDE_TIMEOUT                 = 54u;
    static inline constexpr exception_t REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT      = 55u;
    static inline constexpr exception_t REST_INVALID_URL                        = 56u;
    static inline constexpr exception_t REST_INVALID_ARGUMENT                   = 57u;
    static inline constexpr exception_t REST_BAD_CACHE_UNIQUE_WRITE             = 58u;
    static inline constexpr exception_t REST_INTERNAL_SERVER_ERROR              = 59u;
    static inline constexpr exception_t REST_RESPONSE_DOUBLE_INVOKE             = 60u;
    static inline constexpr exception_t REST_OTHER_ERROR                        = 61u;
    static inline constexpr exception_t REST_TICKET_NOT_FOUND                   = 62u;
    static inline constexpr exception_t REST_TICKET_OBSERVER_NOT_FOUND          = 63u;
    static inline constexpr exception_t REST_LOST_RESPONSE                      = 64u;
    static inline constexpr exception_t REST_INVALID_TIMEOUT                    = 65u;
    static inline constexpr exception_t REST_MAX_CONSUME_SIZE_EXCEEDED          = 66u;
    static inline constexpr exception_t REST_MISMATCHED_SERIALIZATION_METHOD    = 67u;
    static inline constexpr exception_t POISONED_CONTAINER                      = 68u;
    static inline constexpr exception_t OPERATION_CANCELED_ERROR                = 69u;
    static inline constexpr exception_t OPERATION_GRACEFUL_TERMINATION_ERROR    = 70u;

    struct segfault: std::runtime_error,
                     codex_base
    {
        segfault(): std::runtime_error("segfault"),
                    codex_base(SEGFAULT){}
    };

    struct internal_corruption: std::runtime_error,
                                codex_base
    {
        internal_corruption(): std::runtime_error("internal corruption"),
                               codex_base(INTERNAL_CORRUPTION){}
    };

    struct out_of_memory: std::runtime_error,
                          codex_base
    {
        out_of_memory(): std::runtime_error("out of memory"),
                         codex_base(OUT_OF_MEMORY){}
    };

    struct invalid_serialization_format: std::invalid_argument,
                                         codex_base
    {
        invalid_serialization_format(): std::invalid_argument("invalid serialization format"),
                                        codex_base(INVALID_SERIALIZATION_FORMAT){}
    };

    struct invalid_dictionary_key: std::invalid_argument,
                                   codex_base
    {
        invalid_dictionary_key(): std::invalid_argument("invalid dictionary key"),
                                  codex_base(INVALID_DICTIONARY_KEY){}
    };

    struct bad_access: std::invalid_argument,
                       codex_base
    {
        bad_access(): std::invalid_argument("bad access"),
                      codex_base(BAD_ACCESS){}
    };

    struct bad_alignment: std::invalid_argument,
                          codex_base
    {
        bad_alignment(): std::invalid_argument("bad alignment"),
                         codex_base(BAD_ALIGNMENT){}
    };

    struct runtime_socketio_error: std::runtime_error,
                                   codex_base
    {
        runtime_socketio_error(): std::runtime_error("runtime socketio error"),
                                  codex_base(RUNTIME_SOCKETIO_ERROR){}
    };

    struct buffer_overflow: std::invalid_argument,
                            codex_base
    {
        buffer_overflow(): std::invalid_argument("buffer overflow"),
                           codex_base(BUFFER_OVERFLOW){}
    };

    struct lost_retransmission: std::runtime_error,
                                codex_base
    {
        lost_retransmission(): std::runtime_error("lost retrasmission"),
                               codex_base(LOST_RETRANSMISSION){}
    };

    struct unsupported_daemon_kind: std::invalid_argument,
                                    codex_base
    {
        unsupported_daemon_kind(): std::invalid_argument("unsupported daemon kind"),
                                   codex_base(UNSUPPORTED_DAEMON_KIND){}
    };

    struct no_daemon_runner_available: std::runtime_error,
                                       codex_base
    {
        no_daemon_runner_available(): std::runtime_error("no daemon runner available"),
                                      codex_base(NO_DAEMON_RUNNER_AVAILABLE){}
    };

    struct dg_invalid_argument: std::invalid_argument,
                                codex_base
    {
        dg_invalid_argument(): std::invalid_argument("invalid argument"),
                               codex_base(INVALID_ARGUMENT){}
    };

    struct unidentified_error: std::runtime_error,
                               codex_base
    {
        unidentified_error(): std::runtime_error("unidentified error"),
                              codex_base(UNIDENTIFIED_ERROR){}
    };

    struct pthread_efault: std::runtime_error,
                           codex_base
    {
        pthread_efault(): std::runtime_error("pthread efault"),
                          codex_base(PTHREAD_EFAULT){}
    };

    struct pthread_einval: std::invalid_argument,
                           codex_base
    {
        pthread_einval(): std::invalid_argument("pthread einval"),
                          codex_base(PTHREAD_EINVAL){}
    };

    struct pthread_esrch: std::runtime_error,
                          codex_base
    {
        pthread_esrch(): std::runtime_error("pthread esrch"),
                         codex_base(PTHREAD_ESRCH){}
    };

    struct pthread_causa_sui: std::invalid_argument,
                              codex_base
    {
        pthread_causa_sui(): std::invalid_argument("pthread causa sui"),
                             codex_base(PTHREAD_CAUSA_SUI){}
    };

    struct resource_exhaustion: std::runtime_error,
                                codex_base
    {
        resource_exhaustion(): std::runtime_error("resource exhaustion"),
                               codex_base(RESOURCE_EXHAUSTION){}
    };

    struct timeout: std::runtime_error,
                    codex_base
    {
        timeout(): std::runtime_error("timeout"),
                   codex_base(TIMEOUT){}
    };

    struct invalid_format: std::invalid_argument,
                           codex_base
    {
        invalid_format(): std::invalid_argument("invalid format"),
                          codex_base(INVALID_FORMAT){}
    };

    struct bad_polymorphic_access: std::invalid_argument,
                                   codex_base
    {
        bad_polymorphic_access(): std::invalid_argument("bad polymorphic access"),
                                  codex_base(BAD_POLYMORPHIC_ACCESS){}
    };

    struct socket_bad_ip: std::invalid_argument,
                          codex_base
    {
        socket_bad_ip(): std::invalid_argument("socket bad ip"),
                         codex_base(SOCKET_BAD_IP){}
    };

    struct socket_corrupted_packet: std::runtime_error,
                                    codex_base
    {
        socket_corrupted_packet(): std::runtime_error("socket corrupted packet"),
                                   codex_base(SOCKET_CORRUPTED_PACKET){}
    };

    struct socket_malformed_packet: std::runtime_error,
                                    codex_base
    {
        socket_malformed_packet(): std::runtime_error("socket malformed packet"),
                                   codex_base(SOCKET_MALFORMED_PACKET){}
    };

    struct socket_acked_enqueue: std::runtime_error,
                                 codex_base
    {
        socket_acked_enqueue(): std::runtime_error("socket acked enqueue"),
                                codex_base(SOCKET_ACKED_ENQUEUE){}
    };

    struct socket_bad_receipient: std::invalid_argument,
                                  codex_base
    {
        socket_bad_receipient(): std::invalid_argument("socket bad receipient"),
                                 codex_base(SOCKET_BAD_RECEIPIENT){}
    };

    struct socket_bad_traffic: std::runtime_error,
                               codex_base
    {
        socket_bad_traffic(): std::runtime_error("socket bad traffic"),
                              codex_base(SOCKET_BAD_TRAFFIC){}
    };

    struct socket_bad_ip_rule: std::invalid_argument,
                               codex_base
    {
        socket_bad_ip_rule(): std::invalid_argument("socket bad ip rule"),
                              codex_base(SOCKET_BAD_IP_RULE){}
    };

    struct socket_bad_buffer_length: std::invalid_argument,
                                     codex_base
    {
        socket_bad_buffer_length(): std::invalid_argument("socket bad buffer length"),
                                    codex_base(SOCKET_BAD_BUFFER_LENGTH){}
    };

    struct socket_max_retransmission_reached: std::runtime_error,
                                              codex_base
    {
        socket_max_retransmission_reached(): std::runtime_error("socket max retransmission reached"),
                                             codex_base(SOCKET_MAX_RETRANSMISSION_REACHED){}
    };

    struct socket_queue_full: std::runtime_error,
                              codex_base
    {
        socket_queue_full(): std::runtime_error("socket queue full"),
                             codex_base(SOCKET_QUEUE_FULL){}
    };

    struct socket_stream_bad_segment: std::invalid_argument,
                                      codex_base
    {
        socket_stream_bad_segment(): std::invalid_argument("socket stream bad segment"),
                                     codex_base(SOCKET_STREAM_BAD_SEGMENT){}
    };

    struct socket_stream_duplicated_segment: std::runtime_error,
                                             codex_base
    {
        socket_stream_duplicated_segment(): std::runtime_error("socket stream duplicated segment"),
                                            codex_base(SOCKET_STREAM_DUPLICATED_SEGMENT){}
    };

    struct socket_stream_timeout: std::runtime_error,
                                  codex_base
    {
        socket_stream_timeout(): std::runtime_error("socket stream timeout"),
                                 codex_base(SOCKET_STREAM_TIMEOUT){}
    };

    struct socket_stream_blacklisted: std::runtime_error,
                                      codex_base
    {
        socket_stream_blacklisted(): std::runtime_error("socket stream blacklisted"),
                                     codex_base(SOCKET_STREAM_BLACKLISTED){}
    };

    struct socket_stream_bad_buffer_length: std::invalid_argument,
                                            codex_base
    {
        socket_stream_bad_buffer_length(): std::invalid_argument("socket stream bad buffer length"),
                                           codex_base(SOCKET_STREAM_BAD_BUFFER_LENGTH){}
    };

    struct socket_stream_bad_segment_size: std::invalid_argument,
                                           codex_base
    {
        socket_stream_bad_segment_size(): std::invalid_argument("socket stream bad segment size"),
                                          codex_base(SOCKET_STREAM_BAD_SEGMENT_SIZE){}
    };

    struct socket_stream_segment_filling: std::exception,
                                          codex_base
    {
        socket_stream_segment_filling(): std::exception(), codex_base(SOCKET_STREAM_SEGMENT_FILLING){}

        virtual const char * what() const noexcept(noexcept(std::declval<const std::exception&>().what()))
        {
            return "socket stream segment filling";
        }
    };

    struct socket_stream_leak: std::runtime_error,
                               codex_base
    {
        socket_stream_leak(): std::runtime_error("socket stream leak"),
                              codex_base(SOCKET_STREAM_LEAK){}
    };

    struct socket_stream_bad_outbound_rule: std::invalid_argument,
                                            codex_base
    {
        socket_stream_bad_outbound_rule(): std::invalid_argument("socket stream bad outbound rule"),
                                           codex_base(SOCKET_STREAM_BAD_OUTBOUND_RULE){}
    };

    struct socket_stream_corrupted_packet: std::runtime_error,
                                           codex_base
    {
        socket_stream_corrupted_packet(): std::runtime_error("socket stream corrupted packet"),
                                          codex_base(SOCKET_STREAM_CORRUPTED_PACKET){}
    };

    struct socket_channel_max_msg_size_reached: std::invalid_argument,
                                                codex_base
    {
        socket_channel_max_msg_size_reached(): std::invalid_argument("socket channel max message size reached"),
                                               codex_base(SOCKET_CHANNEL_MAX_MSG_SIZE_REACHED){}
    };

    struct duplicated_entry: std::invalid_argument,
                             codex_base
    {
        duplicated_entry(): std::invalid_argument("duplicated entry"),
                            codex_base(DUPLICATED_ENTRY){}
    };

    struct expected_not_initialized: std::runtime_error,
                                     codex_base
    {
        expected_not_initialized(): std::runtime_error("expected not initialized"),
                                    codex_base(EXPECTED_NOT_INITIALIZED){}
    };

    struct variant_vbe: std::runtime_error,
                        codex_base
    {
        variant_vbe(): std::runtime_error("variant vbe"),
                       codex_base(VARIANT_VBE){}
    };

    struct queue_full: std::runtime_error,
                       codex_base
    {
        queue_full(): std::runtime_error("queue full"),
                      codex_base(QUEUE_FULL){}
    };

    struct bad_operation: std::invalid_argument,
                          codex_base
    {
        bad_operation(): std::invalid_argument("bad operation"),
                         codex_base(BAD_OPERATION){}
    };

    struct index_out_of_range: std::invalid_argument,
                               codex_base
    {
        index_out_of_range(): std::invalid_argument("index out of range"),
                              codex_base(INDEX_OUT_OF_RANGE){}
    };

    struct out_of_bound_access: std::invalid_argument,
                                codex_base
    {
        out_of_bound_access(): std::invalid_argument("out of bound access"),
                               codex_base(OUT_OF_BOUND_ACCESS){}
    };

    struct bad_state: std::runtime_error,
                      codex_base
    {
        bad_state(): std::runtime_error("bad state"),
                     codex_base(BAD_STATE){}
    };

    struct rest_cache_max_response_size_reached: std::invalid_argument,
                                                 codex_base
    {
        rest_cache_max_response_size_reached(): std::invalid_argument("rest cache max response size reached"),
                                                codex_base(REST_CACHE_MAX_RESPONSE_SIZE_REACHED){}
    };

    struct rest_cache_population_limit_reached: std::runtime_error,
                                                codex_base
    {
        rest_cache_population_limit_reached(): std::runtime_error("rest cache max population limit reached"),
                                               codex_base(REST_CACHE_POPULATION_LIMIT_REACHED){}
    };

    struct rest_clientside_timeout: std::runtime_error,
                                    codex_base
    {
        rest_clientside_timeout(): std::runtime_error("rest clientside timeout"),
                                   codex_base(REST_CLIENTSIDE_TIMEOUT){}
    };

    struct rest_serverside_abstimeout_timeout: std::runtime_error,
                                               codex_base
    {
        rest_serverside_abstimeout_timeout(): std::runtime_error("rest serverside abstimeout timeout"),
                                              codex_base(REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT){}
    };

    struct rest_invalid_url: std::invalid_argument,
                             codex_base
    {
        rest_invalid_url(): std::invalid_argument("rest invalid url"),
                            codex_base(REST_INVALID_URL){}
    };

    struct rest_invalid_argument: std::invalid_argument,
                                  codex_base
    {
        rest_invalid_argument(): std::invalid_argument("rest invalid argument"),
                                 codex_base(REST_INVALID_ARGUMENT){}
    };

    struct rest_bad_cache_unique_write: std::runtime_error,
                                        codex_base
    {
        rest_bad_cache_unique_write(): std::runtime_error("rest bad cache unique write"),
                                       codex_base(REST_BAD_CACHE_UNIQUE_WRITE){}
    };

    struct rest_internal_server_error: std::runtime_error,
                                       codex_base
    {
        rest_internal_server_error(): std::runtime_error("rest internal server error"),
                                      codex_base(REST_INTERNAL_SERVER_ERROR){}
    };

    struct rest_response_double_invoke: std::invalid_argument,
                                        codex_base
    {
        rest_response_double_invoke(): std::invalid_argument("rest response double invoke"),
                                       codex_base(REST_RESPONSE_DOUBLE_INVOKE){}
    };

    struct rest_other_error: std::runtime_error,
                             codex_base
    {
        rest_other_error(): std::runtime_error("rest other error"),
                            codex_base(REST_OTHER_ERROR){}
    };

    struct rest_ticket_not_found: std::runtime_error,
                                  codex_base
    {
        rest_ticket_not_found(): std::runtime_error("rest ticket not found"),
                                 codex_base(REST_TICKET_NOT_FOUND){}
    };

    struct rest_ticket_observer_not_found: std::runtime_error,
                                           codex_base
    {
        rest_ticket_observer_not_found(): std::runtime_error("rest ticket observer not found"),
                                          codex_base(REST_TICKET_OBSERVER_NOT_FOUND){}
    };

    struct rest_lost_response: std::runtime_error,
                               codex_base
    {
        rest_lost_response(): std::runtime_error("rest lost response"),
                              codex_base(REST_LOST_RESPONSE){}
    };

    struct rest_invalid_timeout: std::invalid_argument,
                                 codex_base
    {
        rest_invalid_timeout(): std::invalid_argument("rest invalid timeout"),
                                codex_base(REST_INVALID_TIMEOUT){}
    };

    struct rest_max_consume_size_exceeded: std::invalid_argument,
                                           codex_base
    {
        rest_max_consume_size_exceeded(): std::invalid_argument("rest max consume size exceeded"),
                                          codex_base(REST_MAX_CONSUME_SIZE_EXCEEDED){}
    };

    struct rest_mismatched_serialization_method: std::invalid_argument,
                                                 codex_base
    {
        rest_mismatched_serialization_method(): std::invalid_argument("rest mismatched serialization method"),
                                                codex_base(REST_MISMATCHED_SERIALIZATION_METHOD){}
    };

    struct poisoned_container: std::runtime_error,
                               codex_base
    {
        poisoned_container(): std::runtime_error("poisoned container"),
                              codex_base(POISONED_CONTAINER){}
    };

    struct operation_canceled_error: std::runtime_error,
                                     codex_base
    {
        operation_canceled_error(): std::runtime_error("operation canceled error"),
                                    codex_base(OPERATION_CANCELED_ERROR){}
    };

    struct operation_graceful_termination_error: std::runtime_error,
                                                 codex_base
    {
        operation_graceful_termination_error(): std::runtime_error("operation graceful termination error"),
                                                codex_base(OPERATION_GRACEFUL_TERMINATION_ERROR){}
    };

    template <class T>
    class stack_embedded_polymorphic_exception: public virtual polymorphic_exception_interface
    {
        public:

            void throw_me()
            {
                throw stack_embedded_error<T>{};
            }

            auto what() const noexcept -> const char *
            {
                static T tmp{};
                static_assert(noexcept(tmp.what()));

                return tmp.what();
            }
    };

    template <class T>
    class polymorphic_exception: public virtual polymorphic_exception_interface
    {
        public:

            void throw_me()
            {
                throw T{};
            }

            auto what() const noexcept -> const char *
            {
                static T tmp{};
                static_assert(noexcept(tmp.what()));

                return tmp.what();
            }
    };

    template <class T>
    using fancy_polymorphic_exception = polymorphic_exception<T>;

    static inline const std::unordered_map<exception_t, std::unique_ptr<polymorphic_exception_interface>> polymorphic_cpp_exception_table = []
    {
        std::unordered_map<exception_t, std::unique_ptr<polymorphic_exception_interface>> result{};

        result.insert(std::make_pair(SEGFAULT, std::make_unique<fancy_polymorphic_exception<segfault>>()));
        result.insert(std::make_pair(INTERNAL_CORRUPTION, std::make_unique<fancy_polymorphic_exception<internal_corruption>>()));
        result.insert(std::make_pair(OUT_OF_MEMORY, std::make_unique<fancy_polymorphic_exception<out_of_memory>>()));
        result.insert(std::make_pair(INVALID_SERIALIZATION_FORMAT, std::make_unique<fancy_polymorphic_exception<invalid_serialization_format>>()));
        result.insert(std::make_pair(INVALID_DICTIONARY_KEY, std::make_unique<fancy_polymorphic_exception<invalid_dictionary_key>>()));
        result.insert(std::make_pair(BAD_ACCESS, std::make_unique<fancy_polymorphic_exception<bad_access>>()));
        result.insert(std::make_pair(BAD_ALIGNMENT, std::make_unique<fancy_polymorphic_exception<bad_alignment>>()));
        result.insert(std::make_pair(RUNTIME_SOCKETIO_ERROR, std::make_unique<fancy_polymorphic_exception<runtime_socketio_error>>()));
        result.insert(std::make_pair(BUFFER_OVERFLOW, std::make_unique<fancy_polymorphic_exception<buffer_overflow>>()));
        result.insert(std::make_pair(LOST_RETRANSMISSION, std::make_unique<fancy_polymorphic_exception<lost_retransmission>>()));
        result.insert(std::make_pair(UNSUPPORTED_DAEMON_KIND, std::make_unique<fancy_polymorphic_exception<unsupported_daemon_kind>>()));
        result.insert(std::make_pair(NO_DAEMON_RUNNER_AVAILABLE, std::make_unique<fancy_polymorphic_exception<no_daemon_runner_available>>()));
        result.insert(std::make_pair(INVALID_ARGUMENT, std::make_unique<fancy_polymorphic_exception<dg_invalid_argument>>()));
        result.insert(std::make_pair(UNIDENTIFIED_ERROR, std::make_unique<fancy_polymorphic_exception<unidentified_error>>()));
        result.insert(std::make_pair(PTHREAD_EFAULT, std::make_unique<fancy_polymorphic_exception<pthread_efault>>()));
        result.insert(std::make_pair(PTHREAD_EINVAL, std::make_unique<fancy_polymorphic_exception<pthread_einval>>()));
        result.insert(std::make_pair(PTHREAD_ESRCH, std::make_unique<fancy_polymorphic_exception<pthread_esrch>>()));
        result.insert(std::make_pair(PTHREAD_CAUSA_SUI, std::make_unique<fancy_polymorphic_exception<pthread_causa_sui>>()));
        result.insert(std::make_pair(RESOURCE_EXHAUSTION, std::make_unique<fancy_polymorphic_exception<resource_exhaustion>>()));
        result.insert(std::make_pair(TIMEOUT, std::make_unique<fancy_polymorphic_exception<timeout>>()));
        result.insert(std::make_pair(INVALID_FORMAT, std::make_unique<fancy_polymorphic_exception<invalid_format>>()));
        result.insert(std::make_pair(BAD_POLYMORPHIC_ACCESS, std::make_unique<fancy_polymorphic_exception<bad_polymorphic_access>>()));
        result.insert(std::make_pair(SOCKET_BAD_IP, std::make_unique<fancy_polymorphic_exception<socket_bad_ip>>()));
        result.insert(std::make_pair(SOCKET_CORRUPTED_PACKET, std::make_unique<fancy_polymorphic_exception<socket_corrupted_packet>>()));
        result.insert(std::make_pair(SOCKET_MALFORMED_PACKET, std::make_unique<fancy_polymorphic_exception<socket_malformed_packet>>()));
        result.insert(std::make_pair(SOCKET_ACKED_ENQUEUE, std::make_unique<fancy_polymorphic_exception<socket_acked_enqueue>>()));
        result.insert(std::make_pair(SOCKET_BAD_RECEIPIENT, std::make_unique<fancy_polymorphic_exception<socket_bad_receipient>>()));
        result.insert(std::make_pair(SOCKET_BAD_TRAFFIC, std::make_unique<fancy_polymorphic_exception<socket_bad_traffic>>()));
        result.insert(std::make_pair(SOCKET_BAD_IP_RULE, std::make_unique<fancy_polymorphic_exception<socket_bad_ip_rule>>()));
        result.insert(std::make_pair(SOCKET_BAD_BUFFER_LENGTH, std::make_unique<fancy_polymorphic_exception<socket_bad_buffer_length>>()));
        result.insert(std::make_pair(SOCKET_MAX_RETRANSMISSION_REACHED, std::make_unique<fancy_polymorphic_exception<socket_max_retransmission_reached>>()));
        result.insert(std::make_pair(SOCKET_QUEUE_FULL, std::make_unique<fancy_polymorphic_exception<socket_queue_full>>()));
        result.insert(std::make_pair(SOCKET_STREAM_BAD_SEGMENT, std::make_unique<fancy_polymorphic_exception<socket_stream_bad_segment>>()));
        result.insert(std::make_pair(SOCKET_STREAM_DUPLICATED_SEGMENT, std::make_unique<fancy_polymorphic_exception<socket_stream_duplicated_segment>>()));
        result.insert(std::make_pair(SOCKET_STREAM_TIMEOUT, std::make_unique<fancy_polymorphic_exception<socket_stream_timeout>>()));
        result.insert(std::make_pair(SOCKET_STREAM_BLACKLISTED, std::make_unique<fancy_polymorphic_exception<socket_stream_blacklisted>>()));
        result.insert(std::make_pair(SOCKET_STREAM_BAD_BUFFER_LENGTH, std::make_unique<fancy_polymorphic_exception<socket_stream_bad_buffer_length>>()));
        result.insert(std::make_pair(SOCKET_STREAM_BAD_SEGMENT_SIZE, std::make_unique<fancy_polymorphic_exception<socket_stream_bad_segment_size>>()));
        result.insert(std::make_pair(SOCKET_STREAM_SEGMENT_FILLING, std::make_unique<fancy_polymorphic_exception<socket_stream_segment_filling>>()));
        result.insert(std::make_pair(SOCKET_STREAM_LEAK, std::make_unique<fancy_polymorphic_exception<socket_stream_leak>>()));
        result.insert(std::make_pair(SOCKET_STREAM_BAD_OUTBOUND_RULE, std::make_unique<fancy_polymorphic_exception<socket_stream_bad_outbound_rule>>()));
        result.insert(std::make_pair(SOCKET_STREAM_CORRUPTED_PACKET, std::make_unique<fancy_polymorphic_exception<socket_stream_corrupted_packet>>()));
        result.insert(std::make_pair(SOCKET_CHANNEL_MAX_MSG_SIZE_REACHED, std::make_unique<fancy_polymorphic_exception<socket_channel_max_msg_size_reached>>()));
        result.insert(std::make_pair(DUPLICATED_ENTRY, std::make_unique<fancy_polymorphic_exception<duplicated_entry>>()));
        result.insert(std::make_pair(EXPECTED_NOT_INITIALIZED, std::make_unique<fancy_polymorphic_exception<expected_not_initialized>>()));
        result.insert(std::make_pair(VARIANT_VBE, std::make_unique<fancy_polymorphic_exception<variant_vbe>>()));
        result.insert(std::make_pair(QUEUE_FULL, std::make_unique<fancy_polymorphic_exception<queue_full>>()));
        result.insert(std::make_pair(BAD_OPERATION, std::make_unique<fancy_polymorphic_exception<bad_operation>>()));
        result.insert(std::make_pair(INDEX_OUT_OF_RANGE, std::make_unique<fancy_polymorphic_exception<index_out_of_range>>()));
        result.insert(std::make_pair(OUT_OF_BOUND_ACCESS, std::make_unique<fancy_polymorphic_exception<out_of_bound_access>>()));
        result.insert(std::make_pair(BAD_STATE, std::make_unique<fancy_polymorphic_exception<bad_state>>()));
        result.insert(std::make_pair(REST_CACHE_MAX_RESPONSE_SIZE_REACHED, std::make_unique<fancy_polymorphic_exception<rest_cache_max_response_size_reached>>()));
        result.insert(std::make_pair(REST_CACHE_POPULATION_LIMIT_REACHED, std::make_unique<fancy_polymorphic_exception<rest_cache_population_limit_reached>>()));
        result.insert(std::make_pair(REST_CLIENTSIDE_TIMEOUT, std::make_unique<fancy_polymorphic_exception<rest_clientside_timeout>>()));
        result.insert(std::make_pair(REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT, std::make_unique<fancy_polymorphic_exception<rest_serverside_abstimeout_timeout>>()));
        result.insert(std::make_pair(REST_INVALID_URL, std::make_unique<fancy_polymorphic_exception<rest_invalid_url>>()));
        result.insert(std::make_pair(REST_INVALID_ARGUMENT, std::make_unique<fancy_polymorphic_exception<rest_invalid_argument>>()));
        result.insert(std::make_pair(REST_BAD_CACHE_UNIQUE_WRITE, std::make_unique<fancy_polymorphic_exception<rest_bad_cache_unique_write>>()));
        result.insert(std::make_pair(REST_INTERNAL_SERVER_ERROR, std::make_unique<fancy_polymorphic_exception<rest_internal_server_error>>()));
        result.insert(std::make_pair(REST_RESPONSE_DOUBLE_INVOKE, std::make_unique<fancy_polymorphic_exception<rest_response_double_invoke>>()));
        result.insert(std::make_pair(REST_OTHER_ERROR, std::make_unique<fancy_polymorphic_exception<rest_other_error>>()));
        result.insert(std::make_pair(REST_TICKET_NOT_FOUND, std::make_unique<fancy_polymorphic_exception<rest_ticket_not_found>>()));
        result.insert(std::make_pair(REST_TICKET_OBSERVER_NOT_FOUND, std::make_unique<fancy_polymorphic_exception<rest_ticket_observer_not_found>>()));
        result.insert(std::make_pair(REST_LOST_RESPONSE, std::make_unique<fancy_polymorphic_exception<rest_lost_response>>()));
        result.insert(std::make_pair(REST_INVALID_TIMEOUT, std::make_unique<fancy_polymorphic_exception<rest_invalid_timeout>>()));
        result.insert(std::make_pair(REST_MAX_CONSUME_SIZE_EXCEEDED, std::make_unique<fancy_polymorphic_exception<rest_max_consume_size_exceeded>>()));
        result.insert(std::make_pair(REST_MISMATCHED_SERIALIZATION_METHOD, std::make_unique<fancy_polymorphic_exception<rest_mismatched_serialization_method>>()));
        result.insert(std::make_pair(POISONED_CONTAINER, std::make_unique<fancy_polymorphic_exception<poisoned_container>>()));
        result.insert(std::make_pair(OPERATION_CANCELED_ERROR, std::make_unique<fancy_polymorphic_exception<operation_canceled_error>>()));
        result.insert(std::make_pair(OPERATION_GRACEFUL_TERMINATION_ERROR, std::make_unique<fancy_polymorphic_exception<operation_graceful_termination_error>>()));

        return result;
    }();

    inline auto wrap_kernel_error(kernel_exception_t) noexcept -> exception_t
    {
        return UNIDENTIFIED_ERROR;
    }

    __attribute__((noinline)) auto wrap_std_exception(std::exception_ptr exception_ptr) -> exception_t
    {
        if (exception_ptr == nullptr)
        {
            return SUCCESS;
        }

        try
        {
            try
            {
                std::rethrow_exception(exception_ptr);
            }
            catch (codex_base& e)
            {
                return e.codex;
            }
        }
        catch (...)
        {
            return UNIDENTIFIED_ERROR;
        }

        return UNIDENTIFIED_ERROR;
    }

    inline auto is_success(exception_t err) noexcept -> bool
    {
        return err == SUCCESS;
    }

    inline auto is_failed(exception_t err) noexcept -> bool
    {
        return err != SUCCESS;
    }

    __attribute__((noinline)) auto verbose(exception_t err) noexcept -> const char *
    {
        if (is_success(err))
        {
            return "";
        }

        auto map_ptr = polymorphic_cpp_exception_table.find(err);

        if (map_ptr == polymorphic_cpp_exception_table.end())
        {
            map_ptr = polymorphic_cpp_exception_table.find(UNIDENTIFIED_ERROR);
        }

        return map_ptr->second->what();
    }

    __attribute__((noinline)) void throw_exception(exception_t err)
    {    
        if (is_success(err))
        {
            return;
        }

        auto map_ptr = polymorphic_cpp_exception_table.find(err);

        if (map_ptr == polymorphic_cpp_exception_table.end())
        {
            map_ptr = polymorphic_cpp_exception_table.find(UNIDENTIFIED_ERROR);
        }

        map_ptr->second->throw_me();
    }

    [[noreturn]] void throw_valid_exception(exception_t err)
    {
        throw_exception(err);
        std::abort();
    }

    template <class Functor>
    inline auto to_cstyle_function(Functor functor) noexcept
    {
        static_assert(std::is_nothrow_move_constructible_v<Functor>);

        auto rs = [f = std::move(functor)]<class ...Args>(Args&& ...args) noexcept(noexcept(functor(std::forward<Args>(args)...)))
        {
            using ret_t = decltype(f(std::forward<Args>(args)...));

            if constexpr(std::is_same_v<ret_t, void>)
            {
                try
                {
                    f(std::forward<Args>(args)...);
                    return SUCCESS;
                }
                catch (...)
                {
                    return wrap_std_exception(std::current_exception());
                }
            }
            else
            {
                try
                {
                    static_assert(std::is_nothrow_move_constructible_v<ret_t>);
                    static_assert(std::is_same_v<ret_t, std::decay_t<ret_t>>);

                    return std::expected<ret_t, exception_t>(f(std::forward<Args>(args)...));
                }
                catch (...)
                {
                    return std::expected<ret_t, exception_t>(std::unexpected(wrap_std_exception(std::current_exception())));
                }
            }
        };

        return rs;
    } 

    template <class T>
    inline auto remove_expected(std::expected<T, exception_t> inp) noexcept -> T
    {    
        static_assert(std::is_nothrow_move_constructible_v<T>);

        if (!inp.has_value())
        {
            std::abort();
        }

        return std::move(inp.value());
    } 

    inline void dg_nothrow(exception_t err) noexcept
    {
        if (common_exception::is_failed(err))
        {
            std::abort();
        }
    } 

    template <class T, class ...Args>
    inline auto cstyle_initialize(Args&& ...args) noexcept -> std::expected<T, exception_t>
    {
        if constexpr(std::is_nothrow_constructible_v<std::expected<T, exception_t>, std::in_place_t, Args&&...>)
        {
            return std::expected<T, exception_t>(std::in_place_t{}, std::forward<Args>(args)...);
        }
        else
        {
            try
            {
                return std::expected<T, exception_t>(std::in_place_t{}, std::forward<Args>(args)...);
            }
            catch (...)
            {
                return std::unexpected(wrap_std_exception(std::current_exception()));
            }
        }
    }

    template <class ...Args, std::enable_if_t<std::conjunction_v<std::is_same<Args, exception_t>...>, bool> = true>
    inline auto disjunction(Args... args) noexcept -> exception_t
    {    
        exception_t rs = SUCCESS; 

        ([&]{
            if (is_failed(args))
            {
                rs = args;
            }
        }(), ...);

        return rs;
    }

    template <class T, class T1>
    class expected_result_if_object
    {    
        private:

            bool evaluator;
            T true_ret;
            T1 false_ret; 

        public:

            constexpr expected_result_if_object(bool evaluator, T true_ret, T1 false_ret) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<T1>): evaluator(evaluator),
                                                                                                                                                                                         true_ret(std::move(true_ret)),
                                                                                                                                                                                         false_ret(std::move(false_ret)){}
            
            template <class ...Args>
            constexpr operator std::expected<Args...>() noexcept(std::is_nothrow_constructible_v<std::expected<Args...>, T&&> && std::is_nothrow_constructible_v<std::expected<Args...>, T1&&>)
            {
                if (this->evaluator)
                {
                    return std::expected<Args...>(std::move(this->true_ret));
                }
                else
                {
                    return std::expected<Args...>(std::move(this->false_ret));
                }
            }
    };

    template <class T, class T1>
    constexpr auto expected_result_if(bool evaluator, T true_ret, T1 false_ret) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_constructible_v<T1>) -> expected_result_if_object<T, T1>
    {
        return expected_result_if_object<T, T1>(evaluator, std::move(true_ret), std::move(false_ret));
    }
}

#endif