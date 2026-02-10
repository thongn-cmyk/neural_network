#ifndef __DG_SENDERLESS_MAILBOX_H__
#define __DG_SENDERLESS_MAILBOX_H__

//define HEADER_CONTROL 9

#include <stdint.h>
#include <vector>
#include <chrono>
#include <optional>
#include <memory>
#include <string>
#include "network_compact_serializer.h"
#include "network_compact_trivial_serializer.h"
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <deque>
#include <sched.h>
#include <pthread.h>
#include <math.h>
#include "assert.h"
#include "network_std_container.h"
#include <array>
#include "network_log.h"
#include "network_exception.h"
#include "network_concurrency_x.h"
#include "stdx.h"
#include <chrono>
#include <array>
#include "network_randomizer.h"
#include "network_exception_handler.h"
#include "network_producer_consumer.h"
#include "network_stack_allocation.h"
#include <errno.h>
#include <error.h>
#include <linux/filter.h>
#include <linux/in.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "network_hash.h"
#include "network_chrono.h"
#include "network_sock_traffic_status_controller.h"
#include "network_kernel_allocator_singleton.h"
#include "network_kernel_buffer.h"

namespace dg::network_kernel_mailbox_impl1::types{

    static_assert(sizeof(size_t) >= sizeof(uint32_t));

    using factory_id_t          = std::array<char, 20>;
    using local_packet_id_t     = uint64_t;
    using packet_polymorphic_t  = uint8_t;
}

namespace dg::network_kernel_mailbox_impl1::allocation{
    
    using Config = dg::network_kernel_allocator_singleton::Config; 

    struct Signature{};
    struct Signature1{};
    struct Signature2{}; 

    using AllocatorInstance                 = dg::network_kernel_allocator_singleton::AllocatorSingletonInstance<Signature>; 
    using RetransmissionAllocatorInstance   = dg::network_kernel_allocator_singleton::AllocatorSingletonInstance<Signature1>; 
    using CrucialAllocatorInstance          = dg::network_kernel_allocator_singleton::AllocatorInstance<Signature2>;
    using PolymorphicAllocator              = dg::network_kernel_allocator_singleton::SingletonPolymorphicAllocator<AllocatorInstance, false>;

    void init(Config config,
              std::optional<Config> retransmission_config = std::nullopt,
              std::optional<Config> crucial_config = std::nullopt)
    {
        if (!retransmission_config.has_value())
        {
            retransmission_config = config;
        }

        if (!crucial_config.has_value())
        {
            crucial_config = config;
        }

        AllocatorInstance::init(config);
        RetransmissionAllocatorInstance::init(retransmission_config.value());
        CrucialAllocatorInstance::init(crucial_config.value());
    }

    void deinit() noexcept
    {
        AllocatorInstance::deinit();
        RetransmissionAllocatorInstance::deinit();
        CrucialAllocatorInstance::deinit();
    }

    template <class T, class ...Args>
    auto urgent_make_shared(Args&& ...args) -> std::shared_ptr<T>
    {
        return std::allocate_shared<T>(dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, CrucialAllocatorInstance>{}, std::forward<Args>(args)...);
    }

    using internal_kernel_buffer = dg::network_kernel_buffer::polymorphic_kernel_string<PolymorphicAllocator>;

    template <class T>
    using internal_vector = std::vector<T, dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, PolymorphicAllocator>>; 
}

namespace dg::network_kernel_mailbox_impl1::types{

    using internal_kernel_buffer = dg::network_kernel_mailbox_impl1::allocation::internal_kernel_buffer;

    template <class T>
    using internal_vector = dg::network_kernel_mailbox_impl1::allocation::internal_vector<T>;
}

namespace dg::network_kernel_mailbox_impl1::model{

    using namespace dg::network_kernel_mailbox_impl1::types;

    struct SocketHandle{
        int kernel_sock_fd;
        int sin_fam;
        int comm;
        int protocol;
        std::unique_ptr<stdx::fair_atomic_flag> mtx;
    };

    struct IPv4{
        std::array<char, 4u> ip_buf;

        auto data() const noexcept -> const char *{

            return this->ip_buf.data();
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(ip_buf);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(ip_buf);
        }
    };

    struct IPv6{
        std::array<char, 16u> ip_buf;

        auto data() const noexcept -> const char *{

            return this->ip_buf.data();
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(ip_buf);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(ip_buf);
        }
    };

    struct IP{
        std::variant<IPv4, IPv6> ip;

        auto data() const noexcept -> const char *{

            if (std::holds_alternative<IPv4>(this->ip)){
                return std::get<IPv4>(this->ip).data();
            } else if (std::holds_alternative<IPv6>(this->ip)){
                return std::get<IPv6>(this->ip).data();
            } else{
                if constexpr(DEBUG_MODE_FLAG){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                } else{
                    std::unreachable();
                }
            }
        }

        auto sin_fam() const noexcept{

            if (std::holds_alternative<IPv4>(this->ip)){
                return AF_INET;
            } else if (std::holds_alternative<IPv6>(this->ip)){
                return AF_INET6;
            } else{
                if constexpr(DEBUG_MODE_FLAG){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                } else{
                    std::unreachable();
                }
            }
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(ip);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(ip);
        }
    };

    struct Address{
        IP ip;
        uint16_t port;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(ip, port);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(ip, port);
        }
    };

    struct GlobalPacketIdentifier{
        local_packet_id_t local_packet_id;
        factory_id_t factory_id;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(local_packet_id, factory_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(local_packet_id, factory_id);
        }
    };

    struct KindGlobalPacketIdentifer{
        GlobalPacketIdentifier id;
        uint8_t kind;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(id, kind);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(id, kind);
        }
    };

    using global_packet_id_t = GlobalPacketIdentifier;
    using kind_global_packet_id_t = KindGlobalPacketIdentifer; 

    struct PacketBase{
        global_packet_id_t id;
        uint8_t retransmission_count;
        uint8_t priority;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(id, retransmission_count, priority);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(id, retransmission_count, priority);
        }
    };

    struct PacketHeader: PacketBase{
        Address fr_addr;
        Address to_addr;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(static_cast<const PacketBase&>(*this), fr_addr, to_addr);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(static_cast<PacketBase&>(*this), fr_addr, to_addr);
        }
    };

    struct XOnlyRequestPacket{
        internal_kernel_buffer content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(content);
        }
    };

    struct RequestPacket: PacketHeader, XOnlyRequestPacket{

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(static_cast<const PacketHeader&>(*this), static_cast<const XOnlyRequestPacket&>(*this));
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(static_cast<PacketHeader&>(*this), static_cast<XOnlyRequestPacket&>(*this));
        }
    };

    struct XOnlyAckPacket{
        internal_vector<PacketBase> ack_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(dg::network_compact_serializer::wrap_container<uint16_t>(ack_vec));
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(dg::network_compact_serializer::wrap_container<uint16_t>(ack_vec));
        }
    };

    struct AckPacket: PacketHeader, XOnlyAckPacket{

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(static_cast<const PacketHeader&>(*this), static_cast<const XOnlyAckPacket&>(*this));
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(static_cast<PacketHeader&>(*this), static_cast<XOnlyAckPacket&>(*this));
        }
    };

    struct XOnlyKRescuePacket{

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            (void) reflector;
        }
    };

    struct KRescuePacket: PacketHeader, XOnlyKRescuePacket{

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(static_cast<const PacketHeader&>(*this), static_cast<const XOnlyKRescuePacket&>(*this));
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(static_cast<PacketHeader&>(*this), static_cast<XOnlyKRescuePacket&>(*this));
        }
    };

    struct Packet: PacketHeader{
        std::variant<XOnlyKRescuePacket, XOnlyRequestPacket, XOnlyAckPacket> xonly_content;
    };

    struct ScheduledPacket{
        Packet pkt;
        std::chrono::time_point<std::chrono::utc_clock> sched_time;
    };

    struct QueuedPacket{
        Packet pkt;
        std::chrono::time_point<std::chrono::steady_clock> queued_time;
    };

    struct MailBoxArgument{
        Address to;
        const void * content;
        size_t content_sz;
    };

    struct RetransmissionWaitingItem
    {
        Packet ** pkt_arr;
        exception_t ** exception_arr;
        size_t pkt_arr_sz;
        std::binary_semaphore * smp;
    };

    struct BufferContainerWaitingItem
    {
        std::move_iterator<internal_kernel_buffer *> buffer_arr;
        size_t buffer_arr_sz;
        std::binary_semaphore * smp;
    };

    struct PacketContainerWaitingItem
    {
        std::move_iterator<Packet *> packet_arr;
        size_t packet_arr_sz;
        std::binary_semaphore * smp;
    };

    struct FairBufferContainerWaitingItem
    {
        dg::vector<internal_kernel_buffer> buffer_vec;
        std::binary_semaphore * smp;
    };

    struct FairPacketContainerWaitingItem
    {
        dg::vector<Packet> packet_vec;
        std::binary_semaphore * smp;
    };
}

namespace dg::network_kernel_mailbox_impl1::constants{

    using namespace std::literals::chrono_literals;
    using namespace std::chrono;

    enum packet_kind: dg::network_kernel_mailbox_impl1::types::packet_polymorphic_t{
        ack     = 0u,
        request = 1u,
        krescue = 2u
    };

    static inline constexpr size_t MAXIMUM_MSG_SIZE                     = size_t{1} << 10;
    static inline constexpr size_t MAX_REQUEST_PACKET_CONTENT_SIZE      = size_t{1} << 9;
    static inline constexpr size_t MAX_ACK_PER_PACKET                   = size_t{1} << 2;
    static inline constexpr size_t DEFAULT_ACCUMULATION_SIZE            = size_t{1} << 8;
    static inline constexpr size_t KERNEL_BATCH_POPCOUNT                = size_t{1} << 8;
    static inline constexpr size_t DEFAULT_KEYVALUE_ACCUMULATION_SIZE   = size_t{1} << 8;

    static inline constexpr int KERNEL_NOBLOCK_TRANSMISSION_FLAG        = MSG_DONTROUTE | MSG_DONTWAIT;
    static inline constexpr bool HAS_STRICT_SOCKET_CLOSE                = false;
}

namespace dg::network_kernel_mailbox_impl1::external_interface{

    using namespace dg::network_kernel_mailbox_impl1::types;
    using namespace dg::network_kernel_mailbox_impl1::model;

    class IPSieverInterface{

        public:

            virtual ~IPSieverInterface() noexcept = default;
            virtual auto thru(Address) noexcept -> std::expected<bool, exception_t> = 0;
    };

    class NATIPControllerInterface{

        public:

            virtual ~NATIPControllerInterface() noexcept = default;

            virtual void add_inbound(Address *, size_t, exception_t *) noexcept = 0;
            virtual void add_outbound(Address *, size_t, exception_t *) noexcept = 0;

            virtual void get_inbound_friend_addr(Address *, size_t off, size_t& sz, size_t cap) noexcept = 0; 
            virtual auto get_inbound_friend_addr_iteration_size() noexcept -> size_t = 0;

            virtual void get_outbound_friend_addr(Address *, size_t off, size_t& sz, size_t cap) noexcept = 0;
            virtual auto get_outbound_friend_addr_iteration_size() noexcept -> size_t = 0;
    };

    class NetworkBusyStatusRetrieverInterface{

        public:

            using busy_level_t = uint8_t;

            static inline constexpr busy_level_t BUSY_0  = 0u;
            static inline constexpr busy_level_t BUSY_1  = 1u;
            static inline constexpr busy_level_t BUSY_2  = 2u; 

            virtual ~NetworkBusyStatusRetrieverInterface() noexcept = default;
            virtual auto get() noexcept -> std::expected<busy_level_t, exception_t> = 0;
    };
}

namespace dg::network_kernel_mailbox_impl1::packet_controller{

    using namespace dg::network_kernel_mailbox_impl1::model;

    class NetworkBusyObserverInterface{

        public:

            using busy_level_t = uint8_t;

            static inline constexpr busy_level_t BUSY_0  = 0u;
            static inline constexpr busy_level_t BUSY_1  = 1u;
            static inline constexpr busy_level_t BUSY_2  = 2u; 

            virtual ~NetworkBusyObserverInterface() noexcept = default;
            virtual void notify(busy_level_t) noexcept = 0;
    };

    class ContainerBusyAdapterInterface{

        public:

            virtual ~ContainerBusyAdapterInterface() noexcept = default;
            virtual auto set_busy() noexcept -> exception_t = 0;
            virtual auto set_ease() noexcept -> exception_t = 0; 
    };

    template <class T>
    class UnitAllocatorInterface
    {
        public:

            virtual ~UnitAllocatorInterface() noexcept = default;
            virtual auto get() noexcept -> std::expected<T, exception_t> = 0;
            virtual void free(T item) noexcept = 0;
    };

    class SchedulerInterface{

        public:

            virtual ~SchedulerInterface() noexcept = default;
            virtual auto schedule(Address) noexcept -> std::expected<std::chrono::time_point<std::chrono::utc_clock>, exception_t> = 0;
    };

    class PacketIntegrityValidatorInterface{

        public:

            virtual ~PacketIntegrityValidatorInterface() noexcept = default;
            virtual auto is_valid(const Packet&) noexcept -> exception_t = 0;
    };

    class RetransmissionDelayNegotiatorInterface{

        public:

            virtual ~RetransmissionDelayNegotiatorInterface() noexcept = default;
            virtual auto get(const Address& to_addr) noexcept -> std::expected<std::chrono::nanoseconds, exception_t> = 0;
    };

    class UpdatableInterface{

        public:

            virtual ~UpdatableInterface() noexcept = default;
            virtual void update() noexcept = 0;
    };

    class ExhaustionControllerInterface{

        public:

            virtual ~ExhaustionControllerInterface() noexcept = default;
            virtual auto is_should_wait() noexcept -> bool = 0;
            virtual auto update_waiting_size(size_t) noexcept -> exception_t = 0;
    };

    class KernelOutBoundTransmissionControllerInterface{

        public:

            virtual ~KernelOutBoundTransmissionControllerInterface() noexcept = default;
            virtual auto get_transmit_frequency() noexcept -> uint32_t = 0;
            virtual auto update_waiting_size(size_t) noexcept -> exception_t = 0;
    };

    class IDGeneratorInterface{

        public:

            virtual ~IDGeneratorInterface() noexcept = default;
            virtual auto get() noexcept -> global_packet_id_t = 0;
    };

    class RequestPacketGeneratorInterface{

        public:

            virtual ~RequestPacketGeneratorInterface() noexcept = default;
            virtual auto get(MailBoxArgument&&) noexcept -> std::expected<RequestPacket, exception_t> = 0;
    };

    class KRescuePacketGeneratorInterface{

        public:

            virtual ~KRescuePacketGeneratorInterface() noexcept = default;
            virtual auto get() noexcept -> std::expected<KRescuePacket, exception_t> = 0;
    };

    class KernelRescuePostInterface{

        public:

            virtual ~KernelRescuePostInterface() noexcept = default;
            virtual auto heartbeat() noexcept -> exception_t = 0;
            virtual auto last_heartbeat() noexcept -> std::expected<std::optional<std::chrono::time_point<std::chrono::utc_clock>>, exception_t> = 0;
            virtual void reset() noexcept = 0;
    };

    class AckPacketGeneratorInterface{

        public:

            virtual ~AckPacketGeneratorInterface() noexcept = default;
            virtual auto get(Address, PacketBase *, size_t) noexcept -> std::expected<AckPacket, exception_t> = 0;
    };

    class RetransmissionControllerInterface{

        public:

            virtual ~RetransmissionControllerInterface() noexcept = default;
            virtual void add_retriables(std::move_iterator<Packet *>, size_t, exception_t *) noexcept = 0;
            virtual void ack(global_packet_id_t *, size_t, exception_t *) noexcept = 0;

            //assume that we have a hard cap at 100, actual cap at 130, and 10 concurrent worker pulling 1, it's impossible for the container to exceed 110 at any given time, because 111 - 100 == 11 => at least 1 worker push 2 times, worker has to see his previous transaction which is a contradiction
            virtual void reentrant_add_retriables(std::move_iterator<Packet *>, size_t, exception_t *) noexcept = 0;
            virtual void get_retriables(Packet *, size_t&, size_t) noexcept = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class BufferContainerInterface{

        public:

            virtual ~BufferContainerInterface() noexcept = default;
            virtual void push(std::move_iterator<internal_kernel_buffer *>, size_t, exception_t *) noexcept = 0;
            virtual void pop(internal_kernel_buffer *, size_t&, size_t) noexcept = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class PacketContainerInterface{

        public:

            virtual ~PacketContainerInterface() noexcept = default;
            virtual void push(std::move_iterator<Packet *>, size_t, exception_t *) noexcept = 0;
            virtual void pop(Packet *, size_t&, size_t) noexcept = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class InBoundIDControllerInterface{

        public:

            virtual ~InBoundIDControllerInterface() noexcept = default;
            virtual void thru(kind_global_packet_id_t *, size_t, std::expected<bool, exception_t> *) noexcept = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class TrafficControllerInterface{

        public:

            virtual ~TrafficControllerInterface() noexcept = default;
            virtual auto thru(Address) noexcept -> std::expected<bool, exception_t> = 0;
            virtual void reset() noexcept = 0;
    };

    class BorderControllerInterface{

        public:

            virtual ~BorderControllerInterface() noexcept = default;
            virtual void thru(Address *, size_t, exception_t *) noexcept = 0;
            virtual auto max_consume_size() noexcept -> size_t = 0;
    };
}

namespace dg::network_kernel_mailbox_impl1::core{

    using namespace dg::network_kernel_mailbox_impl1::model;

    class MailboxInterface{

        public: 

            virtual ~MailboxInterface() noexcept = default;
            virtual void send(MailBoxArgument * mailbox_arr, size_t mailbox_arr_sz, exception_t * exception_arr) noexcept = 0;
            virtual void recv(std::add_pointer_t<void> * output_arr,
                              size_t * output_elemental_capacity_arr,
                              size_t * output_elemental_sz_arr,
                              size_t& output_arr_sz,
                              size_t output_arr_cap) noexcept = 0;

            virtual auto max_consume_size() noexcept -> size_t = 0;
            virtual auto max_content_size() const noexcept -> size_t = 0;
    };
}

namespace dg::network_kernel_mailbox_impl1::utility{

    using namespace dg::network_kernel_mailbox_impl1::model;

    template <class ...Args>
    auto backsplit_str(internal_kernel_buffer s, size_t sz) noexcept -> std::expected<std::pair<internal_kernel_buffer, internal_kernel_buffer>, exception_t>{

        try
        {
            size_t rhs_sz = std::min(s.size(), sz); 
            internal_kernel_buffer rhs(rhs_sz);

            for (size_t i = rhs_sz; i != 0u; --i){
                rhs[i - 1] = s.back();
                s.pop_back();
            }

            return std::make_pair(std::move(s), std::move(rhs));
        }
        catch (...)
        {
            return std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
        }
    }

    template <class T>
    static auto legacy_struct_default_init() noexcept -> T{

        static_assert(std::is_trivial_v<T>);
        T rs{}; //list initializer is important for C++, UB otherwise
        std::memset(&rs, 0, sizeof(T));

        return rs;
    }

    static auto to_factory_id(Address addr) noexcept -> factory_id_t{

        static_assert(dg::network_trivial_serializer::size(Address{}) <= dg::network_trivial_serializer::size(factory_id_t{}));
        static_assert(std::has_unique_object_representations_v<factory_id_t>);

        factory_id_t rs{};
        dg::network_trivial_serializer::serialize_into(reinterpret_cast<char *>(&rs), addr); //-> &rs - fine - this is defined according to std

        return rs;
    }

    template <class T>
    static auto reflectible_is_equal(const T& lhs, const T& rhs) noexcept -> bool{

        return dg::network_trivial_serializer::reflectible_is_equal(lhs, rhs);
    }

    template <class ...Args, class Iterator>
    static auto finite_set_insert(std::unordered_set<Args...>& container, size_t container_cap, 
                                  Iterator first, Iterator last) noexcept -> std::expected<size_t, exception_t>{

        static_assert(std::is_trivial_v<Iterator>);

        dg::network_stack_allocation::NoExceptAllocation<Iterator[]> rewind_buf(std::distance(first, last));
        size_t rewind_buf_sz = 0u; 

        try{
            for (auto it = first; it != last; ++it){
                if (container.size() == container_cap){
                    return std::distance(first, it);
                }

                auto [iptr, status] = container.insert(*it);

                if (status){
                    rewind_buf[rewind_buf_sz++] = it;
                }
            }

            return static_cast<size_t>(std::distance(first, last));
        } catch (...){
            for (size_t i = 0u; i < rewind_buf_sz; ++i){
                container.erase(*rewind_buf[i]);
            }

            return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    static auto ipv4_is_numeric_char(char c) noexcept -> bool{

        return c >= '0' && c <= '9';
    }

    static auto ipv4_strict_str_to_unsigned(const char * first, const char * last) noexcept -> size_t{

        size_t sz   = stdx::safe_integer_cast<size_t>(std::distance(first, last));
        size_t rs   = 0u;

        if constexpr(DEBUG_MODE_FLAG){
            if (sz > std::numeric_limits<size_t>::digits10){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }
        }

        for (size_t i = 0u; i < sz; ++i){
            if constexpr(DEBUG_MODE_FLAG){
                if (!ipv4_is_numeric_char(first[i])){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }

            rs *= 10;
            rs += static_cast<size_t>(first[i] - '0');
        }

        return rs;
    } 

    static auto ipv4_count_unsigned_to_str(uint8_t value) noexcept -> size_t{

        if (value == 0u){
            return 1u;
        }

        size_t counter = 0u;

        while (value != 0u){
            counter += 1;
            value /= 10;
        }

        return counter;
    } 

    static auto ipv4_unsigned_to_str(uint8_t value, char * op) noexcept -> char *{

        if (value == 0u){
            *op = '0';
            return std::next(op);                        
        }

        size_t sz                   = ipv4_count_unsigned_to_str(value);
        char * const ret_last_op    = std::next(op, sz);
        char * last_op              = ret_last_op;

        while (value != 0u){
            size_t back_digit_value = value % 10;
            char back_digit_char    = static_cast<char>('0' + back_digit_value); 
            value                   /= 10;
            *std::prev(last_op)     = back_digit_char;

            std::advance(last_op, -1);
        }

        return ret_last_op;
    } 

    static auto ipv4_std_formatted_str_to_compact(std::string_view data) noexcept -> std::expected<IPv4, exception_t>{

        constexpr size_t IPv4_MIN_PACK_SIZE = 1u;
        constexpr size_t IPv4_MAX_PACK_SIZE = 3u; 
        constexpr size_t IPv4_PACK_COUNT    = 4u;
        constexpr size_t DATA_MIN_SIZE      = 1u;
        constexpr size_t DATA_MAX_SIZE      = 15u;
        constexpr char SEMANTIC_SEPARATOR   = '.'; 

        if (std::clamp(static_cast<size_t>(data.size()), DATA_MIN_SIZE, DATA_MAX_SIZE) != data.size()){
            return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
        }

        static_assert(std::numeric_limits<uint8_t>::min() == 0u);
        static_assert(std::numeric_limits<uint8_t>::max() == 255u);

        IPv4 rs             = {};
        size_t first        = 0u;
        size_t pack_count   = 0u;

        for (size_t i = 0u; i < data.size() + 1u; ++i){
            if (i == data.size() || data[i] == SEMANTIC_SEPARATOR){
                if (pack_count == IPv4_PACK_COUNT){
                    return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
                }

                size_t last             = i; 
                size_t sz               = last - first; 

                if (std::clamp(sz, IPv4_MIN_PACK_SIZE, IPv4_MAX_PACK_SIZE) != sz){
                    return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
                }

                const char * pack_first = std::next(data.data(), first);
                const char * pack_last  = std::next(data.data(), last);
                static_assert(std::numeric_limits<size_t>::digits10 >= IPv4_MAX_PACK_SIZE);
                size_t pack_value       = ipv4_strict_str_to_unsigned(pack_first, pack_last);

                if (std::clamp(pack_value,
                               static_cast<size_t>(std::numeric_limits<uint8_t>::min()),
                               static_cast<size_t>(std::numeric_limits<uint8_t>::max())) != pack_value){

                    return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
                }

                rs.ip_buf[pack_count]   = std::bit_cast<char>(static_cast<uint8_t>(pack_value));
                first                   = last + 1;
                pack_count              += 1;

                continue;
            }

            if (!ipv4_is_numeric_char(data[i])){
                return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
            }
        }

        if (pack_count != IPv4_PACK_COUNT){
            return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
        }

        return rs;
    }

    static auto ipv4_compact_to_std_formatted_str(const IPv4& data, char * output_ptr, size_t output_size) noexcept -> std::expected<size_t, exception_t>{

        constexpr size_t SEPARATOR_SZ       = 3u;
        constexpr char SEMANTIC_SEPARATOR   = '.';

        size_t output_str_sz    = SEPARATOR_SZ;         

        for (size_t i = 0u; i < data.ip_buf.size(); ++i){
            uint8_t pack_numerical_value    = std::bit_cast<uint8_t>(data.ip_buf[i]);
            size_t pack_str_sz              = ipv4_count_unsigned_to_str(pack_numerical_value);
            output_str_sz                   += pack_str_sz;
        }

        if (output_size < output_str_sz){
            return std::unexpected(dg::network_exception::BAD_OPERATION);
        }

        for (size_t i = 0u; i < data.ip_buf.size(); ++i){
            uint8_t pack_numerical_value    = std::bit_cast<uint8_t>(data.ip_buf[i]);
            output_ptr                      = ipv4_unsigned_to_str(pack_numerical_value, output_ptr);

            if (i != data.ip_buf.size() - 1u){
                *output_ptr = SEMANTIC_SEPARATOR;
                std::advance(output_ptr, 1u);
            }
        }

        return output_str_sz;
    }

    static consteval auto ipv4_get_std_formatted_str_max_size() -> size_t{

        return 15u;
    } 

    static auto ipv6_is_hexa_char(char c) noexcept -> bool{

        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    } 

    static auto ipv6_hexa_char_to_unsigned(char c) noexcept -> std::expected<size_t, exception_t>{

        if (c >= '0' && c <= '9'){
            return static_cast<size_t>(c - '0');
        }

        if (c >= 'a' && c <= 'f'){
            return static_cast<size_t>(c - 'a') + 10u;
        }

        if (c >= 'A' && c <= 'F'){
            return static_cast<size_t>(c - 'A') + 10u;
        }

        return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
    }

    static auto ipv6_unsigned_to_hexa_char(size_t c) noexcept -> std::expected<char, exception_t>{

        if (c < 10){
            return static_cast<char>('0' + c);
        }

        if (c < 16){
            return static_cast<char>('a' + (c - 10u));            
        }

        return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
    }

    static auto ipv6_strict_str_pack_to_unsigned(const char * first, const char * last) noexcept -> uint16_t{

        constexpr size_t MAX_IPV6_PACK_SIZE = 4u;

        uint16_t rs = 0u;
        size_t sz   = stdx::safe_integer_cast<size_t>(std::distance(first, last));

        if constexpr(DEBUG_MODE_FLAG){
            if (sz > MAX_IPV6_PACK_SIZE){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }
        }

        for (size_t i = 0u; i < sz; ++i){
            if constexpr(DEBUG_MODE_FLAG){
                if (!ipv6_is_hexa_char(first[i])){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }

            rs *= 16;
            rs += dg::network_exception::remove_expected(ipv6_hexa_char_to_unsigned(first[i]));            
        }

        return rs;
    }

    static auto ipv6_count_unsigned_pack_to_str(uint16_t value) noexcept -> size_t{

        if (value == 0u){
            return 1u;
        }

        size_t counter = 0u;

        while (value != 0u){
            counter += 1u;
            value   /= 16u;
        }

        return counter;
    } 

    static auto ipv6_unsigned_pack_to_str(uint16_t value, char * op) noexcept -> char *{

        if (value == 0u){
            *op = '0';
            return std::next(op);
        }

        size_t str_sz               = ipv6_count_unsigned_pack_to_str(value);
        char * const ret_op_last    = std::next(op, str_sz);
        char * op_last              = ret_op_last;

        while (value != 0u){
            size_t back_hex_value = value % 16;
            value /= 16; 

            char hex_char = dg::network_exception::remove_expected(ipv6_unsigned_to_hexa_char(back_hex_value)); 
            *std::prev(op_last) = hex_char;

            std::advance(op_last, -1u);
        }

        return ret_op_last;
    } 

    static auto ipv6_basicstd_formatted_str_to_compact(std::string_view data) noexcept -> std::expected<IPv6, exception_t>{

        constexpr size_t IPv6_MIN_PACK_SIZE = 1u;
        constexpr size_t IPv6_MAX_PACK_SIZE = 4u;
        constexpr size_t IPv6_PACK_COUNT    = 8u;
        constexpr size_t DATA_MIN_SIZE      = 1u;
        constexpr size_t DATA_MAX_SIZE      = 39u;
        constexpr char SEMANTIC_SEPARATOR   = ':';

        if (std::clamp(static_cast<size_t>(data.size()), DATA_MIN_SIZE, DATA_MAX_SIZE) != data.size()){
            return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
        }

        std::array<uint16_t, IPv6_PACK_COUNT> ipv6_pack_array{};

        IPv6 rs             = {};
        size_t first        = 0u;
        size_t pack_count   = 0u;

        for (size_t i = 0u; i < data.size() + 1; ++i){
            if (i == data.size() || data[i] == SEMANTIC_SEPARATOR){
                if (pack_count == IPv6_PACK_COUNT){
                    return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
                }

                size_t last                 = i;
                size_t sz                   = last - first;

                if (std::clamp(sz, IPv6_MIN_PACK_SIZE, IPv6_MAX_PACK_SIZE) != sz){
                    return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
                }

                const char * pack_first     = std::next(data.data(), first);
                const char * pack_last      = std::next(data.data(), last);
                uint16_t pack_value         = ipv6_strict_str_pack_to_unsigned(pack_first, pack_last);

                ipv6_pack_array[pack_count] = pack_value;
                first                       = last + 1;
                pack_count                  += 1;

                continue;
            }

            if (!ipv6_is_hexa_char(data[i])){
                return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
            }
        }

        if (pack_count != IPv6_PACK_COUNT){
            return std::unexpected(dg::network_exception::SOCKET_BAD_IP);
        }

        static_assert(dg::network_trivial_serializer::size(decltype(ipv6_pack_array){}) == rs.ip_buf.size());
        dg::network_trivial_serializer::serialize_into(rs.ip_buf.data(), ipv6_pack_array);

        return rs;
    }

    static auto ipv6_compact_to_basicstd_formatted_str(const IPv6& data, char * output_ptr, size_t output_sz) noexcept -> std::expected<size_t, exception_t>{

        constexpr size_t SEPARATOR_SZ       = 7u;
        constexpr char SEMANTIC_SEPARATOR   = ':'; 
        constexpr size_t IPv6_PACK_COUNT    = 8u;

        size_t output_str_sz    = SEPARATOR_SZ;

        std::array<uint16_t, IPv6_PACK_COUNT> ipv6_pack_array;
        dg::network_trivial_serializer::deserialize_into(ipv6_pack_array, data.ip_buf.data());

        for (size_t i = 0u; i < ipv6_pack_array.size(); ++i){
            output_str_sz += ipv6_count_unsigned_pack_to_str(ipv6_pack_array[i]);
        }

        if (output_sz < output_str_sz){
            return std::unexpected(dg::network_exception::BAD_OPERATION);
        }

        for (size_t i = 0u; i < ipv6_pack_array.size(); ++i){
            output_ptr = ipv6_unsigned_pack_to_str(ipv6_pack_array[i], output_ptr);

            if (i != ipv6_pack_array.size() - 1u){
                *output_ptr = SEMANTIC_SEPARATOR;
                std::advance(output_ptr, 1u);
            }
        }

        return output_str_sz;
    }

    static consteval auto ipv6_get_basicstd_formatted_str_max_size() -> size_t{

        return 39u;
    } 

    static auto to_cstyle_str(char * normal_str, size_t sz) noexcept -> char *{

        normal_str[sz] = '\0';
        return std::next(normal_str, sz + 1);
    }

    static constexpr auto get_cstyle_str_size(size_t sz) noexcept -> size_t{

        return sz + 1u;
    }

    static auto kernel_get_cstyle_buffer_ipv4(const IPv4& ip) noexcept -> std::expected<std::array<char, get_cstyle_str_size(ipv4_get_std_formatted_str_max_size())>, exception_t>{

        std::array<char, get_cstyle_str_size(ipv4_get_std_formatted_str_max_size())> rs;
        std::expected<size_t, exception_t> sz = ipv4_compact_to_std_formatted_str(ip, rs.data(), rs.size());

        if constexpr(DEBUG_MODE_FLAG){
            if (!sz.has_value()){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }

            if (sz.value() >= rs.size()){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }
        }

        to_cstyle_str(rs.data(), sz.value());

        return rs;
    }

    static auto kernel_get_cstyle_buffer_ipv6(const IPv6& ip) noexcept -> std::expected<std::array<char, get_cstyle_str_size(ipv6_get_basicstd_formatted_str_max_size())>, exception_t>{

        std::array<char, get_cstyle_str_size(ipv6_get_basicstd_formatted_str_max_size())> rs;
        std::expected<size_t, exception_t> sz = ipv6_compact_to_basicstd_formatted_str(ip, rs.data(), rs.size());

        if constexpr(DEBUG_MODE_FLAG){
            if (!sz.has_value()){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }

            if (sz.value() >= rs.size()){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            }
        }

        to_cstyle_str(rs.data(), sz.value());

        return rs;
    } 

    static auto to_generic_ip(const IPv4& data) noexcept -> IP{

        return IP{.ip = data};
    }

    static auto to_generic_ip(const IPv6& data) noexcept -> IP{

        return IP{.ip = data};
    }

    static auto validate_ip(const IP& arg) noexcept -> exception_t{

        return dg::network_exception::SUCCESS;
    }

    static auto validate_addr(const Address& addr) noexcept -> exception_t{

        return validate_ip(addr.ip);
    }
}

namespace dg::network_kernel_mailbox_impl1::socket_utility{

    struct iovec2
    {
        struct iovec * base_arr;
        size_t arr_sz;
    };

    static constexpr auto iovec2_get_allocation_cost(size_t iovec_sz) noexcept -> size_t
    {
        size_t base_arr_sz;

        if (iovec_sz == 0u)
        {
            base_arr_sz = 0u;
        } else
        {
            base_arr_sz = iovec_sz * sizeof(struct iovec) + alignof(struct iovec);
        }

        size_t iovec2_sz        = sizeof(struct iovec2) + alignof(struct iovec2);
        size_t allocation_sz    = base_arr_sz + iovec2_sz; 

        return allocation_sz;
    } 

    static auto iovec2_inplace_make(char * buf, size_t iovec_sz) noexcept -> struct iovec2 *  
    {
        struct iovec * base_arr; 

        if (iovec_sz == 0u)
        {
            buf = buf;
            base_arr = nullptr;
        } else 
        {
            buf = dg::memult::align(buf, std::integral_constant<size_t, alignof(struct iovec)>{});
            base_arr = new (buf) struct iovec[iovec_sz];
            std::memset(base_arr, 0, iovec_sz * sizeof(struct iovec));
            std::advance(buf, iovec_sz * sizeof(struct iovec));
        }

        buf = dg::memult::align(buf, std::integral_constant<size_t, alignof(iovec2)>());
        struct iovec2 * return_obj = new (buf) struct iovec2(iovec2{.base_arr = base_arr, .arr_sz = iovec_sz});
        std::advance(buf, sizeof(struct iovec2));

        return return_obj;
    }

    static void iovec2_inplace_destroy(struct iovec2 * obj) noexcept
    {
        obj = stdx::safe_ptr_access(obj);

        std::destroy(obj->base_arr, std::next(obj->base_arr, obj->arr_sz));
        std::destroy_at(obj);
    }

    static auto iovec2_inplace_raiimake(char * buf, size_t iovec_sz) noexcept -> std::unique_ptr<struct iovec2, decltype(&iovec2_inplace_destroy)>
    {
        return {iovec2_inplace_make(buf, iovec_sz), iovec2_inplace_destroy};
    }

    static auto iovec2_assign_range(struct iovec2 * obj,
                                    size_t offset,
                                    void ** buf_arr,
                                    size_t * sz_arr,
                                    size_t arr_sz) noexcept -> exception_t 
    {
        if (offset + arr_sz > stdx::safe_ptr_access(obj)->arr_sz)
        {
            return dg::network_exception::OUT_OF_BOUND_ACCESS;
        }

        for (size_t i = 0u; i < arr_sz; ++i)
        {
            size_t abs_i = offset + i;

            obj->base_arr[abs_i].iov_base   = buf_arr[i];
            obj->base_arr[abs_i].iov_len    = sz_arr[i];
        }

        return dg::network_exception::SUCCESS;
    }

    struct mmsghdr_vec
    {
        struct mmsghdr * base_arr;
        size_t arr_sz;
    };

    static auto mmsghdr_vec_get_allocation_cost(size_t mmsghdr_sz) noexcept -> size_t
    {
        size_t base_arr_sz;

        if (mmsghdr_sz == 0u)
        {
            base_arr_sz = 0u;
        } else
        {
            base_arr_sz = mmsghdr_sz * sizeof(struct mmsghdr) + alignof(struct mmsghdr);
        }

        size_t mmsghdr_vec_sz   = sizeof(struct mmsghdr_vec) + alignof(struct mmsghdr_vec);
        size_t allocation_sz    = base_arr_sz + mmsghdr_vec_sz;

        return allocation_sz;
    }

    static auto mmsghdr_vec_inplace_make(char * buf, size_t mmsghdr_sz) noexcept -> struct mmsghdr_vec *
    {
        struct mmsghdr * base_arr;

        if (mmsghdr_sz == 0u)
        {
            buf = buf;
            base_arr = nullptr;
        } else
        {
            buf = dg::memult::align(buf, std::integral_constant<size_t, alignof(struct mmsghdr)>());
            base_arr = new (buf) struct mmsghdr[mmsghdr_sz];
            std::memset(base_arr, 0, mmsghdr_sz * sizeof(struct mmsghdr));
            std::advance(buf, mmsghdr_sz * sizeof(struct mmsghdr));
        }

        buf = dg::memult::align(buf, std::integral_constant<size_t, alignof(mmsghdr_vec)>());
        struct mmsghdr_vec * return_obj = new (buf) struct mmsghdr_vec(mmsghdr_vec{.base_arr = base_arr, .arr_sz = mmsghdr_sz});
        std::advance(buf, sizeof(struct mmsghdr_vec));

        return return_obj;
    }

    static void mmsghdr_vec_inplace_destroy(struct mmsghdr_vec * obj) noexcept
    {
        obj = stdx::safe_ptr_access(obj);

        std::destroy(obj->base_arr, std::next(obj->base_arr, obj->arr_sz));
        std::destroy_at(obj);
    }

    static auto mmsghdr_vec_inplace_raiimake(char * buf, size_t mmsghdr_sz) noexcept -> std::unique_ptr<struct mmsghdr_vec, decltype(&mmsghdr_vec_inplace_destroy)>
    {
        return {mmsghdr_vec_inplace_make(buf, mmsghdr_sz), mmsghdr_vec_inplace_destroy};
    }

    static auto mmsghdr_vec_bijective_bind_iovec_range(struct mmsghdr_vec * obj,
                                                       size_t offset,
                                                       struct iovec2 * iovec_vec) noexcept -> exception_t
    {
        obj = stdx::safe_ptr_access(obj);
        iovec_vec = stdx::safe_ptr_access(iovec_vec);

        if (offset + iovec_vec->arr_sz > obj->arr_sz)
        {
            return dg::network_exception::OUT_OF_BOUND_ACCESS;
        }

        for (size_t i = 0u; i < iovec_vec->arr_sz; ++i)
        {
            size_t abs_i = i + offset;
            obj->base_arr[abs_i].msg_hdr.msg_iov    = std::next(iovec_vec->base_arr, i);
            obj->base_arr[abs_i].msg_hdr.msg_iovlen = 1u;
        }

        return dg::network_exception::SUCCESS;
    }

    static auto mmsghdr_vec_bind_iovec_range(struct mmsghdr_vec * obj,
                                             size_t obj_offset,
                                             struct iovec2 * iovec_vec,
                                             size_t iovec_vec_offset,
                                             size_t iovec_vec_range) noexcept -> exception_t
    {
        obj = stdx::safe_ptr_access(obj);
        iovec_vec = stdx::safe_ptr_access(iovec_vec);

        if (obj_offset >= obj->arr_sz)
        {
            return dg::network_exception::OUT_OF_BOUND_ACCESS;
        }

        if (iovec_vec_offset + iovec_vec_range > iovec_vec->arr_sz)
        {
            return dg::network_exception::OUT_OF_BOUND_ACCESS;
        }

        obj->base_arr[obj_offset].msg_hdr.msg_iov       = std::next(iovec_vec->base_arr, iovec_vec_offset);
        obj->base_arr[obj_offset].msg_hdr.msg_iovlen    = iovec_vec_range;

        return dg::network_exception::SUCCESS;
    } 

    static auto mmsghdr_vec_rangebind_server(struct mmsghdr_vec * obj,
                                             void * server_obj,
                                             size_t server_obj_sz,
                                             size_t offset,
                                             size_t sz) noexcept -> exception_t
    {
        if (offset + sz > obj->arr_sz)
        {
            return dg::network_exception::OUT_OF_BOUND_ACCESS;
        }

        for (size_t i = 0u; i < sz; ++i)
        {
            size_t abs_i = offset + i;
            obj->base_arr[abs_i].msg_hdr.msg_name       = server_obj;
            obj->base_arr[abs_i].msg_hdr.msg_namelen    = stdx::safe_integer_cast<int>(server_obj_sz);
        }

        return dg::network_exception::SUCCESS;
    }
}

namespace dg::network_kernel_mailbox_impl1::socket_service{

    using namespace dg::network_kernel_mailbox_impl1::model;
    using namespace dg::network_kernel_mailbox_impl1::socket_utility;

    using socket_close_t = void (*)(SocketHandle *) noexcept; 

    template <class T>
    static auto legacy_struct_default_init() noexcept -> T{

        static_assert(std::is_trivial_v<T>);
        T rs{}; //list initializer is important for C++, UB otherwise
        std::memset(&rs, 0, sizeof(T));

        return rs;
    }
 
    static auto open_socket(int sin_fam, int comm, int protocol) noexcept -> std::expected<std::unique_ptr<SocketHandle, socket_close_t>, exception_t>{

        auto destructor = [](SocketHandle * sock) noexcept{
            if (close(sock->kernel_sock_fd) == -1){
                if constexpr(constants::HAS_STRICT_SOCKET_CLOSE){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::wrap_kernel_error(errno)));
                    std::abort();
                } else{
                    dg::network_log_stackdump::error(dg::network_exception::verbose(dg::network_exception::wrap_kernel_error(errno)));
                }
            }

            delete sock;
        };

        int sock = socket(sin_fam, comm, protocol);

        if (sock == -1){
            return std::unexpected(dg::network_exception::wrap_kernel_error(errno));
        }

        return std::unique_ptr<SocketHandle, socket_close_t>(new SocketHandle{.kernel_sock_fd = sock,
                                                                              .sin_fam = sin_fam,
                                                                              .comm = comm,
                                                                              .protocol = protocol,
                                                                              .mtx = stdx::make_unique_fair_atomic_flag(false, true)},
                                                             destructor);
    }

    static auto port_socket_ipv6(const SocketHandle& sock, uint16_t port, bool has_reuse = true) noexcept -> exception_t{

        if (sock.sin_fam != AF_INET6){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        struct sockaddr_in6 server  = legacy_struct_default_init<struct sockaddr_in6>();
        server.sin6_family          = AF_INET6;
        server.sin6_addr            = in6addr_any;
        server.sin6_port            = htons(port);

        if (has_reuse){
            int reuse_sz = 1;

	    	if (setsockopt(sock.kernel_sock_fd, SOL_SOCKET, SO_REUSEPORT, &reuse_sz, sizeof(reuse_sz)) == -1){
                return dg::network_exception::wrap_kernel_error(errno);
            }
        }

        if (bind(sock.kernel_sock_fd, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        return dg::network_exception::SUCCESS;
    }

    static auto port_socket_ipv4(const SocketHandle& sock, uint16_t port, bool has_reuse = true) noexcept -> exception_t{

        if (sock.sin_fam != AF_INET){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        struct sockaddr_in server   = legacy_struct_default_init<struct sockaddr_in>();
        server.sin_family           = AF_INET;
        server.sin_addr.s_addr      = INADDR_ANY;
        server.sin_port             = htons(port);

        if (has_reuse){
            int reuse_sz = 1;

	    	if (setsockopt(sock.kernel_sock_fd, SOL_SOCKET, SO_REUSEPORT, &reuse_sz, sizeof(reuse_sz)) == -1){
                return dg::network_exception::wrap_kernel_error(errno);
            }
        }

        if (bind(sock.kernel_sock_fd, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        return dg::network_exception::SUCCESS;
    }

    static auto port_socket(const SocketHandle& sock, uint16_t port, bool has_reuse = true) noexcept -> exception_t{

        if (sock.sin_fam == AF_INET6){
            return port_socket_ipv6(sock, port, has_reuse);
        }

        if (sock.sin_fam == AF_INET){
            return port_socket_ipv4(sock, port, has_reuse);
        }

        return dg::network_exception::INVALID_ARGUMENT;
    }

    static auto attach_bpf_socket(const SocketHandle& sock) noexcept -> exception_t{

        //these are mysterious wizard codes that I dont know
        //it seems like this is doing unbalanced modulo by using offset + affinity

        struct sock_filter code[] = {
            { BPF_LD  | BPF_W | BPF_ABS, 0, 0, static_cast<uint32_t>(SKF_AD_OFF) + SKF_AD_CPU},
            { BPF_RET | BPF_A, 0, 0, 0},
        };

        struct sock_fprog p = {
            .len = 2,
            .filter = code,
        };

        if (setsockopt(sock.kernel_sock_fd, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, &p, sizeof(p)) == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        return dg::network_exception::SUCCESS;
    }

    static auto send_noblock_ipv6(const SocketHandle& sock, const model::Address& to_addr, const void * buf, size_t sz) noexcept -> exception_t{

        stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*sock.mtx);

        if (sock.sin_fam != AF_INET6){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sock.comm != SOCK_DGRAM){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (to_addr.ip.sin_fam() != AF_INET6){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sz > constants::MAXIMUM_MSG_SIZE){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        auto kernel_ip = utility::kernel_get_cstyle_buffer_ipv6(std::get<IPv6>(to_addr.ip.ip)); 
        
        if (!kernel_ip.has_value()){
            return kernel_ip.error();
        }

        struct sockaddr_in6 server = legacy_struct_default_init<struct sockaddr_in6>();

        if (inet_pton(AF_INET6, kernel_ip->data(), &server.sin6_addr) == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        server.sin6_family  = AF_INET6;
        server.sin6_port    = htons(to_addr.port);
        auto n              = sendto(sock.kernel_sock_fd, 
                                     buf, stdx::wrap_safe_integer_cast(sz), 
                                     constants::KERNEL_NOBLOCK_TRANSMISSION_FLAG,
                                     reinterpret_cast<const struct sockaddr *>(&server), sizeof(struct sockaddr_in6));

        if (n == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        if (stdx::safe_integer_cast<size_t>(n) != sz){
            return dg::network_exception::RUNTIME_SOCKETIO_ERROR;
        }

        return dg::network_exception::SUCCESS;
    }

    static auto send_noblock_ipv4(const SocketHandle& sock, const model::Address& to_addr, const void * buf, size_t sz) noexcept -> exception_t{

        stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*sock.mtx);

        if (sock.sin_fam != AF_INET){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sock.comm != SOCK_DGRAM){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (to_addr.ip.sin_fam() != AF_INET){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sz > constants::MAXIMUM_MSG_SIZE){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        auto kernel_ip = utility::kernel_get_cstyle_buffer_ipv4(std::get<IPv4>(to_addr.ip.ip)); 

        if (!kernel_ip.has_value()){
            return kernel_ip.error();
        }

        struct sockaddr_in server = legacy_struct_default_init<struct sockaddr_in>();

        if (inet_pton(AF_INET, kernel_ip->data(), &server.sin_addr) == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        server.sin_family   = AF_INET;
        server.sin_port     = htons(to_addr.port);
        auto n              = sendto(sock.kernel_sock_fd, 
                                     buf, stdx::wrap_safe_integer_cast(sz), 
                                     constants::KERNEL_NOBLOCK_TRANSMISSION_FLAG, 
                                     reinterpret_cast<const struct sockaddr *>(&server), sizeof(struct sockaddr_in)); 

        if (n == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        if (stdx::safe_integer_cast<size_t>(n) != sz){
            return dg::network_exception::RUNTIME_SOCKETIO_ERROR;
        }

        return dg::network_exception::SUCCESS;
    } 

    static auto send_noblock(const SocketHandle& sock, const model::Address& to_addr, const void * buf, size_t sz) noexcept -> exception_t{

        if (sock.sin_fam == AF_INET6){
            return send_noblock_ipv6(sock, to_addr, buf, sz);
        }
    
        if (sock.sin_fam == AF_INET){
            return send_noblock_ipv4(sock, to_addr, buf, sz);
        }

        return dg::network_exception::INVALID_ARGUMENT;
    }

    static consteval auto batchsend_max_array_size() -> size_t
    {
        return constants::KERNEL_BATCH_POPCOUNT;
    }

    static consteval auto batchsend_incremental_size() -> size_t
    {
        return 1;
    }

    static consteval auto batchrecv_max_array_size() -> size_t
    {
        return constants::KERNEL_BATCH_POPCOUNT;
    }

    static auto batchsend_noblock_ipv6(const SocketHandle& sock,
                                       const model::Address& to_addr,
                                       void ** buf_arr,
                                       size_t * sz_arr,
                                       size_t sz) -> exception_t
    {
        stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*sock.mtx);

        if (sock.sin_fam != AF_INET6)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sock.comm != SOCK_DGRAM)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (to_addr.ip.sin_fam() != AF_INET6)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sz > batchsend_max_array_size())
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        for (size_t i = 0u; i < sz; ++i)
        {
            if (sz_arr[i] > constants::MAXIMUM_MSG_SIZE)
            {
                return dg::network_exception::INVALID_ARGUMENT;
            }
        }

        auto kernel_ip = utility::kernel_get_cstyle_buffer_ipv6(std::get<IPv6>(to_addr.ip.ip));

        if (!kernel_ip.has_value())
        {
            return kernel_ip.error();
        }

        struct sockaddr_in6 server = legacy_struct_default_init<struct sockaddr_in6>();

        if (inet_pton(AF_INET6, kernel_ip->data(), &server.sin6_addr) == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        server.sin6_family      = AF_INET6;
        server.sin6_port        = htons(to_addr.port);

        size_t iovec2_buf_sz    = iovec2_get_allocation_cost(sz);
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> iovec2_buf(iovec2_buf_sz);
        auto iovec_vec          = iovec2_inplace_raiimake(iovec2_buf.get(), sz);
        dg::network_exception_handler::nothrow_log(iovec2_assign_range(iovec_vec.get(), 0u, buf_arr, sz_arr, sz));

        size_t mmsghdr_sz       = sz / batchsend_incremental_size() + static_cast<size_t>(sz % batchsend_incremental_size() != 0u); 
        size_t mmsghdr_buf_sz   = mmsghdr_vec_get_allocation_cost(mmsghdr_sz);
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> mmsghdr_buf(mmsghdr_buf_sz);

        auto msg_header_vec     = mmsghdr_vec_inplace_raiimake(mmsghdr_buf.get(), mmsghdr_sz); 

        if (connect(sock.kernel_sock_fd, (struct sockaddr *) &server, sizeof(server)) == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        for (size_t i = 0u; i < mmsghdr_sz; ++i)
        {
            size_t first    = i * batchsend_incremental_size();
            size_t last     = std::min(static_cast<size_t>((i + 1) * batchsend_incremental_size()), sz);

            dg::network_exception_handler::nothrow_log(mmsghdr_vec_bind_iovec_range(msg_header_vec.get(),
                                                                                    i,
                                                                                    iovec_vec.get(),
                                                                                    first,
                                                                                    last - first));
        }

        int retval  = sendmmsg(sock.kernel_sock_fd,
                               msg_header_vec->base_arr,
                               msg_header_vec->arr_sz,
                               constants::KERNEL_NOBLOCK_TRANSMISSION_FLAG);

        if (retval == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        if (retval != msg_header_vec->arr_sz)
        {
            return dg::network_exception::RUNTIME_SOCKETIO_ERROR;
        }

        return dg::network_exception::SUCCESS;
    }

    static auto batchsend_noblock_ipv4(const SocketHandle& sock,
                                       const model::Address& to_addr,
                                       void ** buf_arr,
                                       size_t * sz_arr,
                                       size_t sz) -> exception_t
    {
        stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*sock.mtx);

        if (sock.sin_fam != AF_INET)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sock.comm != SOCK_DGRAM)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (to_addr.ip.sin_fam() != AF_INET)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (sz > batchsend_max_array_size())
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        for (size_t i = 0u; i < sz; ++i)
        {
            if (sz_arr[i] > constants::MAXIMUM_MSG_SIZE)
            {
                return dg::network_exception::INVALID_ARGUMENT;
            }
        }

        auto kernel_ip = utility::kernel_get_cstyle_buffer_ipv4(std::get<IPv4>(to_addr.ip.ip)); 

        if (!kernel_ip.has_value())
        {
            return kernel_ip.error();
        }

        struct sockaddr_in server = legacy_struct_default_init<struct sockaddr_in>();

        if (inet_pton(AF_INET, kernel_ip->data(), &server.sin_addr) == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        server.sin_family       = AF_INET;
        server.sin_port         = htons(to_addr.port);

        size_t iovec2_buf_sz    = iovec2_get_allocation_cost(sz);
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> iovec2_buf(iovec2_buf_sz);
        auto iovec_vec          = iovec2_inplace_raiimake(iovec2_buf.get(), sz);
        dg::network_exception_handler::nothrow_log(iovec2_assign_range(iovec_vec.get(), 0u, buf_arr, sz_arr, sz));

        size_t mmsghdr_sz       = sz / batchsend_incremental_size() + static_cast<size_t>(sz % batchsend_incremental_size() != 0u);  
        size_t mmsghdr_buf_sz   = mmsghdr_vec_get_allocation_cost(mmsghdr_sz); 
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> mmsghdr_buf(mmsghdr_buf_sz); 

        auto msg_header_vec     = mmsghdr_vec_inplace_raiimake(mmsghdr_buf.get(), mmsghdr_sz);

        if (connect(sock.kernel_sock_fd, (struct sockaddr *) &server, sizeof(server)) == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        for (size_t i = 0u; i < mmsghdr_sz; ++i)
        {
            size_t first    = i * batchsend_incremental_size();
            size_t last     = std::min(static_cast<size_t>((i + 1) * batchsend_incremental_size()), sz);
            
            dg::network_exception_handler::nothrow_log(mmsghdr_vec_bind_iovec_range(msg_header_vec.get(),
                                                                                    i,
                                                                                    iovec_vec.get(),
                                                                                    first,
                                                                                    last - first));
        }

        int retval  = sendmmsg(sock.kernel_sock_fd,
                               msg_header_vec->base_arr,
                               msg_header_vec->arr_sz,
                               constants::KERNEL_NOBLOCK_TRANSMISSION_FLAG); 

        if (retval == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        if (retval != msg_header_vec->arr_sz)
        {
            return dg::network_exception::RUNTIME_SOCKETIO_ERROR;
        }

        return dg::network_exception::SUCCESS;
    }

    static auto batchsend_noblock(const SocketHandle& sock,
                                  const model::Address& to_addr,
                                  void ** buf_arr,
                                  size_t * sz_arr,
                                  size_t sz) noexcept -> exception_t
    {
        if (sock.sin_fam == AF_INET6)
        {
            return batchsend_noblock_ipv6(sock, to_addr, buf_arr, sz_arr, sz);
        }

        if (sock.sin_fam == AF_INET)
        {
            return batchsend_noblock_ipv4(sock, to_addr, buf_arr, sz_arr, sz);
        }

        return dg::network_exception::INVALID_ARGUMENT;
    } 

    static auto recv_block(const SocketHandle& sock,
                           void * dst,
                           size_t& dst_sz,
                           size_t dst_cap) noexcept -> exception_t{

        if (sock.comm != SOCK_DGRAM){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (dst_cap < constants::MAXIMUM_MSG_SIZE){
            return dg::network_exception::INVALID_ARGUMENT;
        }

        struct sockaddr_storage from    = legacy_struct_default_init<struct sockaddr_storage>();
        socklen_t from_length           = sizeof(from);
        auto n                          = recvfrom(sock.kernel_sock_fd, 
                                                   dst, stdx::wrap_safe_integer_cast(dst_cap),
                                                   0,
                                                   reinterpret_cast<struct sockaddr *>(&from), &from_length);

        if (n == -1){
            return dg::network_exception::wrap_kernel_error(errno);
        }

        dst_sz = stdx::safe_integer_cast<size_t>(n);
        return dg::network_exception::SUCCESS;
    }

    static auto batchrecv_block(const SocketHandle& sock,
                                void ** dst,
                                size_t * dst_sz_arr,
                                size_t * dst_cap_arr,
                                size_t& arr_sz,
                                size_t arr_cap) noexcept -> exception_t 
    {
        if (sock.comm != SOCK_DGRAM)
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        if (arr_cap > batchrecv_max_array_size())
        {
            return dg::network_exception::INVALID_ARGUMENT;
        }

        for (size_t i = 0u; i < arr_cap; ++i)
        {
            if (dst_cap_arr[i] < constants::MAXIMUM_MSG_SIZE)
            {
                return dg::network_exception::INVALID_ARGUMENT;
            }
        }

        size_t mmsghdr_vec_buf_sz = mmsghdr_vec_get_allocation_cost(arr_cap);
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> mmsghdr_vec_buf(mmsghdr_vec_buf_sz);
        auto mmsghdr_vec = mmsghdr_vec_inplace_raiimake(mmsghdr_vec_buf.get(), arr_cap);

        size_t iovec2_buf_sz = iovec2_get_allocation_cost(arr_cap);
        dg::network_stack_allocation::NoExceptRawAllocation<char[]> iovec2_buf(iovec2_buf_sz);
        auto iovec_vec = iovec2_inplace_raiimake(iovec2_buf.get(), arr_cap);

        dg::network_exception_handler::nothrow_log(iovec2_assign_range(iovec_vec.get(), 0u, dst, dst_cap_arr, arr_cap));
        dg::network_exception_handler::nothrow_log(mmsghdr_vec_bijective_bind_iovec_range(mmsghdr_vec.get(), 0u, iovec_vec.get()));

        auto retval = recvmmsg(sock.kernel_sock_fd,
                               mmsghdr_vec->base_arr,
                               mmsghdr_vec->arr_sz,
                               0,
                               nullptr);

        if (retval == -1)
        {
            return dg::network_exception::wrap_kernel_error(errno);
        }

        arr_sz = stdx::safe_integer_cast<size_t>(retval);

        for (size_t i = 0u; i < arr_sz; ++i)
        {
            dst_sz_arr[i] = mmsghdr_vec->base_arr[i].msg_len;
        }

        return dg::network_exception::SUCCESS;
    }

    static auto batchrecv_block_x(const SocketHandle& sock,
                                  void ** dst,
                                  size_t * dst_sz_arr,
                                  size_t * dst_cap_arr,
                                  size_t& arr_sz,
                                  size_t arr_cap) noexcept -> exception_t
    {
        arr_sz = 0u; 

        while (true)
        {
            if (arr_sz == arr_cap)
            {
                return dg::network_exception::SUCCESS;
            }

            void ** tmp_dst             = std::next(dst, arr_sz);
            size_t * tmp_dst_sz_arr     = std::next(dst_sz_arr, arr_sz);
            size_t * tmp_dst_cap_arr    = std::next(dst_cap_arr, arr_sz);
            size_t tmp_sz               = {};
            size_t tmp_arr_cap          = arr_cap - arr_sz;

            exception_t tmp_err         = batchrecv_block(sock, tmp_dst, tmp_dst_sz_arr, tmp_dst_cap_arr, tmp_sz, tmp_arr_cap);

            if (dg::network_exception::is_failed(tmp_err))
            {
                if (arr_sz != 0u)
                {
                    return dg::network_exception::SUCCESS;
                }
                else
                {
                    return tmp_err;
                }
            }

            arr_sz += tmp_sz;
        }
    }
}

namespace dg::network_kernel_mailbox_impl1::data_structure{

    using namespace dg::network_kernel_mailbox_impl1::model;

    struct pow2_initialization_tag{}; 

    template <class Key>
    class bloom_filter{

        private:

            dg::vector<bool> bloom_table;
            dg::vector<uint32_t> murmur_hash_secret_vec;
            size_t sz;

        public:

            bloom_filter(size_t bloom_table_tentative_cap, size_t rehash_sz){

                if (rehash_sz == 0u){
                    throw std::invalid_argument("bad bloom_filter constructor's arguments");
                }

                size_t upcap    = stdx::ceil2(bloom_table_tentative_cap);
                auto rand_gen   = std::bind(std::uniform_int_distribution<uint32_t>{}, std::mt19937{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
                this->sz        = 0u; 

                this->bloom_table               = dg::vector<bool>();
                this->bloom_table.resize(upcap, false);

                this->murmur_hash_secret_vec    = dg::vector<uint32_t>(rehash_sz);
                std::generate(this->murmur_hash_secret_vec.begin(), this->murmur_hash_secret_vec.end(), rand_gen);
            }

            auto not_contains(const Key& key) const noexcept -> bool{ //bloom filter only works for not_contains, true negative, so the interface should be built on such, not not contains does not equal to contains, logically speaking it is, yet we are returning false if we cant determine the result

                for (uint32_t secret: this->murmur_hash_secret_vec){
                    size_t hashed_value = dg::network_hash::hash_reflectible(key, secret);

                    if (!this->internal_numerical_key_search(hashed_value)){
                        return true;
                    }
                }

                return false;
            }

            void insert(const Key& key) noexcept{

                for (uint32_t secret: this->murmur_hash_secret_vec){
                    size_t hashed_value = dg::network_hash::hash_reflectible(key, secret);
                    this->internal_numerical_key_insert(hashed_value);
                }

                this->sz += 1;
            }

            void clear() noexcept{

                std::fill(this->bloom_table.begin(), this->bloom_table.end(), false);
                this->sz = 0u;
            }

            auto size() const noexcept{

                return this->sz;
            }

            auto capacity() const noexcept{

                return this->bloom_table.size();
            }

        private:

            auto to_idx(size_t key) const noexcept -> size_t{

                return key & static_cast<size_t>(this->bloom_table.size() - 1u);
            } 

            auto internal_numerical_key_search(size_t key) const noexcept -> bool{

                return this->bloom_table[this->to_idx(key)];
            }

            void internal_numerical_key_insert(size_t key) noexcept{

                this->bloom_table[this->to_idx(key)] = true;
            }
    };

    template <class Key>
    class temporal_switching_bloom_filter{

        private:

            static inline constexpr uint8_t LEFT_SIDE   = 0u;
            static inline constexpr uint8_t RIGHT_SIDE  = 1u;

            bloom_filter<Key> left_bloom_filter;
            bloom_filter<Key> right_bloom_filter;
            size_t side_capacity;
            size_t side_sz;
            uint8_t side;

        public:

            temporal_switching_bloom_filter(size_t cap,
                                            size_t rehash_sz,
                                            size_t reliability_decay): left_bloom_filter(std::max(size_t{1}, cap), rehash_sz),
                                                                       right_bloom_filter(std::max(size_t{1}, cap), rehash_sz){

                this->side_capacity = std::max(size_t{1}, static_cast<size_t>(this->left_bloom_filter.capacity() >> reliability_decay));
                this->side_sz       = 0u;
                this->side          = LEFT_SIDE;
            }

            auto not_contains(const Key& key) const noexcept -> bool{

                return this->left_bloom_filter.not_contains(key) && this->right_bloom_filter.not_contains(key);
            }

            void insert(const Key& key) noexcept{

                if (this->side_sz == this->side_capacity){
                    this->switch_side();
                }

                if (this->side == LEFT_SIDE){
                    this->left_bloom_filter.insert(key);
                } else if (this->side == RIGHT_SIDE){
                    this->right_bloom_filter.insert(key);
                } else{
                    if constexpr(DEBUG_MODE_FLAG){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    } else{
                        std::unreachable();
                    }
                }

                this->side_sz += 1u;
            }
        
        private:

            void switch_side() noexcept{

                switch (this->side){
                    case LEFT_SIDE:
                    {
                        this->right_bloom_filter.clear();
                        this->side_sz   = 0u;
                        this->side      = RIGHT_SIDE;

                        return;
                    }
                    case RIGHT_SIDE:
                    {
                        this->left_bloom_filter.clear();
                        this->side_sz   = 0u;
                        this->side      = LEFT_SIDE;

                        return;
                    }
                    default:
                    {
                        if constexpr(DEBUG_MODE_FLAG){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        } else{
                            std::unreachable();
                        }
                    }
                }
            }
    };

    template <class T>
    class temporal_finite_unordered_set{

        private:

            dg::cyclic_unordered_node_set<T> base;

        public:

            temporal_finite_unordered_set(size_t cap): base(dg::cyclic_unordered_node_set<T>::size_to_capacity(cap)){
                // std::abort();
            }

            template <class KeyLike>
            inline void insert(KeyLike&& key) noexcept{

                try{
                    this->base.insert(std::forward<KeyLike>(key));
                } catch (...){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::wrap_std_exception(std::current_exception())));
                    std::abort();
                }
            }

            inline auto capacity() const noexcept -> size_t{

                return this->base.capacity();
            }

            template <class KeyLike>
            inline auto contains(const KeyLike& key) const noexcept -> bool{

                return this->base.contains(key);
            }

            inline auto begin() const noexcept -> decltype(auto){

                return this->base.begin();
            }

            inline auto end() const noexcept -> decltype(auto){

                return this->base.end();
            }

            inline auto size() const noexcept -> size_t{
                
                return this->base.size();
            }
    };

    class temporal_ordered_packet_map{

        private:

            struct HeapNode{
                Packet pkt;
                std::chrono::time_point<std::chrono::utc_clock> sched_time;
                size_t heap_idx;
            };

            dg::unordered_unstable_map<global_packet_id_t, HeapNode *> id_heap_map;
            dg::vector<std::unique_ptr<HeapNode>> temporal_heap;
            size_t temporal_heap_sz;

        public:

            temporal_ordered_packet_map(size_t cap): id_heap_map(),
                                                     temporal_heap(),
                                                     temporal_heap_sz(0u){

                this->id_heap_map.reserve(cap);

                for (size_t i = 0u; i < cap; ++i){
                    this->temporal_heap.push_back(std::make_unique<HeapNode>(HeapNode{}));
                }
            }

            auto add(Packet&& pkt,
                     std::chrono::time_point<std::chrono::utc_clock> expiry_time) noexcept -> exception_t{

                if (this->id_heap_map.contains(pkt.id)){
                    return dg::network_exception::DUPLICATE_ENTRY;
                }

                std::expected<HeapNode *, exception_t> reference_node = this->add_heap_node(std::move(pkt), expiry_time);

                if (!reference_node.has_value()){
                    return reference_node.error();
                }

                try{
                    auto [map_ptr, status] = id_heap_map.insert(std::make_pair(reference_node.value()->pkt.id, reference_node.value()));
                    dg::network_exception_handler::dg_assert(status);
                } catch (...){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }

                return dg::network_exception::SUCCESS;
            }

            void erase(global_packet_id_t packet_id) noexcept{

                auto map_ptr = this->id_heap_map.find(packet_id);

                if (map_ptr == this->id_heap_map.end()){
                    return;
                }

                size_t idx = stdx::safe_ptr_access(map_ptr->second)->heap_idx;
                this->id_heap_map.erase(map_ptr);
                this->erase_heap_node_at(idx);
            }

            auto get_expired_packet(std::chrono::time_point<std::chrono::utc_clock> time_bar) noexcept -> std::optional<Packet>{

                if (this->temporal_heap_sz == 0u){
                    return std::nullopt;
                }

                std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar){
                    return std::nullopt;
                }

                Packet result = std::move(front_value->pkt);
                global_packet_id_t associated_id = result.id;

                this->id_heap_map.erase(associated_id);
                this->pop_heap_node();

                return std::optional<Packet>(std::move(result));
            }

            auto has_expired_packet(std::chrono::time_point<std::chrono::utc_clock> time_bar) const noexcept -> bool{

                if (this->temporal_heap_sz == 0u){
                    return false;
                }

                const std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar){
                    return false;
                }

                return true;
            }
            
            auto size() const noexcept -> size_t{

                return this->temporal_heap_sz;
            }

            auto capacity() const noexcept -> size_t{

                return this->temporal_heap.size();
            }

        private:

            static void nullify_heap_node(std::unique_ptr<HeapNode>& arg) noexcept{

                arg->pkt        = {};
                arg->sched_time = {};
                arg->heap_idx   = {};
            }

            static void swap_heap_node(std::unique_ptr<HeapNode>& lhs,
                                       std::unique_ptr<HeapNode>& rhs) noexcept{

                std::swap(lhs->heap_idx, rhs->heap_idx);
                std::swap(lhs, rhs);
            }

            static auto is_less_than(const std::unique_ptr<HeapNode>& lhs,
                                     const std::unique_ptr<HeapNode>& rhs) noexcept -> bool{

                return lhs->sched_time < rhs->sched_time;
            }

            void correct_heap_node_up_at(size_t idx) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->temporal_heap_sz){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (idx == 0u){
                    return;
                }

                size_t parent_idx = (idx - 1) >> 1;

                if (!is_less_than(this->temporal_heap[idx], this->temporal_heap[parent_idx])){
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[parent_idx]);
                this->correct_heap_node_up_at(parent_idx);
            }

            void correct_heap_node_down_at(size_t idx) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->temporal_heap_sz){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t cand_idx = idx * 2 + 1;

                if (cand_idx >= this->temporal_heap_sz){
                    return;
                }

                if (cand_idx + 1 < this->temporal_heap_sz && is_less_than(this->temporal_heap[cand_idx + 1], this->temporal_heap[cand_idx])){
                    cand_idx += 1;
                }

                if (!is_less_than(this->temporal_heap[cand_idx], this->temporal_heap[idx])){
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[cand_idx]);
                this->correct_heap_node_down_at(cand_idx);
            } 

            void correct_heap_node_at(size_t idx) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->temporal_heap_sz){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->correct_heap_node_up_at(idx);
                this->correct_heap_node_down_at(idx);
            }

            auto add_heap_node(Packet&& pkt,
                               std::chrono::time_point<std::chrono::utc_clock> sched_time) noexcept -> std::expected<HeapNode *, exception_t>{

                if (this->temporal_heap_sz == this->temporal_heap.size()){
                    return std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
                }

                HeapNode * operating_node   = stdx::safe_ptr_access(this->temporal_heap[this->temporal_heap_sz].get());

                operating_node->pkt         = std::move(pkt);
                operating_node->sched_time  = sched_time;
                operating_node->heap_idx    = this->temporal_heap_sz;

                this->temporal_heap_sz      += 1;

                this->correct_heap_node_up_at(this->temporal_heap_sz - 1);

                return operating_node;
            }

            void erase_heap_node_at(size_t idx) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->temporal_heap_sz){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == idx){
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                } else{
                    this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_at(idx);
                }
            }

            void pop_heap_node() noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (this->temporal_heap_sz == 0u){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == 0u){
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                } else{
                    this->swap_heap_node(this->temporal_heap.front(), this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_down_at(0u);
                }
            }
    };
}

namespace dg::network_kernel_mailbox_impl1::packet_service{

    //we are to only to reduce statistical chances, and not crash anything if there might be a coordinated attack 
    //we are also to make sure that we dont have internal corruptions by providing each integrity serialization a different encoding format
    //this passes code review, we are to not worry about allocations dg::string uses a special no-fragmenented finite pool of heap memory that's never gonna run out
    //packet_polymorphic_t is no-optional because we adhere to the virtues, it's better that way

    static inline constexpr uint32_t REQUEST_PACKET_SERIALIZATION_SECRET    = 3130801039UL;
    static inline constexpr uint32_t ACK_PACKET_SERIALIZATION_SECRET        = static_cast<uint32_t>(0xFF) - 1u;
    static inline constexpr uint32_t KRESCUE_PACKET_SERIALIZATION_SECRET    = static_cast<uint32_t>(0xFFF) - 1u;

    using namespace dg::network_kernel_mailbox_impl1::model;

    static auto virtualize_request_packet(RequestPacket pkt) noexcept -> std::expected<Packet, exception_t>{

        Packet rs                       = {};
        static_cast<PacketHeader&>(rs)  = std::move(static_cast<PacketHeader&>(pkt));
        rs.xonly_content                = std::move(static_cast<XOnlyRequestPacket&>(pkt));

        return rs;
    }

    static auto virtualize_ack_packet(AckPacket pkt) noexcept -> std::expected<Packet, exception_t>{

        Packet rs                       = {};
        static_cast<PacketHeader&>(rs)  = std::move(static_cast<PacketHeader&>(pkt));
        rs.xonly_content                = std::move(static_cast<XOnlyAckPacket&>(pkt));

        return rs;
    }

    static auto virtualize_krescue_packet(KRescuePacket pkt) noexcept -> std::expected<Packet, exception_t>{

        Packet rs                       = {};
        static_cast<PacketHeader&>(rs)  = std::move(static_cast<PacketHeader&>(pkt));
        rs.xonly_content                = std::move(static_cast<XOnlyKRescuePacket&>(pkt));

        return rs;
    }

    static auto devirtualize_request_packet(Packet pkt) noexcept -> std::expected<RequestPacket, exception_t>{

        if (!std::holds_alternative<XOnlyRequestPacket>(pkt.xonly_content)){
            return std::unexpected(dg::network_exception::BAD_POLYMORPHIC_ACCESS);
        }

        RequestPacket rs                        = {};
        static_cast<PacketHeader&>(rs)          = std::move(static_cast<PacketHeader&>(pkt));
        static_cast<XOnlyRequestPacket&>(rs)    = std::move(std::get<XOnlyRequestPacket>(pkt.xonly_content)); 

        return rs;
    }

    static auto devirtualize_ack_packet(Packet pkt) noexcept -> std::expected<AckPacket, exception_t>{

        if (!std::holds_alternative<XOnlyAckPacket>(pkt.xonly_content)){
            return std::unexpected(dg::network_exception::BAD_POLYMORPHIC_ACCESS);
        }

        AckPacket rs                            = {};
        static_cast<PacketHeader&>(rs)          = std::move(static_cast<PacketHeader&>(pkt));
        static_cast<XOnlyAckPacket&>(rs)        = std::move(std::get<XOnlyAckPacket>(pkt.xonly_content));

        return rs;
    }

    static auto devirtualize_krescue_packet(Packet pkt) noexcept -> std::expected<KRescuePacket, exception_t>{

        if (!std::holds_alternative<XOnlyKRescuePacket>(pkt.xonly_content)){
            return std::unexpected(dg::network_exception::BAD_POLYMORPHIC_ACCESS);
        }

        KRescuePacket rs                        = {};
        static_cast<PacketHeader&>(rs)          = std::move(static_cast<PacketHeader&>(pkt));
        static_cast<XOnlyKRescuePacket&>(rs)    = std::move(std::get<XOnlyKRescuePacket>(pkt.xonly_content));

        return rs;
    }

    static auto frequency_to_period(uint32_t frequency) noexcept -> std::chrono::nanoseconds{

        constexpr uint32_t MINIMUM_FREQUENCY    = uint32_t{1};
        constexpr uint32_t MAXIMUM_FREQUENCY    = uint32_t{1} << 30;
        constexpr uint32_t SECOND_METRIC        = uint32_t{1} << 30; 
        uint32_t clamped_frequency              = std::min(std::max(frequency, MINIMUM_FREQUENCY), MAXIMUM_FREQUENCY);
        uint32_t period                         = SECOND_METRIC / clamped_frequency; 

        return std::chrono::nanoseconds{period};
    }

    static inline auto is_request_packet(const Packet& pkt) noexcept -> bool{

        return std::holds_alternative<XOnlyRequestPacket>(pkt.xonly_content);
    }

    static inline auto is_ack_packet(const Packet& pkt) noexcept -> bool{

        return std::holds_alternative<XOnlyAckPacket>(pkt.xonly_content);
    }

    static inline auto is_krescue_packet(const Packet& pkt) noexcept -> bool{

        return std::holds_alternative<XOnlyKRescuePacket>(pkt.xonly_content);
    }

    static inline auto get_packet_polymorphic_type(const Packet& pkt) noexcept -> packet_polymorphic_t{

        if (is_request_packet(pkt)){
            return constants::request;
        } else if (is_ack_packet(pkt)){
            return constants::ack;
        } else if (is_krescue_packet(pkt)){
            return constants::krescue;
        } else{
            if constexpr(DEBUG_MODE_FLAG){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort();
            } else{
                std::unreachable();
            }
        }
    }

    static auto serialize_request_packet(RequestPacket&& packet) noexcept -> std::expected<internal_kernel_buffer, exception_t>{

        using x_header_t = std::pair<PacketHeader, std::pair<uint64_t, uint64_t>>; 

        std::pair<uint64_t, uint64_t> integrity_hash    = dg::network_hash::murmur_hash_base(packet.content.data(), packet.content.size(), REQUEST_PACKET_SERIALIZATION_SECRET);
        auto x_header                                   = x_header_t{static_cast<const PacketHeader&>(packet), integrity_hash};
        size_t header_sz                                = dg::network_compact_trivial_serializer::size(x_header);
        size_t content_sz                               = packet.content.size();
        size_t total_sz                                 = content_sz + header_sz;

        try{
            packet.content.resize(total_sz);
        } catch (...){
            return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
        }

        char * header_ptr                               = std::next(packet.content.data(), content_sz);
        dg::network_compact_trivial_serializer::serialize_into(header_ptr, x_header, REQUEST_PACKET_SERIALIZATION_SECRET);

        return std::expected<internal_kernel_buffer, exception_t>(std::move(packet.content));
    }

    static auto serialize_ack_packet(AckPacket&& packet) noexcept -> std::expected<internal_kernel_buffer, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<internal_kernel_buffer, AckPacket>)(packet, ACK_PACKET_SERIALIZATION_SECRET); 
    }

    static auto serialize_krescue_packet(KRescuePacket&& packet) noexcept -> std::expected<internal_kernel_buffer, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<internal_kernel_buffer, KRescuePacket>)(packet, KRESCUE_PACKET_SERIALIZATION_SECRET);
    }

    static auto deserialize_request_packet(internal_kernel_buffer bstream) noexcept -> std::expected<RequestPacket, exception_t>{

        using x_header_t    = std::pair<PacketHeader, std::pair<uint64_t, uint64_t>>;

        size_t header_sz    = dg::network_compact_trivial_serializer::size(x_header_t{});

        if (bstream.size() < header_sz){
            return std::unexpected(dg::network_exception::SOCKET_CORRUPTED_PACKET);
        }

        auto x_header       = x_header_t{};
        RequestPacket rs    = {};
        
        std::expected<std::pair<internal_kernel_buffer, internal_kernel_buffer>, exception_t> split_data = utility::backsplit_str(std::move(bstream), header_sz);

        if (!split_data.has_value()){
            return std::unexpected(split_data.error());
        }

        auto [left, right]  = std::move(split_data.value());
        exception_t err     = dg::network_exception::to_cstyle_function(dg::network_compact_trivial_serializer::deserialize_into<x_header_t>)(x_header, right.data(), right.size(), REQUEST_PACKET_SERIALIZATION_SECRET);

        if (dg::network_exception::is_failed(err)){
            return std::unexpected(err);
        }

        rs.content                                      = std::move(left);
        static_cast<PacketHeader&>(rs)                  = std::get<0>(x_header);
        std::pair<uint64_t, uint64_t> integrity_hash    = dg::network_hash::murmur_hash_base(rs.content.data(), rs.content.size(), REQUEST_PACKET_SERIALIZATION_SECRET);

        if (integrity_hash != std::get<1>(x_header)){
            return std::unexpected(dg::network_exception::SOCKET_CORRUPTED_PACKET);
        }

        return rs;
    }

    static auto deserialize_ack_packet(internal_kernel_buffer bstream) noexcept -> std::expected<AckPacket, exception_t>{

        // return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_deserialize<AckPacket, internal_kernel_buffer>)(bstream, ACK_PACKET_SERIALIZATION_SECRET); //
    }

    static auto deserialize_krescue_packet(internal_kernel_buffer bstream) noexcept -> std::expected<KRescuePacket, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_deserialize<KRescuePacket, internal_kernel_buffer>)(bstream, KRESCUE_PACKET_SERIALIZATION_SECRET);
    }

    static auto serialize_packet(Packet packet) noexcept -> std::expected<internal_kernel_buffer, exception_t>{

        constexpr size_t PACKET_POLYMORPHIC_HEADER_SZ                                   = dg::network_compact_trivial_serializer::size(packet_polymorphic_t{});
        std::array<char, PACKET_POLYMORPHIC_HEADER_SZ> polymorphic_writing_container    = {}; 
        internal_kernel_buffer serialized                                               = {};

        if (is_request_packet(packet)){
            dg::network_compact_trivial_serializer::serialize_into(polymorphic_writing_container.data(), static_cast<packet_polymorphic_t>(constants::request));
            std::expected<internal_kernel_buffer, exception_t> tmp = serialize_request_packet(dg::network_exception_handler::nothrow_log(devirtualize_request_packet(std::move(packet))));

            if (!tmp.has_value()){
                return std::unexpected(tmp.error());
            }

            serialized = std::move(tmp.value());
        } else if (is_ack_packet(packet)){
            dg::network_compact_trivial_serializer::serialize_into(polymorphic_writing_container.data(), static_cast<packet_polymorphic_t>(constants::ack));
            std::expected<internal_kernel_buffer, exception_t> tmp = serialize_ack_packet(dg::network_exception_handler::nothrow_log(devirtualize_ack_packet(std::move(packet))));

            if (!tmp.has_value()){
                return std::unexpected(tmp.error());
            }

            serialized = std::move(tmp.value());
        } else if (is_krescue_packet(packet)){
            dg::network_compact_trivial_serializer::serialize_into(polymorphic_writing_container.data(), static_cast<packet_polymorphic_t>(constants::krescue));
            std::expected<internal_kernel_buffer, exception_t> tmp = serialize_krescue_packet(dg::network_exception_handler::nothrow_log(devirtualize_krescue_packet(std::move(packet))));

            if (!tmp.has_value()){
                return std::unexpected(tmp.error());
            }

            serialized = std::move(tmp.value());
        } else{
            if constexpr(DEBUG_MODE_FLAG){
                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                std::abort(); //this is not qualified as an exception, we assume global assumption for packet construction
            } else{
                std::unreachable();
            }
        }

        try{
            std::copy(polymorphic_writing_container.begin(), polymorphic_writing_container.end(), std::back_inserter(serialized));
        } catch (...){
            return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
        }

        return serialized;
    }

    static auto deserialize_packet(internal_kernel_buffer bstream) noexcept -> std::expected<Packet, exception_t>{

        constexpr size_t PACKET_POLYMORPHIC_HEADER_SZ   = dg::network_compact_trivial_serializer::size(packet_polymorphic_t{});

        std::expected<std::pair<internal_kernel_buffer, internal_kernel_buffer>, exception_t> split_data = utility::backsplit_str(std::move(bstream), PACKET_POLYMORPHIC_HEADER_SZ);

        if (!split_data.has_value())
        {
            return std::unexpected(split_data.error());
        }

        auto [left, right]                              = std::move(split_data.value()); 

        if (right.size() != PACKET_POLYMORPHIC_HEADER_SZ){
            return std::unexpected(dg::network_exception::SOCKET_MALFORMED_PACKET);
        }

        packet_polymorphic_t packet_type = {};
        exception_t err = dg::network_exception::to_cstyle_function(dg::network_compact_trivial_serializer::deserialize_into<packet_polymorphic_t>)(packet_type, right.data(), right.size(), 0u);

        if (dg::network_exception::is_failed(err)){
            return std::unexpected(err);
        }

        if (packet_type == constants::request){
            std::expected<RequestPacket, exception_t> devirtualized_packet = deserialize_request_packet(std::move(left));

            if (!devirtualized_packet.has_value()){
                return std::unexpected(devirtualized_packet.error());
            }

            return virtualize_request_packet(std::move(devirtualized_packet.value()));
        } else if (packet_type == constants::ack){
            std::expected<AckPacket, exception_t> devirtualized_packet = deserialize_ack_packet(std::move(left));

            if (!devirtualized_packet.has_value()){
                return std::unexpected(devirtualized_packet.error());
            }

            return virtualize_ack_packet(std::move(devirtualized_packet.value()));
        } else if (packet_type == constants::krescue){
            std::expected<KRescuePacket, exception_t> devirtualized_packet = deserialize_krescue_packet(std::move(left));

            if (!devirtualized_packet.has_value()){
                return std::unexpected(devirtualized_packet.error());
            }

            return virtualize_krescue_packet(std::move(devirtualized_packet.value()));
        } else{
            return std::unexpected(dg::network_exception::SOCKET_MALFORMED_PACKET);
        }
    }

    static auto get_kind_global_id_from_packet(const Packet& packet) noexcept -> kind_global_packet_id_t{

        return KindGlobalPacketIdentifer{.id    = packet.id,
                                         .kind  = stdx::safe_integer_cast<uint8_t>(get_packet_polymorphic_type(packet))};        
    }
}

namespace dg::network_kernel_mailbox_impl1::semaphore_impl{

    class dg_binary_semaphore{

        private:

            std::binary_semaphore base;

        public:

            constexpr dg_binary_semaphore(std::ptrdiff_t initial_count): base(initial_count){}

            inline __attribute__((force_inline)) void acquire() noexcept{

                try{
                    this->base.acquire();
                } catch (...){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::wrap_std_exception(std::current_exception())));
                    std::abort();
                }
            }

            inline __attribute__((force_inline)) void release() noexcept{

                try{
                    this->base.release(1u);
                } catch(...){
                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::wrap_std_exception(std::current_exception())));
                    std::abort();
                }
            }

            inline __attribute__((force_inline)) std::expected<bool, exception_t> try_acquire_for(std::chrono::nanoseconds timeout) noexcept{

                try{
                    return this->base.try_acquire_for(timeout);
                } catch (...){
                    return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
                }
            }
    };
}

namespace dg::network_kernel_mailbox_impl1::packet_controller{

    //OK
    class BusyObserver: public virtual packet_controller::NetworkBusyObserverInterface{

        private:

            dg::vector<std::shared_ptr<packet_controller::ContainerBusyAdapterInterface>> busy_adapter_vec;

            using interface_t = packet_controller::NetworkBusyObserverInterface;

        public:

            using busy_level_t = interface_t::busy_level_t; 

            BusyObserver(dg::vector<std::shared_ptr<packet_controller::ContainerBusyAdapterInterface>> busy_adapter_vec): busy_adapter_vec(std::move(busy_adapter_vec)){}

            void notify(busy_level_t busy_level) noexcept{

                switch (busy_level)
                {
                    case interface_t::BUSY_0:
                    {
                        for (const auto& busy_adapter: this->busy_adapter_vec){
                            exception_t err = busy_adapter->set_ease();

                            if (dg::network_exception::is_failed(err)){
                                dg::network_log_stackdump::error(dg::network_exception::verbose(err));
                            }
                        }

                        break;
                    }
                    case interface_t::BUSY_1:
                    {
                        for (const auto& busy_adapter: this->busy_adapter_vec){
                            exception_t err = busy_adapter->set_busy();

                            if (dg::network_exception::is_failed(err)){
                                dg::network_log_stackdump::error(dg::network_exception::verbose(err));
                            }
                        }

                        break;
                    }
                    case interface_t::BUSY_2:
                    {
                        for (const auto& busy_adapter: this->busy_adapter_vec){
                            exception_t err = busy_adapter->set_busy();

                            if (dg::network_exception::is_failed(err)){
                                dg::network_log_stackdump::error(dg::network_exception::verbose(err));
                            }
                        }

                        break;
                    }
                    default:
                    {
                        if constexpr(DEBUG_MODE_FLAG){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        } else{
                            std::unreachable();
                        }
                    }
                }
            }
    };

    //OK
    class ComplexReactor{

        private:

            dg::unordered_unstable_map<std::thread::id, std::shared_ptr<semaphore_impl::dg_binary_semaphore>> mtx_queue;
            size_t mtx_queue_cap;
            stdx::fair_atomic_flag mtx_mtx_queue;
            stdx::inplace_hdi_container<std::atomic<intmax_t>> counter;
            stdx::inplace_hdi_container<std::atomic<intmax_t>> wakeup_threshold;
            stdx::inplace_hdi_container<std::atomic<size_t>> mtx_queue_sz;

        public:

            ComplexReactor(size_t mtx_queue_cap): mtx_queue(),
                                                  mtx_queue_cap(mtx_queue_cap), 
                                                  mtx_mtx_queue(),
                                                  counter(std::in_place_t{}, 0),
                                                  wakeup_threshold(std::in_place_t{}, 0),
                                                  mtx_queue_sz(std::in_place_t{}, 0u){
                
                stdx::inplace_make_fair_atomic_flag(this->mtx_mtx_queue);
            }

            void increment(size_t sz) noexcept{

                constexpr size_t INCREMENT_RETRY_SZ                 = 4u;
                const std::chrono::nanoseconds FAILED_LOCK_SLEEP    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds{1});

                this->counter.value.fetch_add(sz, std::memory_order_relaxed); //increment
                intmax_t expected = this->wakeup_threshold.value.load(std::memory_order_relaxed);

                for (size_t epoch = 0u; epoch < INCREMENT_RETRY_SZ; ++epoch){
                    intmax_t current = this->counter.value.load(std::memory_order_relaxed);

                    if (current < expected){
                        break;
                    }

                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    size_t current_queue_sz = this->mtx_queue_sz.value.load(std::memory_order_relaxed);

                    if (current_queue_sz == 0u){
                        break;
                    }

                    bool try_lock_rs = stdx::try_lock(this->mtx_mtx_queue, std::memory_order_relaxed); 

                    if (!try_lock_rs){
                        stdx::lock_yield(FAILED_LOCK_SLEEP); //we proved that this just needs to yield the time of critical sections (so we could transfer the responsibility of waking up to whoever holding the lock then on)
                                                             //so a lock acquisition is not mandatory here
                                                             //the odd cases of anomaly, such as kernel intervention of round-robin + etc. is handled by the default wakers
                        continue;
                    }

                    {
                        stdx::unlock_guard<stdx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);

                        this->mtx_queue_sz.value.exchange(0u, std::memory_order_relaxed);
                        this->do_release(this->mtx_queue);
                    }

                    break;
                }
            }

            void decrement(size_t sz) noexcept{

                this->counter.value.fetch_sub(sz, std::memory_order_relaxed);
            }

            void reset() noexcept{

                this->counter.value.exchange(intmax_t{0}, std::memory_order_relaxed);
            } 

            auto set_wakeup_threshold(intmax_t arg) noexcept{

                this->wakeup_threshold.value.exchange(arg, std::memory_order_relaxed);
            } 

            void subscribe(std::chrono::nanoseconds waiting_time) noexcept{

                intmax_t current    = this->counter.value.load(std::memory_order_relaxed);
                intmax_t expected   = this->wakeup_threshold.value.load(std::memory_order_relaxed);

                if (current >= expected){
                    return;
                }

                std::shared_ptr<semaphore_impl::dg_binary_semaphore> waiting_smp = network_kernel_mailbox_impl1::allocation::urgent_make_shared<semaphore_impl::dg_binary_semaphore>(0);

                [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (this->mtx_queue.size() == this->mtx_queue_cap){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    this->mtx_queue[std::this_thread::get_id()] = waiting_smp;
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    this->mtx_queue_sz.value.fetch_add(1u, std::memory_order_relaxed);
                    std::atomic_signal_fence(std::memory_order_seq_cst);

                    intmax_t new_current = this->counter.value.load(std::memory_order_relaxed); 

                    if (new_current >= expected){
                        stdx::seq_cst_guard seqcst_tx;

                        this->mtx_queue_sz.value.exchange(0u, std::memory_order_relaxed);
                        this->do_release(this->mtx_queue);
                    }
                }();

                std::atomic_signal_fence(std::memory_order_seq_cst); // another fence
                std::expected<bool, exception_t> err = waiting_smp->try_acquire_for(waiting_time);

                if (err.has_value() && err.value() == false)
                {
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(this->mtx_mtx_queue);
                    this->mtx_queue.erase(std::this_thread::get_id());
                }
            }

        private:

            inline __attribute__((force_inline)) void do_release(dg::unordered_unstable_map<std::thread::id, std::shared_ptr<semaphore_impl::dg_binary_semaphore>>& smp_vec){

                for (const auto& kv_pair: smp_vec){
                    kv_pair.second->release();
                }

                smp_vec.clear();
            }
    };

    template <class T>
    class LIFOUnitAllocator: public virtual UnitAllocatorInterface<T>{

        private:
            
            dg::vector<T> queue;
            size_t queue_cap;
            size_t allocated_sz;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;

        public:

            LIFOUnitAllocator(dg::vector<T> queue,
                              size_t queue_cap,
                              size_t allocated_sz,
                              std::unique_ptr<stdx::fair_atomic_flag> mtx) noexcept: queue(std::move(queue)),
                                                                                     queue_cap(queue_cap),
                                                                                     allocated_sz(allocated_sz),
                                                                                     mtx(std::move(mtx)){}

            auto get() noexcept -> std::expected<T, exception_t>{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->queue.empty()){
                    if (this->allocated_sz == this->queue_cap){
                        return std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
                    }

                    this->queue.push_back(T{});
                    this->allocated_sz += 1u;
                }

                T rs = std::move(this->queue.back()); 
                this->queue.pop_back();

                return std::expected<T, exception_t>(std::move(rs));
            }

            void free(T item) noexcept
            {
                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                this->queue.push_back(std::move(item));
            }
    };

    //OK
    class BatchUpdater: public virtual UpdatableInterface{

        private:

            dg::vector<std::shared_ptr<UpdatableInterface>> update_vec;

        public:

            BatchUpdater(dg::vector<std::shared_ptr<UpdatableInterface>> update_vec) noexcept: update_vec(std::move(update_vec)){}

            void update() noexcept{

                for (const auto& updatable: this->update_vec){
                    updatable->update();
                }
            }
    };

    //OK
    class ASAPScheduler: public virtual SchedulerInterface{

        public:

            auto schedule(Address) noexcept -> std::expected<std::chrono::time_point<std::chrono::utc_clock>, exception_t>{

                return std::chrono::utc_clock::now();
            }

            auto feedback(Address, std::chrono::nanoseconds) noexcept -> exception_t{

                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class ImmutableKernelOutBoundTransmissionController: public virtual KernelOutBoundTransmissionControllerInterface{

        private:

            uint32_t transmit_frequency;

        public:

            ImmutableKernelOutBoundTransmissionController(uint32_t transmit_frequency) noexcept: transmit_frequency(transmit_frequency){}

            auto get_transmit_frequency() noexcept -> uint32_t{

                return this->transmit_frequency;
            }

            auto update_waiting_size(size_t) noexcept -> exception_t{

                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class NoExhaustionController: public virtual ExhaustionControllerInterface{

        public:

            auto is_should_wait() noexcept -> bool{

                return false;
            }

            auto update_waiting_size(size_t) noexcept -> exception_t{

                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class InBoundPacketIntegrityValidator: public virtual PacketIntegrityValidatorInterface{

        private:

            Address host_addr;

        public:

            InBoundPacketIntegrityValidator(Address host_addr) noexcept: host_addr(std::move(host_addr)){}

            auto is_valid(const Packet& packet) noexcept -> exception_t{

                if (packet.xonly_content.valueless_by_exception()){
                    return dg::network_exception::VARIANT_VBE;
                }

                // if (utility::reflectible_is_equal(packet.to_addr, this->host_addr)){
                //     return dg::network_exception::SOCKET_BAD_RECEIPIENT;
                // }

                exception_t addr_chk = utility::validate_addr(packet.fr_addr); 

                if (dg::network_exception::is_failed(addr_chk)){
                    return addr_chk;
                }

                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class DefaultExhaustionController: public virtual ExhaustionControllerInterface{

        public:

            auto is_should_wait() noexcept -> bool{

                return true;
            }

            auto update_waiting_size(size_t) noexcept -> exception_t{

                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class IncrementalIDGenerator: public virtual IDGeneratorInterface{

        private:

            std::atomic<local_packet_id_t> last_pkt_id;
            stdx::hdi_container<factory_id_t> factory_id;

        public:

            IncrementalIDGenerator(std::atomic<local_packet_id_t> last_pkt_id,
                                   stdx::hdi_container<factory_id_t> factory_id) noexcept: last_pkt_id(last_pkt_id.load()),
                                                                                           factory_id(std::move(factory_id)){}

            auto get() noexcept -> GlobalPacketIdentifier{

                auto rs             = GlobalPacketIdentifier{};
                rs.local_packet_id  = this->last_pkt_id.fetch_add(1u, std::memory_order_relaxed);
                rs.factory_id       = this->factory_id.value;

                return rs;
            }
    };

    //we have to use random_id_generator to avoid coordinated attacks, the chances of packet_id collision are so slim that we dont even care, even if there are collisions, it still follows the rule of 1 request == max_one_receive, ack_sz <= request_sz
    //we see this very often in the kernel TCP coordinated download attacks 
    //affined random function is probably the most important function that we have to implement correctly

    //OK
    class RandomIDGenerator: public virtual IDGeneratorInterface{

        private:

            stdx::hdi_container<factory_id_t> factory_id;

        public:

            RandomIDGenerator(stdx::hdi_container<factory_id_t> factory_id) noexcept: factory_id(std::move(factory_id)){}

            auto get() noexcept -> GlobalPacketIdentifier{

                auto rs             = GlobalPacketIdentifier{};
                rs.local_packet_id  = dg::network_randomizer::randomize_int<local_packet_id_t>();
                rs.factory_id       = this->factory_id.value;

                return rs;
            }
    };

    //OK
    class RequestPacketGenerator: public virtual RequestPacketGeneratorInterface{

        private:

            std::unique_ptr<IDGeneratorInterface> id_gen;
            Address host_addr; 

        public:

            RequestPacketGenerator(std::unique_ptr<IDGeneratorInterface> id_gen,
                                   Address host_addr) noexcept: id_gen(std::move(id_gen)),
                                                                host_addr(std::move(host_addr)){}

            auto get(MailBoxArgument&& arg) noexcept -> std::expected<RequestPacket, exception_t>{

                if (arg.content_sz > constants::MAX_REQUEST_PACKET_CONTENT_SIZE){
                    return std::unexpected(dg::network_exception::SOCKET_BAD_BUFFER_LENGTH);
                }

                RequestPacket pkt           = {};
                pkt.fr_addr                 = this->host_addr;
                pkt.to_addr                 = std::move(arg.to);
                pkt.id                      = this->id_gen->get();
                pkt.retransmission_count    = 0u;
                pkt.priority                = 0u;
                
                try
                {
                    pkt.content = internal_kernel_buffer(std::string_view(static_cast<const char *>(arg.content), arg.content_sz));
                }
                catch (...)
                {
                    return std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
                }

                return pkt;
            }
    };

    //OK
    class AckPacketGenerator: public virtual AckPacketGeneratorInterface{
        

        private:

            std::unique_ptr<IDGeneratorInterface> id_gen;
            Address host_addr;

        public:

            AckPacketGenerator(std::unique_ptr<IDGeneratorInterface> id_gen,
                               Address host_addr) noexcept: id_gen(std::move(id_gen)),
                                                            host_addr(std::move(host_addr)){}

            auto get(Address to_addr, PacketBase * pkt_base_arr, size_t sz) noexcept -> std::expected<AckPacket, exception_t>{

                if (sz > constants::MAX_ACK_PER_PACKET){
                    return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
                }

                auto ack_vec                = dg::network_exception::cstyle_initialize<internal_vector<PacketBase>>(sz);

                if (!ack_vec.has_value()) [[unlikely]]{
                    return std::unexpected(ack_vec.error());
                }

                AckPacket pkt               = {};
                pkt.fr_addr                 = this->host_addr;
                pkt.to_addr                 = to_addr;
                pkt.id                      = this->id_gen->get();
                pkt.retransmission_count    = 0u;
                pkt.ack_vec                 = std::move(ack_vec.value());

                std::copy(pkt_base_arr, std::next(pkt_base_arr, sz), pkt.ack_vec.begin());
                pkt.priority                = this->get_priority(pkt.ack_vec);

                return pkt;
            }

        private:

            auto get_priority(const internal_vector<PacketBase>& pkt_base_vec) noexcept -> uint8_t{

                if (pkt_base_vec.size() == 0u){
                    return 0u;
                }

                uint8_t rs = pkt_base_vec[0].retransmission_count;

                for (size_t i = 1u; i < pkt_base_vec.size(); ++i){
                    rs = std::max(rs, pkt_base_vec[i].retransmission_count);
                }

                return rs;
            }
    };

    //OK
    class KRescuePacketGenerator: public virtual KRescuePacketGeneratorInterface{

        private:

            std::unique_ptr<IDGeneratorInterface> id_gen;
            Address host_addr;

        public:

            KRescuePacketGenerator(std::unique_ptr<IDGeneratorInterface> id_gen,
                                   Address host_addr) noexcept: id_gen(std::move(id_gen)),
                                                                host_addr(std::move(host_addr)){}

            auto get() noexcept -> std::expected<KRescuePacket, exception_t>{

                KRescuePacket pkt           = {};
                pkt.fr_addr                 = this->host_addr;
                pkt.to_addr                 = this->host_addr;
                pkt.id                      = this->id_gen->get();
                pkt.retransmission_count    = 0u;
                pkt.priority                = 0u;

                return pkt;
            }
    };

    //OK
    class KernelRescuePost: public virtual KernelRescuePostInterface{

        private:

            std::atomic<std::chrono::time_point<std::chrono::utc_clock>> ts;

        public:

            using Self = KernelRescuePost;
            static inline constexpr std::chrono::time_point<std::chrono::utc_clock> NULL_TIMEPOINT = std::chrono::time_point<std::chrono::utc_clock>::max(); 

            KernelRescuePost(std::chrono::time_point<std::chrono::utc_clock> ts) noexcept: ts(ts){}

            auto heartbeat() noexcept -> exception_t{

                this->ts.exchange(std::chrono::utc_clock::now(), std::memory_order_relaxed);
                return dg::network_exception::SUCCESS;
            }

            auto last_heartbeat() noexcept -> std::expected<std::optional<std::chrono::time_point<std::chrono::utc_clock>>, exception_t>{

                std::chrono::time_point<std::chrono::utc_clock> rs = this->ts.load(std::memory_order_relaxed);

                if (rs == Self::NULL_TIMEPOINT) [[unlikely]]{
                    return std::optional<std::chrono::time_point<std::chrono::utc_clock>>(std::nullopt);
                } else [[likely]]{
                    return std::optional<std::chrono::time_point<std::chrono::utc_clock>>(rs);
                }
            }

            void reset() noexcept{

                this->ts.exchange(Self::NULL_TIMEPOINT, std::memory_order_relaxed);
            }
    };

    class StaticRetransmissionDelayNegotiator: public virtual RetransmissionDelayNegotiatorInterface{

        private:

            std::chrono::nanoseconds delay_interval;
        
        public:

            StaticRetransmissionDelayNegotiator(std::chrono::nanoseconds delay_interval) noexcept: delay_interval(delay_interval){}

            auto get(const Address& to_addr) noexcept -> std::expected<std::chrono::nanoseconds, exception_t>{

                return this->delay_interval;
            }
    };

    class EmptyRetransmissionController: public virtual RetransmissionControllerInterface{

        public:

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void ack(global_packet_id_t * id_arr, size_t id_arr_sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, id_arr_sz), dg::network_exception::SUCCESS);
            }

            void get_retriables(Packet * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept
            {
                output_arr_sz = 0u;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return 1024u;
            }
    };

    //OK
    class MemoryEfficientRetransmissionController: public virtual RetransmissionControllerInterface{

        private:

            dg::pow2_cyclic_queue<RetransmissionWaitingItem> push_wait_queue_1;
            dg::pow2_cyclic_queue<RetransmissionWaitingItem> push_wait_queue_2;
            data_structure::temporal_ordered_packet_map pkt_map;
            data_structure::temporal_finite_unordered_set<global_packet_id_t> acked_id_hashset;
            std::shared_ptr<packet_controller::RetransmissionDelayNegotiatorInterface> delay_negotiator;
            size_t ticking_clock_resolution;
            size_t max_retransmission_sz;
            size_t retriable_pkt_map_cap;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            MemoryEfficientRetransmissionController(dg::pow2_cyclic_queue<RetransmissionWaitingItem> push_wait_queue_1,
                                                    dg::pow2_cyclic_queue<RetransmissionWaitingItem> push_wait_queue_2,
                                                    data_structure::temporal_ordered_packet_map pkt_map,
                                                    data_structure::temporal_finite_unordered_set<global_packet_id_t> acked_id_hashset,
                                                    std::shared_ptr<packet_controller::RetransmissionDelayNegotiatorInterface> delay_negotiator,
                                                    size_t ticking_clock_resolution,
                                                    size_t max_retransmission_sz,
                                                    size_t retriable_pkt_map_cap,
                                                    std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                                    size_t consume_sz_per_load): push_wait_queue_1(std::move(push_wait_queue_1)),
                                                                                 push_wait_queue_2(std::move(push_wait_queue_2)),
                                                                                 pkt_map(std::move(pkt_map)),
                                                                                 acked_id_hashset(std::move(acked_id_hashset)),
                                                                                 delay_negotiator(std::move(delay_negotiator)),
                                                                                 ticking_clock_resolution(ticking_clock_resolution),
                                                                                 max_retransmission_sz(max_retransmission_sz),
                                                                                 retriable_pkt_map_cap(retriable_pkt_map_cap),
                                                                                 mtx(std::move(mtx)),
                                                                                 consume_sz_per_load(stdx::hdi_container<size_t>{consume_sz_per_load}){}

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->internal_add_retriables(pkt_arr, sz, exception_arr,
                                              std::min(this->retriable_pkt_map_cap, this->pkt_map.capacity()),
                                              this->push_wait_queue_1);
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->internal_add_retriables(pkt_arr, sz, exception_arr,
                                              this->pkt_map.capacity(),
                                              this->push_wait_queue_2);
            }

            void ack(global_packet_id_t * id_arr, size_t id_arr_sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (id_arr_sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < id_arr_sz; ++i){
                    this->pkt_map.erase(id_arr[i]);
                    this->acked_id_hashset.insert(id_arr[i]);

                    exception_arr[i] = dg::network_exception::SUCCESS;
                }

                this->fulfill_wait_queue();
            }

            void get_retriables(Packet * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                output_arr_sz = 0u;
                std::chrono::time_point<std::chrono::utc_clock> time_bar = std::chrono::utc_clock::now();

                while (true){
                    if (output_arr_sz == output_arr_cap){
                        return;
                    }

                    std::optional<Packet> nxt = this->pkt_map.get_expired_packet(time_bar);

                    if (!nxt.has_value()){
                        return;
                    }

                    output_arr[output_arr_sz++] = std::move(nxt.value());
                }

                this->fulfill_wait_queue();
            }

            auto max_consume_size() noexcept -> size_t{
                
                return this->consume_sz_per_load.value;
            }

        private:

            void internal_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr,
                                         size_t map_cap,
                                         dg::pow2_cyclic_queue<RetransmissionWaitingItem>& push_wait_queue) noexcept
            {
                size_t waiting_queue_cap    = sz;
                dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<Packet>[]> waiting_queue(waiting_queue_cap);
                dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<exception_t>[]> exception_ptr_arr(waiting_queue_cap);
                size_t waiting_queue_sz     = 0u;
                std::binary_semaphore waiting_smp(0);

                {
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (sz > this->max_consume_size()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    Packet * base_pkt_arr   = pkt_arr.base(); 
                    auto clock              = dg::ticking_clock<std::chrono::utc_clock>(this->ticking_clock_resolution);

                    for (size_t i = 0u; i < sz; ++i){
                        if (base_pkt_arr[i].retransmission_count >= this->max_retransmission_sz){
                            exception_arr[i] = dg::network_exception::SOCKET_MAX_RETRANSMISSION_REACHED;
                            continue;
                        }

                        if (this->acked_id_hashset.contains(base_pkt_arr[i].id)){
                            exception_arr[i] = dg::network_exception::SOCKET_ADD_ACKED_PACKET;
                            continue;
                        }

                        std::expected<std::chrono::nanoseconds, exception_t> delay = this->delay_negotiator->get(base_pkt_arr[i].to_addr);

                        if (!delay.has_value()){
                            exception_arr[i] = delay.error();
                            continue;
                        }

                        if (this->pkt_map.size() >= map_cap){
                            waiting_queue[waiting_queue_sz]     = std::next(base_pkt_arr, i);
                            exception_ptr_arr[waiting_queue_sz] = std::next(exception_arr, i);
                            waiting_queue_sz                    += 1;
                            exception_arr[i]                    = dg::network_exception::SUCCESS;

                            continue;
                        }

                        base_pkt_arr[i].retransmission_count    += 1;
                        exception_t err                         = this->pkt_map.add(std::move(base_pkt_arr[i]), clock.get() + delay.value());

                        if (dg::network_exception::is_failed(err)){
                            base_pkt_arr[i].retransmission_count    -= 1;
                            exception_arr[i]                        = err;

                            continue;
                        }

                        exception_arr[i] = dg::network_exception::SUCCESS;
                    }

                    if (waiting_queue_sz != 0u)
                    {
                        dg::network_exception_handler::nothrow_log(push_wait_queue.push_back(RetransmissionWaitingItem{
                            .pkt_arr        = waiting_queue.get(),
                            .exception_arr  = exception_ptr_arr.get(),
                            .pkt_arr_sz     = waiting_queue_sz,
                            .smp            = &waiting_smp
                        }));
                    }
                }

                if (waiting_queue_sz != 0u)
                {
                    waiting_smp.acquire();
                }
            }

            auto move_to_pkt_map(RetransmissionWaitingItem wait_item,
                                 size_t push_sz) noexcept -> std::optional<RetransmissionWaitingItem>
            {
                size_t old_sz   = this->pkt_map.size();
                size_t new_sz   = old_sz + push_sz;

                if (new_sz > this->pkt_map.capacity() || wait_item.pkt_arr_sz < push_sz)
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                    else
                    {
                        std::unreachable();
                    }
                }

                Packet ** pkt_arr               = wait_item.pkt_arr;
                exception_t ** exception_arr    = wait_item.exception_arr; 
                auto clock                      = dg::ticking_clock<std::chrono::utc_clock>(this->ticking_clock_resolution);

                for (size_t i = 0u; i < push_sz; ++i)
                {
                    std::expected<std::chrono::nanoseconds, exception_t> delay = this->delay_negotiator->get(pkt_arr[i]->to_addr);

                    if (!delay.has_value())
                    {
                        *exception_arr[i] = delay.error();
                        continue;                        
                    }

                    pkt_arr[i]->retransmission_count    += 1;
                    exception_t err                     = this->pkt_map.add(std::move(*pkt_arr[i]), clock.get() + delay.value());

                    if (dg::network_exception::is_failed(err))
                    {
                        pkt_arr[i]->retransmission_count    -= 1;
                        *exception_arr[i]                   = err;

                        continue;
                    }

                    *exception_arr[i] = dg::network_exception::SUCCESS;
                }

                if (push_sz == wait_item.pkt_arr_sz)
                {
                    return std::nullopt;
                }

                size_t rem_sz = 0u; 

                for (size_t i = push_sz; i < wait_item.pkt_arr_sz; ++i)
                {
                    pkt_arr[rem_sz]         = pkt_arr[i];
                    exception_arr[rem_sz]   = exception_arr[i];
                    rem_sz                  += 1;
                }

                return RetransmissionWaitingItem
                {
                    .pkt_arr        = pkt_arr,
                    .exception_arr  = exception_arr,
                    .pkt_arr_sz     = rem_sz,
                    .smp            = wait_item.smp
                };

            } 

            void fulfill_one_push_wait_queue_2() noexcept
            {
                if (this->push_wait_queue_2.empty())
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                    else
                    {
                        std::unreachable();
                    }
                }

                RetransmissionWaitingItem resolutable = this->push_wait_queue_2.front();

                size_t pushable_cap = this->pkt_map.capacity() - this->pkt_map.size(); 
                size_t push_sz      = std::min(pushable_cap, resolutable.pkt_arr_sz);

                std::optional<RetransmissionWaitingItem> rem = this->move_to_pkt_map(resolutable, push_sz);

                if (rem.has_value())
                {
                    this->push_wait_queue_2.front() = rem.value();
                }
                else
                {
                    this->push_wait_queue_2.pop_front();
                    resolutable.smp->release();
                }
            }

            void fulfill_one_push_wait_queue_1() noexcept
            {
                if (this->push_wait_queue_1.empty())
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                    else
                    {
                        std::unreachable();
                    }
                }

                RetransmissionWaitingItem resolutable = this->push_wait_queue_1.front();

                if (this->pkt_map.size() > this->retriable_pkt_map_cap)
                {
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                    else
                    {
                        std::unreachable();
                    }
                }

                size_t pushable_cap = this->retriable_pkt_map_cap - this->pkt_map.size();
                size_t push_sz      = std::min(pushable_cap, resolutable.pkt_arr_sz);

                std::optional<RetransmissionWaitingItem> rem = this->move_to_pkt_map(resolutable, push_sz);

                if (rem.has_value())
                {
                    this->push_wait_queue_1.front() = rem.value();
                }
                else
                {
                    this->push_wait_queue_1.pop_front();
                    resolutable.smp->release();
                }
            }

            void fulfill_wait_queue() noexcept
            {
                while (true)
                {
                    if (this->pkt_map.size() == this->pkt_map.capacity())
                    {
                        break;
                    }

                    if (this->pkt_map.size() >= this->retriable_pkt_map_cap)
                    {
                        if (this->push_wait_queue_2.empty())
                        {
                            break;
                        }

                        this->fulfill_one_push_wait_queue_2();
                        continue;
                    }

                    if (this->push_wait_queue_1.empty())
                    {
                        if (this->push_wait_queue_2.empty())
                        {
                            break;
                        }

                        this->fulfill_one_push_wait_queue_2();
                        continue;
                    }

                    if (this->push_wait_queue_2.empty())
                    {
                        this->fulfill_one_push_wait_queue_1();
                        continue;
                    }

                    bool coin_flip      = dg::network_randomizer::randomize_int<bool>();
                    bool coin_flip_2    = dg::network_randomizer::randomize_int<bool>();

                    if (coin_flip)
                    {
                        if (coin_flip_2)
                        {
                            this->fulfill_one_push_wait_queue_1();
                            continue;
                        }
                        else
                        {
                            this->fulfill_one_push_wait_queue_2();
                            continue;
                        }
                    }
                    else
                    {
                        this->fulfill_one_push_wait_queue_1();
                        continue;
                    }
                }
            }
    };

    //OK
    class RetransmissionController: public virtual RetransmissionControllerInterface{

        private:

            dg::pow2_cyclic_queue<QueuedPacket> pkt_deque;
            data_structure::temporal_finite_unordered_set<global_packet_id_t> acked_id_hashset;
            std::chrono::nanoseconds transmission_delay_time;
            size_t max_retransmission_sz;
            size_t pkt_deque_capacity;
            size_t retriable_pkt_map_cap;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            RetransmissionController(dg::pow2_cyclic_queue<QueuedPacket> pkt_deque,
                                     data_structure::temporal_finite_unordered_set<global_packet_id_t> acked_id_hashset,
                                     std::chrono::nanoseconds transmission_delay_time,
                                     size_t max_retransmission_sz,
                                     size_t pkt_deque_capacity,
                                     size_t retriable_pkt_map_cap,
                                     std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                     stdx::hdi_container<size_t> consume_sz_per_load) noexcept: pkt_deque(std::move(pkt_deque)),
                                                                                                acked_id_hashset(std::move(acked_id_hashset)),
                                                                                                transmission_delay_time(transmission_delay_time),
                                                                                                max_retransmission_sz(max_retransmission_sz),
                                                                                                pkt_deque_capacity(pkt_deque_capacity),
                                                                                                retriable_pkt_map_cap(retriable_pkt_map_cap),
                                                                                                mtx(std::move(mtx)),
                                                                                                consume_sz_per_load(std::move(consume_sz_per_load)){}

            void internal_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr, size_t map_cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto now                = this->get_now();
                Packet * base_pkt_arr   = pkt_arr.base(); 

                for (size_t i = 0u; i < sz; ++i){
                    if (this->pkt_deque.size() >= map_cap){
                        exception_arr[i] = dg::network_exception::SOCKET_QUEUE_FULL;
                        continue;
                    }

                    if (base_pkt_arr[i].retransmission_count >= this->max_retransmission_sz){ //it seems like this is the packet responsibility yet I think this is the retransmission responsibility - to avoid system flooding
                        exception_arr[i] = dg::network_exception::SOCKET_MAX_RETRANSMISSION_REACHED;
                        continue;
                    }

                    QueuedPacket queued_pkt             = {};
                    queued_pkt.pkt                      = std::move(base_pkt_arr[i]);
                    queued_pkt.pkt.retransmission_count += 1;
                    queued_pkt.queued_time              = now;

                    dg::network_exception_handler::nothrow_log(this->pkt_deque.push_back(std::move(queued_pkt)));
                    exception_arr[i]                    = dg::network_exception::SUCCESS;
                }
            }

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->internal_add_retriables(pkt_arr, sz, exception_arr, std::min(this->retriable_pkt_map_cap, this->pkt_deque.capacity()));
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->internal_add_retriables(pkt_arr, sz, exception_arr, this->pkt_deque.capacity());
            }

            void ack(global_packet_id_t * pkt_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i){
                    this->acked_id_hashset.insert(pkt_id_arr[i]);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            void get_retriables(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                std::chrono::time_point<std::chrono::steady_clock> time_bar = std::chrono::steady_clock::now() - this->transmission_delay_time;

                auto key            = QueuedPacket{};
                key.queued_time     = time_bar;

                //lower bound is fairly expensive, we dont worry about that now
                auto last           = std::lower_bound(this->pkt_deque.begin(), this->pkt_deque.end(), 
                                                       key, 
                                                       [](const auto& lhs, const auto& rhs){return lhs.queued_time < rhs.queued_time;});

                size_t barred_sz    = std::distance(this->pkt_deque.begin(), last);
                sz                  = 0u;
                size_t iterated_sz  = {};

                for (iterated_sz = 0u; iterated_sz < barred_sz; ++iterated_sz){
                    if (sz == output_pkt_arr_cap){
                        break;
                    }

                    if (this->acked_id_hashset.contains(this->pkt_deque[iterated_sz].pkt.id)){
                        continue;
                    }

                    output_pkt_arr[sz++] = std::move(this->pkt_deque[iterated_sz].pkt);
                }

                this->pkt_deque.erase_front_range(iterated_sz);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }

        private:

            auto get_now() const noexcept -> std::chrono::time_point<std::chrono::steady_clock>{

                if (this->pkt_deque.empty()){
                    return std::chrono::steady_clock::now();
                }

                return std::max(this->pkt_deque.back().queued_time, std::chrono::steady_clock::now());
            }
    };

    //OK
    class ExhaustionControlledRetransmissionController: public virtual RetransmissionControllerInterface{

        private:

            std::unique_ptr<RetransmissionControllerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledRetransmissionController(std::unique_ptr<RetransmissionControllerInterface> base,
                                                         std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                         std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                                            executor(std::move(executor)),
                                                                                                                                                            exhaustion_controller(std::move(exhaustion_controller)){}

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                Packet * pkt_arr_base               = pkt_arr.base();
                Packet * pkt_arr_first              = pkt_arr_base;
                Packet * pkt_arr_last               = std::next(pkt_arr_first, sz);
                exception_t * exception_arr_first   = exception_arr;
                exception_t * exception_arr_last    = std::next(exception_arr_first, sz);
                size_t sliding_window_sz            = sz;

                auto task = [&, this]() noexcept{
                    this->base->add_retriables(std::make_move_iterator(pkt_arr_first), sliding_window_sz, exception_arr_first);

                    size_t waiting_sz                   = std::count(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t err                     = this->exhaustion_controller->update_waiting_size(waiting_sz); 

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    exception_t * retriable_eptr_first  = std::find(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t * retriable_eptr_last   = std::find_if(retriable_eptr_first, exception_arr_last, [](exception_t err){return err != dg::network_exception::SOCKET_QUEUE_FULL;});
                    size_t relative_offset              = std::distance(exception_arr_first, retriable_eptr_first);
                    sliding_window_sz                   = std::distance(retriable_eptr_first, retriable_eptr_last);

                    std::advance(pkt_arr_first, relative_offset);
                    std::advance(exception_arr_first, relative_offset);

                    return !this->exhaustion_controller->is_should_wait() || (pkt_arr_first == pkt_arr_last); //TODOs: we want to subscribe these guys to a load_balancer system
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(std::move(task));
                this->executor->exec(virtual_task);
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->base->reentrant_add_retriables(pkt_arr, sz, exception_arr);
            }

            void ack(global_packet_id_t * packet_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->ack(packet_id_arr, sz, exception_arr);
            }

            void get_retriables(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                this->base->get_retriables(output_pkt_arr, sz, output_pkt_arr_cap);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class HashDistributedRetransmissionController: public virtual RetransmissionControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<RetransmissionControllerInterface>[]> retransmission_controller_vec;
            size_t pow2_retransmission_controller_vec_sz;
            size_t keyvalue_aggregation_cap;
            size_t consume_sz_per_load;

        public:

            HashDistributedRetransmissionController(std::unique_ptr<std::unique_ptr<RetransmissionControllerInterface>[]> retransmission_controller_vec,
                                                    size_t pow2_retransmission_controller_vec_sz,
                                                    size_t keyvalue_aggregation_cap,
                                                    size_t consume_sz_per_load) noexcept: retransmission_controller_vec(std::move(retransmission_controller_vec)),
                                                                                          pow2_retransmission_controller_vec_sz(pow2_retransmission_controller_vec_sz),
                                                                                          keyvalue_aggregation_cap(keyvalue_aggregation_cap),
                                                                                          consume_sz_per_load(consume_sz_per_load){}

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                Packet * base_pkt_arr                       = pkt_arr.base(); 

                auto internal_resolutor                     = InternalRetriableDeliveryResolutor{};
                internal_resolutor.dst_vec                  = this->retransmission_controller_vec.get();

                size_t trimmed_keyvalue_aggregation_cap     = std::min(this->keyvalue_aggregation_cap, sz);
                size_t deliverer_allocation_cost            = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&internal_resolutor, trimmed_keyvalue_aggregation_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_mem(deliverer_allocation_cost);
                auto deliverer                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&internal_resolutor, trimmed_keyvalue_aggregation_cap, deliverer_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed                   = dg::network_hash::hash_reflectible(base_pkt_arr[i].id);
                    size_t partitioned_idx          = hashed & (this->pow2_retransmission_controller_vec_sz - 1u);

                    auto delivery_argument          = InternalRetriableDeliveryArgument{};
                    delivery_argument.pkt           = std::move(base_pkt_arr[i]);
                    delivery_argument.fallback_pkt  = std::next(base_pkt_arr, i);
                    delivery_argument.exception_ptr = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(deliverer.get(), partitioned_idx, std::move(delivery_argument));
                }
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                Packet * base_pkt_arr                       = pkt_arr.base();

                auto internal_resolutor                     = InternalReentrantRetriableDeliveryResolutor{};
                internal_resolutor.dst_vec                  = this->retransmission_controller_vec.get();

                size_t trimmed_keyvalue_aggregation_cap     = std::min(this->keyvalue_aggregation_cap, sz);
                size_t deliverer_allocation_cost            = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&internal_resolutor, trimmed_keyvalue_aggregation_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_mem(deliverer_allocation_cost);
                auto deliverer                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&internal_resolutor, trimmed_keyvalue_aggregation_cap, deliverer_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t hashed                   = dg::network_hash::hash_reflectible(base_pkt_arr[i].id);
                    size_t partitioned_idx          = hashed & (this->pow2_retransmission_controller_vec_sz - 1u);

                    auto delivery_argument          = InternalReentrantRetriableDeliveryArgument{};
                    delivery_argument.pkt           = std::move(base_pkt_arr[i]);
                    delivery_argument.fallback_pkt  = std::next(base_pkt_arr, i);
                    delivery_argument.exception_ptr = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(deliverer.get(), partitioned_idx, std::move(delivery_argument));
                }
            }

            void ack(global_packet_id_t * pkt_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                auto internal_resolutor                     = InternalAckDeliveryResolutor{};
                internal_resolutor.dst_vec                  = this->retransmission_controller_vec.get(); 

                size_t trimmed_keyvalue_aggregation_cap     = std::min(this->keyvalue_aggregation_cap, sz);
                size_t deliverer_allocation_cost            = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&internal_resolutor, trimmed_keyvalue_aggregation_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_mem(deliverer_allocation_cost);
                auto deliverer                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&internal_resolutor, trimmed_keyvalue_aggregation_cap, deliverer_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    auto delivery_argument          = InternalAckDeliveryArgument{};
                    delivery_argument.pkt_id        = pkt_id_arr[i];
                    delivery_argument.exception_ptr = std::next(exception_arr, i); 
                    size_t hashed                   = dg::network_hash::hash_reflectible(pkt_id_arr[i]);
                    size_t partitioned_idx          = hashed & (this->pow2_retransmission_controller_vec_sz - 1u);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(deliverer.get(), partitioned_idx, delivery_argument);
                }
            }

            void get_retriables(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                sz = 0u;
                size_t seed = dg::network_randomizer::randomize_int<size_t>() >> 1; 

                for (size_t i = 0u; i < this->pow2_retransmission_controller_vec_sz; ++i){
                    size_t remaining_cap = output_pkt_arr_cap - sz;

                    if (remaining_cap == 0u){
                        return;
                    }

                    size_t idx = (seed + i) & (this->pow2_retransmission_controller_vec_sz - 1u);
                    Packet * tmp_output_pkt_arr = std::next(output_pkt_arr, sz);
                    size_t tmp_sz{};

                    this->retransmission_controller_vec[idx]->get_retriables(tmp_output_pkt_arr, tmp_sz, remaining_cap);
                    sz += tmp_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
        
        private:

            struct InternalRetriableDeliveryArgument{
                Packet pkt;
                Packet * fallback_pkt;
                exception_t * exception_ptr;
            };

            struct InternalRetriableDeliveryResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalRetriableDeliveryArgument>{

                std::unique_ptr<RetransmissionControllerInterface> * dst_vec;

                void push(const size_t& idx, std::move_iterator<InternalRetriableDeliveryArgument *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> pkt_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    InternalRetriableDeliveryArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        pkt_arr[i] = std::move(base_data_arr[i].pkt);
                    }

                    this->dst_vec[idx]->add_retriables(std::make_move_iterator(pkt_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].fallback_pkt  = std::move(pkt_arr[i]);
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        }
                    }
                }
            };

            struct InternalReentrantRetriableDeliveryArgument
            {
                Packet pkt;
                Packet * fallback_pkt;
                exception_t * exception_ptr;
            };

            struct InternalReentrantRetriableDeliveryResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalReentrantRetriableDeliveryArgument>
            {
                std::unique_ptr<RetransmissionControllerInterface> * dst_vec;

                void push(const size_t& idx, std::move_iterator<InternalReentrantRetriableDeliveryArgument *> data_arr, size_t sz) noexcept
                {
                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> pkt_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    InternalReentrantRetriableDeliveryArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        pkt_arr[i] = std::move(base_data_arr[i].pkt);
                    }

                    this->dst_vec[idx]->reentrant_add_retriables(std::make_move_iterator(pkt_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (dg::network_exception::is_failed(exception_arr[i]))
                        {
                            *base_data_arr[i].fallback_pkt  = std::move(pkt_arr[i]);
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        }
                    }
                }
            };
            
            struct InternalAckDeliveryArgument{
                global_packet_id_t pkt_id;
                exception_t * exception_ptr;
            };

            struct InternalAckDeliveryResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalAckDeliveryArgument>{

                std::unique_ptr<RetransmissionControllerInterface> * dst_vec;

                void push(const size_t& idx, std::move_iterator<InternalAckDeliveryArgument *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<global_packet_id_t[]> pkt_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    InternalAckDeliveryArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        pkt_id_arr[i] = base_data_arr[i].pkt_id;
                    }

                    this->dst_vec[idx]->ack(pkt_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        }
                    }
                }
            };
    };

    //OK
    class ReactingRetransmissionController: public virtual RetransmissionControllerInterface{

        private:

            std::unique_ptr<RetransmissionControllerInterface> base;
            std::unique_ptr<ComplexReactor> reactor;
            std::chrono::nanoseconds max_wait_time;

        public:

            ReactingRetransmissionController(std::unique_ptr<RetransmissionControllerInterface> base,
                                             std::unique_ptr<ComplexReactor> reactor,
                                             std::chrono::nanoseconds max_wait_time) noexcept: base(std::move(base)),
                                                                                               reactor(std::move(reactor)),
                                                                                               max_wait_time(max_wait_time){}

            void add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->add_retriables(pkt_arr, sz, exception_arr);
                size_t thru_sz = std::count(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
                this->reactor->increment(thru_sz);
            }

            void reentrant_add_retriables(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                this->base->reentrant_add_retriables(pkt_arr, sz, exception_arr);
                size_t thru_sz = std::count(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
                this->reactor->increment(thru_sz);
            }

            void ack(global_packet_id_t * pkt_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->ack(pkt_id_arr, sz, exception_arr);
            }

            void get_retriables(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->reactor->subscribe(this->max_wait_time);
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->base->get_retriables(output_pkt_arr, sz, output_pkt_arr_cap);
                this->reactor->decrement(sz);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class BufferFIFOContainer: public virtual BufferContainerInterface{

        private:

            dg::pow2_cyclic_queue<BufferContainerWaitingItem> waiting_item_vec;
            dg::pow2_cyclic_queue<internal_kernel_buffer> buffer_vec;
            size_t buffer_vec_capacity;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            BufferFIFOContainer(dg::pow2_cyclic_queue<BufferContainerWaitingItem> waiting_item_vec,
                                dg::pow2_cyclic_queue<internal_kernel_buffer> buffer_vec,
                                size_t buffer_vec_capacity,
                                std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                stdx::hdi_container<size_t> consume_sz_per_load) noexcept: waiting_item_vec(std::move(waiting_item_vec)),
                                                                                           buffer_vec(std::move(buffer_vec)),
                                                                                           buffer_vec_capacity(buffer_vec_capacity),
                                                                                           mtx(std::move(mtx)),
                                                                                           consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<internal_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                std::binary_semaphore smp(0); 

                bool need_acquire = [&]{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (sz > this->max_consume_size()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    size_t app_cap  = this->buffer_vec_capacity - this->buffer_vec.size();
                    size_t app_sz   = std::min(sz, app_cap);
                    size_t old_sz   = this->buffer_vec.size();
                    size_t new_sz   = old_sz + app_sz;

                    this->buffer_vec.resize(new_sz);

                    std::copy(buffer_arr, std::next(buffer_arr, app_sz), std::next(this->buffer_vec.begin(), old_sz));

                    if (app_sz != sz)
                    {
                        dg::network_exception_handler::nothrow_log(this->waiting_item_vec.push_back(BufferContainerWaitingItem
                        {
                            .buffer_arr         = std::next(buffer_arr, app_sz),
                            .buffer_arr_sz      = sz - app_sz,
                            .smp                = &smp 
                        }));

                        return true;
                    }

                    return false;
                }();

                if (need_acquire)
                {
                    smp.acquire();
                }

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void pop(internal_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                sz = 0u;

                while (true)
                {
                    if (sz == output_buffer_arr_cap)
                    {
                        break;
                    }

                    size_t rem_cap = output_buffer_arr_cap - sz; 

                    if (!this->waiting_item_vec.empty())
                    {
                        size_t app_sz = std::min(rem_cap, this->waiting_item_vec.front().buffer_arr_sz);

                        std::copy(this->waiting_item_vec.front().buffer_arr,
                                  std::next(this->waiting_item_vec.front().buffer_arr, app_sz),
                                  std::next(output_buffer_arr, sz));

                        if (app_sz == this->waiting_item_vec.front().buffer_arr_sz)
                        {
                            this->waiting_item_vec.front().smp->release();
                            this->waiting_item_vec.pop_front();
                        }
                        else
                        {
                            this->waiting_item_vec.front() = BufferContainerWaitingItem
                            {
                                .buffer_arr     = std::next(this->waiting_item_vec.front().buffer_arr, app_sz),
                                .buffer_arr_sz  = this->waiting_item_vec.front().buffer_arr_sz - app_sz,
                                .smp            = this->waiting_item_vec.front().smp
                            };
                        }

                        sz += app_sz;
                        continue;
                    }

                    if (!this->buffer_vec.empty())
                    {
                        size_t app_sz = std::min(rem_cap, this->buffer_vec.size());

                        std::copy(std::make_move_iterator(this->buffer_vec.begin()),
                                  std::make_move_iterator(std::next(this->buffer_vec.begin(), app_sz)),
                                  std::next(output_buffer_arr, sz));
                        this->buffer_vec.erase_front_range(app_sz);

                        sz += app_sz;
                        continue;
                    }

                    break;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class ExhaustionControlledBufferContainer: public virtual BufferContainerInterface{

        private:

            std::unique_ptr<BufferContainerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledBufferContainer(std::unique_ptr<BufferContainerInterface> base,
                                                std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                                   executor(std::move(executor)),
                                                                                                                                                   exhaustion_controller(std::move(exhaustion_controller)){}

            void push(std::move_iterator<internal_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                internal_kernel_buffer * buffer_arr_base    = buffer_arr.base();
                internal_kernel_buffer * buffer_arr_first   = buffer_arr_base;
                internal_kernel_buffer * buffer_arr_last    = std::next(buffer_arr_first, sz);
                exception_t * exception_arr_first           = exception_arr;
                exception_t * exception_arr_last            = std::next(exception_arr_first, sz);
                size_t sliding_window_sz                    = sz;

                auto task = [&, this]() noexcept{
                    this->base->push(std::make_move_iterator(buffer_arr_first), sliding_window_sz, exception_arr_first);

                    size_t waiting_sz                   = std::count(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t err                     = this->exhaustion_controller->update_waiting_size(waiting_sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    exception_t * retriable_arr_first   = std::find(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t * retriable_arr_last    = std::find_if(retriable_arr_first, exception_arr_last, [](exception_t err){return err != dg::network_exception::SOCKET_QUEUE_FULL;});

                    size_t relative_offset              = std::distance(exception_arr_first, retriable_arr_first);
                    sliding_window_sz                   = std::distance(retriable_arr_first, retriable_arr_last);

                    std::advance(buffer_arr_first, relative_offset);
                    std::advance(exception_arr_first, relative_offset); 

                    return !this->exhaustion_controller->is_should_wait() || (buffer_arr_first == buffer_arr_last); //TODOs: we want to subscribe these guys to a load_balancer system
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(std::move(task));
                this->executor->exec(virtual_task);
            }

            void pop(internal_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                this->base->pop(output_buffer_arr, sz, output_buffer_arr_cap);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class HashDistributedBufferContainer: public virtual BufferContainerInterface{

        private:

            std::unique_ptr<std::unique_ptr<BufferContainerInterface>[]> buffer_container_vec;
            size_t pow2_buffer_container_vec_sz;
            size_t consume_sz_per_load; 

        public:

            HashDistributedBufferContainer(std::unique_ptr<std::unique_ptr<BufferContainerInterface>[]> buffer_container_vec,
                                           size_t pow2_buffer_container_vec_sz,
                                           size_t consume_sz_per_load) noexcept: buffer_container_vec(std::move(buffer_container_vec)),
                                                                                 pow2_buffer_container_vec_sz(pow2_buffer_container_vec_sz),
                                                                                 consume_sz_per_load(consume_sz_per_load){}

            void push(std::move_iterator<internal_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t random_value = dg::network_randomizer::randomize_int<size_t>();
                size_t random_idx   = random_value & (this->pow2_buffer_container_vec_sz - 1u);

                this->buffer_container_vec[random_idx]->push(buffer_arr, sz, exception_arr);
            }

            void pop(internal_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                sz = 0u;
                size_t seed = dg::network_randomizer::randomize_int<size_t>() >> 1; 

                for (size_t i = 0u; i < this->pow2_buffer_container_vec_sz; ++i){
                    size_t remaining_cap = output_buffer_arr_cap - sz;

                    if (remaining_cap == 0u){
                        return;
                    }

                    size_t idx = (seed + i) & (this->pow2_buffer_container_vec_sz - 1u);
                    internal_kernel_buffer * tmp_output_buffer_arr = std::next(output_buffer_arr, sz); 
                    size_t tmp_sz{};
                    this->buffer_container_vec[idx]->pop(tmp_output_buffer_arr, tmp_sz, remaining_cap);
                    sz += tmp_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
    };

    //OK
    class ReactingBufferContainer: public virtual BufferContainerInterface,
                                   public virtual ContainerBusyAdapterInterface{

        private:

            std::unique_ptr<BufferContainerInterface> base;
            std::unique_ptr<ComplexReactor> reactor;
            std::chrono::nanoseconds max_wait_time;
            const size_t busy_threshold;
            const size_t ease_threshold; 

        public:

            ReactingBufferContainer(std::unique_ptr<BufferContainerInterface> base,
                                    std::unique_ptr<ComplexReactor> reactor,
                                    std::chrono::nanoseconds max_wait_time,
                                    size_t busy_threshold,
                                    size_t ease_threshold) noexcept: base(std::move(base)),
                                                                     reactor(std::move(reactor)),
                                                                     max_wait_time(max_wait_time),
                                                                     busy_threshold(busy_threshold),
                                                                     ease_threshold(ease_threshold){}

            void push(std::move_iterator<internal_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->push(buffer_arr, sz, exception_arr);
                size_t thru_sz = std::count(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
                this->reactor->increment(thru_sz);
            }

            void pop(internal_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->reactor->subscribe(this->max_wait_time);
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->base->pop(output_buffer_arr, sz, output_buffer_arr_cap);
                this->reactor->decrement(sz);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }

            auto set_busy() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->busy_threshold);
                return dg::network_exception::SUCCESS;
            }

            auto set_ease() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->ease_threshold);
                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class FairInBoundBufferContainer: public virtual BufferContainerInterface{

        private:

            dg::pow2_cyclic_queue<FairBufferContainerWaitingItem> push_waiting_item_vec; 
            dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>> distribution_queue;
            dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_kernel_buffer>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue;
            dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>> leftover_queue;
            std::unique_ptr<UnitAllocatorInterface<dg::vector<internal_kernel_buffer>>> bufvec_allocator;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            FairInBoundBufferContainer(dg::pow2_cyclic_queue<FairBufferContainerWaitingItem> push_waiting_item_vec,
                                       dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>> distribution_queue,
                                       dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_kernel_buffer>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue,
                                       dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>> leftover_queue,
                                       std::unique_ptr<UnitAllocatorInterface<dg::vector<internal_kernel_buffer>>> bufvec_allocator,
                                       std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                       size_t consume_sz_per_load): push_waiting_item_vec(std::move(push_waiting_item_vec)),
                                                                    distribution_queue(std::move(distribution_queue)),
                                                                    waiting_queue(std::move(waiting_queue)),
                                                                    leftover_queue(std::move(leftover_queue)),
                                                                    bufvec_allocator(std::move(bufvec_allocator)),
                                                                    mtx(std::move(mtx)),
                                                                    consume_sz_per_load(stdx::hdi_container<size_t>{consume_sz_per_load}){}

            void push(std::move_iterator<internal_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (sz == 0u){
                    return;
                }

                internal_kernel_buffer * base_buffer_arr = buffer_arr.base();
                std::expected<dg::vector<internal_kernel_buffer>, exception_t> buffer_vec_tmp = this->bufvec_allocator->get();

                if (!buffer_vec_tmp.has_value()){
                    std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
                    return;
                }

                dg::vector<internal_kernel_buffer> buffer_vec = std::move(buffer_vec_tmp.value());

                buffer_vec.resize(sz);
                std::copy(std::make_move_iterator(base_buffer_arr),
                          std::next(std::make_move_iterator(base_buffer_arr), sz),
                          buffer_vec.begin());

                semaphore_impl::dg_binary_semaphore * releasing_smp = nullptr;
                std::binary_semaphore wait_smp(0);

                bool need_wait = [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->waiting_queue.empty()){
                        auto [dst, smp] = this->waiting_queue.front();
                        this->waiting_queue.pop_front();
                        *dst = std::move(buffer_vec);
                        releasing_smp = smp;

                        return false;
                    }

                    if (this->distribution_queue.size() != this->distribution_queue.capacity()){
                        dg::network_exception_handler::nothrow_log(this->distribution_queue.push_back(std::move(buffer_vec)));
                        return false;
                    }

                    dg::network_exception_handler::nothrow_log(this->push_waiting_item_vec.push_back(FairBufferContainerWaitingItem
                    {
                        .buffer_vec = std::move(buffer_vec),
                        .smp        = &wait_smp
                    }));
                    
                    return true;
                }();

                if (releasing_smp != nullptr){
                    releasing_smp->release();
                }

                if (need_wait){
                    wait_smp.acquire();
                }

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void pop(internal_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                std::optional<dg::vector<internal_kernel_buffer>> str_vec(std::nullopt); 
                semaphore_impl::dg_binary_semaphore smp(0);
                
                bool is_acquire_required = [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->push_waiting_item_vec.empty()){
                        str_vec = std::move(this->push_waiting_item_vec.front().buffer_vec);
                        this->push_waiting_item_vec.front().smp->release();
                        this->push_waiting_item_vec.pop_front();
                        return false;
                    }

                    if (!this->leftover_queue.empty()){
                        str_vec = std::move(this->leftover_queue.front());
                        this->leftover_queue.pop_front();
                        return false;
                    }

                    if (!this->distribution_queue.empty()){
                        str_vec = std::move(this->distribution_queue.front());
                        this->distribution_queue.pop_front();
                        return false;
                    }

                    dg::network_exception_handler::nothrow_log(this->waiting_queue.push_back(std::make_pair(&str_vec, &smp)));
                    return true;
                }();

                if (is_acquire_required){
                    smp.acquire();
                }

                dg::network_exception_handler::dg_assert(str_vec.has_value());
                sz = std::min(output_buffer_arr_cap, static_cast<size_t>(str_vec->size()));
                size_t rem_sz = str_vec->size() - sz;                

                std::copy(std::make_move_iterator(std::next(str_vec->begin(), rem_sz)),
                          std::make_move_iterator(str_vec->end()),
                          output_buffer_arr);

                str_vec->resize(rem_sz);

                if (!str_vec->empty()){
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->waiting_queue.empty()){
                        auto [dst, smp] = this->waiting_queue.front();
                        this->waiting_queue.pop_front();
                        *dst = std::move(str_vec.value());
                        smp->release();
                    } else{
                        dg::network_exception_handler::nothrow_log(this->leftover_queue.push_back(std::move(str_vec.value())));
                    }
                } else{
                    this->bufvec_allocator->free(std::move(str_vec.value()));
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class PacketFIFOContainer: public virtual PacketContainerInterface{

        private:

            dg::pow2_cyclic_queue<PacketContainerWaitingItem> waiting_item_vec;
            dg::pow2_cyclic_queue<Packet> packet_deque;
            size_t packet_deque_capacity;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            PacketFIFOContainer(dg::pow2_cyclic_queue<PacketContainerWaitingItem> waiting_item_vec,
                                dg::pow2_cyclic_queue<Packet> packet_deque,
                                size_t packet_deque_capacity,
                                std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                stdx::hdi_container<size_t> consume_sz_per_load) noexcept: waiting_item_vec(std::move(waiting_item_vec)),
                                                                                           packet_deque(std::move(packet_deque)),
                                                                                           packet_deque_capacity(packet_deque_capacity),
                                                                                           mtx(std::move(mtx)),
                                                                                           consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<Packet *> packet_arr, size_t sz, exception_t * exception_arr) noexcept{

                std::binary_semaphore smp(0);

                bool need_acquire = [&]
                {
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (sz > this->max_consume_size()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    size_t app_cap  = this->packet_deque_capacity - this->packet_deque.size();
                    size_t app_sz   = std::min(sz, app_cap);
                    size_t old_sz   = this->packet_deque.size();
                    size_t new_sz   = old_sz + app_sz;

                    this->packet_deque.resize(new_sz);

                    std::copy(packet_arr, std::next(packet_arr, app_sz), std::next(this->packet_deque.begin(), old_sz));

                    if (app_sz != sz)
                    {
                        dg::network_exception_handler::nothrow_log(this->waiting_item_vec.push_back(PacketContainerWaitingItem
                        {
                            .packet_arr         = std::next(packet_arr, app_sz),
                            .packet_arr_sz      = sz - app_sz,
                            .smp                = &smp
                        }));

                        return true;
                    }

                    return false;
                }();

                if (need_acquire)
                {
                    smp.acquire();
                }

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                sz = 0u;

                while (true)
                {
                    if (sz == output_pkt_arr_cap)
                    {
                        break;
                    }

                    size_t rem_cap = output_pkt_arr_cap - sz;

                    if (!this->waiting_item_vec.empty())
                    {
                        size_t app_sz = std::min(rem_cap, this->waiting_item_vec.front().packet_arr_sz);

                        std::copy(this->waiting_item_vec.front().packet_arr,
                                  std::next(this->waiting_item_vec.front().packet_arr, app_sz),
                                  std::next(output_pkt_arr, sz));

                        if (app_sz == this->waiting_item_vec.front().packet_arr_sz)
                        {
                            this->waiting_item_vec.front().smp->release();
                            this->waiting_item_vec.pop_front();
                        }
                        else
                        {
                            this->waiting_item_vec.front() = PacketContainerWaitingItem
                            {
                                .packet_arr         = std::next(this->waiting_item_vec.front().packet_arr, app_sz),
                                .packet_arr_sz      = this->waiting_item_vec.front().packet_arr_sz - app_sz,
                                .smp                = this->waiting_item_vec.front().smp,
                            };
                        }

                        sz += app_sz;
                        continue;
                    }

                    if (!this->packet_deque.empty())
                    {
                        size_t app_sz = std::min(rem_cap, this->packet_deque.size());

                        std::copy(std::make_move_iterator(this->packet_deque.begin()),
                                  std::make_move_iterator(std::next(this->packet_deque.begin(), app_sz)),
                                  std::next(output_pkt_arr, sz));
                        this->packet_deque.erase_front_range(app_sz);

                        sz += app_sz;
                        continue;
                    }

                    break;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class PrioritizedPacketContainer: public virtual PacketContainerInterface{

        private:

            dg::vector<Packet> packet_vec;
            size_t packet_vec_capacity;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            PrioritizedPacketContainer(dg::vector<Packet> packet_vec,
                                       size_t packet_vec_capacity,
                                       std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                       stdx::hdi_container<size_t> consume_sz_per_load) noexcept: packet_vec(std::move(packet_vec)),
                                                                                                  packet_vec_capacity(packet_vec_capacity),
                                                                                                  mtx(std::move(mtx)),
                                                                                                  consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto less           = [](const Packet& lhs, const Packet& rhs){return lhs.priority < rhs.priority;};
                auto base_pkt_arr   = pkt_arr.base();

                for (size_t i = 0u; i < sz; ++i){
                    if (this->packet_vec.size() == this->packet_vec_capacity){
                        exception_arr[i] = dg::network_exception::SOCKET_QUEUE_FULL;
                        continue;
                    }

                    this->packet_vec.push_back(std::move(base_pkt_arr[i]));
                    std::push_heap(this->packet_vec.begin(), this->packet_vec.end(), less); //TODOs: optimizables
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }     

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_capacity) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                auto less       = [](const Packet& lhs, const Packet& rhs){return lhs.priority < rhs.priority;};
                sz              = std::min(output_pkt_arr_capacity, this->packet_vec.size());
                Packet * out_it = output_pkt_arr; 

                for (size_t i = 0u; i < sz; ++i){
                    std::pop_heap(this->packet_vec.begin(), this->packet_vec.end(), less); //TODOs: optimizables
                    *out_it = std::move(this->packet_vec.back());
                    this->packet_vec.pop_back();
                    std::advance(out_it, 1u);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }   
    };

    //OK
    class ScheduledPacketContainer: public virtual PacketContainerInterface{

        private:

            dg::vector<ScheduledPacket> packet_vec;
            std::shared_ptr<SchedulerInterface> scheduler;
            size_t packet_vec_capacity;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load; 

        public:

            ScheduledPacketContainer(dg::vector<ScheduledPacket> packet_vec, 
                                     std::shared_ptr<SchedulerInterface> scheduler,
                                     size_t packet_vec_capacity,
                                     std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                     stdx::hdi_container<size_t> consume_sz_per_load) noexcept: packet_vec(std::move(packet_vec)),
                                                                                                scheduler(std::move(scheduler)),
                                                                                                packet_vec_capacity(packet_vec_capacity),
                                                                                                mtx(std::move(mtx)),
                                                                                                consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto greater            = [](const ScheduledPacket& lhs, const ScheduledPacket& rhs){return lhs.sched_time > rhs.sched_time;};
                Packet * base_pkt_arr   = pkt_arr.base();

                for (size_t i = 0u; i < sz; ++i){
                    if (this->packet_vec.size() == this->packet_vec_capacity){
                        exception_arr[i] = dg::network_exception::SOCKET_QUEUE_FULL;
                        continue;
                    }

                    std::expected<std::chrono::time_point<std::chrono::utc_clock>, exception_t> sched_time = this->scheduler->schedule(base_pkt_arr[i].to_addr);

                    if (!sched_time.has_value()){
                        exception_arr[i] = sched_time.error();
                        continue;
                    }

                    auto sched_packet       = ScheduledPacket{};
                    sched_packet.pkt        = std::move(base_pkt_arr[i]);
                    sched_packet.sched_time = sched_time.value();
                    this->packet_vec.push_back(std::move(sched_packet));
                    std::push_heap(this->packet_vec.begin(), this->packet_vec.end(), greater); //TODOs: optimizables
                    exception_arr[i]        = dg::network_exception::SUCCESS;
                }
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_capacity) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                auto greater    = [](const ScheduledPacket& lhs, const ScheduledPacket& rhs){return lhs.sched_time > rhs.sched_time;};
                auto time_bar   = std::chrono::utc_clock::now();
                sz              = 0u;

                for (size_t i = 0u; i < output_pkt_arr_capacity; ++i){
                    if (this->packet_vec.empty()){
                        return;
                    }

                    if (this->packet_vec.front().sched_time > time_bar){ //
                        return;
                    }

                    std::pop_heap(this->packet_vec.begin(), this->packet_vec.end(), greater); //TODOs: optimizables
                    output_pkt_arr[sz++] = std::move(this->packet_vec.back().pkt);
                    this->packet_vec.pop_back();
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class OutboundPacketContainer: public virtual PacketContainerInterface{

        private:

            std::unique_ptr<PacketContainerInterface> ack_container;
            std::unique_ptr<PacketContainerInterface> request_container;
            std::unique_ptr<PacketContainerInterface> krescue_container;
            size_t ack_accum_sz;
            size_t request_accum_sz; 
            size_t krescue_accum_sz;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            OutboundPacketContainer(std::unique_ptr<PacketContainerInterface> ack_container,
                                    std::unique_ptr<PacketContainerInterface> request_container,
                                    std::unique_ptr<PacketContainerInterface> krescue_container,
                                    size_t ack_accum_sz,
                                    size_t request_accum_sz,
                                    size_t krescue_accum_sz,
                                    stdx::hdi_container<size_t> consume_sz_per_load) noexcept: ack_container(std::move(ack_container)),
                                                                                               request_container(std::move(request_container)),
                                                                                               krescue_container(std::move(krescue_container)),
                                                                                               ack_accum_sz(ack_accum_sz),
                                                                                               request_accum_sz(request_accum_sz),
                                                                                               krescue_accum_sz(krescue_accum_sz),
                                                                                               consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Packet * base_pkt_arr = pkt_arr.base();

                //
                auto ack_push_resolutor                 = InternalPushResolutor{};
                ack_push_resolutor.dst                  = this->ack_container.get();

                size_t trimmed_ack_accum_sz             = std::min(std::min(this->ack_accum_sz, sz), this->ack_container->max_consume_size());
                size_t ack_accumulator_alloc_sz         = dg::network_producer_consumer::delvrsrv_allocation_cost(&ack_push_resolutor, trimmed_ack_accum_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ack_accumulator_buf(ack_accumulator_alloc_sz);
                auto ack_accumulator                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&ack_push_resolutor, trimmed_ack_accum_sz, ack_accumulator_buf.get()));

                //
                auto request_push_resolutor             = InternalPushResolutor{};
                request_push_resolutor.dst              = this->request_container.get();

                size_t trimmed_request_accum_sz         = std::min(std::min(this->request_accum_sz, sz), this->request_container->max_consume_size());
                size_t request_accumulator_alloc_sz     = dg::network_producer_consumer::delvrsrv_allocation_cost(&request_push_resolutor, trimmed_request_accum_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> request_accumulator_buf(request_accumulator_alloc_sz);
                auto request_accumulator                = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&request_push_resolutor, trimmed_request_accum_sz, request_accumulator_buf.get()));

                //

                auto krescue_push_resolutor             = InternalPushResolutor{};
                krescue_push_resolutor.dst              = this->krescue_container.get();

                size_t trimmed_krescue_accum_sz         = std::min(std::min(this->krescue_accum_sz, sz), this->krescue_container->max_consume_size());
                size_t krescue_accumulator_alloc_sz     = dg::network_producer_consumer::delvrsrv_allocation_cost(&krescue_push_resolutor, trimmed_krescue_accum_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> krescue_accumulator_buf(krescue_accumulator_alloc_sz);
                auto krescue_accumulator                = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&krescue_push_resolutor, trimmed_krescue_accum_sz, krescue_accumulator_buf.get())); 

                //
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    auto delivery_arg           = DeliveryArgument{};
                    packet_polymorphic_t kind   = packet_service::get_packet_polymorphic_type(base_pkt_arr[i]);
                    delivery_arg.pkt            = std::move(base_pkt_arr[i]);
                    delivery_arg.pkt_ptr        = std::next(base_pkt_arr, i);
                    delivery_arg.exception_ptr  = std::next(exception_arr, i);

                    switch (kind){
                        case constants::ack:
                        {
                            dg::network_producer_consumer::delvrsrv_deliver(ack_accumulator.get(), std::move(delivery_arg));
                            break;
                        }
                        case constants::request:
                        {
                            dg::network_producer_consumer::delvrsrv_deliver(request_accumulator.get(), std::move(delivery_arg));
                            break;
                        }
                        case constants::krescue:
                        {
                            dg::network_producer_consumer::delvrsrv_deliver(krescue_accumulator.get(), std::move(delivery_arg));
                            break;
                        }
                        default:
                        {
                            if constexpr(DEBUG_MODE_FLAG){
                                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            } else{
                                std::unreachable();
                            }
                        }
                    }
                }
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_capacity) noexcept{

                constexpr size_t CONTAINER_SZ   = 3u;
                using container_ptr_t           = PacketContainerInterface *;
                container_ptr_t container_arr[CONTAINER_SZ]; 
                container_arr[0]                = this->ack_container.get();
                container_arr[1]                = this->request_container.get();
                container_arr[2]                = this->krescue_container.get();

                sz                      = 0u;
                Packet * iter_pkt_arr   = output_pkt_arr;
                size_t iter_pkt_arr_cap = output_pkt_arr_capacity; 

                for (size_t i = 0u; i < CONTAINER_SZ; ++i){
                    if (iter_pkt_arr_cap == 0u){
                        return;
                    }

                    size_t tmp_sz       = {};
                    container_arr[i]->pop(iter_pkt_arr, tmp_sz, iter_pkt_arr_cap);

                    std::advance(iter_pkt_arr, tmp_sz);
                    iter_pkt_arr_cap    -= tmp_sz;
                    sz                  += tmp_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }

        private:

            struct DeliveryArgument{
                Packet pkt;
                Packet * pkt_ptr;
                exception_t * exception_ptr;
            };

            struct InternalPushResolutor: dg::network_producer_consumer::ConsumerInterface<DeliveryArgument>{

                PacketContainerInterface * dst;

                void push(std::move_iterator<DeliveryArgument *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> pkt_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    DeliveryArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        pkt_arr[i] = std::move(base_data_arr[i].pkt);
                    }

                    this->dst->push(std::make_move_iterator(pkt_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].pkt_ptr       = std::move(pkt_arr[i]);
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        }
                    }
                }
            };
    };

    //OK
    class NormalOutboundPacketContainer: public virtual PacketContainerInterface{

        private:

            dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> normal_packet_queue_waiting_item_vec;
            dg::pow2_cyclic_queue<dg::vector<Packet>> normal_packet_queue;

            dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> ack_packet_queue_waiting_item_vec;
            dg::pow2_cyclic_queue<dg::vector<Packet>> ack_packet_queue;

            dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> rescue_packet_queue_waiting_item_vec;
            dg::pow2_cyclic_queue<dg::vector<Packet>> rescue_packet_queue;

            dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue;
            dg::pow2_cyclic_queue<dg::vector<Packet>> leftover_queue;
            std::unique_ptr<UnitAllocatorInterface<dg::vector<Packet>>> pktvec_allocator;
            size_t feed_vectorization_sz;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            NormalOutboundPacketContainer(dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> normal_packet_queue_waiting_item_vec,
                                          dg::pow2_cyclic_queue<dg::vector<Packet>> normal_packet_queue,

                                          dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> ack_packet_queue_waiting_item_vec,
                                          dg::pow2_cyclic_queue<dg::vector<Packet>> ack_packet_queue,

                                          dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> rescue_packet_queue_waiting_item_vec,
                                          dg::pow2_cyclic_queue<dg::vector<Packet>> rescue_packet_queue,

                                          dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue,
                                          dg::pow2_cyclic_queue<dg::vector<Packet>> leftover_queue,
                                          std::unique_ptr<UnitAllocatorInterface<dg::vector<Packet>>> pktvec_allocator,
                                          size_t feed_vectorization_sz,
                                          std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                          size_t consume_sz_per_load) noexcept: normal_packet_queue_waiting_item_vec(std::move(normal_packet_queue_waiting_item_vec)),
                                                                                normal_packet_queue(std::move(normal_packet_queue)),
                                                                                ack_packet_queue_waiting_item_vec(std::move(ack_packet_queue_waiting_item_vec)),
                                                                                ack_packet_queue(std::move(ack_packet_queue)),
                                                                                rescue_packet_queue_waiting_item_vec(std::move(rescue_packet_queue_waiting_item_vec)),
                                                                                rescue_packet_queue(std::move(rescue_packet_queue)),
                                                                                waiting_queue(std::move(waiting_queue)),
                                                                                leftover_queue(std::move(leftover_queue)),
                                                                                pktvec_allocator(std::move(pktvec_allocator)),
                                                                                feed_vectorization_sz(feed_vectorization_sz),
                                                                                mtx(std::move(mtx)),
                                                                                consume_sz_per_load(stdx::hdi_container<size_t>{consume_sz_per_load}){}

            void push(std::move_iterator<Packet *> packet_arr, size_t sz, exception_t * exception_arr) noexcept{

                Packet * base_packet_arr                    = packet_arr.base();
                auto internal_resolutor                     = InternalPushFeedResolutor{};

                internal_resolutor.pktvec_allocator         = this->pktvec_allocator.get();

                internal_resolutor.normal_packet_wait_queue = &this->normal_packet_queue_waiting_item_vec;
                internal_resolutor.normal_packet_queue      = &this->normal_packet_queue;
                
                internal_resolutor.ack_packet_wait_queue    = &this->ack_packet_queue_waiting_item_vec;
                internal_resolutor.ack_packet_queue         = &this->ack_packet_queue;

                internal_resolutor.rescue_packet_wait_queue = &this->rescue_packet_queue_waiting_item_vec;
                internal_resolutor.rescue_packet_queue      = &this->rescue_packet_queue;

                internal_resolutor.waiting_queue            = &this->waiting_queue;
                internal_resolutor.queue_mtx                = this->mtx.get();

                size_t trimmed_feed_vectorization_sz    = std::min(this->feed_vectorization_sz, sz);
                size_t feeder_allocation_cost           = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&internal_resolutor, trimmed_feed_vectorization_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                             = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&internal_resolutor, trimmed_feed_vectorization_sz, feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                //we aren't moving packets, we are moving the addresses

                for (size_t i = 0u; i < sz; ++i){
                    types::packet_polymorphic_t key = packet_service::get_packet_polymorphic_type(base_packet_arr[i]);
                    auto feed_arg                   = InternalPushFeedArgument{.packet_ptr      = std::make_move_iterator(std::next(base_packet_arr, i)),
                                                                               .exception_ptr   = std::next(exception_arr, i)};

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), key, feed_arg);
                }
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                semaphore_impl::dg_binary_semaphore smp(0);
                std::optional<dg::vector<Packet>> pkt_vec = std::nullopt;
                bool smp_responsibility = {};  

                [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->leftover_queue.empty()){
                        pkt_vec                     = std::move(this->leftover_queue.front());
                        this->leftover_queue.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if (!this->ack_packet_queue_waiting_item_vec.empty()){
                        pkt_vec                     = std::move(this->ack_packet_queue_waiting_item_vec.front().packet_vec);
                        this->ack_packet_queue_waiting_item_vec.front().smp->release();
                        this->ack_packet_queue_waiting_item_vec.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if (!this->ack_packet_queue.empty()){
                        pkt_vec                     = std::move(this->ack_packet_queue.front());
                        this->ack_packet_queue.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if (!this->normal_packet_queue_waiting_item_vec.empty()){
                        pkt_vec                     = std::move(this->normal_packet_queue_waiting_item_vec.front().packet_vec);
                        this->normal_packet_queue_waiting_item_vec.front().smp->release();
                        this->normal_packet_queue_waiting_item_vec.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if (!this->normal_packet_queue.empty()){
                        pkt_vec                     = std::move(this->normal_packet_queue.front());
                        this->normal_packet_queue.pop_front();
                        smp_responsibility          = false;

                        return;
                    }


                    if (!this->rescue_packet_queue_waiting_item_vec.empty()){
                        pkt_vec                     = std::move(this->rescue_packet_queue_waiting_item_vec.front().packet_vec);
                        this->rescue_packet_queue_waiting_item_vec.front().smp->release();
                        this->rescue_packet_queue_waiting_item_vec.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if (!this->rescue_packet_queue.empty()){
                        pkt_vec                     = std::move(this->rescue_packet_queue.front());
                        this->rescue_packet_queue.pop_front();
                        smp_responsibility          = false;

                        return;
                    }

                    if constexpr(DEBUG_MODE_FLAG){
                        if (this->waiting_queue.capacity() == this->waiting_queue.size()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    smp_responsibility = true;
                    dg::network_exception_handler::nothrow_log(this->waiting_queue.push_back(std::make_pair(&pkt_vec, &smp)));
                }();

                //we'll tackle the pkt_arr_cap problem by using leftover implementation
                //the leftover size cannot exceed the number of concurrent threads accessing the container
                //proof

                //assume that the leftover size exceeds the number of concurrent threads accessing the container
                //it must follow that there is at least one thread does two left_over commits, which is a contradiction, because it would see the previous leftover according to the pop() logic  

                if (smp_responsibility){
                    smp.acquire();
                }

                if constexpr(DEBUG_MODE_FLAG){
                    if (!pkt_vec.has_value()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                semaphore_impl::dg_binary_semaphore * waiting_queue_smp = nullptr; 

                sz                  = std::min(output_pkt_arr_cap, static_cast<size_t>(pkt_vec->size()));
                size_t remaining_sz = pkt_vec->size() - sz; 
                std::copy(std::next(std::make_move_iterator(pkt_vec->begin()), remaining_sz), std::make_move_iterator(pkt_vec->end()), output_pkt_arr);
                pkt_vec->resize(remaining_sz);

                if (!pkt_vec->empty()){
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (this->leftover_queue.size() == this->leftover_queue.capacity()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    if (!this->waiting_queue.empty()){
                        auto [dst, smp] = this->waiting_queue.front();
                        this->waiting_queue.pop_front();
                        *dst = std::move(pkt_vec.value()); 
                        waiting_queue_smp = smp;
                    } else{
                        dg::network_exception_handler::nothrow_log(this->leftover_queue.push_back(std::move(pkt_vec.value())));
                    }
                }
                else{
                    this->pktvec_allocator->free(std::move(pkt_vec.value()));
                }

                if (waiting_queue_smp != nullptr){
                    waiting_queue_smp->release();
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
        
        private:
            
            struct InternalPushFeedArgument{
                std::move_iterator<Packet *> packet_ptr;
                exception_t * exception_ptr;
            };

            struct InternalPushFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<types::packet_polymorphic_t, InternalPushFeedArgument>{

                UnitAllocatorInterface<dg::vector<Packet>> * pktvec_allocator;

                dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> * normal_packet_wait_queue;
                dg::pow2_cyclic_queue<dg::vector<Packet>> * normal_packet_queue;

                dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> * ack_packet_wait_queue;
                dg::pow2_cyclic_queue<dg::vector<Packet>> * ack_packet_queue;

                dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> * rescue_packet_wait_queue;
                dg::pow2_cyclic_queue<dg::vector<Packet>> * rescue_packet_queue;

                dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>> * waiting_queue;
                stdx::fair_atomic_flag * queue_mtx;

                void push(const types::packet_polymorphic_t& key, std::move_iterator<InternalPushFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalPushFeedArgument * base_data_arr                    = data_arr.base();
                    std::expected<dg::vector<Packet>, exception_t> inbound_vec  = pktvec_allocator->get();

                    if (!inbound_vec.has_value()){
                        for (size_t i = 0u; i < sz; ++i){
                            *base_data_arr[i].exception_ptr = inbound_vec.error();
                        }

                        return;
                    }

                    inbound_vec->resize(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        inbound_vec.value()[i] = std::move(*base_data_arr[i].packet_ptr.base());
                    }

                    std::binary_semaphore wait_smp(0);

                    bool need_acquire = [&, this]() noexcept{
                        stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->queue_mtx);

                        if (!this->waiting_queue->empty()){
                            auto [fetching_addr, smp]   = this->waiting_queue->front();
                            this->waiting_queue->pop_front();
                            *fetching_addr              = std::move(inbound_vec.value());
                            smp->release();

                            return false;
                        }

                        switch (key){
                            case constants::request:
                            {
                                if (this->normal_packet_queue->size() == this->normal_packet_queue->capacity()){
                                    
                                    dg::network_exception_handler::nothrow_log(this->normal_packet_wait_queue->push_back(FairPacketContainerWaitingItem
                                    {
                                        .packet_vec = std::move(inbound_vec.value()),
                                        .smp        = &wait_smp
                                    }));

                                    return true;
                                }

                                dg::network_exception_handler::nothrow_log(this->normal_packet_queue->push_back(std::move(inbound_vec.value())));

                                return false;
                            }
                            case constants::ack:
                            {
                                if (this->ack_packet_queue->size() == this->ack_packet_queue->capacity()){
                                    dg::network_exception_handler::nothrow_log(this->ack_packet_wait_queue->push_back(FairPacketContainerWaitingItem
                                    {
                                        .packet_vec = std::move(inbound_vec.value()),
                                        .smp        = &wait_smp
                                    }));

                                    return true;
                                }

                                dg::network_exception_handler::nothrow_log(this->ack_packet_queue->push_back(std::move(inbound_vec.value())));

                                return false;
                            }
                            case constants::krescue:
                            {
                                if (this->rescue_packet_queue->size() == this->rescue_packet_queue->capacity()){
                                    dg::network_exception_handler::nothrow_log(this->rescue_packet_wait_queue->push_back(FairPacketContainerWaitingItem
                                    {
                                        .packet_vec = std::move(inbound_vec.value()),
                                        .smp        = &wait_smp
                                    }));

                                    return true;
                                }

                                dg::network_exception_handler::nothrow_log(this->rescue_packet_queue->push_back(std::move(inbound_vec.value())));

                                return false;
                            }
                            default:
                            {
                                if constexpr(DEBUG_MODE_FLAG){
                                    dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                                    std::abort();
                                } else{
                                    std::unreachable();
                                }
                            }
                        }
                    }();

                    if (need_acquire){
                        wait_smp.acquire();
                    }
                }
            };
    };

    //OK
    class ReactingPacketContainer: public virtual PacketContainerInterface,
                                   public virtual ContainerBusyAdapterInterface{

        private:

            std::unique_ptr<PacketContainerInterface> base;
            std::unique_ptr<ComplexReactor> reactor;
            std::chrono::nanoseconds max_wait_time;
            const size_t busy_threshold;
            const size_t ease_threshold;
        
        public:

            ReactingPacketContainer(std::unique_ptr<PacketContainerInterface> base,
                                    std::unique_ptr<ComplexReactor> reactor,
                                    std::chrono::nanoseconds max_wait_time,
                                    size_t busy_threshold,
                                    size_t ease_threshold) noexcept: base(std::move(base)),
                                                                     reactor(std::move(reactor)),
                                                                     max_wait_time(max_wait_time),
                                                                     busy_threshold(busy_threshold),
                                                                     ease_threshold(ease_threshold){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->push(pkt_arr, sz, exception_arr);
                size_t thru_sz = std::count(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
                this->reactor->increment(thru_sz);
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->reactor->subscribe(this->max_wait_time);
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->base->pop(output_pkt_arr, sz, output_pkt_arr_cap);
                this->reactor->decrement(sz);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }

            auto set_busy() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->busy_threshold);
                return dg::network_exception::SUCCESS;
            }

            auto set_ease() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->ease_threshold);
                return dg::network_exception::SUCCESS;
            }
    };

    //OK
    class FairInBoundPacketContainer: public virtual PacketContainerInterface{

        private:

            dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> push_waiting_item_vec;
            dg::pow2_cyclic_queue<dg::vector<Packet>> packet_queue;
            dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue;
            dg::pow2_cyclic_queue<dg::vector<Packet>> leftover_queue;
            std::unique_ptr<UnitAllocatorInterface<dg::vector<Packet>>> pktvec_allocator;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            FairInBoundPacketContainer(dg::pow2_cyclic_queue<FairPacketContainerWaitingItem> push_waiting_item_vec,
                                       dg::pow2_cyclic_queue<dg::vector<Packet>> packet_queue,
                                       dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>> waiting_queue,
                                       dg::pow2_cyclic_queue<dg::vector<Packet>> leftover_queue,
                                       std::unique_ptr<UnitAllocatorInterface<dg::vector<Packet>>> pktvec_allocator,
                                       std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                       size_t consume_sz_per_load) noexcept: push_waiting_item_vec(std::move(push_waiting_item_vec)),
                                                                             packet_queue(std::move(packet_queue)),
                                                                             waiting_queue(std::move(waiting_queue)),
                                                                             leftover_queue(std::move(leftover_queue)),
                                                                             pktvec_allocator(std::move(pktvec_allocator)),
                                                                             mtx(std::move(mtx)),
                                                                             consume_sz_per_load(stdx::hdi_container<size_t>{consume_sz_per_load}){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (sz == 0u){
                    return;
                }

                Packet * base_pkt_arr = pkt_arr.base();

                std::expected<dg::vector<Packet>, exception_t> pkt_vec_tmp = this->pktvec_allocator->get();

                if (!pkt_vec_tmp.has_value()){
                    std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::QUEUE_FULL);
                    return;
                }

                dg::vector<Packet> pkt_vec = std::move(pkt_vec_tmp.value());
                pkt_vec.resize(sz);
                std::copy(pkt_arr, std::next(pkt_arr, sz), pkt_vec.begin());

                semaphore_impl::dg_binary_semaphore * releasing_smp = nullptr;
                std::binary_semaphore wait_smp(0);

                bool need_wait = [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->waiting_queue.empty()){
                        auto [dst, smp] = this->waiting_queue.front();
                        this->waiting_queue.pop_front();
                        *dst = std::move(pkt_vec);
                        releasing_smp = smp;

                        return false;
                    }

                    if (this->packet_queue.size() != this->packet_queue.capacity()){
                        dg::network_exception_handler::nothrow_log(this->packet_queue.push_back(std::move(pkt_vec)));
                        return false;
                    }

                    dg::network_exception_handler::nothrow_log(this->push_waiting_item_vec.push_back(FairPacketContainerWaitingItem
                    {
                        .packet_vec = std::move(pkt_vec),
                        .smp        = &wait_smp
                    }));

                    return true;
                }();

                if (releasing_smp != nullptr){
                    releasing_smp->release();
                }

                if (need_wait){
                    wait_smp.acquire();
                }

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_cap) noexcept{

                std::optional<dg::vector<Packet>> pkt_vec(std::nullopt);
                semaphore_impl::dg_binary_semaphore smp(0);

                bool is_acquire_required = [&, this]() noexcept{
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->push_waiting_item_vec.empty()){
                        pkt_vec = std::move(this->push_waiting_item_vec.front().packet_vec);
                        this->push_waiting_item_vec.front().smp->release();
                        this->push_waiting_item_vec.pop_front();

                        return false;
                    }

                    if (!this->leftover_queue.empty()){
                        pkt_vec = std::move(this->leftover_queue.front());
                        this->leftover_queue.pop_front();
 
                        return false;
                    }

                    if (!this->packet_queue.empty()){
                        pkt_vec = std::move(this->packet_queue.front());
                        this->packet_queue.pop_front();

                        return false;
                    }

                    dg::network_exception_handler::nothrow_log(this->waiting_queue.push_back(std::make_pair(&pkt_vec, &smp)));
                    return true;
                }();

                if (is_acquire_required){
                    smp.acquire();
                }

                dg::network_exception_handler::dg_assert(pkt_vec.has_value());
                sz = std::min(static_cast<size_t>(pkt_vec->size()), output_pkt_arr_cap); 
                size_t rem_sz = pkt_vec->size() - sz;

                std::copy(std::next(std::make_move_iterator(pkt_vec->begin()), rem_sz),
                          std::make_move_iterator(pkt_vec->end()),
                          output_pkt_arr);

                pkt_vec->resize(rem_sz);

                if (!pkt_vec->empty()){
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                    
                    if (!this->waiting_queue.empty()){
                        auto [dst, smp] = this->waiting_queue.front();
                        this->waiting_queue.pop_front();
                        *dst = std::move(pkt_vec.value());
                        smp->release();
                    } else{
                        dg::network_exception_handler::nothrow_log(this->leftover_queue.push_back(std::move(pkt_vec.value())));
                    }
                }
                else{
                    this->pktvec_allocator->free(std::move(pkt_vec.value()));
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class InBoundIDController: public virtual InBoundIDControllerInterface{

        private:

            data_structure::temporal_finite_unordered_set<kind_global_packet_id_t> id_hashset;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            InBoundIDController(data_structure::temporal_finite_unordered_set<kind_global_packet_id_t> id_hashset,
                                std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                stdx::hdi_container<size_t> consume_sz_per_load) noexcept: id_hashset(std::move(id_hashset)),
                                                                                           mtx(std::move(mtx)),
                                                                                           consume_sz_per_load(std::move(consume_sz_per_load)){}

            void thru(kind_global_packet_id_t * packet_id_arr, size_t sz, std::expected<bool, exception_t> * op) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i){
                    if (this->id_hashset.contains(packet_id_arr[i])){
                        op[i] = false;
                        continue;
                    }

                    this->id_hashset.insert(packet_id_arr[i]);
                    op[i] = true;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }
    };

    //OK
    class HashDistributedInBoundIDController: public virtual InBoundIDControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<InBoundIDControllerInterface>[]> inbound_id_controller_vec;
            size_t pow2_inbound_id_controller_vec_sz;
            size_t keyvalue_aggregation_cap;
            size_t consume_sz_per_load;

        public:

            HashDistributedInBoundIDController(std::unique_ptr<std::unique_ptr<InBoundIDControllerInterface>[]> inbound_id_controller_vec,
                                               size_t pow2_inbound_id_controller_vec_sz,
                                               size_t keyvalue_aggregation_cap,
                                               size_t consume_sz_per_load) noexcept: inbound_id_controller_vec(std::move(inbound_id_controller_vec)),
                                                                                     pow2_inbound_id_controller_vec_sz(pow2_inbound_id_controller_vec_sz),
                                                                                     keyvalue_aggregation_cap(keyvalue_aggregation_cap),
                                                                                     consume_sz_per_load(consume_sz_per_load){}

            void thru(kind_global_packet_id_t * packet_id_arr, size_t sz, std::expected<bool, exception_t> * op) noexcept{

                InternalResolutor internal_resolutor    = {};
                internal_resolutor.dst_vec              = this->inbound_id_controller_vec.get();

                size_t trimmed_keyvalue_aggregation_cap = std::min(this->keyvalue_aggregation_cap, sz);
                size_t deliverer_allocation_cost        = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&internal_resolutor, trimmed_keyvalue_aggregation_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_mem(deliverer_allocation_cost);
                auto deliverer                          = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&internal_resolutor, trimmed_keyvalue_aggregation_cap, deliverer_mem.get())); 

                std::fill(op, std::next(op, sz), std::expected<bool, exception_t>(true));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed           = dg::network_hash::hash_reflectible(packet_id_arr[i]);
                    size_t partitioned_idx  = hashed & (this->pow2_inbound_id_controller_vec_sz - 1u);

                    auto resolutor_arg      = InternalResolutorArgument{};
                    resolutor_arg.pkt_id    = packet_id_arr[i];
                    resolutor_arg.bad_op    = std::next(op, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(deliverer.get(), partitioned_idx, resolutor_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
        
        private:

            struct InternalResolutorArgument{
                kind_global_packet_id_t pkt_id;
                std::expected<bool, exception_t> * bad_op;
            };

            struct InternalResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalResolutorArgument>{

                std::unique_ptr<InBoundIDControllerInterface> * dst_vec;

                void push(const size_t& idx, std::move_iterator<InternalResolutorArgument *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<kind_global_packet_id_t[]> pkt_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> response_arr(sz);
                    InternalResolutorArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        pkt_id_arr[i] = base_data_arr[i].pkt_id;
                    }

                    this->dst_vec[idx]->thru(pkt_id_arr.get(), sz, response_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (!response_arr[i].has_value() || !response_arr[i].value()){
                            *base_data_arr[i].bad_op = response_arr[i];
                        }
                    }
                }
            };
    };

    //OK
    class ExhaustionControlledPacketContainer: public virtual PacketContainerInterface{

        private:

            std::unique_ptr<PacketContainerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledPacketContainer(std::unique_ptr<PacketContainerInterface> base,
                                                std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                                   executor(std::move(executor)),
                                                                                                                                                   exhaustion_controller(std::move(exhaustion_controller)){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                Packet * pkt_arr_base               = pkt_arr.base();
                Packet * pkt_arr_first              = pkt_arr_base;
                Packet * pkt_arr_last               = std::next(pkt_arr_first, sz);
                exception_t * exception_arr_first   = exception_arr;
                exception_t * exception_arr_last    = std::next(exception_arr_first, sz);
                size_t sliding_window_sz            = sz;

                auto task = [&, this]() noexcept{
                    this->base->push(std::make_move_iterator(pkt_arr_first), sliding_window_sz, exception_arr_first);

                    size_t waiting_sz                   = std::count(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t err                     = this->exhaustion_controller->update_waiting_size(waiting_sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    } 

                    exception_t * retriable_eptr_first  = std::find(exception_arr_first, exception_arr_last, dg::network_exception::SOCKET_QUEUE_FULL);
                    exception_t * retriable_eptr_last   = std::find_if(retriable_eptr_first, exception_arr_last, [](exception_t err){return err != dg::network_exception::SOCKET_QUEUE_FULL;});
                    size_t relative_offset              = std::distance(exception_arr_first, retriable_eptr_first);
                    sliding_window_sz                   = std::distance(retriable_eptr_first, retriable_eptr_last);

                    std::advance(pkt_arr_first, relative_offset);
                    std::advance(exception_arr_first, relative_offset);

                    return !this->exhaustion_controller->is_should_wait() || (pkt_arr_first == pkt_arr_last);  //TODOs: we want to subscribe these guys to a load_balancer system
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(std::move(task));
                this->executor->exec(virtual_task);
            } 

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_capacity) noexcept{

                this->base->pop(output_pkt_arr, sz, output_pkt_arr_capacity);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class HashDistributedPacketContainer: public virtual PacketContainerInterface{

        private:

            std::unique_ptr<std::unique_ptr<PacketContainerInterface>[]> packet_container_vec;
            size_t pow2_packet_container_vec_sz;
            size_t consume_sz_per_load;

        public:

            HashDistributedPacketContainer(std::unique_ptr<std::unique_ptr<PacketContainerInterface>[]> packet_container_vec,
                                           size_t pow2_packet_container_vec_sz,
                                           size_t consume_sz_per_load) noexcept: packet_container_vec(std::move(packet_container_vec)),
                                                                                 pow2_packet_container_vec_sz(pow2_packet_container_vec_sz),
                                                                                 consume_sz_per_load(consume_sz_per_load){}

            void push(std::move_iterator<Packet *> pkt_arr, size_t sz, exception_t * exception_arr) noexcept{

                size_t random_value = dg::network_randomizer::randomize_int<size_t>();
                size_t random_idx   = random_value & (this->pow2_packet_container_vec_sz - 1u);

                this->packet_container_vec[random_idx]->push(pkt_arr, sz, exception_arr);
            }

            void pop(Packet * output_pkt_arr, size_t& sz, size_t output_pkt_arr_capacity) noexcept{

                sz = 0u;
                size_t seed = dg::network_randomizer::randomize_int<size_t>() >> 1; 

                for (size_t i = 0u; i < this->pow2_packet_container_vec_sz; ++i){
                    size_t remaining_cap = output_pkt_arr_capacity - sz;

                    if (remaining_cap == 0u){
                        return;
                    }

                    size_t idx = (seed + i) & (this->pow2_packet_container_vec_sz - 1u); 
                    Packet * tmp_output_pkt_arr = std::next(output_pkt_arr, sz);
                    size_t tmp_sz{};
                    this->packet_container_vec[idx]->pop(tmp_output_pkt_arr, tmp_sz, remaining_cap);
                    sz += tmp_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
    };

    //OK
    class TrafficController: public virtual TrafficControllerInterface{

        private:

            dg::unordered_unstable_map<Address, size_t> address_counter_map;
            size_t address_cap;
            size_t global_cap;
            size_t map_cap;
            size_t global_counter;

        public:

            TrafficController(dg::unordered_unstable_map<Address, size_t> address_counter_map,
                              size_t address_cap,
                              size_t global_cap,
                              size_t map_cap,
                              size_t global_counter) noexcept: address_counter_map(std::move(address_counter_map)),
                                                               address_cap(address_cap),
                                                               global_cap(global_cap),
                                                               map_cap(map_cap),
                                                               global_counter(global_counter){}

            auto thru(Address addr) noexcept -> std::expected<bool, exception_t>{

                if (this->global_counter == this->global_cap){
                    return false;
                }

                auto map_ptr = this->address_counter_map.find(addr);

                if (map_ptr == this->address_counter_map.end()){
                    if (this->address_counter_map.size() == this->map_cap){
                        return false;
                    }

                    if (this->address_cap == 0u){
                        return false;
                    }

                    auto [emplace_ptr, status] = this->address_counter_map.emplace(std::make_pair(addr, 0u));
                    dg::network_exception_handler::dg_assert(status);
                    map_ptr = emplace_ptr;
                }

                if (map_ptr->second == this->address_cap){
                    return false;
                }

                map_ptr->second += 1;
                this->global_counter += 1;

                return true;
            }

            void reset() noexcept{

                this->address_counter_map.clear();
                this->global_counter = 0u;
            }
    };

    //OK
    class InBoundBorderController: public virtual BorderControllerInterface, 
                                   public virtual UpdatableInterface{

        private:

            std::shared_ptr<external_interface::NATIPControllerInterface> nat_ip_controller;
            std::unique_ptr<packet_controller::TrafficControllerInterface> traffic_controller;
            dg::unordered_set<Address> thru_ip_set;
            dg::unordered_set<Address> inbound_ip_side_set;
            size_t inbound_ip_side_set_cap;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            InBoundBorderController(std::shared_ptr<external_interface::NATIPControllerInterface> nat_ip_controller,
                                    std::unique_ptr<packet_controller::TrafficControllerInterface> traffic_controller,
                                    dg::unordered_set<Address> thru_ip_set,
                                    dg::unordered_set<Address> inbound_ip_side_set,
                                    size_t inbound_ip_side_set_cap,
                                    std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                    stdx::hdi_container<size_t> consume_sz_per_load) noexcept: nat_ip_controller(std::move(nat_ip_controller)),
                                                                                               traffic_controller(std::move(traffic_controller)),
                                                                                               thru_ip_set(std::move(thru_ip_set)),
                                                                                               inbound_ip_side_set(std::move(inbound_ip_side_set)),
                                                                                               inbound_ip_side_set_cap(inbound_ip_side_set_cap),
                                                                                               mtx(std::move(mtx)),
                                                                                               consume_sz_per_load(std::move(consume_sz_per_load)){}

            void thru(Address * addr_arr, size_t sz, exception_t * response_exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                std::expected<size_t, exception_t> insert_sz = utility::finite_set_insert(this->inbound_ip_side_set, this->inbound_ip_side_set_cap, 
                                                                                          addr_arr, std::next(addr_arr, sz)); 

                if (!insert_sz.has_value() || insert_sz.value() != sz){
                    if (!insert_sz.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(insert_sz.error()));
                    } else{
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(dg::network_exception::RESOURCE_EXHAUSTION));
                    }
                }

                for (size_t i = 0u; i < sz; ++i){
                    // if (!this->thru_ip_set.contains(addr_arr[i])){
                    //     response_exception_arr[i] = dg::network_exception::SOCKET_BAD_IP_RULE; //TODOs: this might be a bug
                    //     continue;
                    // }

                    std::expected<bool, exception_t> traffic_status = this->traffic_controller->thru(addr_arr[i]);

                    if (!traffic_status.has_value()){
                        response_exception_arr[i] = traffic_status.error();
                        continue;
                    }

                    if (!traffic_status.value()){
                        response_exception_arr[i] = dg::network_exception::SOCKET_BAD_TRAFFIC;
                        continue;
                    }

                    response_exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }

            void update() noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> ibapp_exception_arr(this->inbound_ip_side_set.size());
                dg::network_stack_allocation::NoExceptAllocation<Address[]> ibapp_ip_arr(this->inbound_ip_side_set.size());

                std::copy(this->inbound_ip_side_set.begin(), this->inbound_ip_side_set.end(), ibapp_ip_arr.get());
                this->nat_ip_controller->add_inbound(ibapp_ip_arr.get(), this->inbound_ip_side_set.size(), ibapp_exception_arr.get());

                for (size_t i = 0u; i < this->inbound_ip_side_set.size(); ++i){
                    if (dg::network_exception::is_failed(ibapp_exception_arr[i])){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(ibapp_exception_arr[i]));
                    }
                }

                this->inbound_ip_side_set.clear();
                this->traffic_controller->reset();
                this->thru_ip_set.clear();

                size_t inbound_addr_cap = this->nat_ip_controller->get_inbound_friend_addr_iteration_size();
                size_t inbound_addr_sz  = {};
                dg::network_stack_allocation::NoExceptAllocation<Address[]> addr_arr(inbound_addr_cap);

                this->nat_ip_controller->get_inbound_friend_addr(addr_arr.get(), 0u, inbound_addr_sz, inbound_addr_cap);
                this->thru_ip_set.insert(addr_arr.get(), std::next(addr_arr.get(), inbound_addr_sz));
            }
    };

    //OK
    class OutBoundBorderController: public virtual BorderControllerInterface, 
                                    public virtual UpdatableInterface{

        private:

            std::shared_ptr<external_interface::NATIPControllerInterface> nat_ip_controller;
            std::unique_ptr<packet_controller::TrafficControllerInterface> traffic_controller;
            dg::unordered_set<Address> outbound_ip_side_set;
            size_t outbound_ip_side_set_cap; //TODOs: bug_control next iteration
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            OutBoundBorderController(std::shared_ptr<external_interface::NATIPControllerInterface> nat_ip_controller,
                                     std::unique_ptr<packet_controller::TrafficControllerInterface> traffic_controller,
                                     dg::unordered_set<Address> outbound_ip_side_set,
                                     size_t outbound_ip_side_set_cap,
                                     std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                     stdx::hdi_container<size_t> consume_sz_per_load) noexcept: nat_ip_controller(std::move(nat_ip_controller)),
                                                                                                traffic_controller(std::move(traffic_controller)),
                                                                                                outbound_ip_side_set(std::move(outbound_ip_side_set)),
                                                                                                outbound_ip_side_set_cap(outbound_ip_side_set_cap),
                                                                                                mtx(std::move(mtx)),
                                                                                                consume_sz_per_load(std::move(consume_sz_per_load)){}

            void thru(Address * addr_arr, size_t sz, exception_t * response_exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                std::expected<size_t, exception_t> insert_sz = utility::finite_set_insert(this->outbound_ip_side_set, this->outbound_ip_side_set_cap, 
                                                                                          addr_arr, std::next(addr_arr, sz));

                if (!insert_sz.has_value() || insert_sz.value() != sz){
                    if (!insert_sz.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(insert_sz.error()));
                    } else{
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(dg::network_exception::RESOURCE_EXHAUSTION));
                    }                
                }

                for (size_t i = 0u; i < sz; ++i){
                    std::expected<bool, exception_t> traffic_status = this->traffic_controller->thru(addr_arr[i]);

                    if (!traffic_status.has_value()){
                        response_exception_arr[i] = traffic_status.error();
                        continue;
                    }

                    if (!traffic_status.value()){
                        response_exception_arr[i] = dg::network_exception::SOCKET_BAD_TRAFFIC;
                        continue;
                    }

                    response_exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }

            void update() noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> obapp_exception_arr(this->outbound_ip_side_set.size());
                dg::network_stack_allocation::NoExceptAllocation<Address[]> obapp_ip_arr(this->outbound_ip_side_set.size());

                std::copy(this->outbound_ip_side_set.begin(), this->outbound_ip_side_set.end(), obapp_ip_arr.get());
                this->nat_ip_controller->add_outbound(obapp_ip_arr.get(), this->outbound_ip_side_set.size(), obapp_exception_arr.get());

                for (size_t i = 0u; i < this->outbound_ip_side_set.size(); ++i){
                    if (dg::network_exception::is_failed(obapp_exception_arr[i])){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(obapp_exception_arr[i]));
                    }
                }

                this->outbound_ip_side_set.clear();
                this->traffic_controller->reset();
            }
    };

    //OK
    class NATPunchIPController: public virtual external_interface::NATIPControllerInterface{

        private:

            data_structure::temporal_finite_unordered_set<Address> inbound_ip_set;
            data_structure::temporal_finite_unordered_set<Address> outbound_ip_set;

        public:

            NATPunchIPController(data_structure::temporal_finite_unordered_set<Address> inbound_ip_set,
                                 data_structure::temporal_finite_unordered_set<Address> outbound_ip_set) noexcept: inbound_ip_set(std::move(inbound_ip_set)),
                                                                                                                   outbound_ip_set(std::move(outbound_ip_set)){}

            void add_inbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                size_t insert_cap   = this->inbound_ip_set.capacity();
                size_t insert_sz    = std::min(sz, insert_cap);

                for (size_t i = 0u; i < insert_sz; ++i){
                    this->inbound_ip_set.insert(addr_arr[i]);
                }

                std::fill(exception_arr, std::next(exception_arr, insert_sz), dg::network_exception::SUCCESS);
                std::fill(std::next(exception_arr, insert_sz), std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
            }

            void add_outbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                size_t insert_cap   = this->outbound_ip_set.capacity();
                size_t insert_sz    = std::min(sz, insert_cap);

                for (size_t i = 0u; i < insert_sz; ++i){
                    this->outbound_ip_set.insert(addr_arr[i]);
                }

                std::fill(exception_arr, std::next(exception_arr, insert_sz), dg::network_exception::SUCCESS);
                std::fill(std::next(exception_arr, insert_sz), std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
            }

            void get_inbound_friend_addr(Address * out_arr, size_t off, size_t& sz, size_t cap) noexcept{

                sz                  = 0u;
                size_t trimmed_off  = std::min(off, this->inbound_ip_set.size());
                auto first          = std::next(this->inbound_ip_set.begin(), trimmed_off);  
                auto last           = this->inbound_ip_set.end();

                for (auto it = first; it != last; ++it){
                    if (sz == cap){
                        return;
                    }

                    if (this->outbound_ip_set.contains(*it)){
                        out_arr[sz++] = *it; 
                    }
                }
            }

            auto get_inbound_friend_addr_iteration_size() noexcept -> size_t{

                return this->inbound_ip_set.size();
            }

            void get_outbound_friend_addr(Address * out_arr, size_t off, size_t& sz, size_t cap) noexcept{

                size_t trimmed_off  = std::min(off, this->outbound_ip_set.size());
                size_t peek_cap     = this->outbound_ip_set.size() - trimmed_off;
                sz                  = std::min(cap, peek_cap); 
                auto first          = std::next(this->outbound_ip_set.begin(), trimmed_off);
                auto last           = std::next(first, sz); 

                std::copy(first, last, out_arr);
            }

            auto get_outbound_friend_addr_iteration_size() noexcept -> size_t{

                return this->outbound_ip_set.size();
            }
    };

    //OK
    class NATFriendIPController: public virtual external_interface::NATIPControllerInterface{

        private:

            std::shared_ptr<external_interface::IPSieverInterface> inbound_ip_siever;
            std::shared_ptr<external_interface::IPSieverInterface> outbound_ip_siever;
            data_structure::temporal_finite_unordered_set<Address> inbound_friend_set;
            data_structure::temporal_finite_unordered_set<Address> outbound_friend_set;
            
        public:

            NATFriendIPController(std::shared_ptr<external_interface::IPSieverInterface> inbound_ip_siever,
                                  std::shared_ptr<external_interface::IPSieverInterface> outbound_ip_siever,
                                  data_structure::temporal_finite_unordered_set<Address> inbound_friend_set,
                                  data_structure::temporal_finite_unordered_set<Address> outbound_friend_set) noexcept: inbound_ip_siever(std::move(inbound_ip_siever)),
                                                                                                                        outbound_ip_siever(std::move(outbound_ip_siever)),
                                                                                                                        inbound_friend_set(std::move(inbound_friend_set)),
                                                                                                                        outbound_friend_set(std::move(outbound_friend_set)){}

            void add_inbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                size_t insert_cap   = this->inbound_friend_set.capacity();
                size_t insert_sz    = std::min(sz, insert_cap); 

                for (size_t i = 0u; i < insert_sz; ++i){
                    std::expected<bool, exception_t> is_thru = this->inbound_ip_siever->thru(addr_arr[i]);

                    if (!is_thru.has_value()){
                        exception_arr[i] = is_thru.error();
                        continue;
                    }

                    if (!is_thru.value()){
                        exception_arr[i] = dg::network_exception::SOCKET_BAD_IP_RULE;
                        continue;
                    }

                    this->inbound_friend_set.insert(addr_arr[i]);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }

                std::fill(std::next(exception_arr, insert_sz), std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
            }

            void add_outbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                size_t insert_cap   = this->outbound_friend_set.capacity();
                size_t insert_sz    = std::min(sz, insert_cap);

                for (size_t i = 0u; i < insert_sz; ++i){
                    std::expected<bool, exception_t> is_thru = this->outbound_ip_siever->thru(addr_arr[i]);

                    if (!is_thru.has_value()){
                        exception_arr[i] = is_thru.error();
                        continue;
                    }

                    if (!is_thru.value()){
                        exception_arr[i] = dg::network_exception::SOCKET_BAD_IP_RULE;
                        continue;
                    }

                    this->outbound_friend_set.insert(addr_arr[i]);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }

                std::fill(std::next(exception_arr, insert_sz), std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
            }

            void get_inbound_friend_addr(Address * addr_arr, size_t off, size_t& sz, size_t cap) noexcept{

                size_t adjusted_off     = std::min(off, this->inbound_friend_set.size()); 
                size_t peek_cap         = this->inbound_friend_set.size() - adjusted_off;
                sz                      = std::min(cap, peek_cap);  
                auto first              = std::next(this->inbound_friend_set.begin(), adjusted_off);
                auto last               = std::next(first, sz);

                std::copy(first, last, addr_arr);
            }

            auto get_inbound_friend_addr_iteration_size() noexcept -> size_t{

                return this->inbound_friend_set.size();
            }

            void get_outbound_friend_addr(Address * addr_arr, size_t off, size_t& sz, size_t cap) noexcept{

                size_t adjusted_off     = std::min(off, this->outbound_friend_set.size());
                size_t peek_cap         = this->outbound_friend_set.size() - adjusted_off;
                sz                      = std::min(cap, peek_cap);
                auto first              = std::next(this->outbound_friend_set.begin(), adjusted_off);
                auto last               = std::next(first, sz);

                std::copy(first, last, addr_arr);
            }

            auto get_outbound_friend_addr_iteration_size() noexcept -> size_t{

                return this->outbound_friend_set.size();
            }
    };

    //OK
    class NATIPController: public virtual external_interface::NATIPControllerInterface{

        private:

            std::unique_ptr<external_interface::NATIPControllerInterface> punch_controller;
            std::unique_ptr<external_interface::NATIPControllerInterface> friend_controller;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;

        public:

            NATIPController(std::unique_ptr<external_interface::NATIPControllerInterface> punch_controller,
                            std::unique_ptr<external_interface::NATIPControllerInterface> friend_controller,
                            std::unique_ptr<stdx::fair_atomic_flag> mtx) noexcept: punch_controller(std::move(punch_controller)),
                                                                       friend_controller(std::move(friend_controller)),
                                                                       mtx(std::move(mtx)){}

            void add_inbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> punch_exception_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> friend_exception_arr(sz);

                this->punch_controller->add_inbound(addr_arr, sz, punch_exception_arr.get());
                this->friend_controller->add_inbound(addr_arr, sz, friend_exception_arr.get());

                for (size_t i = 0u; i < sz; ++i){
                    if (dg::network_exception::is_success(punch_exception_arr[i]) && dg::network_exception::is_success(friend_exception_arr[i])){
                        exception_arr[i] = dg::network_exception::SUCCESS;
                    } else{
                        if (dg::network_exception::is_failed(punch_exception_arr[i])){
                            exception_arr[i] = punch_exception_arr[i];
                        } else{
                            exception_arr[i] = friend_exception_arr[i];
                        }
                    }
                }
            }

            void add_outbound(Address * addr_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> punch_exception_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> friend_exception_arr(sz);

                this->punch_controller->add_outbound(addr_arr, sz, punch_exception_arr.get());
                this->friend_controller->add_outbound(addr_arr, sz, friend_exception_arr.get());

                for (size_t i = 0u; i < sz; ++i){
                    if (dg::network_exception::is_success(punch_exception_arr[i]) && dg::network_exception::is_success(friend_exception_arr[i])){
                        exception_arr[i] = dg::network_exception::SUCCESS;
                    } else{
                        if (dg::network_exception::is_failed(punch_exception_arr[i])){
                            exception_arr[i] = punch_exception_arr[i];
                        } else{
                            exception_arr[i] = friend_exception_arr[i];
                        }
                    }
                }
            }

            void get_inbound_friend_addr(Address * output, size_t off, size_t& sz, size_t cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                size_t punch_controller_sz = this->punch_controller->get_inbound_friend_addr_iteration_size();

                if (off < punch_controller_sz){
                    if (off + cap <= punch_controller_sz){
                        this->punch_controller->get_inbound_friend_addr(output, off, sz, cap);
                    } else{                        
                        size_t tmp_sz           = {};
                        this->punch_controller->get_inbound_friend_addr(output, off, tmp_sz, cap);
                        Address * new_output    = std::next(output, tmp_sz);
                        size_t new_off          = 0u;
                        size_t new_sz           = {};
                        size_t new_cap          = cap - tmp_sz; 
                        this->friend_controller->get_inbound_friend_addr(new_output, new_off, new_sz, new_cap);
                        sz                      = tmp_sz + new_sz; 
                    }
                } else{
                    this->friend_controller->get_inbound_friend_addr(output, off - punch_controller_sz, sz, cap);
                }

                sz = std::distance(output, this->setify(output, std::next(output, sz))); //this might be malicious as we are writing data post the agreed-upon sz
            }

            auto get_inbound_friend_addr_iteration_size() noexcept -> size_t{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->punch_controller->get_inbound_friend_addr_iteration_size() + this->friend_controller->get_inbound_friend_addr_iteration_size();
            }

            void get_outbound_friend_addr(Address * output, size_t off, size_t& sz, size_t cap) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                size_t punch_controller_sz = this->punch_controller->get_outbound_friend_addr_iteration_size();

                if (off < punch_controller_sz){                    
                    if (off + cap <= punch_controller_sz){
                        this->punch_controller->get_outbound_friend_addr(output, off, sz, cap);
                    } else{
                        size_t tmp_sz           = {};
                        this->punch_controller->get_outbound_friend_addr(output, off, tmp_sz, cap);
                        Address * new_output    = std::next(output, tmp_sz);
                        size_t new_off          = 0u;
                        size_t new_sz           = {};
                        size_t new_cap          = cap - tmp_sz;
                        this->friend_controller->get_outbound_friend_addr(new_output, new_off, new_sz, new_cap);
                        sz                      = tmp_sz + new_sz;
                    }
                } else{
                    this->friend_controller->get_outbound_friend_addr(output, off - punch_controller_sz, sz, cap);
                }

                sz = std::distance(output, this->setify(output, std::next(output, sz))); //this might be malicious as we are writing data post the agreed-upon sz
            }

            auto get_outbound_friend_addr_iteration_size() noexcept -> size_t{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->punch_controller->get_outbound_friend_addr_iteration_size() + this->friend_controller->get_outbound_friend_addr_iteration_size();
            }

        private:

            inline auto setify(Address * first, Address * last) noexcept -> Address *{

                dg::unordered_set<Address> rs(first, last);
                return std::copy(rs.begin(), rs.end(), first);
            }
    };

    struct ComponentFactory{

        template <class T, class ...Args>
        static auto to_dg_vector(std::vector<T, Args...> vec) -> dg::vector<T>{

            dg::vector<T> rs{};

            for (size_t i = 0u; i < vec.size(); ++i){
                rs.emplace_back(std::move(vec[i]));
            }

            return rs;
        }

        static auto get_complex_reactor(intmax_t wakeup_threshold,
                                        size_t concurrent_subscriber_cap) -> std::unique_ptr<ComplexReactor>{
            
            const size_t MIN_CONCURRENT_SUBSCRIBER_CAP  = 1u;
            const size_t MAX_CONCURRENT_SUBSCRIBER_CAP  = size_t{1} << 25;

            if (std::clamp(concurrent_subscriber_cap, MIN_CONCURRENT_SUBSCRIBER_CAP, MAX_CONCURRENT_SUBSCRIBER_CAP) != concurrent_subscriber_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto rs = std::make_unique<ComplexReactor>(concurrent_subscriber_cap);
            rs->set_wakeup_threshold(wakeup_threshold);

            return rs;
        } 

        static auto get_batch_updater(std::vector<std::shared_ptr<UpdatableInterface>> updatable_vec) -> std::unique_ptr<UpdatableInterface>{

            for (const auto& updatable: updatable_vec){
                if (updatable == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }
            }

            return std::make_unique<BatchUpdater>(to_dg_vector(std::move(updatable_vec)));
        }

        static auto get_busy_observer(std::vector<std::shared_ptr<ContainerBusyAdapterInterface>> busy_adapter_vec) -> std::unique_ptr<NetworkBusyObserverInterface>{
            
            if (busy_adapter_vec.empty()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            for (const auto& busy_adapter: busy_adapter_vec){
                if (busy_adapter == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }
            }
            
            return std::make_unique<BusyObserver>(to_dg_vector(std::move(busy_adapter_vec)));
        }

        static auto get_asap_scheduler() -> std::unique_ptr<SchedulerInterface>{

            return std::make_unique<ASAPScheduler>();
        }

        static auto get_kernel_outbound_static_transmission_controller(uint32_t transmit_frequency) -> std::unique_ptr<KernelOutBoundTransmissionControllerInterface>{

            const uint32_t MIN_TRANSMIT_FREQUENCY = size_t{1};
            const uint32_t MAX_TRANSMIT_FREQUENCY = size_t{1} << 30; 

            if (std::clamp(transmit_frequency, MIN_TRANSMIT_FREQUENCY, MAX_TRANSMIT_FREQUENCY) != transmit_frequency){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ImmutableKernelOutBoundTransmissionController>(transmit_frequency);
        } 

        static auto get_no_exhaustion_controller() -> std::unique_ptr<ExhaustionControllerInterface>{

            return std::make_unique<NoExhaustionController>();
        }

        static auto get_default_exhaustion_controller() -> std::unique_ptr<ExhaustionControllerInterface>{

            return std::make_unique<DefaultExhaustionController>();
        }

        static auto get_incremental_id_generator(factory_id_t factory_id) -> std::unique_ptr<IDGeneratorInterface>{

            return std::make_unique<IncrementalIDGenerator>(dg::network_randomizer::randomize_int<local_packet_id_t>(), 
                                                            stdx::hdi_container<factory_id_t>{factory_id});
        }

        static auto get_random_id_generator(factory_id_t factory_id) -> std::unique_ptr<IDGeneratorInterface>{

            return std::make_unique<RandomIDGenerator>(stdx::hdi_container<factory_id_t>{factory_id});
        }

        static auto get_randomid_request_packet_generator(factory_id_t factory_id, Address host_addr) -> std::unique_ptr<RequestPacketGeneratorInterface>{

            return std::make_unique<RequestPacketGenerator>(get_random_id_generator(factory_id), 
                                                            host_addr);
        }

        static auto get_randomid_ack_packet_generator(factory_id_t factory_id, Address host_addr) -> std::unique_ptr<AckPacketGeneratorInterface>{

            return std::make_unique<AckPacketGenerator>(get_random_id_generator(factory_id),
                                                        host_addr);
        }
        
        static auto get_randomid_krescue_packet_generator(factory_id_t factory_id, Address host_addr) -> std::unique_ptr<KRescuePacketGeneratorInterface>{

            return std::make_unique<KRescuePacketGenerator>(get_random_id_generator(factory_id),
                                                            host_addr);
        }

        static auto get_inbound_packet_integrity_validator(Address host_addr) -> std::unique_ptr<PacketIntegrityValidatorInterface>{

            return std::make_unique<InBoundPacketIntegrityValidator>(host_addr);
        } 

        static auto get_kernel_rescue_post() -> std::unique_ptr<KernelRescuePostInterface>{

            std::chrono::time_point<std::chrono::utc_clock> arg = KernelRescuePost::NULL_TIMEPOINT;

            return std::make_unique<KernelRescuePost>(arg);
        } 

        static auto get_static_retransmission_delay_negotiator(std::chrono::nanoseconds interval) -> std::unique_ptr<RetransmissionDelayNegotiatorInterface>{

            using namespace std::chrono_literals;

            const std::chrono::nanoseconds MIN_INTERVAL = std::chrono::duration_cast<std::chrono::nanoseconds>(1ns);
            const std::chrono::nanoseconds MAX_INTERVAL = std::chrono::duration_cast<std::chrono::nanoseconds>(10min);

            if (std::clamp(interval, MIN_INTERVAL, MAX_INTERVAL) != interval){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<StaticRetransmissionDelayNegotiator>(interval);
        } 

        static auto get_empty_retransmission_controller() -> std::unique_ptr<RetransmissionControllerInterface>
        {
            return std::make_unique<EmptyRetransmissionController>();
        }

        static auto get_memory_efficient_retransmission_controller(size_t pkt_map_capacity,
                                                                   size_t acked_set_capacity,
                                                                   std::shared_ptr<packet_controller::RetransmissionDelayNegotiatorInterface> delay_negotiator,
                                                                   size_t ticking_clock_resolution,
                                                                   size_t max_retransmission_sz,
                                                                   size_t retriable_pkt_map_cap,
                                                                   size_t user_push_concurrency_sz,
                                                                   size_t retriable_push_concurrency_sz,
                                                                   size_t tentative_consume_sz) -> std::unique_ptr<RetransmissionControllerInterface>{

            const size_t MIN_PKT_MAP_CAPACITY               = 0u;
            const size_t MAX_PKT_MAP_CAPACITY               = size_t{1} << 30;
            const size_t MIN_ACKED_SET_CAPACITY             = 1u;
            const size_t MAX_ACKED_SET_CAPACITY             = size_t{1} << 30;
            const size_t MIN_TICKING_CLOCK_RESOLUTION       = 1u;
            const size_t MAX_TICKING_CLOCK_RESOLUTION       = size_t{1} << 30;
            const size_t MIN_MAX_RETRANSMISSION_SZ          = 0u;
            const size_t MAX_MAX_RETRANSMISSION_SZ          = 256u;
            const size_t MIN_RETRIABLE_PKT_MAP_CAP          = 1u;
            const size_t MAX_RETRIABLE_PKT_MAP_CAP          = size_t{1} << 30;
            
            const size_t MIN_USER_PUSH_CONCURRENCY_SZ       = 1u;
            const size_t MAX_USER_PUSH_CONCURRENCY_SZ       = size_t{1} << 20;
            
            const size_t MIN_RETRIABLE_PUSH_CONCURRENCY_SZ  = 1u;
            const size_t MAX_RETRIABLE_PUSH_CONCURRENCY_SZ  = size_t{1} << 20; 
            
            const size_t MIN_TENTATIVE_CONSUME_SZ           = 0u;
            const size_t MAX_TENTATIVE_CONSUME_SZ           = size_t{1} << 30;

            if (std::clamp(pkt_map_capacity, MIN_PKT_MAP_CAPACITY, MAX_PKT_MAP_CAPACITY) != pkt_map_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(acked_set_capacity, MIN_ACKED_SET_CAPACITY, MAX_ACKED_SET_CAPACITY) != acked_set_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (delay_negotiator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ticking_clock_resolution, MIN_TICKING_CLOCK_RESOLUTION, MAX_TICKING_CLOCK_RESOLUTION) != ticking_clock_resolution){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(max_retransmission_sz, MIN_MAX_RETRANSMISSION_SZ, MAX_MAX_RETRANSMISSION_SZ) != max_retransmission_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retriable_pkt_map_cap, MIN_RETRIABLE_PKT_MAP_CAP, MAX_RETRIABLE_PKT_MAP_CAP) != retriable_pkt_map_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(user_push_concurrency_sz, MIN_USER_PUSH_CONCURRENCY_SZ, MAX_USER_PUSH_CONCURRENCY_SZ) != user_push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retriable_push_concurrency_sz, MIN_RETRIABLE_PUSH_CONCURRENCY_SZ, MAX_RETRIABLE_PUSH_CONCURRENCY_SZ) != retriable_push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(tentative_consume_sz, MIN_TENTATIVE_CONSUME_SZ, MAX_TENTATIVE_CONSUME_SZ) != tentative_consume_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_pkt_map_consume_sz     = pkt_map_capacity;
            size_t tentative_acked_set_consume_sz   = acked_set_capacity;
            size_t consume_sz                       = std::max(std::min(std::min(tentative_pkt_map_consume_sz, tentative_acked_set_consume_sz), tentative_consume_sz), size_t{1u});

            return std::make_unique<MemoryEfficientRetransmissionController>(dg::pow2_cyclic_queue<RetransmissionWaitingItem>(stdx::ulog2(stdx::ceil2(user_push_concurrency_sz))),
                                                                             dg::pow2_cyclic_queue<RetransmissionWaitingItem>(stdx::ulog2(stdx::ceil2(retriable_push_concurrency_sz))),
                                                                             data_structure::temporal_ordered_packet_map(pkt_map_capacity),
                                                                             data_structure::temporal_finite_unordered_set<global_packet_id_t>(acked_set_capacity),
                                                                             std::move(delay_negotiator),
                                                                             ticking_clock_resolution,
                                                                             max_retransmission_sz,
                                                                             retriable_pkt_map_cap,
                                                                             stdx::make_unique_fair_atomic_flag(),
                                                                             consume_sz);
        }

        static auto get_retransmission_controller(std::chrono::nanoseconds transmission_delay, 
                                                  size_t max_retransmission_sz, 
                                                  size_t idhashset_cap, 
                                                  size_t retransmission_queue_cap,
                                                  size_t retriable_pkt_map_cap,
                                                  size_t consume_factor = 4u) -> std::unique_ptr<RetransmissionControllerInterface>{

            using namespace std::chrono_literals; 

            const std::chrono::nanoseconds MIN_DELAY    = std::chrono::duration_cast<std::chrono::nanoseconds>(1ns);
            const std::chrono::nanoseconds MAX_DELAY    = std::chrono::duration_cast<std::chrono::nanoseconds>(1min);
            const size_t MIN_MAX_RETRANSMISSION         = 0u;
            const size_t MAX_MAX_RETRANSMISSION         = 256u;
            const size_t MIN_IDHASHSET_CAP              = 1u;
            const size_t MAX_IDHASHSET_CAP              = size_t{1} << 25; 
            const size_t MIN_RETRANSMISSION_QUEUE_CAP   = 1u;
            const size_t MAX_RETRANSMISSION_QUEUE_CAP   = size_t{1} << 25; 
            const size_t MIN_RETRIABLE_PKT_MAP_CAP      = 1u;
            const size_t MAX_RETRIABLE_PKT_MAP_CAP      = size_t{1} << 30;

            if (std::clamp(transmission_delay, MIN_DELAY, MAX_DELAY) != transmission_delay){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(max_retransmission_sz, MIN_MAX_RETRANSMISSION, MAX_MAX_RETRANSMISSION) != max_retransmission_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(idhashset_cap, MIN_IDHASHSET_CAP, MAX_IDHASHSET_CAP) != idhashset_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retransmission_queue_cap, MIN_RETRANSMISSION_QUEUE_CAP, MAX_RETRANSMISSION_QUEUE_CAP) != retransmission_queue_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retriable_pkt_map_cap, MIN_RETRIABLE_PKT_MAP_CAP, MAX_RETRIABLE_PKT_MAP_CAP) != retriable_pkt_map_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_retransmission_consume_sz  = retransmission_queue_cap >> consume_factor;
            size_t tentative_idhashset_consume_sz       = idhashset_cap >> consume_factor; 
            size_t consume_sz                           = std::max(std::min(tentative_retransmission_consume_sz, tentative_idhashset_consume_sz), size_t{1u});

            return std::make_unique<RetransmissionController>(dg::pow2_cyclic_queue<QueuedPacket>{stdx::ulog2(stdx::ceil2(retransmission_queue_cap))},
                                                              data_structure::temporal_finite_unordered_set<global_packet_id_t>(idhashset_cap),
                                                              transmission_delay,
                                                              max_retransmission_sz,
                                                              retransmission_queue_cap,
                                                              retriable_pkt_map_cap,
                                                              stdx::make_unique_fair_atomic_flag(),
                                                              stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_exhaustion_controlled_retransmission_controller(std::unique_ptr<RetransmissionControllerInterface> base,
                                                                        std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                                        std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<RetransmissionControllerInterface>{

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (executor == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExhaustionControlledRetransmissionController>(std::move(base), std::move(executor), std::move(exhaustion_controller));
        } 

        static auto get_randomhash_distributed_retransmission_controller(dg::vector<std::unique_ptr<RetransmissionControllerInterface>> base_vec,
                                                                         size_t keyvalue_aggregation_cap = 2048u) -> std::unique_ptr<RetransmissionControllerInterface>{

            const size_t MIN_BASE_VEC_SZ                = size_t{1};
            const size_t MAX_BASE_VEC_SZ                = size_t{1} << 20;
            const size_t MIN_KEYVALUE_AGGREGATION_CAP   = size_t{1};
            const size_t MAX_KEYVALUE_AGGREGATION_CAP   = size_t{1} << 25;

            if (std::clamp(base_vec.size(), MIN_BASE_VEC_SZ, MAX_BASE_VEC_SZ) != base_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_vec_up        = std::make_unique<std::unique_ptr<RetransmissionControllerInterface>[]>(base_vec.size());
            size_t consumption_sz   = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < base_vec.size(); ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                consumption_sz  = std::min(consumption_sz, base_vec[i]->max_consume_size());
                base_vec_up[i]  = std::move(base_vec[i]);
            }

            if (std::clamp(keyvalue_aggregation_cap, MIN_KEYVALUE_AGGREGATION_CAP, MAX_KEYVALUE_AGGREGATION_CAP) != keyvalue_aggregation_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (keyvalue_aggregation_cap > consumption_sz){
                keyvalue_aggregation_cap = consumption_sz;
                // dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<HashDistributedRetransmissionController>(std::move(base_vec_up),
                                                                             base_vec.size(),
                                                                             keyvalue_aggregation_cap,
                                                                             consumption_sz);
        }

        static auto get_reacting_retransmission_controller(std::unique_ptr<RetransmissionControllerInterface> base,
                                                           size_t reacting_threshold,
                                                           size_t concurrent_subscriber_cap,
                                                           std::chrono::nanoseconds wait_time) -> std::unique_ptr<RetransmissionControllerInterface>{
 
            const size_t MIN_REACTING_THRESHOLD             = size_t{1};
            const size_t MAX_REACTING_THRESHOLD             = size_t{1} << 30;
            const std::chrono::nanoseconds MIN_WAIT_TIME    = std::chrono::nanoseconds{1}; 
            const std::chrono::nanoseconds MAX_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes{1});

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reacting_threshold, MIN_REACTING_THRESHOLD, MAX_REACTING_THRESHOLD) != reacting_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(wait_time, MIN_WAIT_TIME, MAX_WAIT_TIME) != wait_time){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ReactingRetransmissionController>(std::move(base),
                                                                      get_complex_reactor(reacting_threshold, concurrent_subscriber_cap),
                                                                      wait_time);
        }

        static auto get_buffer_fifo_container(size_t buffer_capacity,
                                              size_t push_concurrency_sz,
                                              size_t consume_factor = 4u) -> std::unique_ptr<BufferContainerInterface>{
            
            const size_t MIN_BUFFER_CAPACITY        = 1u;
            const size_t MAX_BUFFER_CAPACITY        = size_t{1} << 25;
            const size_t MIN_PUSH_CONCURRENCY_SZ    = 1u;
            const size_t MAX_PUSH_CONCURRENCY_SZ    = size_t{1} << 20;

            if (std::clamp(buffer_capacity, MIN_BUFFER_CAPACITY, MAX_BUFFER_CAPACITY) != buffer_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz         = buffer_capacity >> consume_factor;
            size_t consume_sz                   = std::max(tentative_consume_sz, size_t{1u});

            return std::make_unique<BufferFIFOContainer>(dg::pow2_cyclic_queue<BufferContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                         dg::pow2_cyclic_queue<internal_kernel_buffer>(stdx::ulog2(stdx::ceil2(buffer_capacity))),
                                                         buffer_capacity,
                                                         stdx::make_unique_fair_atomic_flag(),
                                                         stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_exhaustion_controlled_buffer_container(std::unique_ptr<BufferContainerInterface> base,
                                                               std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                               std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<BufferContainerInterface>{
            
            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (executor == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExhaustionControlledBufferContainer>(std::move(base),
                                                                         std::move(executor),
                                                                         std::move(exhaustion_controller));
        }

        static auto get_reacting_buffer_container(std::unique_ptr<BufferContainerInterface> base,
                                                  size_t reacting_threshold,
                                                  size_t concurrent_subscriber_cap,
                                                  std::chrono::nanoseconds wait_time,
                                                  size_t reactor_ease_threshold = 1u,
                                                  size_t reactor_busy_threshold = 1024u) -> std::unique_ptr<BufferContainerInterface>{

            const size_t MIN_REACTING_THRESHOLD             = size_t{1};
            const size_t MAX_REACTING_THRESHOLD             = size_t{1} << 30;
            const std::chrono::nanoseconds MIN_WAIT_TIME    = std::chrono::nanoseconds{1}; 
            const std::chrono::nanoseconds MAX_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes{1});
            const size_t MIN_REACTOR_EASE_THRESHOLD         = size_t{1};
            const size_t MAX_REACTOR_EASE_THRESHOLD         = size_t{1} << 30;
            const size_t MIN_REACTOR_BUSY_THRESHOLD         = size_t{1};
            const size_t MAX_REACTOR_BUSY_THRESHOLD         = size_t{1} << 30; 

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reacting_threshold, MIN_REACTING_THRESHOLD, MAX_REACTING_THRESHOLD) != reacting_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(wait_time, MIN_WAIT_TIME, MAX_WAIT_TIME) != wait_time){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }
            
            if (std::clamp(reactor_ease_threshold, MIN_REACTOR_EASE_THRESHOLD, MAX_REACTOR_EASE_THRESHOLD) != reactor_ease_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reactor_busy_threshold, MIN_REACTOR_BUSY_THRESHOLD, MAX_REACTOR_BUSY_THRESHOLD) != reactor_busy_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ReactingBufferContainer>(std::move(base),
                                                             get_complex_reactor(reacting_threshold, concurrent_subscriber_cap),
                                                             wait_time,
                                                             reactor_ease_threshold,
                                                             reactor_busy_threshold);

        }

        static auto get_randomhash_distributed_buffer_container(std::vector<std::unique_ptr<BufferContainerInterface>> base_vec) -> std::unique_ptr<BufferContainerInterface>{

            const size_t MIN_BASE_VEC_SZ            = size_t{1};
            const size_t MAX_BASE_VEC_SZ            = size_t{1} << 20;

            if (std::clamp(base_vec.size(), MIN_BASE_VEC_SZ, MAX_BASE_VEC_SZ) != base_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_vec_up        = std::make_unique<std::unique_ptr<BufferContainerInterface>[]>(base_vec.size());
            size_t consumption_sz   = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < base_vec.size(); ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                consumption_sz  = std::min(consumption_sz, base_vec[i]->max_consume_size());
                base_vec_up[i]  = std::move(base_vec[i]);
            }

            return std::make_unique<HashDistributedBufferContainer>(std::move(base_vec_up),
                                                                    base_vec.size(),
                                                                    consumption_sz);
        }
        
        template <class T>
        static auto get_default_lifo_unit_allocator(size_t queue_cap) -> std::unique_ptr<UnitAllocatorInterface<T>>{

            const size_t MIN_QUEUE_CAP = 1u;
            const size_t MAX_QUEUE_CAP = size_t{1} << 30;

            if (std::clamp(queue_cap, MIN_QUEUE_CAP, MAX_QUEUE_CAP) != queue_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<LIFOUnitAllocator<T>>(dg::vector<T>(),
                                                          queue_cap,
                                                          0u,
                                                          stdx::make_unique_fair_atomic_flag());
        }

        static auto get_fair_inbound_buffer_container(size_t distribution_queue_sz,
                                                      size_t waiting_queue_sz,
                                                      size_t leftover_queue_sz,
                                                      size_t push_concurrency_sz,
                                                      size_t vec_unit_sz) -> std::unique_ptr<BufferContainerInterface>{

            const size_t MIN_DISTRIBUTION_QUEUE_SZ  = size_t{1};
            const size_t MAX_DISTRIBUTION_QUEUE_SZ  = size_t{1} << 20;
            const size_t MIN_WAITING_QUEUE_SZ       = size_t{1};
            const size_t MAX_WAITING_QUEUE_SZ       = size_t{1} << 20;
            const size_t MIN_LEFTOVER_QUEUE_SZ      = size_t{1};
            const size_t MAX_LEFTOVER_QUEUE_SZ      = size_t{1} << 20;
            const size_t MIN_VEC_UNIT_SZ            = size_t{1};
            const size_t MAX_VEC_UNIT_SZ            = size_t{1} << 20;
            const size_t MIN_PUSH_CONCURRENCY_SZ    = size_t{1};
            const size_t MAX_PUSH_CONCURRENCY_SZ    = size_t{1} << 20;

            if (std::clamp(distribution_queue_sz, MIN_DISTRIBUTION_QUEUE_SZ, MAX_DISTRIBUTION_QUEUE_SZ) != distribution_queue_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(waiting_queue_sz, MIN_WAITING_QUEUE_SZ, MAX_WAITING_QUEUE_SZ) != waiting_queue_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(leftover_queue_sz, MIN_LEFTOVER_QUEUE_SZ, MAX_LEFTOVER_QUEUE_SZ) != leftover_queue_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(vec_unit_sz, MIN_VEC_UNIT_SZ, MAX_VEC_UNIT_SZ) != vec_unit_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t queue_cap = distribution_queue_sz + waiting_queue_sz + leftover_queue_sz; 

            return std::make_unique<FairInBoundBufferContainer>(dg::pow2_cyclic_queue<FairBufferContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>>(stdx::ulog2(stdx::ceil2(distribution_queue_sz))),
                                                                dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_kernel_buffer>> *, semaphore_impl::dg_binary_semaphore *>>(stdx::ulog2(stdx::ceil2(waiting_queue_sz))),
                                                                dg::pow2_cyclic_queue<dg::vector<internal_kernel_buffer>>(stdx::ulog2(stdx::ceil2(leftover_queue_sz))),
                                                                get_default_lifo_unit_allocator<dg::vector<internal_kernel_buffer>>(queue_cap),
                                                                stdx::make_unique_fair_atomic_flag(),
                                                                vec_unit_sz);
        }

        static auto get_prioritized_packet_container(size_t heap_capacity,
                                                     size_t consume_factor = 4u) -> std::unique_ptr<PacketContainerInterface>{
            
            const size_t MIN_HEAP_CAPACITY  = size_t{1};
            const size_t MAX_HEAP_CAPACITY  = size_t{1} << 25;

            if (std::clamp(heap_capacity, MIN_HEAP_CAPACITY, MAX_HEAP_CAPACITY) != heap_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz     = heap_capacity >> consume_factor;
            size_t consume_sz               = std::max(tentative_consume_sz, size_t{1u});
            auto vec                        = dg::vector<Packet>();
            vec.reserve(heap_capacity);

            return std::make_unique<PrioritizedPacketContainer>(std::move(vec),
                                                                heap_capacity,
                                                                stdx::make_unique_fair_atomic_flag(),
                                                                stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_packet_fifo_container(size_t packet_vec_capacity,
                                              size_t push_concurrency_sz,
                                              size_t consume_factor = 4u) -> std::unique_ptr<PacketContainerInterface>{
            
            const size_t MIN_VEC_CAPACITY           = size_t{1};
            const size_t MAX_VEC_CAPACITY           = size_t{1} << 25;
            const size_t MIN_PUSH_CONCURRENCY_SZ    = 1u;
            const size_t MAX_PUSH_CONCURRENCY_SZ    = size_t{1} << 20;

            if (std::clamp(packet_vec_capacity, MIN_VEC_CAPACITY, MAX_VEC_CAPACITY) != packet_vec_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz     = packet_vec_capacity >> consume_factor;
            size_t consume_sz               = std::max(tentative_consume_sz, size_t{1u});

            return std::make_unique<PacketFIFOContainer>(dg::pow2_cyclic_queue<PacketContainerWaitingItem>(stdx::ulog2(stdx::ceil2(packet_vec_capacity))),
                                                         dg::pow2_cyclic_queue<Packet>(stdx::ulog2(stdx::ceil2(packet_vec_capacity))),
                                                         packet_vec_capacity,
                                                         stdx::make_unique_fair_atomic_flag(),
                                                         stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_scheduled_packet_container(std::shared_ptr<SchedulerInterface> scheduler,
                                                   size_t packet_vec_capacity,
                                                   size_t consume_factor = 4u) -> std::unique_ptr<PacketContainerInterface>{
            
            const size_t MIN_VEC_CAPACITY   = size_t{1};
            const size_t MAX_VEC_CAPACITY   = size_t{1} << 25;

            if (scheduler == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(packet_vec_capacity, MIN_VEC_CAPACITY, MAX_VEC_CAPACITY) != packet_vec_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz     = packet_vec_capacity >> consume_factor;
            size_t consume_sz               = std::max(tentative_consume_sz, size_t{1u});
            auto vec                        = dg::vector<ScheduledPacket>{};
            vec.reserve(packet_vec_capacity); 

            return std::make_unique<ScheduledPacketContainer>(std::move(vec), 
                                                              std::move(scheduler),
                                                              packet_vec_capacity,
                                                              stdx::make_unique_fair_atomic_flag(),
                                                              stdx::hdi_container<size_t>{consume_sz});
        }

        // static auto get_std_outbound_packet_container(size_t ack_capacity,
        //                                               size_t request_capacity,
        //                                               size_t krescue_capacity,
        //                                               size_t accum_sz = constants::DEFAULT_ACCUMULATION_SIZE,
        //                                               size_t consume_factor = 4u) -> std::unique_ptr<PacketContainerInterface>{
            
        //     const size_t MIN_ACCUM_SZ   = 1u;
        //     const size_t MAX_ACCUM_SZ   = size_t{1} << 25; 

        //     if (std::clamp(accum_sz, MIN_ACCUM_SZ, MAX_ACCUM_SZ) != accum_sz){
        //         dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
        //     }

        //     auto ack_container  = get_prioritized_packet_container(ack_capacity, consume_factor);
        //     auto req_container  = get_packet_fifo_container(request_capacity, consume_factor);
        //     auto rsc_container  = get_packet_fifo_container(krescue_capacity, consume_factor);
        //     size_t consume_sz   = std::min(std::min(ack_container->max_consume_size(), req_container->max_consume_size()), rsc_container->max_consume_size());

        //     return std::make_unique<OutboundPacketContainer>(std::move(ack_container),
        //                                                      std::move(req_container),
        //                                                      std::move(rsc_container),
        //                                                      accum_sz,
        //                                                      accum_sz,
        //                                                      accum_sz,
        //                                                      stdx::hdi_container<size_t>{consume_sz});
        // }
        
        static auto get_normal_outbound_packet_container(size_t normal_packet_vec_queue_capacity,
                                                         size_t ack_packet_vec_queue_capacity,
                                                         size_t rescue_packet_vec_queue_capacity,
                                                         size_t waiting_queue_capacity,
                                                         size_t leftover_queue_capacity,
                                                         size_t unit_sz,
                                                         size_t push_concurrency_sz,
                                                         size_t accum_sz = constants::DEFAULT_ACCUMULATION_SIZE) -> std::unique_ptr<PacketContainerInterface>{
                
            const size_t MIN_NORMAL_PACKET_VEC_QUEUE_CAPACITY   = 1u;
            const size_t MAX_NORMAL_PACKET_VEC_QUEUE_CAPACITY   = size_t{1} << 25;
            const size_t MIN_ACK_PACKET_VEC_QUEUE_CAPACITY      = 1u;
            const size_t MAX_ACK_PACKET_VEC_QUEUE_CAPACITY      = size_t{1} << 25;
            const size_t MIN_RESCUE_PACKET_QUEUE_CAPACITY       = 1u;
            const size_t MAX_RESCUE_PACKET_QUEUE_CAPACITY       = size_t{1} << 25;
            const size_t MIN_ACCUM_SZ                           = 1u;
            const size_t MAX_ACCUM_SZ                           = size_t{1} << 25;
            const size_t MIN_UNIT_SZ                            = 1u;
            const size_t MAX_UNIT_SZ                            = size_t{1} << 25;
            const size_t MIN_PUSH_CONCURRENCY_SZ                = 1u;
            const size_t MAX_PUSH_CONCURRENCY_SZ                = size_t{1} << 20; 

            if (std::clamp(normal_packet_vec_queue_capacity, MIN_NORMAL_PACKET_VEC_QUEUE_CAPACITY, MAX_NORMAL_PACKET_VEC_QUEUE_CAPACITY) != normal_packet_vec_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ack_packet_vec_queue_capacity, MIN_ACK_PACKET_VEC_QUEUE_CAPACITY, MAX_ACK_PACKET_VEC_QUEUE_CAPACITY) != ack_packet_vec_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(rescue_packet_vec_queue_capacity, MIN_RESCUE_PACKET_QUEUE_CAPACITY, MAX_RESCUE_PACKET_QUEUE_CAPACITY) != rescue_packet_vec_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(accum_sz, MIN_ACCUM_SZ, MAX_ACCUM_SZ) != accum_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(unit_sz, MIN_UNIT_SZ, MAX_UNIT_SZ) != unit_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t queue_cap = normal_packet_vec_queue_capacity + ack_packet_vec_queue_capacity + rescue_packet_vec_queue_capacity
                                + waiting_queue_capacity + leftover_queue_capacity;

            return std::make_unique<NormalOutboundPacketContainer>(dg::pow2_cyclic_queue<FairPacketContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                   dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(normal_packet_vec_queue_capacity))),
                                                                   dg::pow2_cyclic_queue<FairPacketContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                   dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(ack_packet_vec_queue_capacity))),
                                                                   dg::pow2_cyclic_queue<FairPacketContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                   dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(rescue_packet_vec_queue_capacity))),
                                                                   dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>>(stdx::ulog2(stdx::ceil2(waiting_queue_capacity))),
                                                                   dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(leftover_queue_capacity))),
                                                                   get_default_lifo_unit_allocator<dg::vector<Packet>>(queue_cap),
                                                                   accum_sz,
                                                                   stdx::make_unique_fair_atomic_flag(),
                                                                   unit_sz);
        }

        static auto get_fair_inbound_packet_container(size_t packet_vec_queue_capacity,
                                                      size_t waiting_queue_capacity,
                                                      size_t leftover_queue_capacity,
                                                      size_t push_concurrency_sz,
                                                      size_t vec_unit_sz) -> std::unique_ptr<FairInBoundPacketContainer>{
                                                
            const size_t MIN_PACKET_VEC_QUEUE_CAPACITY  = 1u;
            const size_t MAX_PACKET_VEC_QUEUE_CAPACITY  = size_t{1} << 25;
            const size_t MIN_WAITING_QUEUE_CAPACITY     = 1u;
            const size_t MAX_WAITING_QUEUE_CAPACITY     = size_t{1} << 25;
            const size_t MIN_LEFTOVER_QUEUE_CAPACITY    = 1u;
            const size_t MAX_LEFTOVER_QUEUE_CAPACITY    = size_t{1} << 25; 
            const size_t MIN_VEC_UNIT_SZ                = 1u;
            const size_t MAX_VEC_UNIT_SZ                = size_t{1} << 20;
            const size_t MIN_PUSH_CONCURRENCY_SZ        = 1u;
            const size_t MAX_PUSH_CONCURRENCY_SZ        = size_t{1} << 20;

            if (std::clamp(packet_vec_queue_capacity, MIN_PACKET_VEC_QUEUE_CAPACITY, MAX_PACKET_VEC_QUEUE_CAPACITY) != packet_vec_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(waiting_queue_capacity, MIN_WAITING_QUEUE_CAPACITY, MAX_WAITING_QUEUE_CAPACITY) != waiting_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(leftover_queue_capacity, MIN_LEFTOVER_QUEUE_CAPACITY, MAX_LEFTOVER_QUEUE_CAPACITY) != leftover_queue_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }
            
            if (std::clamp(vec_unit_sz, MIN_VEC_UNIT_SZ, MAX_VEC_UNIT_SZ) != vec_unit_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t unit_allocator_cap = packet_vec_queue_capacity + waiting_queue_capacity + leftover_queue_capacity; 

            return std::make_unique<FairInBoundPacketContainer>(dg::pow2_cyclic_queue<FairPacketContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(packet_vec_queue_capacity))),
                                                                dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<Packet>> *, semaphore_impl::dg_binary_semaphore *>>(stdx::ulog2(stdx::ceil2(waiting_queue_capacity))),
                                                                dg::pow2_cyclic_queue<dg::vector<Packet>>(stdx::ulog2(stdx::ceil2(leftover_queue_capacity))),
                                                                get_default_lifo_unit_allocator<dg::vector<Packet>>(unit_allocator_cap),
                                                                stdx::make_unique_fair_atomic_flag(),
                                                                vec_unit_sz);
        }

        static auto get_reacting_packet_container(std::unique_ptr<PacketContainerInterface> base,
                                                  size_t reacting_threshold,
                                                  size_t concurrent_subscriber_cap,
                                                  std::chrono::nanoseconds wait_time,
                                                  size_t reactor_ease_threshold = 1u,
                                                  size_t reactor_busy_threshold = 1024u) -> std::unique_ptr<PacketContainerInterface>{

            const size_t MIN_REACTING_THRESHOLD             = size_t{1};
            const size_t MAX_REACTING_THRESHOLD             = size_t{1} << 30;
            const std::chrono::nanoseconds MIN_WAIT_TIME    = std::chrono::nanoseconds{1}; 
            const std::chrono::nanoseconds MAX_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes{1});
            const size_t MIN_REACTOR_EASE_THRESHOLD         = size_t{1};
            const size_t MAX_REACTOR_EASE_THRESHOLD         = size_t{1} << 30;
            const size_t MIN_REACTOR_BUSY_THRESHOLD         = size_t{1};
            const size_t MAX_REACTOR_BUSY_THRESHOLD         = size_t{1} << 30; 

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reacting_threshold, MIN_REACTING_THRESHOLD, MAX_REACTING_THRESHOLD) != reacting_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(wait_time, MIN_WAIT_TIME, MAX_WAIT_TIME) != wait_time){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reactor_ease_threshold, MIN_REACTOR_EASE_THRESHOLD, MAX_REACTOR_EASE_THRESHOLD) != reactor_ease_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reactor_busy_threshold, MIN_REACTOR_BUSY_THRESHOLD, MAX_REACTOR_BUSY_THRESHOLD) != reactor_busy_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ReactingPacketContainer>(std::move(base),
                                                             get_complex_reactor(reacting_threshold, concurrent_subscriber_cap),
                                                             wait_time,
                                                             reactor_ease_threshold,
                                                             reactor_busy_threshold);
        }

        static auto get_randomhash_distributed_packet_container(std::vector<std::unique_ptr<PacketContainerInterface>> base_vec) -> std::unique_ptr<HashDistributedPacketContainer>{


            const size_t MIN_BASE_VEC_SZ            = size_t{1};
            const size_t MAX_BASE_VEC_SZ            = size_t{1} << 20;

            if (std::clamp(base_vec.size(), MIN_BASE_VEC_SZ, MAX_BASE_VEC_SZ) != base_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_vec_up        = std::make_unique<std::unique_ptr<PacketContainerInterface>[]>(base_vec.size());
            size_t consumption_sz   = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < base_vec.size(); ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                consumption_sz  = std::min(consumption_sz, base_vec[i]->max_consume_size());
                base_vec_up[i]  = std::move(base_vec[i]);
            }

            return std::make_unique<HashDistributedPacketContainer>(std::move(base_vec_up),
                                                                    base_vec.size(),
                                                                    consumption_sz);
        }

        static auto get_inbound_id_controller(size_t idhashset_cap,
                                              size_t consume_factor = 4u) -> std::unique_ptr<InBoundIDControllerInterface>{
            
            const size_t MIN_IDHASHSET_CAP  = size_t{1};
            const size_t MAX_IDHASHSET_CAP  = size_t{1} << 25;

            if (std::clamp(idhashset_cap, MIN_IDHASHSET_CAP, MAX_IDHASHSET_CAP) != idhashset_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_consume_sz     = idhashset_cap >> consume_factor;
            size_t consume_sz               = std::max(tentative_consume_sz, size_t{1u});

            return std::make_unique<InBoundIDController>(data_structure::temporal_finite_unordered_set<kind_global_packet_id_t>(idhashset_cap), 
                                                         stdx::make_unique_fair_atomic_flag(),
                                                         stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_randomhash_distributed_inbound_id_controller(std::vector<std::unique_ptr<InBoundIDControllerInterface>> base_vec,
                                                                     size_t keyvalue_aggregation_cap = 2048u) -> std::unique_ptr<InBoundIDControllerInterface>{
            
            const size_t MIN_BASE_VEC_SZ                = size_t{1};
            const size_t MAX_BASE_VEC_SZ                = size_t{1} << 20;
            const size_t MIN_KEYVALUE_AGGREGATION_CAP   = size_t{1};
            const size_t MAX_KEYVALUE_AGGREGATION_CAP   = size_t{1} << 25;

            if (std::clamp(base_vec.size(), MIN_BASE_VEC_SZ, MAX_BASE_VEC_SZ) != base_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_vec_up        = std::make_unique<std::unique_ptr<InBoundIDControllerInterface>[]>(base_vec.size());
            size_t consumption_sz   = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < base_vec.size(); ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                consumption_sz  = std::min(consumption_sz, base_vec[i]->max_consume_size());
                base_vec_up[i]  = std::move(base_vec[i]);
            }

            if (std::clamp(keyvalue_aggregation_cap, MIN_KEYVALUE_AGGREGATION_CAP, MAX_KEYVALUE_AGGREGATION_CAP) != keyvalue_aggregation_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (keyvalue_aggregation_cap > consumption_sz){
                keyvalue_aggregation_cap = consumption_sz;
                // dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<HashDistributedInBoundIDController>(std::move(base_vec_up),
                                                                        base_vec.size(),
                                                                        keyvalue_aggregation_cap,
                                                                        consumption_sz);
        }

        static auto get_synchronous_traffic_controller(size_t peraddr_capacity, 
                                                       size_t global_capacity, 
                                                       size_t addr_capacity) -> std::unique_ptr<TrafficControllerInterface>{

            const size_t MIN_PERADDR_CAPACITY   = 1u;
            const size_t MAX_PERADDR_CAPACITY   = size_t{1} << 25;
            const size_t MIN_GLOBAL_CAPACITY    = 1u;
            const size_t MAX_GLOBAL_CAPACITY    = size_t{1} << 25;
            const size_t MIN_ADDR_CAPACITY      = 1u;
            const size_t MAX_ADDR_CAPACITY      = size_t{1} << 25;

            if (std::clamp(peraddr_capacity, MIN_PERADDR_CAPACITY, MAX_PERADDR_CAPACITY) != peraddr_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(global_capacity, MIN_GLOBAL_CAPACITY, MAX_GLOBAL_CAPACITY) != global_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(addr_capacity, MIN_ADDR_CAPACITY, MAX_ADDR_CAPACITY) != addr_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto address_counter_map    = dg::unordered_unstable_map<Address, size_t>{};
            size_t global_counter       = 0u; 

            address_counter_map.reserve(addr_capacity);

            return std::make_unique<TrafficController>(std::move(address_counter_map), 
                                                       peraddr_capacity, 
                                                       global_capacity, 
                                                       addr_capacity, 
                                                       global_counter);
        }

        static auto get_inbound_border_controller(std::shared_ptr<external_interface::NATIPControllerInterface> natip_controller,
                                                  size_t peraddr_capacity,
                                                  size_t global_capacity,
                                                  size_t addr_capacity,
                                                  size_t side_update_buf_capacity,
                                                  size_t consume_factor = 4u) -> std::unique_ptr<InBoundBorderController>{

            if (natip_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            const size_t MIN_SIDE_UPDATE_BUF_CAPACITY   = size_t{1};
            const size_t MAX_SIDE_UPDATE_BUF_CAPACITY   = size_t{1} << 25;

            if (std::clamp(side_update_buf_capacity, MIN_SIDE_UPDATE_BUF_CAPACITY, MAX_SIDE_UPDATE_BUF_CAPACITY) != side_update_buf_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            dg::unordered_set<Address> thru_ip_set{};
            dg::unordered_set<Address> inbound_ip_side_set{};

            inbound_ip_side_set.reserve(side_update_buf_capacity);

            size_t tentative_consume_sz = side_update_buf_capacity >> consume_factor;
            size_t consume_sz           = std::max(tentative_consume_sz, size_t{1u});

            return std::make_unique<InBoundBorderController>(std::move(natip_controller),
                                                             get_synchronous_traffic_controller(peraddr_capacity, global_capacity, addr_capacity),
                                                             std::move(thru_ip_set),
                                                             std::move(inbound_ip_side_set),
                                                             side_update_buf_capacity,
                                                             stdx::make_unique_fair_atomic_flag(),
                                                             stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_outbound_border_controller(std::shared_ptr<external_interface::NATIPControllerInterface> natip_controller,
                                                   size_t peraddr_capacity,
                                                   size_t global_capacity,
                                                   size_t addr_capacity,
                                                   size_t side_update_buf_capacity,
                                                   size_t consume_factor = 4u) -> std::unique_ptr<OutBoundBorderController>{
            
            if (natip_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            const size_t MIN_SIDE_UPDATE_BUF_CAPACITY   = size_t{1};
            const size_t MAX_SIDE_UPDATE_BUF_CAPACITY   = size_t{1} << 25;

            if (std::clamp(side_update_buf_capacity, MIN_SIDE_UPDATE_BUF_CAPACITY, MAX_SIDE_UPDATE_BUF_CAPACITY) != side_update_buf_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            dg::unordered_set<Address> outbound_ip_side_set{};
            outbound_ip_side_set.reserve(side_update_buf_capacity);

            size_t tentative_consume_sz = side_update_buf_capacity >> consume_factor;
            size_t consume_sz           = std::max(tentative_consume_sz, size_t{1});

            return std::make_unique<OutBoundBorderController>(std::move(natip_controller),
                                                              get_synchronous_traffic_controller(peraddr_capacity, global_capacity, addr_capacity),
                                                              std::move(outbound_ip_side_set),
                                                              side_update_buf_capacity,
                                                              stdx::make_unique_fair_atomic_flag(),
                                                              stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_synchronous_natpunch_ip_controller(size_t inbound_set_capacity,
                                                           size_t outbound_set_capacity) -> std::unique_ptr<external_interface::NATIPControllerInterface>{
            
            const size_t MIN_INBOUND_SET_CAPACITY   = size_t{1};
            const size_t MAX_INBOUND_SET_CAPACITY   = size_t{1} << 25;
            const size_t MIN_OUTBOUND_SET_CAPACITY  = size_t{1};
            const size_t MAX_OUTBOUND_SET_CAPACITY  = size_t{1} << 25;

            if (std::clamp(inbound_set_capacity, MIN_INBOUND_SET_CAPACITY, MAX_INBOUND_SET_CAPACITY) != inbound_set_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(outbound_set_capacity, MIN_OUTBOUND_SET_CAPACITY, MAX_OUTBOUND_SET_CAPACITY) != outbound_set_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<NATPunchIPController>(data_structure::temporal_finite_unordered_set<Address>(inbound_set_capacity),
                                                          data_structure::temporal_finite_unordered_set<Address>(outbound_set_capacity));
        }

        static auto get_synchronous_natfriend_ip_controller(std::shared_ptr<external_interface::IPSieverInterface> inbound_rule,
                                                            std::shared_ptr<external_interface::IPSieverInterface> outbound_rule,
                                                            size_t inbound_set_capacity,
                                                            size_t outbound_set_capacity) -> std::unique_ptr<external_interface::NATIPControllerInterface>{

            const size_t MIN_INBOUND_SET_CAPACITY   = size_t{1};
            const size_t MAX_INBOUND_SET_CAPACITY   = size_t{1} << 25;
            const size_t MIN_OUTBOUND_SET_CAPACITY  = size_t{1};
            const size_t MAX_OUTBOUND_SET_CAPACITY  = size_t{1} << 25;

            if (inbound_rule == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (outbound_rule == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(inbound_set_capacity, MIN_INBOUND_SET_CAPACITY, MAX_INBOUND_SET_CAPACITY) != inbound_set_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(outbound_set_capacity, MIN_OUTBOUND_SET_CAPACITY, MAX_OUTBOUND_SET_CAPACITY) != outbound_set_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<NATFriendIPController>(inbound_rule,
                                                           outbound_rule,
                                                           data_structure::temporal_finite_unordered_set<Address>(inbound_set_capacity),
                                                           data_structure::temporal_finite_unordered_set<Address>(outbound_set_capacity));
        }

        static auto get_nat_ip_controller(std::shared_ptr<external_interface::IPSieverInterface> inbound_rule,
                                          std::shared_ptr<external_interface::IPSieverInterface> outbound_rule,
                                          size_t inbound_set_capacity,
                                          size_t outbound_set_capacity) -> std::unique_ptr<external_interface::NATIPControllerInterface>{
            
            return std::make_unique<NATIPController>(get_synchronous_natpunch_ip_controller(inbound_set_capacity, outbound_set_capacity),
                                                     get_synchronous_natfriend_ip_controller(inbound_rule, outbound_rule, inbound_set_capacity, outbound_set_capacity),
                                                     stdx::make_unique_fair_atomic_flag());
        }

        static auto get_exhaustion_controlled_packet_container(std::unique_ptr<PacketContainerInterface> base, 
                                                               std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> retry_device, 
                                                               std::shared_ptr<packet_controller::ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<PacketContainerInterface>{

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (retry_device == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExhaustionControlledPacketContainer>(std::move(base),
                                                                         std::move(retry_device),
                                                                         std::move(exhaustion_controller));
        }
    };
}

namespace dg::network_kernel_mailbox_impl1::worker{

    using namespace dg::network_kernel_mailbox_impl1::model; 

    //initially I thought that we could push data through the pipe, yet we are missing one variable to observe the traffic to set the reactor + inbound accordingly
    //we have set up for this to be externally dependency-injectable, yet it's advisable that we are observing the global network configuration to adjust the variables

    //it's very super complicated that we have come up with this idea, we'd want uninterruptable recv because it'd avoid RAM BUS for the intermediate containers
    //we'd want to receive batches of datas to avoid context switch + keep the subscriber queue hot (so we could pull data from the kernel as soon as possible) 

    class NetworkStatusMonitorWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<external_interface::NetworkBusyStatusRetrieverInterface> netstat_retriever;
            std::shared_ptr<packet_controller::NetworkBusyObserverInterface> busy_observer;
            std::chrono::nanoseconds break_time; 

        public:

            NetworkStatusMonitorWorker(std::shared_ptr<external_interface::NetworkBusyStatusRetrieverInterface> netstat_retriever,
                                       std::shared_ptr<packet_controller::NetworkBusyObserverInterface> busy_observer,
                                       std::chrono::nanoseconds break_time) noexcept: netstat_retriever(std::move(netstat_retriever)),
                                                                                      busy_observer(std::move(busy_observer)),
                                                                                      break_time(break_time){}
            
            bool run_one_epoch() noexcept{

                using netstat_interface_t = external_interface::NetworkBusyStatusRetrieverInterface;
                using observer_interface_t = packet_controller::NetworkBusyObserverInterface;

                std::expected<netstat_interface_t::busy_level_t, exception_t> busy_level = this->netstat_retriever->get();

                if (busy_level.has_value()){
                    switch (busy_level.value()){
                        case netstat_interface_t::BUSY_0:
                        {
                            this->busy_observer->notify(observer_interface_t::BUSY_0);
                            break;
                        }
                        case netstat_interface_t::BUSY_1:
                        {
                            this->busy_observer->notify(observer_interface_t::BUSY_1);
                            break;
                        }
                        case netstat_interface_t::BUSY_2:
                        {
                            this->busy_observer->notify(observer_interface_t::BUSY_2);
                            break;
                        }
                        default:
                        {
                            if constexpr(DEBUG_MODE_FLAG){
                                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            } else{
                                std::unreachable();
                            }
                        }
                    }
                } else{
                    dg::network_log_stackdump::error(dg::network_exception::verbose(busy_level.error()));
                }

                std::this_thread::sleep_for(this->break_time);
                return true;
            }
    };

    class BufferContainerRedistributorWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::BufferContainerInterface> fr_warehouse;
            std::shared_ptr<packet_controller::BufferContainerInterface> to_warehouse;
            size_t fr_warehouse_get_cap;
            size_t to_warehouse_push_cap;
            size_t busy_threshold;

        public:

            BufferContainerRedistributorWorker(std::shared_ptr<packet_controller::BufferContainerInterface> fr_warehouse,
                                               std::shared_ptr<packet_controller::BufferContainerInterface> to_warehouse,
                                               size_t fr_warehouse_get_cap,
                                               size_t to_warehouse_push_cap,
                                               size_t busy_threshold) noexcept: fr_warehouse(std::move(fr_warehouse)),
                                                                                to_warehouse(std::move(to_warehouse)),
                                                                                fr_warehouse_get_cap(fr_warehouse_get_cap),
                                                                                to_warehouse_push_cap(to_warehouse_push_cap),
                                                                                busy_threshold(busy_threshold){}

            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<internal_kernel_buffer[]> recv_buf_arr(this->fr_warehouse_get_cap);
                size_t fr_warehouse_get_sz = 0u;

                this->fr_warehouse->pop(recv_buf_arr.get(), fr_warehouse_get_sz, this->fr_warehouse_get_cap);

                auto delivery_resolutor             = InternalDeliveryResolutor{};
                delivery_resolutor.dst              = this->to_warehouse.get();

                size_t adjusted_delivery_sz         = std::min(std::min(fr_warehouse_get_sz, this->to_warehouse_push_cap), this->to_warehouse->max_consume_size());
                size_t deliverer_allocation_cost    = dg::network_producer_consumer::delvrsrv_allocation_cost(&delivery_resolutor, adjusted_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_buf(deliverer_allocation_cost);
                auto deliverer                      = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&delivery_resolutor, adjusted_delivery_sz, deliverer_buf.get()));

                for (size_t i = 0u; i < fr_warehouse_get_sz; ++i){
                    dg::network_producer_consumer::delvrsrv_deliver(deliverer.get(), std::move(recv_buf_arr[i]));
                }

                return fr_warehouse_get_sz >= this->busy_threshold;
            }
        
        private:

            struct InternalDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<internal_kernel_buffer>{

                packet_controller::BufferContainerInterface * dst;

                void push(std::move_iterator<internal_kernel_buffer *> data_arr, size_t data_arr_sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(data_arr_sz);
                    this->dst->push(data_arr, data_arr_sz, exception_arr.get());

                    for (size_t i = 0u; i < data_arr_sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    class PacketContainerRedistributorWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::PacketContainerInterface> fr_warehouse;
            std::shared_ptr<packet_controller::PacketContainerInterface> to_warehouse;
            size_t fr_warehouse_get_cap;
            size_t to_warehouse_push_cap;
            size_t busy_threshold;
        
        public:

            PacketContainerRedistributorWorker(std::shared_ptr<packet_controller::PacketContainerInterface> fr_warehouse,
                                               std::shared_ptr<packet_controller::PacketContainerInterface> to_warehouse,
                                               size_t fr_warehouse_get_cap,
                                               size_t to_warehouse_push_cap,
                                               size_t busy_threshold) noexcept: fr_warehouse(std::move(fr_warehouse)),
                                                                                to_warehouse(std::move(to_warehouse)),
                                                                                fr_warehouse_get_cap(fr_warehouse_get_cap),
                                                                                to_warehouse_push_cap(to_warehouse_push_cap),
                                                                                busy_threshold(busy_threshold){}
            
            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<Packet[]> recv_packet_arr(this->fr_warehouse_get_cap);
                size_t fr_warehouse_get_sz = 0u;

                this->fr_warehouse->pop(recv_packet_arr.get(), fr_warehouse_get_sz, this->fr_warehouse_get_cap);

                auto delivery_resolutor             = InternalDeliveryResolutor{};
                delivery_resolutor.dst              = this->to_warehouse.get();

                size_t adjusted_delivery_sz         = std::min(std::min(fr_warehouse_get_sz, this->to_warehouse_push_cap), this->to_warehouse->max_consume_size());
                size_t deliverer_allocation_cost    = dg::network_producer_consumer::delvrsrv_allocation_cost(&delivery_resolutor, adjusted_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> deliverer_buf(deliverer_allocation_cost);
                auto deliverer                      = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&delivery_resolutor, adjusted_delivery_sz, deliverer_buf.get()));

                for (size_t i = 0u; i < fr_warehouse_get_sz; ++i){
                    dg::network_producer_consumer::delvrsrv_deliver(deliverer.get(), std::move(recv_packet_arr[i]));
                }

                return fr_warehouse_get_sz >= this->busy_threshold;
            }
        
        private:
            
            struct InternalDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                packet_controller::PacketContainerInterface * dst;

                void push(std::move_iterator<Packet *> pkt_arr, size_t pkt_arr_sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(pkt_arr_sz);
                    this->dst->push(pkt_arr, pkt_arr_sz, exception_arr.get());

                    for (size_t i = 0u; i < pkt_arr_sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    class OutBoundWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container;
            std::shared_ptr<packet_controller::BorderControllerInterface> border_controller;
            std::shared_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface> exhaustion_controller;
            std::shared_ptr<model::SocketHandle> socket;
            size_t packet_consumption_cap;
            size_t packet_transmit_cap;
            size_t busy_threshold_sz;

        public:

            OutBoundWorker(std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                           std::shared_ptr<packet_controller::BorderControllerInterface> border_controller,
                           std::shared_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface> exhaustion_controller,
                           std::shared_ptr<model::SocketHandle> socket,
                           size_t packet_consumption_cap,
                           size_t packet_transmit_cap,
                           size_t busy_threshold_sz) noexcept: outbound_packet_container(std::move(outbound_packet_container)),
                                                               border_controller(std::move(border_controller)),
                                                               exhaustion_controller(std::move(exhaustion_controller)),
                                                               socket(std::move(socket)),
                                                               packet_consumption_cap(packet_consumption_cap),
                                                               packet_transmit_cap(packet_transmit_cap),
                                                               busy_threshold_sz(busy_threshold_sz){}

            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<Packet[]> packet_arr(this->packet_consumption_cap);
                size_t packet_arr_sz    = {};
                this->outbound_packet_container->pop(packet_arr.get(), packet_arr_sz, this->packet_consumption_cap);

                dg::network_stack_allocation::NoExceptAllocation<Address[]> addr_arr(packet_arr_sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> traffic_response_arr(packet_arr_sz);

                std::transform(packet_arr.get(), std::next(packet_arr.get(), packet_arr_sz), addr_arr.get(), [](const Packet& pkt){return pkt.to_addr;});
                this->border_controller->thru(addr_arr.get(), packet_arr_sz, traffic_response_arr.get());

                {
                    auto mailchimp_resolutor                    = InternalMailChimpResolutor{};
                    mailchimp_resolutor.socket                  = this->socket.get();
                    mailchimp_resolutor.exhaustion_controller   = this->exhaustion_controller.get();

                    size_t trimmed_mailchimp_delivery_sz        = std::min(std::min(this->packet_transmit_cap, packet_arr_sz), socket_service::batchsend_max_array_size());
                    size_t mailchimp_deliverer_alloc_sz         = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&mailchimp_resolutor, trimmed_mailchimp_delivery_sz);
                    dg::network_stack_allocation::NoExceptRawAllocation<char[]> mailchimp_deliverer_mem(mailchimp_deliverer_alloc_sz);
                    auto mailchimp_deliverer                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&mailchimp_resolutor, trimmed_mailchimp_delivery_sz, mailchimp_deliverer_mem.get())); 

                    for (size_t i = 0u; i < packet_arr_sz; ++i){
                        if (dg::network_exception::is_failed(traffic_response_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(traffic_response_arr[i]));
                            continue;
                        }

                        auto mailchimp_arg      = InternalMailChimpArgument{};
                        Address to_addr         = packet_arr[i].to_addr;

                        std::expected<internal_kernel_buffer, exception_t> serialized_buffer = packet_service::serialize_packet(std::move(packet_arr[i]));
                        
                        if (!serialized_buffer.has_value()){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(serialized_buffer.error()));
                            continue;
                        }

                        mailchimp_arg.content   = std::move(serialized_buffer.value());

                        dg::network_producer_consumer::delvrsrv_kv_deliver(mailchimp_deliverer.get(), to_addr, std::move(mailchimp_arg));
                    }
                }

                return packet_arr_sz >= this->busy_threshold_sz;
            }

        private:

            struct InternalMailChimpArgument{
                internal_kernel_buffer content;
            };

            struct InternalMailChimpResolutor: dg::network_producer_consumer::KVConsumerInterface<Address, InternalMailChimpArgument>{

                model::SocketHandle * socket;
                packet_controller::KernelOutBoundTransmissionControllerInterface * exhaustion_controller;

                void push(const Address& to_addr, std::move_iterator<InternalMailChimpArgument *> data_arr, size_t sz) noexcept{

                    exception_t mailchimp_freq_update_err           = this->exhaustion_controller->update_waiting_size(sz);

                    if (dg::network_exception::is_failed(mailchimp_freq_update_err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(mailchimp_freq_update_err));
                    } 

                    InternalMailChimpArgument * base_data_arr       = data_arr.base();
                    uint32_t frequency                              = this->exhaustion_controller->get_transmit_frequency();
                    uint32_t agg_frequency                          = frequency / std::max(sz, size_t{1}); 

                    std::chrono::nanoseconds transmit_period        = packet_service::frequency_to_period(agg_frequency);

                    dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<void>[]> buf_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<size_t[]> sz_arr(sz);                    

                    for (size_t i = 0u; i < sz; ++i){
                        buf_arr[i]  = base_data_arr[i].content.data();
                        sz_arr[i]   = base_data_arr[i].content.size();
                    }
                    
                    exception_t err = dg::network_kernel_mailbox_impl1::socket_service::batchsend_noblock(*socket,
                                                                                                          to_addr,
                                                                                                          buf_arr.get(),
                                                                                                          sz_arr.get(),
                                                                                                          sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    stdx::high_resolution_sleep(transmit_period);
                }
            };
    };
 
    //what I have desparately trying to work on is to distinct two buffer pools of retransmission and that of the global buffer queues
    //because this is the contract of guaranteeing to deliver the message up to 1 - 10 ** -8 success rate 
    //so I guess we'd have to polymorphic the allocator, by writing directly on the prefix of the allocations
    //this hinge of logic is so important that we must implement, even if there isnt an exhaustion case
    //according to the observable of the software and the contracts, also the stable logics theory, I think that this is correctly implemented in the sense of in_queue, SUCCESS == guarantee of delivery up to certain percentages based on the configurations

    //it's actually a marvel of engineering that we have got to this point
    //because the retriables with the right configuration (2s * 10 times), it's almost impossible to miss a SUCCESS packet if correctly scheduled
    //and it's also almost impossible for the retried packet to be duplicated (such is that the previous retry of the same packet is still in its lifetime)
    //but we already capped all the components to make sure that our memory consumption is converging as the application is running, and not diverging in the sense of fragmentation or usages or etc.

    class RetransmissionWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller;
            std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container;
            size_t retransmission_consumption_cap;
            size_t retransmission_accumulation_cap;
            size_t busy_threshold_sz;

        public:

            RetransmissionWorker(std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller,
                                 std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                                 size_t retransmission_consumption_cap,
                                 size_t retransmission_accumulation_cap,
                                 size_t busy_threshold_sz) noexcept: retransmission_controller(std::move(retransmission_controller)),
                                                                     outbound_packet_container(std::move(outbound_packet_container)),
                                                                     retransmission_consumption_cap(retransmission_consumption_cap),
                                                                     retransmission_accumulation_cap(retransmission_accumulation_cap),
                                                                     busy_threshold_sz(busy_threshold_sz){}

            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<Packet[]> packet_arr(this->retransmission_consumption_cap);
                size_t packet_arr_sz                = {};
                this->retransmission_controller->get_retriables(packet_arr.get(), packet_arr_sz, this->retransmission_consumption_cap);

                auto delivery_resolutor             = InternalDeliveryResolutor{};
                delivery_resolutor.retransmit_dst   = this->retransmission_controller.get();
                delivery_resolutor.container_dst    = this->outbound_packet_container.get();

                size_t trimmed_delivery_handle_sz   = std::min(std::min(std::min(this->retransmission_controller->max_consume_size(), this->outbound_packet_container->max_consume_size()), packet_arr_sz), this->retransmission_accumulation_cap);
                size_t dh_allocation_cost           = dg::network_producer_consumer::delvrsrv_allocation_cost(&delivery_resolutor, trimmed_delivery_handle_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> dh_mem(dh_allocation_cost);
                auto delivery_handle                = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&delivery_resolutor, trimmed_delivery_handle_sz, dh_mem.get()));

                //--------------------------------------

                auto single_delivery_resolutor              = InternalSingleDeliveryResolutor{};
                single_delivery_resolutor.dst               = this->outbound_packet_container.get();

                size_t trimmed_single_delivery_handle_sz    = std::min(std::min(this->outbound_packet_container->max_consume_size(), packet_arr_sz), this->retransmission_accumulation_cap);
                size_t sdh_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&single_delivery_resolutor, trimmed_single_delivery_handle_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> sdh_mem(sdh_allocation_cost);
                auto single_delivery_handle                 = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&single_delivery_resolutor, trimmed_single_delivery_handle_sz, sdh_mem.get())); 

                for (size_t i = 0u; i < packet_arr_sz; ++i){
                    std::expected<Packet, exception_t> retransmit_pkt = this->copy_packet(packet_arr[i]);

                    if (!retransmit_pkt.has_value()){
                        dg::network_producer_consumer::delvrsrv_deliver(single_delivery_handle.get(), std::move(packet_arr[i]));
                        continue;
                    }

                    auto arg = DeliveryArgument{
                        .container_pkt  = std::move(packet_arr[i]),
                        .retransmit_pkt = std::move(retransmit_pkt.value())
                    };

                    dg::network_producer_consumer::delvrsrv_deliver(delivery_handle.get(), std::move(arg));
                }

                return packet_arr_sz >= this->busy_threshold_sz;
            }

        private:

            auto copy_packet(const Packet& pkt) noexcept -> std::expected<Packet, exception_t>
            {
                if (packet_service::is_request_packet(pkt))
                {
                    RequestPacket new_pkt               = {};
                    new_pkt.content                     = internal_kernel_buffer(allocation::RetransmissionAllocatorInstance::get());
                    static_cast<PacketHeader&>(new_pkt) = static_cast<const PacketHeader&>(pkt);

                    try
                    {
                        static_cast<XOnlyRequestPacket&>(new_pkt) = std::get<XOnlyRequestPacket>(pkt.xonly_content);
                    }
                    catch (...)
                    {
                        return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
                    }

                    return packet_service::virtualize_request_packet(std::move(new_pkt));
                }
                else
                {
                    return dg::network_exception::cstyle_initialize<Packet>(pkt);
                }
            } 

            struct DeliveryArgument{

                Packet container_pkt;
                Packet retransmit_pkt;
            };

            struct InternalDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<DeliveryArgument>{

                packet_controller::RetransmissionControllerInterface * retransmit_dst;
                packet_controller::PacketContainerInterface * container_dst;

                void push(std::move_iterator<DeliveryArgument *> delivery_arg_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> packet_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> cpy_packet_arr(sz);

                    DeliveryArgument * base_delivery_arg_arr = delivery_arg_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        packet_arr[i]       = std::move(base_delivery_arg_arr[i].container_pkt);
                        cpy_packet_arr[i]   = std::move(base_delivery_arg_arr[i].retransmit_pkt);
                    }

                    this->container_dst->push(std::make_move_iterator(packet_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }

                    this->retransmit_dst->reentrant_add_retriables(std::make_move_iterator(cpy_packet_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            struct InternalSingleDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                packet_controller::PacketContainerInterface * dst;

                void push(std::move_iterator<Packet *> pkt_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    this->dst->push(pkt_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    class KernelRescueWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container;
            std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post;
            std::shared_ptr<packet_controller::KRescuePacketGeneratorInterface> krescue_gen;
            size_t rescue_packet_sz;
            std::chrono::nanoseconds rescue_threshold;
            std::chrono::nanoseconds disaster_sleep_dur;

        public:

            KernelRescueWorker(std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                               std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post,
                               std::shared_ptr<packet_controller::KRescuePacketGeneratorInterface> krescue_gen,
                               size_t rescue_packet_sz,
                               std::chrono::nanoseconds rescue_threshold,
                               std::chrono::nanoseconds disaster_sleep_dur) noexcept: outbound_packet_container(std::move(outbound_packet_container)),
                                                                                      rescue_post(std::move(rescue_post)),
                                                                                      krescue_gen(std::move(krescue_gen)),
                                                                                      rescue_packet_sz(rescue_packet_sz),
                                                                                      rescue_threshold(std::move(rescue_threshold)),
                                                                                      disaster_sleep_dur(disaster_sleep_dur){}

            bool run_one_epoch() noexcept{

                std::expected<std::optional<std::chrono::time_point<std::chrono::utc_clock>>, exception_t> last_heartbeat = this->rescue_post->last_heartbeat();

                if (!last_heartbeat.has_value()){
                    dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(last_heartbeat.error()));
                    return false;
                }

                if (!last_heartbeat.value().has_value()){
                    return false;
                }

                std::chrono::time_point<std::chrono::utc_clock> now = std::chrono::utc_clock::now();
                std::chrono::nanoseconds lapsed                     = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_heartbeat.value().value());

                if (lapsed < this->rescue_threshold){
                    std::this_thread::sleep_for(this->disaster_sleep_dur);
                    return false;
                }

                dg::network_log_stackdump::journal_fast_optional("UDP Rescue Packets enrouting");

                dg::network_stack_allocation::NoExceptAllocation<Packet[]> rescue_packet_arr(this->rescue_packet_sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(this->rescue_packet_sz);

                for (size_t i = 0u; i < this->rescue_packet_sz; ++i){
                    std::expected<model::KRescuePacket, exception_t> rescue_packet = this->krescue_gen->get();            

                    if (!rescue_packet.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(rescue_packet.error()));
                        std::this_thread::sleep_for(this->disaster_sleep_dur);
                        return false;
                    }

                    rescue_packet_arr[i] = dg::network_exception_handler::nothrow_log(packet_service::virtualize_krescue_packet(std::move(rescue_packet.value())));
                }

                this->outbound_packet_container->push(std::make_move_iterator(rescue_packet_arr.get()), this->rescue_packet_sz, exception_arr.get());
                bool is_failed = false; 

                for (size_t i = 0u; i < this->rescue_packet_sz; ++i){
                    if (dg::network_exception::is_failed(exception_arr[i])){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        is_failed = true;
                    }
                }

                if (is_failed){
                    std::this_thread::sleep_for(this->disaster_sleep_dur);
                }

                return true;
            }
    };

    class InBoundKernelWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::BufferContainerInterface> buffer_container;
            std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post;
            std::shared_ptr<model::SocketHandle> socket;
            size_t buffer_accumulation_sz;
            size_t container_delivery_sz;
            size_t pow2_rescue_heartbeat_interval;

        public:

            InBoundKernelWorker(std::shared_ptr<packet_controller::BufferContainerInterface> buffer_container,
                                std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post,
                                std::shared_ptr<model::SocketHandle> socket,
                                size_t buffer_accumulation_sz,
                                size_t container_delivery_sz,
                                size_t pow2_rescue_heartbeat_interval) noexcept: buffer_container(std::move(buffer_container)),
                                                                                 rescue_post(std::move(rescue_post)),
                                                                                 socket(std::move(socket)),
                                                                                 buffer_accumulation_sz(buffer_accumulation_sz),
                                                                                 container_delivery_sz(container_delivery_sz),
                                                                                 pow2_rescue_heartbeat_interval(pow2_rescue_heartbeat_interval){}

            bool run_one_epoch() noexcept{

                auto buffer_delivery_resolutor  = InternalBufferDeliveryResolutor{};
                buffer_delivery_resolutor.dst   = this->buffer_container.get(); 

                size_t adjusted_delivery_sz     = std::min(std::min(this->container_delivery_sz, this->buffer_accumulation_sz), this->buffer_container->max_consume_size());
                size_t bdh_allocation_cost      = dg::network_producer_consumer::delvrsrv_allocation_cost(&buffer_delivery_resolutor, adjusted_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> bdh_buf(bdh_allocation_cost);
                auto buffer_delivery_handle     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&buffer_delivery_resolutor, adjusted_delivery_sz, bdh_buf.get()));

                // for (size_t i = 0u; i < this->buffer_accumulation_sz; ++i){
                dg::network_stack_allocation::NoExceptAllocation<internal_kernel_buffer[]> bstream_arr(socket_service::batchrecv_max_array_size());
                exception_t resize_err = resize_string_array(bstream_arr.get(), socket_service::batchrecv_max_array_size(), constants::MAXIMUM_MSG_SIZE); 

                if (dg::network_exception::is_failed(resize_err)){
                    dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(resize_err));
                    return false;
                }

                size_t sz       = {};
                exception_t err = batch_recvblock_helper(*this->socket, bstream_arr.get(), sz, socket_service::batchrecv_max_array_size()); 

                if (dg::network_exception::is_failed(err)){
                    dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    return false;
                }

                for (size_t i = 0u; i < sz; ++i){
                    dg::network_producer_consumer::delvrsrv_deliver(buffer_delivery_handle.get(), std::move(bstream_arr[i]));
                }

                size_t dice = dg::network_randomizer::randomize_int<size_t>() & (this->pow2_rescue_heartbeat_interval - 1u);

                if (dice == 0u){
                    exception_t rescue_heartbeat_err = this->rescue_post->heartbeat();

                    if (dg::network_exception::is_failed(rescue_heartbeat_err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(rescue_heartbeat_err));
                    }
                }
                // }

                return true;
            }

        private:

            auto resize_string_array(internal_kernel_buffer * str_arr,
                                     size_t str_arr_sz,
                                     size_t elemental_size) noexcept -> exception_t
            {
                try
                {
                    for (size_t i = 0u; i < str_arr_sz; ++i)
                    {
                        str_arr[i].resize(elemental_size); //memory allocation, better to be raw allocation without memset to make this daemon solely for transportation
                    }

                    return dg::network_exception::SUCCESS;
                }
                catch (...)
                {
                    return dg::network_exception::wrap_std_exception(std::current_exception()); 
                }
            }

            auto batch_recvblock_helper(const SocketHandle& sock,
                                        internal_kernel_buffer * str_arr,
                                        size_t& str_arr_sz,
                                        size_t str_arr_cap) noexcept -> exception_t
            {
                dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<void>[]> buf_arr(str_arr_cap);
                dg::network_stack_allocation::NoExceptAllocation<size_t[]> buf_sz_arr(str_arr_cap);
                dg::network_stack_allocation::NoExceptAllocation<size_t[]> buf_cap_arr(str_arr_cap);

                for (size_t i = 0u; i < str_arr_cap; ++i)
                {
                    buf_arr[i]      = str_arr[i].data();
                    buf_sz_arr[i]   = 0u;
                    buf_cap_arr[i]  = str_arr[i].size();
                }

                size_t arr_sz   = 0u;
                size_t arr_cap  = str_arr_cap;

                exception_t err = dg::network_kernel_mailbox_impl1::socket_service::batchrecv_block_x(sock,
                                                                                                      buf_arr.get(),
                                                                                                      buf_sz_arr.get(),
                                                                                                      buf_cap_arr.get(),
                                                                                                      arr_sz,
                                                                                                      arr_cap);

                if (dg::network_exception::is_failed(err))
                {
                    return err;
                }

                for (size_t i = 0u; i < arr_sz; ++i)
                {
                    str_arr[i].resize(buf_sz_arr[i]);
                }

                str_arr_sz = arr_sz;
 
                return dg::network_exception::SUCCESS;
            } 

            struct InternalBufferDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<internal_kernel_buffer>{

                packet_controller::BufferContainerInterface * dst;

                void push(std::move_iterator<internal_kernel_buffer *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    this->dst->push(data_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    class InBoundWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller;
            std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container;
            std::shared_ptr<packet_controller::PacketContainerInterface> inbound_packet_container;
            std::shared_ptr<packet_controller::BufferContainerInterface> inbound_buffer_container;
            std::shared_ptr<packet_controller::InBoundIDControllerInterface> inbound_id_controller;
            std::shared_ptr<packet_controller::BorderControllerInterface> inbound_border_controller;
            std::shared_ptr<packet_controller::AckPacketGeneratorInterface> ack_packet_gen;
            std::shared_ptr<packet_controller::PacketIntegrityValidatorInterface> packet_integrity_validator;
            size_t ack_vectorization_sz;
            size_t inbound_consumption_cap;
            size_t busy_threshold_sz;

        public:

            InBoundWorker(std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller,
                          std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                          std::shared_ptr<packet_controller::PacketContainerInterface> inbound_packet_container,
                          std::shared_ptr<packet_controller::BufferContainerInterface> inbound_buffer_container,
                          std::shared_ptr<packet_controller::InBoundIDControllerInterface> inbound_id_controller,
                          std::shared_ptr<packet_controller::BorderControllerInterface> inbound_border_controller,
                          std::shared_ptr<packet_controller::AckPacketGeneratorInterface> ack_packet_gen,
                          std::shared_ptr<packet_controller::PacketIntegrityValidatorInterface> packet_integrity_validator,
                          size_t ack_vectorization_sz,
                          size_t inbound_consumption_cap,
                          size_t busy_threshold_sz) noexcept: retransmission_controller(std::move(retransmission_controller)),
                                                              outbound_packet_container(std::move(outbound_packet_container)),
                                                              inbound_packet_container(std::move(inbound_packet_container)),
                                                              inbound_buffer_container(std::move(inbound_buffer_container)),
                                                              inbound_id_controller(std::move(inbound_id_controller)),
                                                              inbound_border_controller(std::move(inbound_border_controller)),
                                                              ack_packet_gen(std::move(ack_packet_gen)),
                                                              packet_integrity_validator(std::move(packet_integrity_validator)),
                                                              ack_vectorization_sz(ack_vectorization_sz),
                                                              inbound_consumption_cap(inbound_consumption_cap),
                                                              busy_threshold_sz(busy_threshold_sz){}

            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<internal_kernel_buffer[]> buf_arr(this->inbound_consumption_cap);
                size_t buf_arr_sz = {};
                this->inbound_buffer_container->pop(buf_arr.get(), buf_arr_sz, this->inbound_consumption_cap);

                //setting up feeds, this is the punchline of performance 

                auto ackid_delivery_resolutor                           = InternalRetransmissionAckDeliveryResolutor{};
                ackid_delivery_resolutor.retransmission_controller      = this->retransmission_controller.get();

                size_t trimmed_ackid_delivery_sz                        = std::min(std::min(this->retransmission_controller->max_consume_size(), buf_arr_sz * constants::MAX_ACK_PER_PACKET), constants::DEFAULT_ACCUMULATION_SIZE); //acked_id_sz <= ack_packet_sz * corresponding_ack_pkt_sz <= ack_packet_sz * MAX_ACK_PER_PACKET <= buf_arr_sz * MAX_ACK_PER_PACKET
                size_t ackid_deliverer_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&ackid_delivery_resolutor, trimmed_ackid_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ackid_deliverer_mem(ackid_deliverer_allocation_cost);
                auto ackid_deliverer                                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&ackid_delivery_resolutor, trimmed_ackid_delivery_sz, ackid_deliverer_mem.get())); 

                //

                auto ibpkt_delivery_resolutor                           = InternalPacketDeliveryResolutor{};
                ibpkt_delivery_resolutor.dst                            = this->inbound_packet_container.get();

                size_t trimmed_ibpkt_delivery_sz                        = std::min(std::min(this->inbound_packet_container->max_consume_size(), buf_arr_sz), constants::DEFAULT_ACCUMULATION_SIZE); //in_bound_sz == req_packet_sz <= buf_arr_sz
                size_t ibpkt_deliverer_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&ibpkt_delivery_resolutor, trimmed_ibpkt_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ibpkt_deliverer_mem(ibpkt_deliverer_allocation_cost);
                auto ibpkt_deliverer                                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&ibpkt_delivery_resolutor, trimmed_ibpkt_delivery_sz, ibpkt_deliverer_mem.get()));

                //

                auto obpkt_delivery_resolutor                           = InternalPacketDeliveryResolutor{};
                obpkt_delivery_resolutor.dst                            = this->outbound_packet_container.get();

                size_t trimmed_obpkt_delivery_sz                        = std::min(std::min(this->outbound_packet_container->max_consume_size(), buf_arr_sz), constants::DEFAULT_ACCUMULATION_SIZE); //outbound_sz == accumulated_ack_packet_sz <= ack_packet_sz <= buf_arr_sz  
                size_t obpkt_deliverer_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&obpkt_delivery_resolutor, trimmed_obpkt_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> obpkt_deliverer_mem(obpkt_deliverer_allocation_cost);
                auto obpkt_deliverer                                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&obpkt_delivery_resolutor, trimmed_obpkt_delivery_sz, obpkt_deliverer_mem.get()));

                //

                auto ack_vectorizer_resolutor                           = InternalAckVectorizerResolutor{};
                ack_vectorizer_resolutor.dst                            = obpkt_deliverer.get(); 
                ack_vectorizer_resolutor.ack_packet_gen                 = this->ack_packet_gen.get();

                size_t trimmed_ack_vectorization_sz                     = std::min(this->ack_vectorization_sz, buf_arr_sz); //ack_vectorization_sz <= ack_pkt_sz <= buf_arr_sz
                size_t ack_vectorizer_allocation_cost                   = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&ack_vectorizer_resolutor, trimmed_ack_vectorization_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ack_vectorizer_mem(ack_vectorizer_allocation_cost);
                auto ack_vectorizer                                     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&ack_vectorizer_resolutor, trimmed_ack_vectorization_sz, ack_vectorizer_mem.get())); 

                //

                auto thru_ack_delivery_resolutor                        = InternalThruAckResolutor{};
                thru_ack_delivery_resolutor.packet_id_deliverer         = ackid_deliverer.get();

                size_t trimmed_thru_ack_delivery_sz                     = std::min(constants::DEFAULT_ACCUMULATION_SIZE, buf_arr_sz); //thru_ack_sz <= buf_arr_sz
                size_t thru_ack_allocation_cost                         = dg::network_producer_consumer::delvrsrv_allocation_cost(&thru_ack_delivery_resolutor, trimmed_thru_ack_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> thru_ack_mem(thru_ack_allocation_cost);
                auto thru_ack_deliverer                                 = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&thru_ack_delivery_resolutor, trimmed_thru_ack_delivery_sz, thru_ack_mem.get())); 

                //

                auto thru_request_delivery_resolutor                    = InternalThruRequestResolutor{};
                thru_request_delivery_resolutor.ack_vectorizer          = ack_vectorizer.get();
                thru_request_delivery_resolutor.inbound_deliverer       = ibpkt_deliverer.get();

                size_t trimmed_thru_request_delivery_sz                 = std::min(constants::DEFAULT_ACCUMULATION_SIZE, buf_arr_sz); //thru_request_sz <= buf_arr_sz
                size_t thru_request_allocation_cost                     = dg::network_producer_consumer::delvrsrv_allocation_cost(&thru_request_delivery_resolutor, trimmed_thru_request_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> thru_request_mem(thru_request_allocation_cost);
                auto thru_request_deliverer                             = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&thru_request_delivery_resolutor, trimmed_thru_request_delivery_sz, thru_request_mem.get())); 

                //

                auto thru_krescue_delivery_resolutor                    = InternalThruKRescueResolutor{};

                size_t trimmed_thru_krescue_delivery_sz                 = std::min(constants::DEFAULT_ACCUMULATION_SIZE, buf_arr_sz); //thru_rescue_sz <= buf_arr_sz
                size_t thru_krescue_allocation_cost                     = dg::network_producer_consumer::delvrsrv_allocation_cost(&thru_krescue_delivery_resolutor, trimmed_thru_krescue_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> thru_krescue_mem(thru_krescue_allocation_cost);
                auto thru_krescue_deliverer                             = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&thru_krescue_delivery_resolutor, trimmed_thru_krescue_delivery_sz, thru_krescue_mem.get())); 

                //

                auto thru_delivery_resolutor                            = InternalThruResolutor{};
                thru_delivery_resolutor.ack_thru_deliverer              = thru_ack_deliverer.get();
                thru_delivery_resolutor.request_thru_deliverer          = thru_request_deliverer.get();
                thru_delivery_resolutor.krescue_thru_deliverer          = thru_krescue_deliverer.get();

                size_t trimmed_thru_delivery_sz                         = std::min(constants::DEFAULT_ACCUMULATION_SIZE, buf_arr_sz); //thru_sz <= buf_arr_sz
                size_t thru_delivery_allocation_cost                    = dg::network_producer_consumer::delvrsrv_allocation_cost(&thru_delivery_resolutor, trimmed_thru_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> thru_delivery_mem(thru_delivery_allocation_cost);
                auto thru_deliverer                                     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&thru_delivery_resolutor, trimmed_thru_delivery_sz, thru_delivery_mem.get())); 

                //

                auto nothru_ack_delivery_resolutor                      = InternalNoThruAckResolutor{};
                nothru_ack_delivery_resolutor.ack_vectorizer            = ack_vectorizer.get();

                size_t trimmed_nothru_ack_delivery_sz                   = std::min(constants::DEFAULT_ACCUMULATION_SIZE, buf_arr_sz); //no_thru_sz <= buf_arr_sz
                size_t nothru_ack_allocation_cost                       = dg::network_producer_consumer::delvrsrv_allocation_cost(&nothru_ack_delivery_resolutor, trimmed_nothru_ack_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> nothru_ack_delivery_mem(nothru_ack_allocation_cost);
                auto nothru_ack_deliverer                               = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&nothru_ack_delivery_resolutor, trimmed_nothru_ack_delivery_sz, nothru_ack_delivery_mem.get())); 

                //

                auto inbound_delivery_resolutor                         = InternalInBoundIDResolutor{};
                inbound_delivery_resolutor.downstream_dst               = thru_deliverer.get();
                inbound_delivery_resolutor.nothru_ack_dst               = nothru_ack_deliverer.get();
                inbound_delivery_resolutor.inbound_id_controller        = this->inbound_id_controller.get();

                size_t trimmed_inbound_delivery_sz                      = std::min(std::min(this->inbound_id_controller->max_consume_size(), buf_arr_sz), constants::DEFAULT_ACCUMULATION_SIZE); //inbound_sz <= buf_arr_sz
                size_t inbound_allocation_cost                          = dg::network_producer_consumer::delvrsrv_allocation_cost(&inbound_delivery_resolutor, trimmed_inbound_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> inbound_mem(inbound_allocation_cost);
                auto inbound_deliverer                                  = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&inbound_delivery_resolutor, trimmed_inbound_delivery_sz, inbound_mem.get())); 

                //

                auto traffic_resolutor                                  = InternalTrafficResolutor{};
                traffic_resolutor.downstream_dst                        = inbound_deliverer.get();
                traffic_resolutor.border_controller                     = this->inbound_border_controller.get();

                size_t trimmed_traffic_resolutor_delivery_sz            = std::min(std::min(this->inbound_border_controller->max_consume_size(), buf_arr_sz), constants::DEFAULT_ACCUMULATION_SIZE); //traffic_stop_sz <= buf_arr_sz
                size_t traffic_resolutor_allocation_cost                = dg::network_producer_consumer::delvrsrv_allocation_cost(&traffic_resolutor, trimmed_traffic_resolutor_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> traffic_resolutor_mem(traffic_resolutor_allocation_cost);
                auto traffic_resolutor_deliverer                        = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&traffic_resolutor, trimmed_traffic_resolutor_delivery_sz, traffic_resolutor_mem.get())); 

                for (size_t i = 0u; i < buf_arr_sz; ++i){
                    std::expected<Packet, exception_t> pkt = packet_service::deserialize_packet(std::move(buf_arr[i]));

                    if (!pkt.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(pkt.error()));
                        continue;
                    }

                    exception_t validation_err = this->packet_integrity_validator->is_valid(pkt.value());

                    if (dg::network_exception::is_failed(validation_err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(validation_err));
                        continue;
                    }

                    dg::network_producer_consumer::delvrsrv_deliver(traffic_resolutor_deliverer.get(), std::move(pkt.value()));
                }

                return buf_arr_sz >= this->busy_threshold_sz;
            }

        private:

            struct InternalRetransmissionAckDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<global_packet_id_t>{

                packet_controller::RetransmissionControllerInterface * retransmission_controller;

                void push(std::move_iterator<global_packet_id_t *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    global_packet_id_t * base_data_arr = data_arr.base();
                    this->retransmission_controller->ack(base_data_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            struct InternalPacketDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                packet_controller::PacketContainerInterface * dst;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    this->dst->push(data_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            struct InternalAckVectorizerResolutor: dg::network_producer_consumer::KVConsumerInterface<Address, PacketBase>{

                dg::network_producer_consumer::DeliveryHandle<Packet> * dst;
                packet_controller::AckPacketGeneratorInterface * ack_packet_gen;

                void push(const Address& fr_addr, std::move_iterator<PacketBase *> data_arr, size_t sz) noexcept{

                    PacketBase * base_data_arr = data_arr.base();
                    std::expected<AckPacket, exception_t> ack_pkt = this->ack_packet_gen->get(fr_addr, base_data_arr, sz);

                    if (!ack_pkt.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(ack_pkt.error()));
                        return;
                    }

                    Packet virtualized_pkt = dg::network_exception_handler::nothrow_log(packet_service::virtualize_ack_packet(std::move(ack_pkt.value())));
                    dg::network_producer_consumer::delvrsrv_deliver(this->dst, std::move(virtualized_pkt));
                }
            };

            struct InternalTrafficResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::DeliveryHandle<Packet> * downstream_dst;
                packet_controller::BorderControllerInterface * border_controller;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<Address[]> addr_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> response_arr(sz);

                    Packet * base_data_arr = data_arr.base();
                    std::transform(base_data_arr, std::next(base_data_arr, sz), addr_arr.get(), [](const Packet& packet){return packet.fr_addr;});
                    this->border_controller->thru(addr_arr.get(), sz, response_arr.get());
                    
                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(response_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(response_arr[i]));
                            continue;
                        }

                        dg::network_producer_consumer::delvrsrv_deliver(this->downstream_dst, std::move(base_data_arr[i]));
                    }
                }
            };

            struct InternalInBoundIDResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::DeliveryHandle<Packet> * downstream_dst;
                dg::network_producer_consumer::DeliveryHandle<Packet> * nothru_ack_dst;
                packet_controller::InBoundIDControllerInterface * inbound_id_controller;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<kind_global_packet_id_t[]> id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> response_arr(sz);

                    Packet * base_data_arr = data_arr.base();
                    std::transform(base_data_arr, std::next(base_data_arr, sz), id_arr.get(), [](const Packet& packet){return packet_service::get_kind_global_id_from_packet(packet);});
                    this->inbound_id_controller->thru(id_arr.get(), sz, response_arr.get());

                    using radix_t   = dg::network_producer_consumer::DeliveryHandle<Packet> *;
                    radix_t radix_table[2];
                    radix_table[0]  = this->nothru_ack_dst;
                    radix_table[1]  = this->downstream_dst;

                    for (size_t i = 0u; i < sz; ++i){
                        if (!response_arr[i].has_value()){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(response_arr[i].error()));
                            continue;
                        }

                        if (!response_arr[i].value() && packet_service::is_request_packet(base_data_arr[i]) || response_arr[i].value()){
                            dg::network_producer_consumer::delvrsrv_deliver(radix_table[static_cast<int>(response_arr[i].value())], std::move(base_data_arr[i]));
                        }
                    }
                }
            };

            struct InternalNoThruAckResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::KVDeliveryHandle<Address, PacketBase> * ack_vectorizer;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    Packet * base_data_arr  = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        dg::network_producer_consumer::delvrsrv_kv_deliver(this->ack_vectorizer, base_data_arr[i].fr_addr, static_cast<const PacketBase&>(base_data_arr[i]));
                    }
                }
            };

            struct InternalThruResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::DeliveryHandle<Packet> * ack_thru_deliverer;
                dg::network_producer_consumer::DeliveryHandle<Packet> * request_thru_deliverer;
                dg::network_producer_consumer::DeliveryHandle<Packet> * krescue_thru_deliverer;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    Packet * base_data_arr  = data_arr.base(); 

                    for (size_t i = 0u; i < sz; ++i){                        

                        if (packet_service::is_ack_packet(base_data_arr[i])){
                            dg::network_producer_consumer::delvrsrv_deliver(this->ack_thru_deliverer, std::move(base_data_arr[i]));
                        } else if (packet_service::is_request_packet(base_data_arr[i])){
                            dg::network_producer_consumer::delvrsrv_deliver(this->request_thru_deliverer, std::move(base_data_arr[i]));
                        } else if (packet_service::is_krescue_packet(base_data_arr[i])){
                            dg::network_producer_consumer::delvrsrv_deliver(this->krescue_thru_deliverer, std::move(base_data_arr[i]));
                        } else{
                            if constexpr(DEBUG_MODE_FLAG){
                                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            } else{
                                std::unreachable();
                            }
                        }
                    }
                }
            };

            struct InternalThruAckResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::DeliveryHandle<global_packet_id_t> * packet_id_deliverer;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    Packet * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        for (const PacketBase& e: std::get<XOnlyAckPacket>(base_data_arr[i].xonly_content).ack_vec){
                            dg::network_producer_consumer::delvrsrv_deliver(this->packet_id_deliverer, e.id);
                        }
                    }
                }
            };

            struct InternalThruRequestResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                dg::network_producer_consumer::KVDeliveryHandle<Address, PacketBase> * ack_vectorizer;
                dg::network_producer_consumer::DeliveryHandle<Packet> * inbound_deliverer; 

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    Packet * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        dg::network_producer_consumer::delvrsrv_kv_deliver(this->ack_vectorizer, base_data_arr[i].fr_addr, static_cast<const PacketBase&>(base_data_arr[i]));
                        dg::network_producer_consumer::delvrsrv_deliver(this->inbound_deliverer, std::move(base_data_arr[i]));
                    }
                }
            };

            struct InternalThruKRescueResolutor: dg::network_producer_consumer::ConsumerInterface<Packet>{

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    (void) data_arr;
                }
            };
    };

    class UpdateWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<packet_controller::UpdatableInterface> updatable;
            std::chrono::nanoseconds wait_dur;

        public:

            UpdateWorker(std::shared_ptr<packet_controller::UpdatableInterface> updatable,
                         std::chrono::nanoseconds wait_dur) noexcept: updatable(std::move(updatable)),
                                                                      wait_dur(std::move(wait_dur)){}

            bool run_one_epoch() noexcept{

                this->updatable->update();
                std::this_thread::sleep_for(this->wait_dur); //we got complained because this would introduce lock-contentions, wait_dur has to be of different values
                return true;
            }
    };

    struct ComponentFactory{
        
        static auto get_netstat_monitor_worker(std::shared_ptr<external_interface::NetworkBusyStatusRetrieverInterface> netstat_retriever,
                                               std::shared_ptr<packet_controller::NetworkBusyObserverInterface> busy_observer,
                                               std::chrono::nanoseconds break_time){
            using namespace std::literals::chrono_literals;

            const std::chrono::nanoseconds MIN_BREAK_TIME = std::chrono::duration_cast<std::chrono::nanoseconds>(0ns);
            const std::chrono::nanoseconds MAX_BREAK_TIME = std::chrono::duration_cast<std::chrono::nanoseconds>(1h);

            if (netstat_retriever == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (busy_observer == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(break_time, MIN_BREAK_TIME, MAX_BREAK_TIME) != break_time){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<NetworkStatusMonitorWorker>(std::move(netstat_retriever),
                                                                std::move(busy_observer),
                                                                break_time);
        } 

        static auto get_buffer_redistributor_worker(std::shared_ptr<packet_controller::BufferContainerInterface> fr_warehouse,
                                                    std::shared_ptr<packet_controller::BufferContainerInterface> to_warehouse,
                                                    size_t fr_warehouse_get_cap,
                                                    size_t to_warehouse_push_cap,
                                                    size_t busy_threshold){

            const size_t MIN_FR_WAREHOUSE_GET_CAP   = 1u;
            const size_t MAX_FR_WAREHOUSE_GET_CAP   = size_t{1} << 25;
            const size_t MIN_TO_WAREHOUSE_PUSH_CAP  = 1u;
            const size_t MAX_TO_WAREHOUSE_PUSH_CAP  = size_t{1} << 25;
            const size_t MIN_BUSY_THRESHOLD         = 0u;
            const size_t MAX_BUSY_THRESHOLD         = std::numeric_limits<size_t>::max(); 

            if (fr_warehouse == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (to_warehouse == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(fr_warehouse_get_cap, MIN_FR_WAREHOUSE_GET_CAP, MAX_FR_WAREHOUSE_GET_CAP) != fr_warehouse_get_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(to_warehouse_push_cap, MIN_TO_WAREHOUSE_PUSH_CAP, MAX_TO_WAREHOUSE_PUSH_CAP) != to_warehouse_push_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold, MIN_BUSY_THRESHOLD, MAX_BUSY_THRESHOLD)){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<BufferContainerRedistributorWorker>(std::move(fr_warehouse),
                                                                        std::move(to_warehouse),
                                                                        fr_warehouse_get_cap,
                                                                        to_warehouse_push_cap,
                                                                        busy_threshold);
        }

        static auto get_packet_redistributor_worker(std::shared_ptr<packet_controller::PacketContainerInterface> fr_warehouse,
                                                    std::shared_ptr<packet_controller::PacketContainerInterface> to_warehouse,
                                                    size_t fr_warehouse_get_cap,
                                                    size_t to_warehouse_push_cap,
                                                    size_t busy_threshold){
                                            
            const size_t MIN_FR_WAREHOUSE_GET_CAP   = 1u;
            const size_t MAX_FR_WAREHOUSE_GET_CAP   = size_t{1} << 25;
            const size_t MIN_TO_WAREHOUSE_PUSH_CAP  = 1u;
            const size_t MAX_TO_WAREHOUSE_PUSH_CAP  = size_t{1} << 25;
            const size_t MIN_BUSY_THRESHOLD         = 0u;
            const size_t MAX_BUSY_THRESHOLD         = std::numeric_limits<size_t>::max();
            
            if (fr_warehouse == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (to_warehouse == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(fr_warehouse_get_cap, MIN_FR_WAREHOUSE_GET_CAP, MAX_FR_WAREHOUSE_GET_CAP) != fr_warehouse_get_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(to_warehouse_push_cap, MIN_TO_WAREHOUSE_PUSH_CAP, MAX_TO_WAREHOUSE_PUSH_CAP) != to_warehouse_push_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold, MIN_BUSY_THRESHOLD, MAX_BUSY_THRESHOLD) != busy_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<PacketContainerRedistributorWorker>(std::move(fr_warehouse),
                                                                        std::move(to_warehouse),
                                                                        fr_warehouse_get_cap,
                                                                        to_warehouse_push_cap,
                                                                        busy_threshold);
        }

        static auto get_outbound_worker(std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                                        std::shared_ptr<packet_controller::BorderControllerInterface> border_controller,
                                        std::shared_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface> transmission_controller,
                                        std::shared_ptr<model::SocketHandle> socket,
                                        size_t packet_consumption_cap,
                                        size_t busy_threshold_sz,
                                        size_t packet_aggtransmit_cap = constants::DEFAULT_ACCUMULATION_SIZE) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            const size_t MIN_PACKET_CONSUMPTION_CAP = size_t{1};
            const size_t MAX_PACKET_CONSUMPTION_CAP = size_t{1} << 25;
            const size_t MIN_BUSY_THRESHOLD_SZ      = size_t{0u};
            const size_t MAX_BUSY_THRESHOLD_SZ      = std::numeric_limits<size_t>::max(); 
            const size_t MIN_PACKET_AGGTRANSMIT_CAP = size_t{1};
            const size_t MAX_PACKET_AGGTRANSMIT_CAP = size_t{1} << 25; 

            if (outbound_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (border_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (transmission_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (socket == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(packet_consumption_cap, MIN_PACKET_CONSUMPTION_CAP, MAX_PACKET_CONSUMPTION_CAP) != packet_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold_sz, MIN_BUSY_THRESHOLD_SZ, MAX_BUSY_THRESHOLD_SZ) != busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(packet_aggtransmit_cap, MIN_PACKET_AGGTRANSMIT_CAP, MAX_PACKET_AGGTRANSMIT_CAP) != packet_aggtransmit_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<OutBoundWorker>(std::move(outbound_packet_container),
                                                    std::move(border_controller),
                                                    std::move(transmission_controller), 
                                                    std::move(socket),
                                                    packet_consumption_cap,
                                                    packet_aggtransmit_cap,
                                                    busy_threshold_sz);
        }

        static auto get_retransmission_worker(std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller, 
                                              std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                                              size_t transmission_consumption_cap,
                                              size_t busy_threshold_sz, 
                                              size_t transmission_accumulation_cap = constants::DEFAULT_ACCUMULATION_SIZE) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            const size_t MIN_TRANSMISSION_CONSUMPTION_CAP   = size_t{1};
            const size_t MAX_TRANSMISSION_CONSUMPTION_CAP   = size_t{1} << 25; 
            const size_t MIN_BUSY_THRESHOLD_SZ              = size_t{0u};
            const size_t MAX_BUSY_THRESHOLD_SZ              = std::numeric_limits<size_t>::max();
            const size_t MIN_TRANSMISSION_ACCUM_CAP         = size_t{1};
            const size_t MAX_TRANSMISSION_ACCUM_CAP         = size_t{1} << 25;

            if (retransmission_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (outbound_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(transmission_consumption_cap, MIN_TRANSMISSION_CONSUMPTION_CAP, MAX_TRANSMISSION_CONSUMPTION_CAP) != transmission_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold_sz, MIN_BUSY_THRESHOLD_SZ, MAX_BUSY_THRESHOLD_SZ) != busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(transmission_accumulation_cap, MIN_TRANSMISSION_ACCUM_CAP, MAX_TRANSMISSION_ACCUM_CAP) != transmission_accumulation_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<RetransmissionWorker>(std::move(retransmission_controller), 
                                                          std::move(outbound_packet_container),
                                                          transmission_consumption_cap,
                                                          transmission_accumulation_cap,
                                                          busy_threshold_sz);
        }

        static auto get_kernel_rescue_worker(std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                                             std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post,
                                             std::shared_ptr<packet_controller::KRescuePacketGeneratorInterface> krescue_generator,
                                             std::chrono::nanoseconds rescue_threshold,
                                             std::chrono::nanoseconds disaster_sleep_dur,
                                             size_t transmitting_rescue_packet_sz) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            const std::chrono::nanoseconds MIN_RESCUE_THRESHOLD     = std::chrono::nanoseconds{size_t{0u}};
            const std::chrono::nanoseconds MAX_RESCUE_THRESHOLD     = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{size_t{1}});
            const std::chrono::nanoseconds MIN_DISASTER_SLEEP_DUR   = std::chrono::nanoseconds{size_t{0u}};
            const std::chrono::nanoseconds MAX_DISASTER_SLEEP_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{size_t{1}}); 
            const size_t MIN_TRANSMITTING_RESCUE_PACKET_SZ          = size_t{1};
            const size_t MAX_TRANSMITTING_RESCUE_PACKET_SZ          = size_t{1} << 25;

            if (outbound_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (rescue_post == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (krescue_generator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(rescue_threshold, MIN_RESCUE_THRESHOLD, MAX_RESCUE_THRESHOLD) != rescue_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(disaster_sleep_dur, MIN_DISASTER_SLEEP_DUR, MAX_DISASTER_SLEEP_DUR) != disaster_sleep_dur){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(transmitting_rescue_packet_sz, MIN_TRANSMITTING_RESCUE_PACKET_SZ, MAX_TRANSMITTING_RESCUE_PACKET_SZ) != transmitting_rescue_packet_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<KernelRescueWorker>(std::move(outbound_packet_container),
                                                        std::move(rescue_post),
                                                        std::move(krescue_generator),
                                                        transmitting_rescue_packet_sz,
                                                        rescue_threshold,
                                                        disaster_sleep_dur);
        }

        static auto get_kernel_inbound_worker(std::shared_ptr<packet_controller::BufferContainerInterface> buffer_container,
                                              std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post,
                                              std::shared_ptr<model::SocketHandle> socket,
                                              size_t rescue_heartbeat_interval,
                                              size_t buffer_accumulation_sz,
                                              size_t container_delivery_sz = constants::DEFAULT_ACCUMULATION_SIZE){
            
            const size_t MIN_BUFFER_ACCUMULATION_SZ = size_t{1};
            const size_t MAX_BUFFER_ACCUMULATION_SZ = size_t{1} << 25;
            const size_t MIN_CONTAINER_DELIVERY_SZ  = size_t{1};
            const size_t MAX_CONTAINER_DELIVERY_SZ  = size_t{1} << 25;

            if (buffer_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            } 

            if (rescue_post == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (socket == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(buffer_accumulation_sz, MIN_BUFFER_ACCUMULATION_SZ, MAX_BUFFER_ACCUMULATION_SZ) != buffer_accumulation_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(container_delivery_sz, MIN_CONTAINER_DELIVERY_SZ, MAX_CONTAINER_DELIVERY_SZ) != container_delivery_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(rescue_heartbeat_interval)){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<InBoundKernelWorker>(std::move(buffer_container),
                                                         std::move(rescue_post),
                                                         std::move(socket),
                                                         buffer_accumulation_sz,
                                                         container_delivery_sz,
                                                         rescue_heartbeat_interval);

        }

        static auto get_process_inbound_worker(std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller, 
                                               std::shared_ptr<packet_controller::PacketContainerInterface> outbound_packet_container,
                                               std::shared_ptr<packet_controller::PacketContainerInterface> inbound_packet_container,
                                               std::shared_ptr<packet_controller::BufferContainerInterface> inbound_buffer_container,
                                               std::shared_ptr<packet_controller::InBoundIDControllerInterface> inbound_id_controller,
                                               std::shared_ptr<packet_controller::BorderControllerInterface> inbound_border_controller,
                                               std::shared_ptr<packet_controller::AckPacketGeneratorInterface> ack_packet_generator,
                                               std::shared_ptr<packet_controller::PacketIntegrityValidatorInterface> packet_integrity_validator,
                                               size_t inbound_consumption_cap,
                                               size_t busy_threshold_sz,
                                               size_t ack_vectorization_sz = constants::MAX_ACK_PER_PACKET) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            const size_t MIN_INBOUND_CONSUMPTION_CAP    = size_t{1};
            const size_t MAX_INBOUND_CONSUMPTION_CAP    = size_t{1} << 25;
            const size_t MIN_BUSY_THRESHOLD_SZ          = size_t{0};
            const size_t MAX_BUSY_THRESHOLD_SZ          = std::numeric_limits<size_t>::max();
            const size_t MIN_ACK_VECTORIZATION_SZ       = size_t{1};
            const size_t MAX_ACK_VECTORIZATION_SZ       = constants::MAX_ACK_PER_PACKET;

            if (retransmission_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (outbound_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_buffer_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_id_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_border_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ack_packet_generator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (packet_integrity_validator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(inbound_consumption_cap, MIN_INBOUND_CONSUMPTION_CAP, MAX_INBOUND_CONSUMPTION_CAP) != inbound_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold_sz, MIN_BUSY_THRESHOLD_SZ, MAX_BUSY_THRESHOLD_SZ) != busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ack_vectorization_sz, MIN_ACK_VECTORIZATION_SZ, MAX_ACK_VECTORIZATION_SZ) != ack_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<InBoundWorker>(std::move(retransmission_controller), 
                                                   std::move(outbound_packet_container), 
                                                   std::move(inbound_packet_container), 
                                                   std::move(inbound_buffer_container),
                                                   std::move(inbound_id_controller),
                                                   std::move(inbound_border_controller), 
                                                   std::move(ack_packet_generator),
                                                   std::move(packet_integrity_validator),
                                                   ack_vectorization_sz,
                                                   inbound_consumption_cap,
                                                   busy_threshold_sz);
        }

        static auto get_update_worker(std::shared_ptr<packet_controller::UpdatableInterface> updatable,
                                      std::chrono::nanoseconds update_dur) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            using namespace std::chrono_literals; 

            const std::chrono::nanoseconds MIN_UPDATE_DUR   = std::chrono::nanoseconds{1u}; 
            const std::chrono::nanoseconds MAX_UPDATE_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{1u});

            if (updatable == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(update_dur, MIN_UPDATE_DUR, MAX_UPDATE_DUR) != update_dur){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<UpdateWorker>(std::move(updatable), update_dur);
        }
    };
}

namespace dg::network_kernel_mailbox_impl1::core{

    class RetransmittableMailBoxController: public virtual MailboxInterface{

        private:

            dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec;
            std::unique_ptr<packet_controller::RequestPacketGeneratorInterface> packet_gen;
            std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller;
            std::shared_ptr<packet_controller::PacketContainerInterface> ob_packet_container;
            std::shared_ptr<packet_controller::PacketContainerInterface> ib_packet_container;
            size_t inbound_capacity;
            size_t outbound_capacity; 

        public:

            RetransmittableMailBoxController(dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec, 
                                             std::unique_ptr<packet_controller::RequestPacketGeneratorInterface> packet_gen,
                                             std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller,
                                             std::shared_ptr<packet_controller::PacketContainerInterface> ob_packet_container,
                                             std::shared_ptr<packet_controller::PacketContainerInterface> ib_packet_container,
                                             size_t inbound_capacity,
                                             size_t outbound_capacity) noexcept: daemon_vec(std::move(daemon_vec)),
                                                                                 packet_gen(std::move(packet_gen)),
                                                                                 retransmission_controller(std::move(retransmission_controller)),
                                                                                 ob_packet_container(std::move(ob_packet_container)),
                                                                                 ib_packet_container(std::move(ib_packet_container)),
                                                                                 inbound_capacity(inbound_capacity),
                                                                                 outbound_capacity(outbound_capacity){}

            void send(MailBoxArgument * data_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto retransmission_deliverer                       = InternalRetransmissionDeliverer{};
                retransmission_deliverer.retransmission_controller  = this->retransmission_controller.get();

                size_t trimmed_retransmission_delivery_sz           = std::min(std::min(this->retransmission_controller->max_consume_size(), sz), constants::DEFAULT_ACCUMULATION_SIZE); 
                size_t retransmission_deliverer_allocation_cost     = dg::network_producer_consumer::delvrsrv_allocation_cost(&retransmission_deliverer, trimmed_retransmission_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> retransmission_deliverer_mem(retransmission_deliverer_allocation_cost);
                auto retransmission_tmp_deliverer                   = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&retransmission_deliverer, trimmed_retransmission_delivery_sz, retransmission_deliverer_mem.get()));

                MailBoxArgument * base_data_arr                     = data_arr;

                dg::network_stack_allocation::NoExceptAllocation<RequestPacket[]> request_pkt_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<RequestPacket[]> retry_pkt_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<size_t[]> idx_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> request_exception_arr(sz);

                auto internal_deliverer                             = InternalOBDeliverer{};
                internal_deliverer.ob_packet_container              = this->ob_packet_container.get();
                internal_deliverer.retransmission_deliverer         = retransmission_tmp_deliverer.get();

                size_t trimmed_ob_delivery_sz                       = std::min(std::min(this->ob_packet_container->max_consume_size(), sz), constants::DEFAULT_ACCUMULATION_SIZE);
                size_t ob_deliverer_allocation_cost                 = dg::network_producer_consumer::delvrsrv_allocation_cost(&internal_deliverer, trimmed_ob_delivery_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ob_deliverer_mem(ob_deliverer_allocation_cost);
                auto ob_deliverer                                   = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&internal_deliverer, trimmed_ob_delivery_sz, ob_deliverer_mem.get()));

                size_t thru_sz = 0u; 
                std::fill(request_exception_arr.get(), std::next(request_exception_arr.get(), sz), dg::network_exception::SUCCESS);
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    std::expected<RequestPacket, exception_t> pkt = this->packet_gen->get(static_cast<MailBoxArgument&&>(base_data_arr[i])); 

                    if (!pkt.has_value()){
                        exception_arr[i] = pkt.error();
                        continue;
                    }
                    
                    std::expected<RequestPacket, exception_t> cpy_pkt = dg::network_exception::cstyle_initialize<RequestPacket>(pkt.value());
                    
                    if (!cpy_pkt.has_value()){
                        exception_arr[i] = cpy_pkt.error();
                        continue;
                    }

                    request_pkt_arr[thru_sz]    = std::move(pkt.value());
                    retry_pkt_arr[thru_sz]      = std::move(cpy_pkt.value());
                    idx_arr[thru_sz]            = i;

                    auto ob_argument            = InternalOBArgument{
                        .arg            = std::make_move_iterator(&request_pkt_arr[thru_sz]),
                        .cpy_arg        = std::make_move_iterator(&retry_pkt_arr[thru_sz]),
                        .exception_ptr  = std::next(request_exception_arr.get(), thru_sz)
                    };

                    thru_sz                     += 1u;

                    dg::network_producer_consumer::delvrsrv_deliver(ob_deliverer.get(), std::move(ob_argument));
                }
            }

            void recv(std::add_pointer_t<void> * output_arr,
                      size_t * output_elemental_capacity_arr,
                      size_t * output_elemental_sz_arr,
                      size_t& sz,
                      size_t capacity) noexcept{

                sz                      = 0u;
                size_t pkt_arr_sz       = {};
                size_t pkt_arr_capacity = std::min(capacity, this->outbound_capacity);

                dg::network_stack_allocation::NoExceptAllocation<Packet[]> pkt_arr(pkt_arr_capacity);
                this->ib_packet_container->pop(pkt_arr.get(), pkt_arr_sz, pkt_arr_capacity);

                for (size_t i = 0u; i < pkt_arr_sz; ++i){
                    RequestPacket rq_pkt        = dg::network_exception_handler::nothrow_log(packet_service::devirtualize_request_packet(std::move(pkt_arr[i])));
                    size_t elemental_sz         = std::min(output_elemental_capacity_arr[i], static_cast<size_t>(rq_pkt.content.size()));
                    std::copy(rq_pkt.content.begin(), std::next(rq_pkt.content.begin(), elemental_sz), static_cast<char *>(output_arr[i]));
                    output_elemental_sz_arr[i]  = elemental_sz;
                    sz                          += 1u;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->inbound_capacity;
            }

            auto max_content_size() const noexcept -> size_t{

                return dg::network_kernel_mailbox_impl1::constants::MAX_REQUEST_PACKET_CONTENT_SIZE;
            }

        private:

            struct InternalOBArgument{
                std::move_iterator<RequestPacket *> arg;
                std::move_iterator<RequestPacket *> cpy_arg;
                exception_t * exception_ptr;
            };

            struct InternalOBDeliverer: dg::network_producer_consumer::ConsumerInterface<InternalOBArgument>{

                packet_controller::PacketContainerInterface * ob_packet_container;
                dg::network_producer_consumer::DeliveryHandle<Packet> * retransmission_deliverer;

                void push(std::move_iterator<InternalOBArgument *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> pkt_data_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Packet[]> cpy_data_arr(sz); //memory allocation, where? has to be on dedicated 1024 byte leaf allocation in order to avoid memory fragmentation
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    InternalOBArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i){
                        pkt_data_arr[i] = dg::network_exception_handler::nothrow_log(packet_service::virtualize_request_packet(std::move(*base_data_arr[i].arg.base())));                   
                        cpy_data_arr[i] = dg::network_exception_handler::nothrow_log(packet_service::virtualize_request_packet(std::move(*base_data_arr[i].cpy_arg.base())));
                    }

                    this->ob_packet_container->push(std::make_move_iterator(pkt_data_arr.get()), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *(base_data_arr[i].arg.base())  = dg::network_exception_handler::nothrow_log(packet_service::devirtualize_request_packet(std::move(pkt_data_arr[i])));
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        } else{
                            dg::network_producer_consumer::delvrsrv_deliver(retransmission_deliverer, std::move(cpy_data_arr[i]));
                        }
                    }
                }
            };

            struct InternalRetransmissionDeliverer: dg::network_producer_consumer::ConsumerInterface<Packet>{

                packet_controller::RetransmissionControllerInterface * retransmission_controller;

                void push(std::move_iterator<Packet *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    this->retransmission_controller->add_retriables(data_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    struct ComponentFactory{

        template <class ...Args, class T>
        static auto up_vector_to_vsp_vector(std::vector<std::unique_ptr<T, Args...>> vec) -> std::vector<std::shared_ptr<T>>{

            std::vector<std::shared_ptr<T>> rs{};

            for (size_t i = 0u; i < vec.size(); ++i){
                if (vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                rs.emplace_back(std::move(vec[i]));
            }

            return rs;
        } 

        static auto get_retransmittable_mailbox_controller(std::unique_ptr<packet_controller::BufferContainerInterface> ib_buffer_container,
                                                           size_t ib_buffer_accumulation_sz,
 
                                                           std::unique_ptr<packet_controller::PacketContainerInterface> ib_packet_container,
                                                           std::unique_ptr<packet_controller::InBoundIDControllerInterface> ib_id_controller,
                                                           std::vector<std::unique_ptr<packet_controller::InBoundBorderController>> ib_border_controller_vec,
                                                           size_t ib_packet_consumption_cap,
                                                           size_t ib_packet_busy_threshold_sz,

                                                           std::unique_ptr<packet_controller::KernelRescuePostInterface> rescue_post,
                                                           std::unique_ptr<packet_controller::KRescuePacketGeneratorInterface> krescue_packet_generator,
                                                           size_t rescue_packet_sz,
                                                           std::chrono::nanoseconds rescue_dispatch_threshold,
                                                           std::chrono::nanoseconds rescue_disaster_sleep_dur,

                                                           std::unique_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller,
                                                           size_t retransmission_consumption_cap,
                                                           size_t retransmission_busy_threshold_sz,

                                                           std::unique_ptr<packet_controller::PacketContainerInterface> ob_packet_container,
                                                           std::vector<std::unique_ptr<packet_controller::OutBoundBorderController>> ob_border_controller_vec,
                                                           std::unique_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface> ob_exhaustion_controller,
                                                           size_t ob_packet_consumption_cap,
                                                           size_t ob_packet_busy_threshold_sz,

                                                           std::unique_ptr<packet_controller::AckPacketGeneratorInterface> ack_packet_generator,
                                                           std::unique_ptr<packet_controller::PacketIntegrityValidatorInterface> packet_integrity_validator,
                                                           std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>> socket_vec,
                                                           std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>> dedicated_outbound_socket_vec,

                                                           std::unique_ptr<packet_controller::RequestPacketGeneratorInterface> req_packet_generator,
                                                           size_t mailbox_inbound_cap,
                                                           size_t mailbox_outbound_cap,  

                                                           std::chrono::nanoseconds traffic_reset_duration,

                                                           size_t num_kernel_inbound_worker,
                                                           size_t num_process_inbound_worker,
                                                           size_t num_outbound_worker,
                                                           size_t num_kernel_rescue_worker,
                                                           size_t num_retry_worker,
                                                           
                                                           std::unique_ptr<packet_controller::BufferContainerInterface> ib_buffer_fair_container,
                                                           size_t buffer_fair_container_get_cap,
                                                           size_t buffer_fair_container_push_cap,
                                                           size_t buffer_fair_container_busy_threshold,

                                                           std::unique_ptr<packet_controller::PacketContainerInterface> ib_packet_fair_container,
                                                           size_t packet_fair_container_get_cap,
                                                           size_t packet_fair_container_push_cap,
                                                           size_t packet_fair_container_busy_threshold) -> std::unique_ptr<MailboxInterface>{

            const size_t DEFAULT_HEARTBEAT_INTERVAL                         = size_t{1} << 10;

            const size_t MIN_IB_BUFFER_ACCUMULATION_SZ                      = size_t{1};  
            const size_t MAX_IB_BUFFER_ACCUMULATION_SZ                      = size_t{1} << 20;

            const size_t MIN_IB_BORDER_CONTROLLER_VEC_SZ                    = size_t{1};
            const size_t MAX_IB_BORDER_CONTROLLER_VEC_SZ                    = size_t{1} << 20; 

            const size_t MIN_IB_PACKET_CONSUMPTION_CAP                      = size_t{1};
            const size_t MAX_IB_PACKET_CONSUMPTION_CAP                      = size_t{1} << 20; 

            const size_t MIN_IB_PACKET_BUSY_THRESHOLD_SZ                    = size_t{0u};
            const size_t MAX_IB_PACKET_BUSY_THRESHOLD_SZ                    = std::numeric_limits<size_t>::max();

            const size_t MIN_RESCUE_PACKET_SZ                               = size_t{1};
            const size_t MAX_RESCUE_PACKET_SZ                               = size_t{1} << 20; 

            const std::chrono::nanoseconds MIN_RESCUE_DISPATCH_THRESHOLD    = std::chrono::nanoseconds{1u};
            const std::chrono::nanoseconds MAX_RESCUE_DISPATCH_THRESHOLD    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{1u}); 

            const size_t MIN_RETRANSMISSION_CONSUMPTION_CAP                 = size_t{1};
            const size_t MAX_RETRANSMISSION_CONSUMPTION_CAP                 = size_t{1} << 20;

            const size_t MIN_RETRANSMISSION_BUSY_THRESHOLD_SZ               = size_t{0u};
            const size_t MAX_RETRANSMISSION_BUSY_THRESHOLD_SZ               = std::numeric_limits<size_t>::max();

            const size_t MIN_OB_BORDER_CONTROLLER_VEC_SZ                    = size_t{1};
            const size_t MAX_OB_BORDER_CONTROLLER_VEC_SZ                    = size_t{1} << 20; 

            const size_t MIN_OB_PACKET_CONSUMPTION_CAP                      = size_t{1};
            const size_t MAX_OB_PACKET_CONSUMPTION_CAP                      = size_t{1} << 20;

            const size_t MIN_OB_PACKET_BUSY_THRESHOLD_SZ                    = size_t{0u};
            const size_t MAX_OB_PACKET_BUSY_THRESHOLD_SZ                    = std::numeric_limits<size_t>::max();

            const size_t MIN_SOCKET_VEC_SZ                                  = size_t{1};
            const size_t MAX_SOCKET_VEC_SZ                                  = size_t{1} << 20; 

            const size_t MIN_MAILBOX_INBOUND_CAP                            = size_t{1};
            const size_t MAX_MAILBOX_INBOUND_CAP                            = size_t{1} << 20;

            const size_t MIN_MAILBOX_OUTBOUND_CAP                           = size_t{1};
            const size_t MAX_MAILBOX_OUTBOUND_CAP                           = size_t{1} << 20;

            const std::chrono::nanoseconds MIN_SUBSCRIBED_UPDATE_DUR        = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds{1u});
            const std::chrono::nanoseconds MAX_SUBSCRIBED_UPDATE_DUR        = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{1u});

            const size_t MIN_WORKER_SZ                                      = size_t{1};
            const size_t MAX_WORKER_SZ                                      = size_t{1} << 10;

            if (ib_buffer_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ib_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ib_id_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ib_border_controller_vec.size(), MIN_IB_BORDER_CONTROLLER_VEC_SZ, MAX_IB_BORDER_CONTROLLER_VEC_SZ) != ib_border_controller_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (rescue_post == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (krescue_packet_generator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (retransmission_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ob_packet_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ob_border_controller_vec.size(), MIN_OB_BORDER_CONTROLLER_VEC_SZ, MAX_OB_BORDER_CONTROLLER_VEC_SZ) != ob_border_controller_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ob_exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (ack_packet_generator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (packet_integrity_validator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(socket_vec.size(), MIN_SOCKET_VEC_SZ, MAX_SOCKET_VEC_SZ) != socket_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(dedicated_outbound_socket_vec.size(), MIN_SOCKET_VEC_SZ, MAX_SOCKET_VEC_SZ) != dedicated_outbound_socket_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (req_packet_generator == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ib_buffer_accumulation_sz, MIN_IB_BUFFER_ACCUMULATION_SZ, MAX_IB_BUFFER_ACCUMULATION_SZ) != ib_buffer_accumulation_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ib_packet_consumption_cap, MIN_IB_PACKET_CONSUMPTION_CAP, MAX_IB_PACKET_CONSUMPTION_CAP) != ib_packet_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ib_packet_busy_threshold_sz, MIN_IB_PACKET_BUSY_THRESHOLD_SZ, MAX_IB_PACKET_BUSY_THRESHOLD_SZ) != ib_packet_busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(rescue_packet_sz, MIN_RESCUE_PACKET_SZ, MAX_RESCUE_PACKET_SZ) != rescue_packet_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(rescue_dispatch_threshold, MIN_RESCUE_DISPATCH_THRESHOLD, MAX_RESCUE_DISPATCH_THRESHOLD) != rescue_dispatch_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retransmission_consumption_cap, MIN_RETRANSMISSION_CONSUMPTION_CAP, MAX_RETRANSMISSION_CONSUMPTION_CAP) != retransmission_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(retransmission_busy_threshold_sz, MIN_RETRANSMISSION_BUSY_THRESHOLD_SZ, MAX_RETRANSMISSION_BUSY_THRESHOLD_SZ) != retransmission_busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ob_packet_consumption_cap, MIN_OB_PACKET_CONSUMPTION_CAP, MAX_OB_PACKET_CONSUMPTION_CAP) != ob_packet_consumption_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ob_packet_busy_threshold_sz, MIN_OB_PACKET_BUSY_THRESHOLD_SZ, MAX_OB_PACKET_BUSY_THRESHOLD_SZ) != ob_packet_busy_threshold_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(mailbox_inbound_cap, MIN_MAILBOX_INBOUND_CAP, MAX_MAILBOX_INBOUND_CAP) != mailbox_inbound_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(mailbox_outbound_cap, MIN_MAILBOX_OUTBOUND_CAP, MAX_MAILBOX_OUTBOUND_CAP) != mailbox_outbound_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(traffic_reset_duration, MIN_SUBSCRIBED_UPDATE_DUR, MAX_SUBSCRIBED_UPDATE_DUR) != traffic_reset_duration){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(num_kernel_inbound_worker, MIN_WORKER_SZ, MAX_WORKER_SZ) != num_kernel_inbound_worker){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(num_process_inbound_worker, MIN_WORKER_SZ, MAX_WORKER_SZ) != num_process_inbound_worker){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(num_outbound_worker, MIN_WORKER_SZ, MAX_WORKER_SZ) != num_outbound_worker){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(num_kernel_rescue_worker, MIN_WORKER_SZ, MAX_WORKER_SZ) != num_kernel_rescue_worker){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(num_retry_worker, MIN_WORKER_SZ, MAX_WORKER_SZ) != num_retry_worker){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (num_kernel_inbound_worker < socket_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (num_outbound_worker < dedicated_outbound_socket_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            std::shared_ptr<packet_controller::BufferContainerInterface> ib_buffer_container_sp                             = std::move(ib_buffer_container);
            std::shared_ptr<packet_controller::PacketContainerInterface> ib_packet_container_sp                             = std::move(ib_packet_container);
            std::shared_ptr<packet_controller::InBoundIDControllerInterface> ib_id_controller_sp                            = std::move(ib_id_controller);
            std::vector<std::shared_ptr<packet_controller::InBoundBorderController>> ib_border_controller_sp_vec            = up_vector_to_vsp_vector(std::move(ib_border_controller_vec));            
            std::shared_ptr<packet_controller::KernelRescuePostInterface> rescue_post_sp                                    = std::move(rescue_post);
            std::shared_ptr<packet_controller::KRescuePacketGeneratorInterface> krescue_packet_generator_sp                 = std::move(krescue_packet_generator);
            std::shared_ptr<packet_controller::RetransmissionControllerInterface> retransmission_controller_sp              = std::move(retransmission_controller);
            std::shared_ptr<packet_controller::PacketContainerInterface> ob_packet_container_sp                             = std::move(ob_packet_container);
            std::vector<std::shared_ptr<packet_controller::OutBoundBorderController>> ob_border_controller_sp_vec           = up_vector_to_vsp_vector(std::move(ob_border_controller_vec)); 
            std::shared_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface> ob_exhaustion_controller_sp   = std::move(ob_exhaustion_controller);
            std::shared_ptr<packet_controller::AckPacketGeneratorInterface> ack_packet_generator_sp                         = std::move(ack_packet_generator);
            std::shared_ptr<packet_controller::PacketIntegrityValidatorInterface> packet_integrity_validator_sp             = std::move(packet_integrity_validator);
            std::vector<std::shared_ptr<model::SocketHandle>> socket_sp_vec                                                 = up_vector_to_vsp_vector(std::move(socket_vec));
            std::vector<std::shared_ptr<model::SocketHandle>> dedicated_outbound_socket_sp_vec                              = up_vector_to_vsp_vector(std::move(dedicated_outbound_socket_vec));
            dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec                                            = {};

            size_t ib_border_controller_sp_vec_ptr                                                                          = 0u;
            size_t ob_border_controller_sp_vec_ptr                                                                          = 0u;
            size_t socket_sp_vec_ptr                                                                                        = 0u;
            size_t dedicated_outbound_socket_sp_vec_ptr                                                                     = 0u; 

            std::shared_ptr<packet_controller::BufferContainerInterface> ib_process_consume_end                             = {};

            if (ib_buffer_fair_container != nullptr){
                std::shared_ptr<packet_controller::BufferContainerInterface> intermediate_container = std::move(ib_buffer_fair_container);
                auto worker_ins     = worker::ComponentFactory::get_buffer_redistributor_worker(ib_buffer_container_sp,
                                                                                                intermediate_container,
                                                                                                buffer_fair_container_get_cap,
                                                                                                buffer_fair_container_push_cap,
                                                                                                buffer_fair_container_busy_threshold);

                auto daemon_handle  = dg::network_exception_handler::nothrow_log(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));

                ib_process_consume_end = intermediate_container;
            } else{
                ib_process_consume_end = ib_buffer_container_sp;
            }

            std::shared_ptr<packet_controller::PacketContainerInterface> mailbox_consume_end                                = {};

            if (ib_packet_fair_container != nullptr){
                std::shared_ptr<packet_controller::PacketContainerInterface> intermediate_container = std::move(ib_packet_fair_container);
                auto worker_ins     = worker::ComponentFactory::get_packet_redistributor_worker(ib_packet_container_sp,
                                                                                                intermediate_container,
                                                                                                packet_fair_container_get_cap,
                                                                                                packet_fair_container_push_cap,
                                                                                                packet_fair_container_busy_threshold);

                auto daemon_handle  = dg::network_exception_handler::nothrow_log(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));

                mailbox_consume_end = intermediate_container;
            } else{
                mailbox_consume_end = ib_packet_container_sp;
            }

            for (size_t i = 0u; i < num_kernel_inbound_worker; ++i){
                auto worker_ins     = worker::ComponentFactory::get_kernel_inbound_worker(ib_buffer_container_sp, rescue_post_sp, 
                                                                                          socket_sp_vec[socket_sp_vec_ptr++ % socket_sp_vec.size()], 
                                                                                          DEFAULT_HEARTBEAT_INTERVAL, ib_buffer_accumulation_sz);

                auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));
            }

            for (size_t i = 0u; i < num_process_inbound_worker; ++i){
                auto worker_ins     = worker::ComponentFactory::get_process_inbound_worker(retransmission_controller_sp, ob_packet_container_sp, ib_packet_container_sp, 
                                                                                           ib_process_consume_end, ib_id_controller_sp, 
                                                                                           ib_border_controller_sp_vec[ib_border_controller_sp_vec_ptr++ % ib_border_controller_sp_vec.size()], 
                                                                                           ack_packet_generator_sp, packet_integrity_validator_sp, ib_packet_consumption_cap,
                                                                                           ib_packet_busy_threshold_sz);

                auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));
            }

            for (size_t i = 0u; i < num_outbound_worker; ++i){
                auto worker_ins     = worker::ComponentFactory::get_outbound_worker(ob_packet_container_sp, 
                                                                                    ob_border_controller_sp_vec[ob_border_controller_sp_vec_ptr++ % ob_border_controller_sp_vec.size()], 
                                                                                    ob_exhaustion_controller_sp,
                                                                                    dedicated_outbound_socket_sp_vec[dedicated_outbound_socket_sp_vec_ptr++ % dedicated_outbound_socket_sp_vec.size()], 
                                                                                    ob_packet_consumption_cap, ob_packet_busy_threshold_sz);

                auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));
            }

            for (size_t i = 0u; i < num_kernel_rescue_worker; ++i){
                auto worker_ins     = worker::ComponentFactory::get_kernel_rescue_worker(ob_packet_container_sp, rescue_post_sp, krescue_packet_generator_sp, 
                                                                                         rescue_dispatch_threshold, rescue_disaster_sleep_dur, rescue_packet_sz);

                auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));
            }

            for (size_t i = 0u; i < num_retry_worker; ++i){
                auto worker_ins     = worker::ComponentFactory::get_retransmission_worker(retransmission_controller_sp, ob_packet_container_sp, retransmission_consumption_cap,
                                                                                          retransmission_busy_threshold_sz);

                auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker_ins)));
                daemon_vec.emplace_back(std::move(daemon_handle));
            }

            std::vector<std::shared_ptr<packet_controller::UpdatableInterface>> update_vec{};
            std::transform(ib_border_controller_sp_vec.begin(), ib_border_controller_sp_vec.end(), std::back_inserter(update_vec), [](const auto& e){return std::static_pointer_cast<packet_controller::UpdatableInterface>(e);});
            std::transform(ob_border_controller_sp_vec.begin(), ob_border_controller_sp_vec.end(), std::back_inserter(update_vec), [](const auto& e){return std::static_pointer_cast<packet_controller::UpdatableInterface>(e);});

            auto updater                = packet_controller::ComponentFactory::get_batch_updater(std::move(update_vec));
            auto traffic_update_ins     = worker::ComponentFactory::get_update_worker(std::move(updater), traffic_reset_duration);
            auto traffic_daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(traffic_update_ins)));
            daemon_vec.push_back(std::move(traffic_daemon_handle));


            return std::make_unique<RetransmittableMailBoxController>(std::move(daemon_vec), std::move(req_packet_generator), retransmission_controller_sp, 
                                                                      ob_packet_container_sp, mailbox_consume_end, mailbox_inbound_cap, 
                                                                      mailbox_outbound_cap);
        }
    };
}

namespace dg::network_kernel_mailbox_impl1::external_extension{

    class SockTrafficAdapter : public virtual external_interface::NetworkBusyStatusRetrieverInterface{

        public:

            using busy_level_t =  external_interface::NetworkBusyStatusRetrieverInterface::busy_level_t;

            auto get() noexcept -> std::expected<busy_level_t, exception_t>{

                using traffic_t = dg::network_sock_traffic_status_controller::types::traffic_status_t;

                traffic_t inbound_traffic_status    = dg::network_sock_traffic_status_controller::get_inbound_status();
                traffic_t outbound_traffic_status   = dg::network_sock_traffic_status_controller::get_outbound_status();

                if (inbound_traffic_status == dg::network_sock_traffic_status_controller::constants::congested
                    && outbound_traffic_status == dg::network_sock_traffic_status_controller::constants::congested){

                    return external_interface::NetworkBusyStatusRetrieverInterface::BUSY_2;
                }

                return external_interface::NetworkBusyStatusRetrieverInterface::BUSY_0;
            }
    };

    struct ComponentFactory
    {
        static auto get_network_busy_status_retriever_from_sock_traffic_status_controller() -> std::unique_ptr<external_interface::NetworkBusyStatusRetrieverInterface>{

            return std::make_unique<SockTrafficAdapter>();
        } 
    };
}

namespace dg::network_kernel_mailbox_impl1{

    //alright we are to offload this to testing team (which is our team)
    //we will implement other components for now, the code is clear
    //thank God we have completed 0.1% of our code
    //we are way way off track

    //the main bottleneck is probably from NIC -> RAM
    //each unordered_map access probably consumes ~= 100 bytes of memory accesses, avg case is probably 1000 bytes of memory accesses
    //this overhead is probably a lot, we dont really care yet
    //we'll attempt to solve the problem of unordered_map by using bucket_hint, essentially decreasing the entropy of buckets + aggregation to micro optimize this
    //that'd be unstable engineering, for the reason being that we dont really bend the system in the direction (we'd probably achieve 20% perf squeeze yet decreasing the system value overall) 

    //what we could do is to move the lever of the reactor depending on the incoming packets + outcoming packets

    struct Config{
        uint32_t num_kernel_inbound_worker;
        uint32_t num_process_inbound_worker;
        uint32_t num_outbound_worker;
        uint32_t num_kernel_rescue_worker;
        uint32_t num_retry_worker;

        uint32_t inbound_socket_concurrency_sz;
        uint32_t outbound_socket_concurrency_sz;
        int sin_fam;  
        int comm;
        int protocol;
        model::IP host_ip;
        uint16_t host_port_inbound;
        uint16_t host_port_outbound; 

        bool is_void_retransmission_controller;
        std::chrono::nanoseconds retransmission_delay;
        uint32_t retransmission_concurrency_sz;
        uint32_t retransmission_queue_cap;
        uint32_t retransmission_user_queue_cap;
        uint32_t retransmission_packet_cap;
        uint32_t retransmission_idhashset_cap;
        uint32_t retransmission_ticking_clock_resolution;
        bool retransmission_has_react_pattern;
        uint32_t retransmission_react_sz;
        uint32_t retransmission_react_queue_cap;
        uint32_t retransmission_user_push_concurrency_sz;
        uint32_t retransmission_retriable_push_concurrency_sz;
        uint32_t retransmission_unit_sz;
        std::chrono::nanoseconds retransmission_react_time;
        bool retransmission_has_exhaustion_control; 

        uint32_t inbound_buffer_concurrency_sz;
        uint32_t inbound_buffer_container_cap;
        bool inbound_buffer_has_react_pattern;
        uint32_t inbound_buffer_react_sz;
        uint32_t inbound_buffer_react_queue_cap;
        uint32_t inbound_buffer_push_concurrency_sz;
        std::chrono::nanoseconds inbound_buffer_react_time;
        bool inbound_buffer_has_fair_redistribution;
        uint32_t inbound_buffer_fair_distribution_queue_cap;
        uint32_t inbound_buffer_fair_waiting_queue_cap;
        uint32_t inbound_buffer_fair_leftover_queue_cap;
        uint32_t inbound_buffer_fair_push_concurrency_sz;
        uint32_t inbound_buffer_fair_unit_sz;
        bool inbound_buffer_has_exhaustion_control; 
 
        uint32_t inbound_packet_concurrency_sz;
        uint32_t inbound_packet_container_cap;
        bool inbound_packet_has_react_pattern;
        uint32_t inbound_packet_react_sz;
        uint32_t inbound_packet_react_queue_cap;
        uint32_t inbound_packet_push_concurrency_sz;
        std::chrono::nanoseconds inbound_packet_react_time;
        bool inbound_packet_has_fair_redistribution;
        uint32_t inbound_packet_fair_packet_queue_cap;
        uint32_t inbound_packet_fair_waiting_queue_cap;
        uint32_t inbound_packet_fair_leftover_queue_cap; 
        uint32_t inbound_packet_fair_push_concurrency_sz;
        uint32_t inbound_packet_fair_unit_sz;
        bool inbound_packet_has_exhaustion_control;

        uint32_t inbound_idhashset_concurrency_sz; 
        uint32_t inbound_idhashset_cap;

        uint32_t worker_inbound_buffer_fair_container_fr_warehouse_get_cap;
        uint32_t worker_inbound_buffer_fair_container_to_warehouse_push_cap;
        uint32_t worker_inbound_buffer_fair_container_busy_threshold;

        uint32_t worker_inbound_fair_packet_fr_warehouse_get_cap;
        uint32_t worker_inbound_fair_packet_to_warehouse_push_cap;
        uint32_t worker_inbound_fair_packet_busy_threshold;

        uint32_t worker_inbound_buffer_accumulation_sz;
        uint32_t worker_inbound_packet_consumption_cap;
        uint32_t worker_inbound_packet_busy_threshold_sz;
        uint32_t worker_rescue_packet_sz_per_transmit; 
        std::chrono::nanoseconds worker_kernel_rescue_dispatch_threshold;
        std::chrono::nanoseconds worker_kernel_rescue_disaster_sleep_dur;
        uint32_t worker_retransmission_consumption_cap;
        uint32_t worker_retransmission_busy_threshold_sz;
        uint32_t worker_outbound_packet_consumption_cap;
        uint32_t worker_outbound_packet_busy_threshold_sz;

        uint32_t mailbox_inbound_cap;
        uint32_t mailbox_outbound_cap;
        std::chrono::nanoseconds traffic_reset_duration;

        uint32_t outbound_transmit_frequency;

        uint32_t outbound_container_request_packet_container_cap;
        uint32_t outbound_container_ack_packet_container_cap;
        uint32_t outbound_container_krescue_packet_container_cap;
        uint32_t outbound_container_waiting_queue_capacity;
        uint32_t outbound_container_leftover_queue_capacity; 
        uint32_t outbound_container_unit_sz;
        uint32_t outbound_container_push_concurrency_sz;
        bool outbound_container_has_exhaustion_control; 

        bool inbound_tc_has_borderline_per_inbound_worker;
        uint32_t inbound_tc_peraddr_cap;
        uint32_t inbound_tc_global_cap;
        uint32_t inbound_tc_addrmap_cap;
        uint32_t inbound_tc_side_cap;

        bool outbound_tc_has_borderline_per_outbound_worker;
        uint32_t outbound_tc_border_line_sz;
        uint32_t outbound_tc_peraddr_cap;
        uint32_t outbound_tc_global_cap;
        uint32_t outbound_tc_addrmap_cap;
        uint32_t outbound_tc_side_cap;

        std::shared_ptr<external_interface::NATIPControllerInterface> natip_controller;
        std::shared_ptr<external_interface::NetworkBusyStatusRetrieverInterface> busy_retriever;
        std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> retry_device;
    };

    extern auto get_network_busy_status_retriever_from_sock_traffic_status_controller() -> std::unique_ptr<external_interface::NetworkBusyStatusRetrieverInterface>{

        return external_extension::ComponentFactory::get_network_busy_status_retriever_from_sock_traffic_status_controller();
    }

    extern auto get_default_natip_controller(std::shared_ptr<external_interface::IPSieverInterface> inbound_rule,
                                             std::shared_ptr<external_interface::IPSieverInterface> outbound_rule,
                                             uint32_t inbound_set_capacity,
                                             uint32_t outbound_set_capacity) -> std::unique_ptr<external_interface::NATIPControllerInterface>{

        return packet_controller::ComponentFactory::get_nat_ip_controller(inbound_rule, outbound_rule, inbound_set_capacity, outbound_set_capacity);
    }

    struct ConfigMaker{

        private:

            static auto make_inbound_buffer_container(Config config) -> std::unique_ptr<packet_controller::BufferContainerInterface>{

                if (config.inbound_buffer_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.inbound_buffer_concurrency_sz == 1u){
                    if (config.inbound_buffer_has_exhaustion_control){
                        if (config.inbound_buffer_has_react_pattern){
                            return packet_controller::ComponentFactory::get_exhaustion_controlled_buffer_container(packet_controller::ComponentFactory::get_reacting_buffer_container(packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz),
                                                                                                                                                                                      config.inbound_buffer_react_sz,
                                                                                                                                                                                      config.inbound_buffer_react_queue_cap,
                                                                                                                                                                                      config.inbound_buffer_react_time),
                                                                                                                   config.retry_device,
                                                                                                                   packet_controller::ComponentFactory::get_default_exhaustion_controller());
                        }

                        return packet_controller::ComponentFactory::get_exhaustion_controlled_buffer_container(packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz),
                                                                                                               config.retry_device,
                                                                                                               packet_controller::ComponentFactory::get_default_exhaustion_controller());
                    }

                    if (config.inbound_buffer_has_react_pattern){
                        return packet_controller::ComponentFactory::get_reacting_buffer_container(packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz),
                                                                                                  config.inbound_buffer_react_sz,
                                                                                                  config.inbound_buffer_react_queue_cap,
                                                                                                  config.inbound_buffer_react_time);
                    }

                    return packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz);
                }

                std::vector<std::unique_ptr<packet_controller::BufferContainerInterface>> buffer_container_vec{};

                for (size_t i = 0u; i < config.inbound_buffer_concurrency_sz; ++i){
                    auto current_buffer_container = std::unique_ptr<packet_controller::BufferContainerInterface>{};

                    if (config.inbound_buffer_has_exhaustion_control){
                        current_buffer_container = packet_controller::ComponentFactory::get_exhaustion_controlled_buffer_container(packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz),
                                                                                                                                   config.retry_device,
                                                                                                                                   packet_controller::ComponentFactory::get_default_exhaustion_controller());
                    } else{
                        current_buffer_container = packet_controller::ComponentFactory::get_buffer_fifo_container(config.inbound_buffer_container_cap, config.inbound_buffer_push_concurrency_sz);
                    }

                    buffer_container_vec.push_back(std::move(current_buffer_container));
                }

                if (config.inbound_buffer_has_react_pattern){
                    return packet_controller::ComponentFactory::get_reacting_buffer_container(packet_controller::ComponentFactory::get_randomhash_distributed_buffer_container(std::move(buffer_container_vec)),
                                                                                              config.inbound_buffer_react_sz,
                                                                                              config.inbound_buffer_react_queue_cap,
                                                                                              config.inbound_buffer_react_time);
                }

                return packet_controller::ComponentFactory::get_randomhash_distributed_buffer_container(std::move(buffer_container_vec));
            }
            
            static auto make_inbound_buffer_fair_container(Config config) -> std::unique_ptr<packet_controller::BufferContainerInterface>{

                if (!config.inbound_buffer_has_fair_redistribution){
                    return nullptr;
                }

                return packet_controller::ComponentFactory::get_fair_inbound_buffer_container(config.inbound_buffer_fair_distribution_queue_cap,
                                                                                              config.inbound_buffer_fair_waiting_queue_cap,
                                                                                              config.inbound_buffer_fair_leftover_queue_cap,
                                                                                              config.inbound_buffer_fair_push_concurrency_sz,
                                                                                              config.inbound_buffer_fair_unit_sz);
            }

            static auto make_inbound_packet_container(Config config) -> std::unique_ptr<packet_controller::PacketContainerInterface>{

                if (config.inbound_packet_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.inbound_packet_concurrency_sz == 1u){
                    if (config.inbound_packet_has_exhaustion_control){
                        if (config.inbound_packet_has_react_pattern){
                            return packet_controller::ComponentFactory::get_exhaustion_controlled_packet_container(packet_controller::ComponentFactory::get_reacting_packet_container(packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz),
                                                                                                                                                                                      config.inbound_packet_react_sz,
                                                                                                                                                                                      config.inbound_packet_react_queue_cap,
                                                                                                                                                                                      config.inbound_packet_react_time),
                                                                                                                   config.retry_device,
                                                                                                                   packet_controller::ComponentFactory::get_default_exhaustion_controller());
                        }

                        return packet_controller::ComponentFactory::get_exhaustion_controlled_packet_container(packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz),
                                                                                                               config.retry_device,
                                                                                                               packet_controller::ComponentFactory::get_default_exhaustion_controller());
                    }

                    if (config.inbound_packet_has_react_pattern){
                        return packet_controller::ComponentFactory::get_reacting_packet_container(packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz),
                                                                                                  config.inbound_packet_react_sz,
                                                                                                  config.inbound_packet_react_queue_cap,
                                                                                                  config.inbound_packet_react_time);
                    }

                    return packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz);
                }

                std::vector<std::unique_ptr<packet_controller::PacketContainerInterface>> packet_container_vec{};

                for (size_t i = 0u; i < config.inbound_packet_concurrency_sz; ++i){
                    auto current_packet_container = std::unique_ptr<packet_controller::PacketContainerInterface>{};
                     
                    if (config.inbound_packet_has_exhaustion_control){
                        current_packet_container = packet_controller::ComponentFactory::get_exhaustion_controlled_packet_container(packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz),
                                                                                                                                   config.retry_device,
                                                                                                                                   packet_controller::ComponentFactory::get_default_exhaustion_controller());
                    } else{
                        current_packet_container = packet_controller::ComponentFactory::get_packet_fifo_container(config.inbound_packet_container_cap, config.inbound_packet_push_concurrency_sz);
                    }

                    packet_container_vec.push_back(std::move(current_packet_container));
                }

                if (config.inbound_packet_has_react_pattern){
                    return packet_controller::ComponentFactory::get_reacting_packet_container(packet_controller::ComponentFactory::get_randomhash_distributed_packet_container(std::move(packet_container_vec)),
                                                                                              config.inbound_packet_react_sz,
                                                                                              config.inbound_packet_react_queue_cap,
                                                                                              config.inbound_packet_react_time);
                }

                return packet_controller::ComponentFactory::get_randomhash_distributed_packet_container(std::move(packet_container_vec));
            }

            static auto make_inbound_packet_fair_container(Config config) -> std::unique_ptr<packet_controller::PacketContainerInterface>{

                if (!config.inbound_packet_has_fair_redistribution){
                    return nullptr;
                }

                return packet_controller::ComponentFactory::get_fair_inbound_packet_container(config.inbound_packet_fair_packet_queue_cap,
                                                                                              config.inbound_packet_fair_waiting_queue_cap,
                                                                                              config.inbound_packet_fair_leftover_queue_cap,
                                                                                              config.inbound_packet_fair_push_concurrency_sz,
                                                                                              config.inbound_packet_fair_unit_sz);
            }

            static auto make_inbound_id_controller(Config config) -> std::unique_ptr<packet_controller::InBoundIDControllerInterface>{

                if (config.inbound_idhashset_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.inbound_idhashset_concurrency_sz == 1u){
                    return packet_controller::ComponentFactory::get_inbound_id_controller(config.inbound_idhashset_cap);
                }

                std::vector<std::unique_ptr<packet_controller::InBoundIDControllerInterface>> inbound_id_vec{};

                for (size_t i = 0u; i < config.inbound_idhashset_concurrency_sz; ++i){
                    auto current_id_controller = packet_controller::ComponentFactory::get_inbound_id_controller(config.inbound_idhashset_cap);
                    inbound_id_vec.push_back(std::move(current_id_controller)); 
                }

                return packet_controller::ComponentFactory::get_randomhash_distributed_inbound_id_controller(std::move(inbound_id_vec));

            }

            static auto make_inbound_border_controller(Config config) -> std::vector<std::unique_ptr<packet_controller::InBoundBorderController>>{

                if (config.num_kernel_inbound_worker == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                auto ib_border_line_controller_vec  = std::vector<std::unique_ptr<packet_controller::InBoundBorderController>>{};
                size_t inbound_borderline_sz        = static_cast<size_t>(config.inbound_tc_has_borderline_per_inbound_worker) * config.num_kernel_inbound_worker;

                for (size_t i = 0u; i < inbound_borderline_sz; ++i){
                    auto current_border_controller = packet_controller::ComponentFactory::get_inbound_border_controller(config.natip_controller, 
                                                                                                                        config.inbound_tc_peraddr_cap,
                                                                                                                        config.inbound_tc_global_cap,
                                                                                                                        config.inbound_tc_addrmap_cap,
                                                                                                                        config.inbound_tc_side_cap);

                    ib_border_line_controller_vec.push_back(std::move(current_border_controller)); 
                }

                return ib_border_line_controller_vec;
            }

            static auto make_kernel_rescue_post(Config config) -> std::unique_ptr<packet_controller::KernelRescuePostInterface>{

                return packet_controller::ComponentFactory::get_kernel_rescue_post();
            }

            static auto make_kernel_rescue_packet_generator(Config config) -> std::unique_ptr<packet_controller::KRescuePacketGeneratorInterface>{

                return packet_controller::ComponentFactory::get_randomid_krescue_packet_generator(utility::to_factory_id(model::Address{config.host_ip, config.host_port_inbound}), model::Address{config.host_ip, config.host_port_inbound});
            }

            static auto make_retransmission_controller(Config config) -> std::unique_ptr<packet_controller::RetransmissionControllerInterface>{

                if (config.retransmission_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.is_void_retransmission_controller){
                    return packet_controller::ComponentFactory::get_empty_retransmission_controller();
                }

                if (config.retransmission_concurrency_sz == 1u){
                    return packet_controller::ComponentFactory::get_memory_efficient_retransmission_controller(config.retransmission_queue_cap,
                                                                                                               config.retransmission_idhashset_cap,
                                                                                                               packet_controller::ComponentFactory::get_static_retransmission_delay_negotiator(config.retransmission_delay),
                                                                                                               config.retransmission_ticking_clock_resolution,
                                                                                                               config.retransmission_packet_cap,
                                                                                                               config.retransmission_user_queue_cap,
                                                                                                               config.retransmission_user_push_concurrency_sz,
                                                                                                               config.retransmission_retriable_push_concurrency_sz,
                                                                                                               config.retransmission_unit_sz);
                }

                std::vector<std::unique_ptr<packet_controller::RetransmissionControllerInterface>> retransmission_controller_vec{};

                for (size_t i = 0u; i < config.retransmission_concurrency_sz; ++i){
                    auto current_retransmission_controller = std::unique_ptr<packet_controller::RetransmissionControllerInterface>{}; 

                    current_retransmission_controller = packet_controller::ComponentFactory::get_memory_efficient_retransmission_controller(config.retransmission_queue_cap,
                                                                                                                                            config.retransmission_idhashset_cap,
                                                                                                                                            packet_controller::ComponentFactory::get_static_retransmission_delay_negotiator(config.retransmission_delay),
                                                                                                                                            config.retransmission_ticking_clock_resolution,
                                                                                                                                            config.retransmission_packet_cap,
                                                                                                                                            config.retransmission_user_queue_cap,
                                                                                                                                            config.retransmission_user_push_concurrency_sz,
                                                                                                                                            config.retransmission_retriable_push_concurrency_sz,
                                                                                                                                            config.retransmission_unit_sz);

                    retransmission_controller_vec.push_back(std::move(current_retransmission_controller));
                }

                return packet_controller::ComponentFactory::get_randomhash_distributed_retransmission_controller(std::move(retransmission_controller_vec));
            }

            static auto make_outbound_packet_container(Config config) -> std::unique_ptr<packet_controller::PacketContainerInterface>{

                auto ins = packet_controller::ComponentFactory::get_normal_outbound_packet_container(config.outbound_container_request_packet_container_cap,
                                                                                                     config.outbound_container_ack_packet_container_cap,
                                                                                                     config.outbound_container_krescue_packet_container_cap,
                                                                                                     config.outbound_container_waiting_queue_capacity,
                                                                                                     config.outbound_container_leftover_queue_capacity,
                                                                                                     config.outbound_container_unit_sz,
                                                                                                     config.outbound_container_push_concurrency_sz);
                
                if (config.outbound_container_has_exhaustion_control){
                    return packet_controller::ComponentFactory::get_exhaustion_controlled_packet_container(std::move(ins), 
                                                                                                           config.retry_device,
                                                                                                           packet_controller::ComponentFactory::get_default_exhaustion_controller());
                } else{
                    return ins;
                }
            }

            static auto make_outbound_border_controller(Config config) -> std::vector<std::unique_ptr<packet_controller::OutBoundBorderController>>{

                if (config.num_outbound_worker == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                auto ob_borderline_controller_vec   = std::vector<std::unique_ptr<packet_controller::OutBoundBorderController>>{};
                size_t outbound_borderline_sz       = static_cast<size_t>(config.outbound_tc_has_borderline_per_outbound_worker) * config.num_outbound_worker;

                for (size_t i = 0u; i < outbound_borderline_sz; ++i){
                    auto current_border_controller = packet_controller::ComponentFactory::get_outbound_border_controller(config.natip_controller,
                                                                                                                         config.outbound_tc_peraddr_cap,
                                                                                                                         config.outbound_tc_global_cap,
                                                                                                                         config.outbound_tc_addrmap_cap,
                                                                                                                         config.outbound_tc_side_cap);

                    ob_borderline_controller_vec.push_back(std::move(current_border_controller));
                }

                return ob_borderline_controller_vec;
            }

            static auto make_outbound_transmission_controller(Config config) -> std::unique_ptr<packet_controller::KernelOutBoundTransmissionControllerInterface>{

                return packet_controller::ComponentFactory::get_kernel_outbound_static_transmission_controller(config.outbound_transmit_frequency);
            }

            static auto make_ack_packet_generator(Config config) -> std::unique_ptr<packet_controller::AckPacketGeneratorInterface>{

                return packet_controller::ComponentFactory::get_randomid_ack_packet_generator(utility::to_factory_id(model::Address{config.host_ip, config.host_port_inbound}), model::Address{config.host_ip, config.host_port_inbound});
            }

            static auto make_inbound_packet_integrity_validator(Config config) -> std::unique_ptr<packet_controller::PacketIntegrityValidatorInterface>{

                return packet_controller::ComponentFactory::get_inbound_packet_integrity_validator(model::Address{config.host_ip, config.host_port_inbound});
            }

            static auto make_inbound_socket(Config config) -> std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>>{

                if (config.inbound_socket_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                auto socket_vec = std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>>{};

                if (config.inbound_socket_concurrency_sz == 1u){
                    auto current_socket = dg::network_exception_handler::throw_nolog(socket_service::open_socket(config.sin_fam, config.comm, config.protocol));
                    dg::network_exception_handler::throw_nolog(socket_service::port_socket(*current_socket, config.host_port_inbound, false));
                    socket_vec.push_back(std::move(current_socket));
                } else{
                    for (size_t i = 0u; i < config.inbound_socket_concurrency_sz; ++i){
                        auto current_socket = dg::network_exception_handler::throw_nolog(socket_service::open_socket(config.sin_fam, config.comm, config.protocol));
                        dg::network_exception_handler::throw_nolog(socket_service::port_socket(*current_socket, config.host_port_inbound, true));
                        socket_vec.push_back(std::move(current_socket));
                    }

                    for (size_t i = 0u; i < config.inbound_socket_concurrency_sz; ++i){
                        dg::network_exception_handler::throw_nolog(socket_service::attach_bpf_socket(*socket_vec[i]));
                    }
                }

                return socket_vec;
            }

            static auto make_outbound_socket(Config config) -> std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>>{

                if (config.outbound_socket_concurrency_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                auto socket_vec = std::vector<std::unique_ptr<model::SocketHandle, socket_service::socket_close_t>>{};

                if (config.outbound_socket_concurrency_sz == 1u){
                    auto current_socket = dg::network_exception_handler::throw_nolog(socket_service::open_socket(config.sin_fam, config.comm, config.protocol));
                    dg::network_exception_handler::throw_nolog(socket_service::port_socket(*current_socket, config.host_port_outbound, false));
                    socket_vec.push_back(std::move(current_socket));
                } else{
                    for (size_t i = 0u; i < config.outbound_socket_concurrency_sz; ++i){
                        auto current_socket = dg::network_exception_handler::throw_nolog(socket_service::open_socket(config.sin_fam, config.comm, config.protocol));
                        dg::network_exception_handler::throw_nolog(socket_service::port_socket(*current_socket, config.host_port_outbound, true));
                        socket_vec.push_back(std::move(current_socket));
                    }

                    for (size_t i = 0u; i < config.outbound_socket_concurrency_sz; ++i){
                        dg::network_exception_handler::throw_nolog(socket_service::attach_bpf_socket(*socket_vec[i]));
                    }
                }

                return socket_vec;
            }

            static auto make_request_packet_generator(Config config) -> std::unique_ptr<packet_controller::RequestPacketGeneratorInterface>{

                return packet_controller::ComponentFactory::get_randomid_request_packet_generator(utility::to_factory_id(model::Address{config.host_ip, config.host_port_inbound}), model::Address{config.host_ip, config.host_port_inbound});
            }

        public:

            static auto make(Config config) -> std::unique_ptr<core::MailboxInterface>{

                return core::ComponentFactory::get_retransmittable_mailbox_controller(make_inbound_buffer_container(config),
                                                                                      config.worker_inbound_buffer_accumulation_sz,

                                                                                      make_inbound_packet_container(config),
                                                                                      make_inbound_id_controller(config),

                                                                                      make_inbound_border_controller(config),
                                                                                      config.worker_inbound_packet_consumption_cap,
                                                                                      config.worker_inbound_packet_busy_threshold_sz,

                                                                                      make_kernel_rescue_post(config),
                                                                                      make_kernel_rescue_packet_generator(config),
                                                                                      config.worker_rescue_packet_sz_per_transmit,
                                                                                      config.worker_kernel_rescue_dispatch_threshold,
                                                                                      config.worker_kernel_rescue_disaster_sleep_dur,

                                                                                      make_retransmission_controller(config),
                                                                                      config.worker_retransmission_consumption_cap,
                                                                                      config.worker_retransmission_busy_threshold_sz,
                                                                                    
                                                                                      make_outbound_packet_container(config),
                                                                                      make_outbound_border_controller(config),
                                                                                      make_outbound_transmission_controller(config),
                                                                                      config.worker_outbound_packet_consumption_cap,
                                                                                      config.worker_outbound_packet_busy_threshold_sz,

                                                                                      make_ack_packet_generator(config),
                                                                                      make_inbound_packet_integrity_validator(config),
                                                                                      make_inbound_socket(config),
                                                                                      make_outbound_socket(config),

                                                                                      make_request_packet_generator(config),
                                                                                      config.mailbox_inbound_cap,
                                                                                      config.mailbox_outbound_cap,

                                                                                      config.traffic_reset_duration,

                                                                                      config.num_kernel_inbound_worker,
                                                                                      config.num_process_inbound_worker,
                                                                                      config.num_outbound_worker,
                                                                                      config.num_kernel_rescue_worker,
                                                                                      config.num_retry_worker,
                                                                                    
                                                                                      make_inbound_buffer_fair_container(config),
                                                                                      config.worker_inbound_buffer_fair_container_fr_warehouse_get_cap,
                                                                                      config.worker_inbound_buffer_fair_container_to_warehouse_push_cap,
                                                                                      config.worker_inbound_buffer_fair_container_busy_threshold,
                                                                                    
                                                                                      make_inbound_packet_fair_container(config),
                                                                                      config.worker_inbound_fair_packet_fr_warehouse_get_cap,
                                                                                      config.worker_inbound_fair_packet_to_warehouse_push_cap,
                                                                                      config.worker_inbound_fair_packet_busy_threshold);
            }
    };

    extern auto spawn(Config config) -> std::unique_ptr<core::MailboxInterface>{

        return ConfigMaker::make(config);
    }
}

#endif