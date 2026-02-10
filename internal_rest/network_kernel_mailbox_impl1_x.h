#ifndef __NETWORK_KERNEL_MAILBOX_IMPL1_X_H__
#define __NETWORK_KERNEL_MAILBOX_IMPL1_X_H__

//define HEADER_CONTROL 10

#include <memory>
#include "network_kernel_mailbox_impl1.h"
#include "network_compact_trivial_serializer.h"
#include "network_concurrency.h"
#include "network_std_container.h"
#include <chrono>
#include "network_log.h"
#include "network_concurrency_x.h"
#include "stdx.h"
#include "network_exception_handler.h"
#include <chrono>
#include "network_chrono.h"
#include "network_sock_traffic_status_controller.h"

// namespace dg::network_kernel_mailbox_impl1_meterlogx{

//     using Address           = dg::network_kernel_mailbox_impl1::model::Address; 
//     using MailBoxArgument   = dg::network_kernel_mailbox_impl1::model::MailBoxArgument;

//     struct MeterInterface{
//         virtual ~MeterInterface() noexcept = default;
//         virtual void tick(size_t) noexcept = 0;
//         virtual auto get_count() noexcept -> size_t = 0;
//         virtual auto get_count_since() noexcept -> std::chrono::time_point<std::chrono::steady_clock> = 0;  
//         virtual void reset() noexcept = 0;
//     };

//     struct MessageStreamerInterface{
//         virtual ~MessageStreamerInterface() noexcept = default;
//         virtual void stream(std::string_view) noexcept = 0;
//     };

//     class AtomicMeter: public virtual MeterInterface{

//         private:

//             //alright, people would argue that this is hardware_destructive_interference_sz, for best practices, I agree

//             stdx::inplace_hdi_container<std::atomic<size_t>> count;
//             stdx::inplace_hdi_container<std::atomic<std::chrono::time_point<std::chrono::steady_clock>>> since;

//         public:

//             AtomicMeter() noexcept: count(std::in_place_t{}, 0u),
//                                     since(std::in_place_t{}, std::chrono::steady_clock::now()){};

//             void tick(size_t incoming_sz) noexcept{

//                 this->count.value.fetch_add(incoming_sz, std::memory_order_relaxed);
//             }

//             auto get_count() noexcept -> size_t{

//                 return this->count.value.load(std::memory_order_relaxed);
//             }

//             auto get_count_since() noexcept -> std::chrono::time_point<std::chrono::steady_clock>{

//                 return this->since.value.load(std::memory_order_relaxed);
//             }

//             void reset() noexcept{

//                 stdx::seq_cst_guard seqcst_tx;

//                 this->count.value.exchange(0u, std::memory_order_relaxed);
//                 this->since.value.exchange(std::chrono::steady_clock::now(), std::memory_order_relaxed);
//             }
//     };

//     class RandomDistributedMeter: public virtual MeterInterface{

//         private:

//             std::unique_ptr<std::unique_ptr<MeterInterface>[]> base_arr;
//             size_t pow2_base_arr_sz;
        
//         public:

//             RandomDistributedMeter(std::unique_ptr<std::unique_ptr<MeterInterface>[]> base_arr,
//                                    size_t pow2_base_arr_sz) noexcept: base_arr(std::move(base_arr)),
//                                                                       pow2_base_arr_sz(pow2_base_arr_sz){}

//             void tick(size_t incoming_sz) noexcept{

//                 size_t random_value = dg::network_randomizer::randomize_int<size_t>();
//                 size_t random_idx   = random_value & (this->pow2_base_arr_sz - 1u);

//                 this->base_arr[random_idx]->tick(incoming_sz);
//             }

//             auto get_count() noexcept -> size_t{

//                 size_t total_sz = 0u;

//                 for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
//                     total_sz += this->base_arr[i]->get_count();
//                 }

//                 return total_sz;
//             }

//             auto get_count_since() noexcept -> std::chrono::time_point<std::chrono::steady_clock>{

//                 std::chrono::time_point<std::chrono::steady_clock> since = this->base_arr[0]->get_count_since();

//                 for (size_t i = 1u; i < this->pow2_base_arr_sz; ++i){
//                     since = std::min(since, this->base_arr[i]->get_count_since());
//                 }

//                 return since;
//             }

//             void reset() noexcept{

//                 for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
//                     this->base_arr[i]->reset();
//                 }
//             }
//     };

//     class MeterLogWorker: public virtual dg::network_concurrency::WorkerInterface{

//         private:

//             dg::string device_id;
//             std::shared_ptr<MeterInterface> send_meter;
//             std::shared_ptr<MeterInterface> recv_meter;
//             std::shared_ptr<MessageStreamerInterface> msg_streamer;
//             std::chrono::nanoseconds meter_dur;

//         public:

//             MeterLogWorker(dg::string device_id,
//                            std::shared_ptr<MeterInterface> send_meter, 
//                            std::shared_ptr<MeterInterface> recv_meter,
//                            std::shared_ptr<MessageStreamerInterface> msg_streamer,
//                            std::chrono::nanoseconds meter_dur) noexcept: device_id(std::move(device_id)),
//                                                                          send_meter(std::move(send_meter)),
//                                                                          recv_meter(std::move(recv_meter)),
//                                                                          msg_streamer(std::move(msg_streamer)),
//                                                                          meter_dur(meter_dur){}

//             bool run_one_epoch() noexcept{
 
//                 std::expected<dg::string, exception_t> send_msg = this->make_send_meter_msg(this->send_meter->get_count(), this->send_meter->get_count_since());
//                 std::expected<dg::string, exception_t> recv_msg = this->make_recv_meter_msg(this->recv_meter->get_count(), this->recv_meter->get_count_since());

//                 if (!send_msg.has_value()){
//                     dg::network_log_stackdump::error_fast(dg::network_exception::verbose(send_msg.error()));
//                     return false;
//                 }

//                 if (!recv_msg.has_value()){
//                     dg::network_log_stackdump::error_fast(dg::network_exception::verbose(recv_msg.error()));
//                     return false;
//                 }

//                 this->msg_streamer->stream(send_msg.value());
//                 this->msg_streamer->stream(recv_msg.value());

//                 this->send_meter->reset();
//                 this->recv_meter->reset();
//                 std::this_thread::sleep_for(this->meter_dur);

//                 return true;
//             }
        
//         private:

//             auto make_send_meter_msg(size_t bsz, std::chrono::time_point<std::chrono::steady_clock> since) noexcept -> std::expected<dg::string, exception_t>{

//                 auto now        = std::chrono::steady_clock::now();
//                 auto dur_in_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now - since);
//                 size_t tick_sz  = dur_in_ms.count();

//                 if (tick_sz == 0u){
//                     return {};
//                     // return stdx::expected_format("[METER_REPORT] low meter precision resolution (device_id: {}, part: send_meter)", this->device_id);
//                 } 

//                 size_t bsz_per_ms   = bsz / tick_sz;
//                 size_t bsz_per_s    = bsz_per_ms * 1000u;

//                 // return stdx::expected_format("[METER_REPORT] {} bytes/s sent to {}", bsz_per_s, this->device_id);
//                 return {};
//             }

//             auto make_recv_meter_msg(size_t bsz, std::chrono::time_point<std::chrono::steady_clock> since) noexcept -> std::expected<dg::string, exception_t>{

//                 auto now        = std::chrono::steady_clock::now();
//                 auto dur_in_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now - since);
//                 size_t tick_sz  = dur_in_ms.count();

//                 if (tick_sz == 0u){
//                     // return stdx::expected_format("[METER_REPORT] low meter precision resolution (device_id: {}, part: recv_meter)", this->device_id);
//                     return {};
//                 }

//                 size_t bsz_per_ms   = bsz / tick_sz;
//                 size_t bsz_per_s    = bsz_per_ms * 1000u;

//                 // return stdx::expected_format("[METER_REPORT] {} bytes/s recv from {}", bsz_per_s, this->device_id);
//                 return {};
//             }
//     };

//     class MeteredMailBox: public virtual dg::network_kernel_mailbox_impl1::core::MailboxInterface{

//         private:

//             dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec;
//             std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base;
//             std::shared_ptr<MeterInterface> send_meter;
//             std::shared_ptr<MeterInterface> recv_meter;
        
//         public:

//             MeteredMailBox(dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec, 
//                            std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
//                            std::shared_ptr<MeterInterface> send_meter,
//                            std::shared_ptr<MeterInterface> recv_meter): daemon_vec(std::move(daemon_vec)),
//                                                                         base(std::move(base)),
//                                                                         send_meter(std::move(send_meter)),
//                                                                         recv_meter(std::move(recv_meter)){}

//             void send(std::move_iterator<MailBoxArgument *> data_arr, size_t sz, exception_t * exception_arr) noexcept{
                
//                 MailBoxArgument * base_data_arr = data_arr.base();
//                 size_t total_sz = 0u;

//                 for (size_t i = 0u; i < sz; ++i){
//                     total_sz += base_data_arr[i].content.size();
//                 }

//                 this->base->send(std::make_move_iterator(base_data_arr), sz, exception_arr);

//                 for (size_t i = 0u; i < sz; ++i){
//                     if (dg::network_exception::is_failed(exception_arr[i])){
//                         total_sz -= base_data_arr[i].content.size();
//                     }
//                 }

//                 this->send_meter->tick(total_sz);
//             }

//             void recv(dg::string * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept{

//                 this->base->recv(output_arr, output_arr_sz, output_arr_cap);
//                 size_t total_sz = 0u;

//                 for (size_t i = 0u; i < output_arr_sz; ++i){
//                     total_sz += output_arr[i].size();
//                 }

//                 this->recv_meter->tick(total_sz);
//             }

//             auto max_consume_size() noexcept -> size_t{

//                 return this->base->max_consume_size();
//             }
//     };

//     struct ComponentFactory{

//         template <class T, class ...Args>
//         static auto to_dg_vector(std::vector<T, Args...> vec) -> dg::vector<T>{

//             dg::vector<T> rs{};

//             for (size_t i = 0u; i < vec.size(); ++i){
//                 rs.emplace_back(std::move(vec[i]));
//             }

//             return rs;
//         }

//         static auto to_dg_string(std::string_view inp) -> dg::string{

//             dg::string rs{};
//             std::copy(inp.begin(), inp.end(), std::back_inserter(rs));

//             return rs;
//         }

//         static auto get_meter() -> std::unique_ptr<MeterInterface>{

//             return std::make_unique<AtomicMeter>();
//         }

//         static auto get_distributed_meter(size_t tentative_concurrent_meter_sz) -> std::unique_ptr<MeterInterface>{

//             const size_t MIN_CONCURRENT_METER_SZ    = size_t{1};
//             const size_t MAX_CONCURRENT_METER_SZ    = size_t{1} << 20; 

//             if (std::clamp(tentative_concurrent_meter_sz, MIN_CONCURRENT_METER_SZ, MAX_CONCURRENT_METER_SZ) != tentative_concurrent_meter_sz){
//                 dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
//             }

//             size_t meter_arr_sz = stdx::ceil2(tentative_concurrent_meter_sz);
//             auto meter_arr      = std::make_unique<std::unique_ptr<MeterInterface>[]>(meter_arr_sz);

//             for (size_t i = 0u; i < meter_arr_sz; ++i){
//                 meter_arr[i] = ComponentFactory::get_meter();
//             }

//             return std::make_unique<RandomDistributedMeter>(std::move(meter_arr), meter_arr_sz);
//         }

//         static auto get_meter_log_worker(std::string device_id,
//                                          std::shared_ptr<MeterInterface> send_meter,
//                                          std::shared_ptr<MeterInterface> recv_meter,
//                                          std::shared_ptr<MessageStreamerInterface> msg_stream,
//                                          std::chrono::nanoseconds meter_dur) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{
            
//             const size_t MIN_DEVICE_ID_SZ                   = size_t{0u};
//             const size_t MAX_DEVICE_ID_SZ                   = size_t{1} << 10;
//             const std::chrono::nanoseconds MIN_METER_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::microseconds(1));
//             const std::chrono::nanoseconds MAX_METER_DUR    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

//             if (std::clamp(static_cast<size_t>(device_id.size()), MIN_DEVICE_ID_SZ, MAX_DEVICE_ID_SZ) != device_id.size()){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (send_meter == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (recv_meter == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (msg_stream == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (std::clamp(meter_dur, MIN_METER_DUR, MAX_METER_DUR) != meter_dur){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             return std::make_unique<MeterLogWorker>(ComponentFactory::to_dg_string(device_id), 
//                                                     std::move(send_meter), 
//                                                     std::move(recv_meter),
//                                                     std::move(msg_stream),
//                                                     meter_dur);
//         }

//         static auto get_metered_mailbox(std::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec,
//                                         std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
//                                         std::shared_ptr<MeterInterface> send_meter,
//                                         std::shared_ptr<MeterInterface> recv_meter) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

//             if (daemon_vec.empty()){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (base == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (send_meter == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             if (recv_meter == nullptr){
//                 dg::network_exception::throw_exception(dg::network_exception::INTERNAL_CORRUPTION);
//             }

//             return std::make_unique<MeteredMailBox>(ComponentFactory::to_dg_vector(std::move(daemon_vec)),
//                                                     std::move(base),
//                                                     std::move(send_meter),
//                                                     std::move(recv_meter));
//         }
//     };

//     struct Config{
//         std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base;
//         uint32_t concurrent_recv_meter_sz;
//         uint32_t concurrent_send_meter_sz;
//         std::string device_id;
//         std::shared_ptr<MessageStreamerInterface> msg_streamer;
//         std::chrono::nanoseconds meter_dur;
//     };

//     struct ConfigMaker{
        
//         private:

//             static auto make_recv_meter(Config config) -> std::unique_ptr<MeterInterface>{
                
//                 if (config.concurrent_recv_meter_sz == 0u){
//                     dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
//                 }

//                 if (config.concurrent_recv_meter_sz == 1u){
//                     return ComponentFactory::get_meter();
//                 }

//                 return ComponentFactory::get_distributed_meter(config.concurrent_recv_meter_sz);
//             }

//             static auto make_send_meter(Config config) -> std::unique_ptr<MeterInterface>{

//                 if (config.concurrent_send_meter_sz == 0u){
//                     dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
//                 }

//                 if (config.concurrent_send_meter_sz == 1u){
//                     return ComponentFactory::get_meter();
//                 }

//                 return ComponentFactory::get_distributed_meter(config.concurrent_send_meter_sz);
//             }

//             static auto make_workers(Config config, 
//                                      std::shared_ptr<MeterInterface> send_meter, 
//                                      std::shared_ptr<MeterInterface> recv_meter) -> std::vector<dg::network_concurrency::daemon_raii_handle_t>{

//                 std::unique_ptr<dg::network_concurrency::WorkerInterface> worker = ComponentFactory::get_meter_log_worker(config.device_id, send_meter, recv_meter, config.msg_streamer, config.meter_dur);

//                 auto daemon_vec     = std::vector<dg::network_concurrency::daemon_raii_handle_t>();
//                 auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::HEARTBEAT_DAEMON, std::move(worker)));
//                 daemon_vec.emplace_back(std::move(daemon_handle));

//                 return daemon_vec;
//             }

//         public:

//             static auto make(Config config) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

//                 std::shared_ptr<MeterInterface> send_meter  = ConfigMaker::make_send_meter(config);
//                 std::shared_ptr<MeterInterface> recv_meter  = ConfigMaker::make_recv_meter(config);

//                 return ComponentFactory::get_metered_mailbox(ConfigMaker::make_workers(config, send_meter, recv_meter),
//                                                              std::move(config.base),
//                                                              send_meter,
//                                                              recv_meter);
//             }
//     };

//     extern auto make(Config config) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

//         return ConfigMaker::make(std::move(config));
//     }
// }

namespace dg::network_kernel_mailbox_impl1_flash_streamx{

    struct Signature{};
    struct Signature1{};
    struct Signature2{};
    struct Signature3{};

    //this is too hard to solve but I'd try to quantify the problem as followed
    //in system design, we'd want to cancel, transmit, guarantee of deliveries

    //let's first define our problems so we'd have to solve it
    //assume uniform bandwidth in our system (such is A - B bandwidth is uniform for all edges) 

    //in short, let's assume that our retransmission queue is of size 1, such is that we put 1 packet in the queue, we wait for the packet to be acked, we'd continue with another packet
    //we dont have problems

    //so we increase the number of packets in the queue to 2, such is that ..., we dont have problem
    //we increase the number of packets in queue to n, such is that there is a problem of bandwidth or consumption or packet id counter reached or etc.

    //the number n is binary-searchable, configurable and fixed, because our environment is known and uniform
    //so we don't have any problems with networks, assume that its for pure consumption and dump the buffer thereafter (which we'd put in a filesystem, which is another topic to talk about)

    //assume in a perfect conditions, our system is flawless, such is all packets sent == all packets received and there is no congestion whatsoever, we are very super happy
    //how about we'd want to abort some of the operating tasks or transmissions?
    //so its compromised at the function call level, such is that the compromission time is dispatch_sz / pipe_sz * retryable_times * time_per_retry, so if you call the functions for 1 millions items, with a 1-item pipe, it's a guaranteed NO-NO, deadlock for you
    //and that's an expected functionality in system design, not a flaw of function call compromission, for the function must deliver the arguing packets 

    //I was working on the proof of concept that this could work very well, in the sense of 100% delivery

    //remember that we were offloading the responsibility of delivery to the sender, not the sendee
    //also that we are on finite resources, especially our cyclic_unordered_node_set or cyclic_unordered_node_map
    //which is governed by the traffic controller, such is that the packet flux is kept under a certain velocity so that the retransmission cannot have duplicate packets because of discarded ids

    //so we are OK for the mailbox_impl1 implementation
    //how about this implementation?

    //remember that we are counting the lifetime based on the cyclic queue size, such is that the lifetime of an enqueued node is at least queue_sz
    //in the worst case scenerio, the packet delivery is so fragmented that every slot has exactly one segment, which is never going to be the case
    //so we are in the average case scenerio, where the fragmented packet rate is probably 1%

    //we just need to prove that in the average case scenerio, the packet lifetime outlives the retransmission lifetime
    //there is a formula for this, but I can guarantee that this realistically systematically reliably can deliver fixed size segments 

    //I was asked to connect with the peers to not do retransmission, but that's logically flawed as we need to retransmit the connection as well
    //and it does not improve the worst case scenerio also (which is what we actually care about)

    //so it's best to leave the responsibility to the callee, such is that we would have to inject the commands to abort the remaining queued packets 

    //real life system is very messy, so we can only rely on counts, semaphore of transmission and we'd hope that we'd dump or forward the received packets as fast as possible
    //we have to also guarantee that all pipes at full capacity is fair (in the network sense, such is discard_on_delivery), which is the purpose of all this

    //remember that our novel of packet transmission is based on the round trip time, such is max velocity is queue_sz / round_trip_time, min velocity is queue_sz / retransmission_time
    //this, in real life, is very applicable, so we can build on top of this

    //it's already incredibly hard to have this worked for a particular scenerio, we can't generalize the non-uniformity or other assumptions, we can, by building on top of the mailbox_impl1, but that's another topic to talk about
    //we'd settle this being the solution for flash_streamx, until we've found another solution 
    //we have also implemented a retry-system for our upper layer protocol, so I think that this is beyond sufficient in the sense of reliability

    using MemoryConfig                      = dg::network_kernel_allocator_singleton::Config;
    using HugeAllocatorInstance             = dg::network_kernel_allocator_singleton::AllocatorInstance<Signature>;
    using VectorAllocatorInstance           = dg::network_kernel_allocator_singleton::AllocatorInstance<Signature1>;

    template <class T>
    using HugeStdWrappedAllocator           = dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, HugeAllocatorInstance>;

    template <class T>
    using VectorStdWrappedAllocator         = dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, VectorAllocatorInstance>;

    using SegmentAllocatorInstance          = dg::network_kernel_allocator_singleton::AllocatorInstance<Signature2>;

    template <class T>
    using SegmentStdWrappedAllocator        = dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, SegmentAllocatorInstance>;

    using UrgentAllocatorInstance           = dg::network_kernel_allocator_singleton::AllocatorInstance<Signature3>;
    
    template <class T>
    using UrgentStdWrappedAllocator         = dg::network_kernel_allocator_singleton::StdWrappedAllocator<T, UrgentAllocatorInstance>;

    void init_memory(MemoryConfig big_packet_config,
                     MemoryConfig segment_packet_config,
                     MemoryConfig urgent_packet_config)
    {
        HugeAllocatorInstance::init(big_packet_config);
        VectorAllocatorInstance::init(big_packet_config);
        SegmentAllocatorInstance::init(segment_packet_config);
        UrgentAllocatorInstance::init(urgent_packet_config);
    }

    void deinit_memory() noexcept
    {
        HugeAllocatorInstance::deinit();
        SegmentAllocatorInstance::deinit();
        UrgentAllocatorInstance::deinit();
    }

    using internal_huge_kernel_buffer       = dg::network_kernel_buffer::kernel_string<HugeAllocatorInstance>;
    using internal_segment_kernel_buffer    = dg::network_kernel_buffer::kernel_string<SegmentAllocatorInstance>; 

    template <class T>
    using internal_vector                   = std::vector<T, VectorStdWrappedAllocator<T>>;

    template <class T, class ...Args>
    auto urgent_make_shared(Args&& ...args){

        return std::allocate_shared<T>(UrgentStdWrappedAllocator<T>{}, std::forward<Args>(args)...);
    } 

    using Address                           = dg::network_kernel_mailbox_impl1::model::Address; 
    using MailBoxArgument                   = dg::network_kernel_mailbox_impl1::model::MailBoxArgument;

    static inline constexpr size_t DEFAULT_KEYVALUE_FEED_SIZE               = size_t{1} << 10; 
    static inline constexpr size_t DEFAULT_KEY_FEED_SIZE                    = size_t{1} << 8;
    static inline constexpr uint32_t PACKET_SEGMENT_SERIALIZATION_SECRET    = 3036322422ULL; //we randomize the secret within the uint32_t range to make sure that we dont have internal corruptions
    static inline constexpr uint32_t PACKET_INTEGRITY_SECRET                = 2203221141ULL;

    //OK
    struct GlobalIdentifier{
        Address addr;
        std::pair<uint64_t, uint64_t> local_id; //avoid __uint128_t

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const{
            reflector(addr, local_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector){
            reflector(addr, local_id);
        }
    };

    //OK
    struct PacketSegment{
        internal_segment_kernel_buffer buf;
        GlobalIdentifier id;
        uint64_t segment_idx;
        uint64_t segment_sz;

        uint64_t mm_integrity_value;
        bool has_mm_integrity_value;
    };

    //OK
    struct AssembledPacket{
        internal_vector<std::optional<internal_segment_kernel_buffer>> data;
        size_t collected_segment_sz;
        size_t total_segment_sz;
        uint64_t mm_integrity_value;
        bool has_mm_integrity_value;
    };

    //OK
    struct FairBufferContainerWaitingItem
    {
        dg::vector<internal_huge_kernel_buffer> buffer_vec;
        std::binary_semaphore * smp;
    };

    //OK
    struct BufferContainerWaitingItem
    {
        std::move_iterator<internal_huge_kernel_buffer *> buffer_arr;
        size_t buffer_arr_sz;
        std::binary_semaphore * smp;
    };

    //OK
    struct EntranceControllerWaitingItem
    {
        GlobalIdentifier * id_arr;
        size_t id_arr_sz;
        std::binary_semaphore * release_smp;
    };

    //OK
    static auto serialize_packet_segment(PacketSegment&& segment) noexcept -> std::expected<internal_segment_kernel_buffer, exception_t>{

        using header_t      = std::tuple<GlobalIdentifier, uint64_t, uint64_t, uint64_t, bool>; //

        size_t header_sz    = dg::network_compact_trivial_serializer::size(header_t{});
        size_t old_sz       = segment.buf.size();
        size_t new_sz       = old_sz + header_sz;
        auto header         = header_t{segment.id, segment.segment_idx, segment.segment_sz, segment.mm_integrity_value, segment.has_mm_integrity_value}; //

        try{
            segment.buf.resize(new_sz);
            dg::network_compact_trivial_serializer::serialize_into(std::next(segment.buf.data(), old_sz), header, PACKET_SEGMENT_SERIALIZATION_SECRET);
            return std::expected<internal_segment_kernel_buffer, exception_t>(std::move(segment.buf));
        } catch (...){
            return std::unexpected(dg::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    //OK
    static auto deserialize_packet_segment(internal_segment_kernel_buffer&& buf) noexcept -> std::expected<PacketSegment, exception_t>{

        using header_t      = std::tuple<GlobalIdentifier, uint64_t, uint64_t, uint64_t, bool>; //

        size_t header_sz    = dg::network_compact_trivial_serializer::size(header_t{});
        size_t buf_sz       = buf.size();
        auto header         = header_t{}; 

        if (buf_sz < header_sz){
            return std::unexpected(dg::network_exception::SOCKET_STREAM_BAD_SEGMENT);           
        }

        size_t hdr_off      = buf.size() - header_sz;
        char * hdr_buf      = std::next(buf.data(), hdr_off);  

        exception_t err     = dg::network_exception::to_cstyle_function(dg::network_compact_trivial_serializer::deserialize_into<header_t>)(header, hdr_buf, header_sz, PACKET_SEGMENT_SERIALIZATION_SECRET);

        if (dg::network_exception::is_failed(err)){
            return std::unexpected(err);
        }

        buf.resize(hdr_off);

        PacketSegment rs    = {};
        rs.buf              = std::move(buf);
        std::tie(rs.id, rs.segment_idx, rs.segment_sz, rs.mm_integrity_value, rs.has_mm_integrity_value) = header; //

        return rs;
    }

    //OK
    static auto internal_assembled_packet_to_buffer(AssembledPacket&& pkt) noexcept -> std::expected<internal_huge_kernel_buffer, exception_t>{

        if (pkt.total_segment_sz != pkt.collected_segment_sz){
            return std::unexpected(dg::network_exception::SOCKET_STREAM_CORRUPTED_PACKET);
        }

        if (pkt.total_segment_sz != pkt.data.size()){
            return std::unexpected(dg::network_exception::SOCKET_STREAM_CORRUPTED_PACKET);
        }

        if (pkt.total_segment_sz == 0u){
            return internal_huge_kernel_buffer();
        }

        if (pkt.total_segment_sz == 1u){
            if (!pkt.data.front().has_value()){
                return std::unexpected(dg::network_exception::SOCKET_STREAM_CORRUPTED_PACKET);
            }

            return std::expected<internal_huge_kernel_buffer, exception_t>(std::move(pkt.data.front().value()));
        }

        size_t total_bsz = 0u;

        for (size_t i = 0u; i < pkt.total_segment_sz; ++i){
            if (!pkt.data[i].has_value()){
                return std::unexpected(dg::network_exception::SOCKET_STREAM_CORRUPTED_PACKET);
            }

            total_bsz += pkt.data[i]->size();
        }

        std::expected<internal_huge_kernel_buffer, exception_t> rs = dg::network_exception::cstyle_initialize<internal_huge_kernel_buffer>(total_bsz);

        if (!rs.has_value()){
            return std::unexpected(rs.error());
        }

        char * out_it = rs.value().data(); 

        for (size_t i = 0u; i < pkt.total_segment_sz; ++i){
            out_it = std::copy(pkt.data[i]->begin(), pkt.data[i]->end(), out_it);
        }

        return rs;
    }

    //OK
    static auto internal_integrity_assembled_packet_to_buffer(AssembledPacket&& pkt) noexcept -> std::expected<internal_huge_kernel_buffer, exception_t>{

        uint64_t mm_integrity_value                                 = pkt.mm_integrity_value;
        bool has_mm_integrity_value                                 = pkt.has_mm_integrity_value;
        std::expected<internal_huge_kernel_buffer, exception_t> rs  = internal_assembled_packet_to_buffer(static_cast<AssembledPacket&&>(pkt));

        if (!rs.has_value()){
            return std::unexpected(rs.error());
        }

        if (has_mm_integrity_value){
            uint64_t integrity_value = dg::network_hash::murmur_hash(rs->data(), rs->size(), PACKET_INTEGRITY_SECRET);

            if (mm_integrity_value != integrity_value){
                return std::unexpected(dg::network_exception::SOCKET_STREAM_CORRUPTED_PACKET);
            }
        }

        return rs;
    }

    //OK
    template <class T>
    struct UnitAllocatorInterface{
        virtual ~UnitAllocatorInterface() noexcept = default;
        virtual auto get() noexcept -> std::expected<T, exception_t> = 0;
        virtual void free(T item) noexcept = 0;
    };

    //OK
    struct ExhaustionControllerInterface{
        virtual ~ExhaustionControllerInterface() noexcept = default;
        virtual auto is_should_wait() noexcept -> bool = 0;
        virtual auto update_waiting_size(size_t) noexcept -> exception_t = 0;
    };

    //OK
    struct PacketIDGeneratorInterface{
        virtual ~PacketIDGeneratorInterface() noexcept = default;
        virtual auto get_id() noexcept -> GlobalIdentifier = 0;
    };

    //OK
    struct ProducerDrainerPredicateInterface{
        virtual ~ProducerDrainerPredicateInterface() noexcept = default;
        virtual auto is_should_drain() noexcept -> bool = 0;
        virtual void reset() noexcept = 0;
    };

    //OK
    struct PacketizerInterface{
        virtual ~PacketizerInterface() noexcept = default;
        virtual auto packetize(internal_segment_kernel_buffer&&) noexcept -> std::expected<internal_vector<PacketSegment>, exception_t> = 0;
        virtual auto segment_byte_size() const noexcept -> size_t = 0;
        virtual auto max_packet_size() const noexcept -> size_t = 0;
        virtual auto max_segment_count() const noexcept -> size_t = 0;
    };

    //OK
    struct InBoundGateInterface{
        virtual ~InBoundGateInterface() noexcept = default;
        virtual void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    //OK
    struct BlackListGateInterface: virtual InBoundGateInterface{
        virtual ~BlackListGateInterface() noexcept = default;
        virtual void blacklist(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept = 0;
    };

    //OK
    struct EntranceControllerInterface{
        virtual ~EntranceControllerInterface() noexcept = default;
        virtual void tick(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept = 0; 
        virtual void get_expired_id(GlobalIdentifier * output_arr, size_t& sz, size_t cap) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    //OK
    struct PacketAssemblerInterface{
        virtual ~PacketAssemblerInterface() noexcept = default;
        virtual void assemble(std::move_iterator<PacketSegment *> segment_arr, size_t sz, std::expected<AssembledPacket, exception_t> * assembled_arr) noexcept = 0; //it seems like this breaks the rule of unexpected == no_consume, we have to alter the semantic, exception_t is only for exceptions, we dont really know if std::optional<> is worth it
        virtual void destroy(GlobalIdentifier * id_arr, size_t sz) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    //OK
    struct ContainerBusyAdapterInterface{
        virtual ~ContainerBusyAdapterInterface() noexcept = default;
        virtual auto set_busy() noexcept -> exception_t = 0;
        virtual auto set_ease() noexcept -> exception_t = 0;
    };

    //OK
    struct NetworkBusyObserverInterface{
        using busy_level_t = uint8_t;
        
        static inline constexpr busy_level_t BUSY_0 = 0u;
        static inline constexpr busy_level_t BUSY_1 = 1u;
        static inline constexpr busy_level_t BUSY_2 = 2u;

        virtual ~NetworkBusyObserverInterface() noexcept = default;
        virtual void notify(busy_level_t) noexcept = 0;
    };

    //OK
    struct NetworkBusyStatusRetrieverInterface{
        using busy_level_t = uint8_t;

        static inline constexpr busy_level_t BUSY_0  = 0u;
        static inline constexpr busy_level_t BUSY_1  = 1u;
        static inline constexpr busy_level_t BUSY_2  = 2u; 

        virtual ~NetworkBusyStatusRetrieverInterface() noexcept = default;
        virtual auto get() noexcept -> std::expected<busy_level_t, exception_t> = 0;
    };

    //OK
    struct InBoundContainerInterface{
        virtual ~InBoundContainerInterface() noexcept = default;
        virtual void push(std::move_iterator<internal_huge_kernel_buffer *> arr, size_t sz, exception_t * exception_arr) noexcept = 0;
        virtual void pop(internal_huge_kernel_buffer * output_arr, size_t& sz, size_t cap) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    //OK
    struct OutBoundRuleInterface{
        virtual ~OutBoundRuleInterface() noexcept = default;
        virtual auto thru(const Address&) noexcept -> std::expected<bool, exception_t> = 0;
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

    class SockTrafficAdapter : public virtual NetworkBusyStatusRetrieverInterface{

        public:

            using busy_level_t = NetworkBusyStatusRetrieverInterface::busy_level_t;

            auto get() noexcept -> std::expected<busy_level_t, exception_t>{

                using traffic_t = dg::network_sock_traffic_status_controller::types::traffic_status_t;

                traffic_t inbound_traffic_status    = dg::network_sock_traffic_status_controller::get_inbound_status();
                traffic_t outbound_traffic_status   = dg::network_sock_traffic_status_controller::get_outbound_status();

                if (inbound_traffic_status == dg::network_sock_traffic_status_controller::constants::congested
                    && outbound_traffic_status == dg::network_sock_traffic_status_controller::constants::congested){

                    return NetworkBusyStatusRetrieverInterface::BUSY_2;
                }

                return NetworkBusyStatusRetrieverInterface::BUSY_0;
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
    class EmptyOutBoundRule: public virtual OutBoundRuleInterface{

        public:

            auto thru(const Address&) noexcept -> std::expected<bool, exception_t>{

                return true;
            }
    };

    //OK
    template <class Key, class Value>
    class temporal_finite_unordered_map{

        private:

            dg::cyclic_unordered_node_map<Key, Value> map_container;

        public:

            temporal_finite_unordered_map(size_t tentative_cap): map_container(dg::cyclic_unordered_node_map<Key, Value>::size_to_capacity(tentative_cap)){}

            template <class KeyLike>
            constexpr auto find(const KeyLike& keylike) noexcept{

                return this->map_container.find(keylike);
            }

            template <class KeyLike>
            constexpr auto find(const KeyLike& keylike) const noexcept{

                return this->map_container.find(keylike);
            }

            constexpr auto begin() noexcept{

                return this->map_container.begin();
            }

            constexpr auto begin() const noexcept{

                return this->map_container.begin();
            }

            constexpr auto end() noexcept{
                
                return this->map_container.end();
            }

            constexpr auto end() const noexcept{

                return this->map_container.end();
            }

            template <class ...Args>
            constexpr auto emplace(Args&& ...args) -> decltype(auto){

                return this->map_container.emplace(std::forward<Args>(args)...);
            }

            template <class ...Args>
            constexpr auto try_emplace(Args&&... args) -> decltype(auto){

                return this->map_container.try_emplace(std::forward<Args>(args)...);
            }
    };

    //OK
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

    //OK
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

            temporal_switching_bloom_filter(size_t cap, size_t rehash_sz, size_t reliability_decay): left_bloom_filter(std::max(size_t{1}, cap), rehash_sz),
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

    //OK
    template <class Key>
    class temporal_finite_unordered_set
    {
        private:

            dg::cyclic_unordered_node_set<Key> set_container;
        
        public:

            temporal_finite_unordered_set(size_t tentative_cap): set_container(tentative_cap * 2u){}

            auto not_contains(const Key& key) const noexcept -> bool
            {
                return this->set_container.contains(key);
            }

            void insert(const Key& key) noexcept
            {
                this->set_container.insert(key);
            }
    };

    //OK
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

    //OK
    class ComplexReactor{

        private:

            dg::unordered_unstable_map<std::thread::id, std::shared_ptr<dg_binary_semaphore>> mtx_queue;
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

                std::shared_ptr<dg_binary_semaphore> waiting_smp = dg::network_kernel_mailbox_impl1_flash_streamx::urgent_make_shared<dg_binary_semaphore>(0);

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

            inline __attribute__((force_inline)) void do_release(dg::unordered_unstable_map<std::thread::id, std::shared_ptr<dg_binary_semaphore>>& smp_vec){

                for (const auto& kv_pair: smp_vec){
                    kv_pair.second->release();
                }

                smp_vec.clear();
            }
    };

    //OK
    class PacketIDGenerator: public virtual PacketIDGeneratorInterface{

        private:

            Address factory_addr;
            std::atomic<uint64_t> incrementing_id;

        public:

            PacketIDGenerator(Address factory_addr,
                              uint64_t incrementing_id) noexcept: factory_addr(std::move(factory_addr)),
                                                                  incrementing_id(incrementing_id){}

            auto get_id() noexcept -> GlobalIdentifier{

                GlobalIdentifier id = {};
                id.addr             = this->factory_addr;
                id.local_id         = std::make_pair(this->incrementing_id.fetch_add(1u, std::memory_order_relaxed), uint64_t{});

                return id;
            }
    };

    //OK
    class RandomPacketIDGenerator: public virtual PacketIDGeneratorInterface{

        private:

            Address factory_addr;
        
        public:

            RandomPacketIDGenerator(Address factory_addr) noexcept: factory_addr(std::move(factory_addr)){}

            auto get_id() noexcept -> GlobalIdentifier{

                GlobalIdentifier id = {};
                id.addr             = this->factory_addr;
                id.local_id         = std::make_pair(dg::network_randomizer::randomize_int<uint64_t>(), dg::network_randomizer::randomize_int<uint64_t>()); //increase locality of packet_id to help with the memory pattern (maybe or maybe not)

                return id;
            }
    };

    //OK
    class TemporalAbsoluteTimeoutInBoundGate: public virtual InBoundGateInterface{

        private:

            temporal_finite_unordered_map<GlobalIdentifier, std::chrono::time_point<std::chrono::steady_clock>> abstimeout_map;
            const std::chrono::nanoseconds abs_timeout_dur;
            const size_t ticking_clock_resolution;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> thru_sz_per_load;

        public:

            TemporalAbsoluteTimeoutInBoundGate(temporal_finite_unordered_map<GlobalIdentifier, std::chrono::time_point<std::chrono::steady_clock>> abstimeout_map,
                                               std::chrono::nanoseconds abs_timeout_dur,
                                               size_t ticking_clock_resolution,
                                               std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                               stdx::hdi_container<size_t> thru_sz_per_load) noexcept: abstimeout_map(std::move(abstimeout_map)),
                                                                                                       abs_timeout_dur(abs_timeout_dur),
                                                                                                       ticking_clock_resolution(ticking_clock_resolution),
                                                                                                       mtx(std::move(mtx)),
                                                                                                       thru_sz_per_load(std::move(thru_sz_per_load)){}

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                auto ticking_steady_clock = dg::ticking_clock<std::chrono::steady_clock>(this->ticking_clock_resolution); 

                for (size_t i = 0u; i < sz; ++i){
                    auto map_ptr = this->abstimeout_map.find(global_id_arr[i]);

                    if (map_ptr == this->abstimeout_map.end()){
                        auto timeout_value = ticking_steady_clock.get() + this->abs_timeout_dur;
                        auto [emplace_ptr, status] = this->abstimeout_map.try_emplace(global_id_arr[i], timeout_value);
                        dg::network_exception_handler::dg_assert(status);
                        exception_arr[i] = dg::network_exception::SUCCESS; 
                        continue;
                    }

                    if (map_ptr->second <= ticking_steady_clock.get()){
                        exception_arr[i] = dg::network_exception::SOCKET_STREAM_TIMEOUT;
                        continue;                        
                    }

                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->thru_sz_per_load.value;
            }        
    };

    //OK
    class RandomHashDistributedInBoundGate: public virtual InBoundGateInterface{

        private:

            std::unique_ptr<std::unique_ptr<InBoundGateInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t keyvalue_feed_cap;
            size_t thru_sz_per_load;

        public:

            RandomHashDistributedInBoundGate(std::unique_ptr<std::unique_ptr<InBoundGateInterface>[]> base_arr,
                                             size_t pow2_base_arr_sz,
                                             size_t keyvalue_feed_cap,
                                             size_t thru_sz_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                thru_sz_per_load(thru_sz_per_load){}

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto feed_resolutor                 = InternalFeedResolutor{};
                feed_resolutor.dst                  = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_value         = dg::network_hash::hash_reflectible(global_id_arr[i]);
                    size_t partitioned_idx      = hashed_value & (this->pow2_base_arr_sz - 1u);
                    auto feed_arg               = InternalFeedArgument{};
                    feed_arg.id                 = global_id_arr[i];
                    feed_arg.bad_exception_ptr  = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->thru_sz_per_load;
            }

        private:

            struct InternalFeedArgument{
                GlobalIdentifier id;
                exception_t * bad_exception_ptr;
            };

            struct InternalFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalFeedArgument>{

                std::unique_ptr<InBoundGateInterface> * dst;

                void push(const size_t& idx, std::move_iterator<InternalFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id;
                    }

                    this->dst[idx]->thru(global_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].bad_exception_ptr = exception_arr[i];
                        }
                    }
                }
            };
    };

    class VoidInboundGate: public virtual InBoundGateInterface
    {
        public:

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            auto max_consume_size() noexcept -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }
    };

    //OK
    class TemporalBlackListGate: public virtual BlackListGateInterface{

        private:

            temporal_switching_bloom_filter<GlobalIdentifier> blacklist_set;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> thru_sz_per_load;

        public:

            TemporalBlackListGate(temporal_switching_bloom_filter<GlobalIdentifier> blacklist_set,
                                  std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                  stdx::hdi_container<size_t> thru_sz_per_load) noexcept: blacklist_set(std::move(blacklist_set)),
                                                                                          mtx(std::move(mtx)),
                                                                                          thru_sz_per_load(thru_sz_per_load){}

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    if (this->blacklist_set.not_contains(global_id_arr[i])){
                        exception_arr[i] = dg::network_exception::SUCCESS;
                    } else{
                        exception_arr[i] = dg::network_exception::SOCKET_STREAM_MIGHT_BE_BLACKLISTED;
                    }
                }
            }

            void blacklist(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();        
                    }
                }

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    this->blacklist_set.insert(global_id_arr[i]);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->thru_sz_per_load.value;
            }
    };

    //OK
    class HighPrecisionTemporalBlackListGate: public virtual BlackListGateInterface{

        private:

            temporal_finite_unordered_set<GlobalIdentifier> blacklist_set;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> thru_sz_per_load;

        public:

            HighPrecisionTemporalBlackListGate(temporal_finite_unordered_set<GlobalIdentifier> blacklist_set,
                                               std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                               stdx::hdi_container<size_t> thru_sz_per_load) noexcept: blacklist_set(std::move(blacklist_set)),
                                                                                                       mtx(std::move(mtx)),
                                                                                                       thru_sz_per_load(thru_sz_per_load){}

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    if (this->blacklist_set.not_contains(global_id_arr[i])){
                        exception_arr[i] = dg::network_exception::SUCCESS;
                    } else{
                        exception_arr[i] = dg::network_exception::SOCKET_STREAM_MIGHT_BE_BLACKLISTED;
                    }
                }
            }

            void blacklist(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();        
                    }
                }

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    this->blacklist_set.insert(global_id_arr[i]);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->thru_sz_per_load.value;
            }
    };

    //OK
    class RandomHashDistributedBlackListGate: public virtual BlackListGateInterface{

        private:

            std::unique_ptr<std::unique_ptr<BlackListGateInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t keyvalue_feed_cap;
            size_t consume_sz_per_load;
        
        public:

            RandomHashDistributedBlackListGate(std::unique_ptr<std::unique_ptr<BlackListGateInterface>[]> base_arr,
                                               size_t pow2_base_arr_sz,
                                               size_t keyvalue_feed_cap,
                                               size_t consume_sz_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                     pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                     keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                     consume_sz_per_load(consume_sz_per_load){}

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                auto thru_feed_resolutor            = InternalThruFeedResolutor{};
                thru_feed_resolutor.dst             = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t thru_feeder_allocation_cost  = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&thru_feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> thru_feeder_mem(thru_feeder_allocation_cost);
                auto thru_feeder                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&thru_feed_resolutor, trimmed_keyvalue_feed_cap, thru_feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_value             = dg::network_hash::hash_reflectible(global_id_arr[i]);
                    size_t partitioned_idx          = hashed_value & (this->pow2_base_arr_sz - 1u);
                    auto thru_feed_arg              = InternalThruFeedArgument{};
                    thru_feed_arg.id                = global_id_arr[i];
                    thru_feed_arg.bad_exception_ptr = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(thru_feeder.get(), partitioned_idx, thru_feed_arg);
                }
            }

            void blacklist(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto blacklist_feed_resolutor           = InternalBlackListFeedResolutor{};
                blacklist_feed_resolutor.dst            = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap        = std::min(this->keyvalue_feed_cap, sz);
                size_t blacklist_feeder_allocation_cost = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&blacklist_feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> blacklist_feeder_mem(blacklist_feeder_allocation_cost);
                auto blacklist_feeder                   = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&blacklist_feed_resolutor, trimmed_keyvalue_feed_cap, blacklist_feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u ; i < sz; ++i){
                    size_t hashed_value                     = dg::network_hash::hash_reflectible(global_id_arr[i]);
                    size_t partitioned_idx                  = hashed_value & (this->pow2_base_arr_sz - 1u);
                    auto blacklist_feed_arg                 = InternalBlackListFeedArgument{};
                    blacklist_feed_arg.id                   = global_id_arr[i];
                    blacklist_feed_arg.bad_exception_ptr    = std::next(exception_arr, i); 

                    dg::network_producer_consumer::delvrsrv_kv_deliver(blacklist_feeder.get(), partitioned_idx, blacklist_feed_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
        
        private:

            struct InternalThruFeedArgument{
                GlobalIdentifier id;
                exception_t * bad_exception_ptr;
            };

            struct InternalThruFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalThruFeedArgument>{

                std::unique_ptr<BlackListGateInterface> * dst;

                void push(const size_t& idx, std::move_iterator<InternalThruFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalThruFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id;
                    }

                    this->dst[idx]->thru(global_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].bad_exception_ptr = exception_arr[i];
                        }
                    }
                }
            };

            struct InternalBlackListFeedArgument{
                GlobalIdentifier id;
                exception_t * bad_exception_ptr;
            };

            struct InternalBlackListFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalBlackListFeedArgument>{

                std::unique_ptr<BlackListGateInterface> * dst;

                void push(const size_t& idx, std::move_iterator<InternalBlackListFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalBlackListFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id;
                    }

                    this->dst[idx]->blacklist(global_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].bad_exception_ptr = exception_arr[i];
                        }
                    }
                }
            };
    };

    class VoidBlackListGate: public virtual BlackListGateInterface
    {
        public:

            void thru(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void blacklist(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            auto max_consume_size() noexcept -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }
    };

    //OK
    class Packetizer: public virtual PacketizerInterface{

        private:

            std::unique_ptr<PacketIDGeneratorInterface> packet_id_gen;
            const size_t segment_bsz_pow2_exponent;
            const size_t pow2_max_stream_bsz;
            bool has_mm_integrity;

        public:

            Packetizer(std::unique_ptr<PacketIDGeneratorInterface> packet_id_gen,
                       size_t segment_bsz_pow2_exponent,
                       size_t pow2_max_stream_bsz,
                       bool has_mm_integrity) noexcept: packet_id_gen(std::move(packet_id_gen)),
                                                        segment_bsz_pow2_exponent(segment_bsz_pow2_exponent),
                                                        pow2_max_stream_bsz(pow2_max_stream_bsz),
                                                        has_mm_integrity(has_mm_integrity){}

            auto packetize(internal_segment_kernel_buffer&& buf) noexcept -> std::expected<internal_vector<PacketSegment>, exception_t>{

                if (buf.size() > this->pow2_max_stream_bsz){
                    return std::unexpected(dg::network_exception::SOCKET_STREAM_BAD_BUFFER_LENGTH);
                }

                size_t segment_bsz      = size_t{1} << this->segment_bsz_pow2_exponent;
                size_t segment_even_sz  = buf.size() >> this->segment_bsz_pow2_exponent; //alright it seems silly but we have to not use division under every circumstances, a division operation is very costly on CPU, it is equivalent to a L2 cache fetch instruction (this will bring our program -> python speed)
                size_t segment_odd_sz   = size_t{(static_cast<size_t>(buf.size()) & (segment_bsz - 1u)) != 0u};
                size_t segment_sz       = segment_even_sz + segment_odd_sz;

                std::expected<internal_vector<PacketSegment>, exception_t> rs = dg::network_exception::cstyle_initialize<internal_vector<PacketSegment>>(std::max(size_t{1}, segment_sz));

                if (!rs.has_value()){
                    return std::unexpected(rs.error());
                }

                GlobalIdentifier pkt_stream_id = this->packet_id_gen->get_id();

                if (segment_sz == 0u){
                    PacketSegment segment           = {};
                    segment.buf                     = {};
                    segment.id                      = pkt_stream_id;
                    segment.segment_idx             = 0u; //this is logically incorrect, we are not protected by range bro anymore
                    segment.segment_sz              = 0u;
                    segment.mm_integrity_value      = this->integrity_encode(segment.buf, this->has_mm_integrity);
                    segment.has_mm_integrity_value  = this->has_mm_integrity;
                    rs.value()[0]                   = std::move(segment);

                    return rs;
                }

                if (segment_sz == 1u){ //premature very useful optimization, effectively skip 1 buffer iteration
                    PacketSegment segment           = {};
                    segment.buf                     = std::move(buf);
                    segment.id                      = pkt_stream_id;
                    segment.segment_idx             = 0u;
                    segment.segment_sz              = 1u;
                    segment.mm_integrity_value      = this->integrity_encode(segment.buf, this->has_mm_integrity);
                    segment.has_mm_integrity_value  = this->has_mm_integrity;
                    rs.value()[0]                   = std::move(segment);

                    return rs;
                }

                uint64_t integrity_value = this->integrity_encode(buf, this->has_mm_integrity); 

                for (size_t i = 0u; i < segment_sz; ++i){
                    size_t first                                    = segment_bsz * i;
                    size_t last                                     = std::min(static_cast<size_t>(segment_bsz * (i + 1)), static_cast<size_t>(buf.size())); 
                    PacketSegment segment                           = {};
                    segment.id                                      = pkt_stream_id;
                    segment.segment_idx                             = i;
                    segment.segment_sz                              = segment_sz;
                    segment.mm_integrity_value                      = integrity_value;
                    segment.has_mm_integrity_value                  = this->has_mm_integrity;
                    std::expected<internal_segment_kernel_buffer, exception_t> app_buf  = dg::network_exception::cstyle_initialize<internal_segment_kernel_buffer>((last - first)); 

                    if (!app_buf.has_value()){
                        return std::unexpected(app_buf.error()); //leaking ids
                    }

                    std::copy(std::next(buf.begin(), first), std::next(buf.begin(), last), app_buf.value().begin());

                    segment.buf                                     = std::move(app_buf.value());
                    rs.value()[i]                                   = std::move(segment);
                }

                return rs;
            }

            auto segment_byte_size() const noexcept -> size_t{

                return size_t{1} << this->segment_bsz_pow2_exponent;
            }

            auto max_packet_size() const noexcept -> size_t{

                return this->pow2_max_stream_bsz;
            }

            auto max_segment_count() const noexcept -> size_t{

                return std::max(size_t{1}, this->pow2_max_stream_bsz >> this->segment_bsz_pow2_exponent);
            }
        
        private:

            auto integrity_encode(std::string_view buf, bool has_integrity_encode) noexcept -> uint64_t{

                if (!has_integrity_encode){
                    return 0u;
                }

                return dg::network_hash::murmur_hash(buf.data(), buf.size(), PACKET_INTEGRITY_SECRET);
            }
    };

    //OK
    struct EntranceEntry{
        std::chrono::time_point<std::chrono::steady_clock> timestamp; //steady clock is a clock that never goes back in time, only true if we are on the same thread of execution, to avoid exotic error or errors that chance worse than RAM, we are to make sure that our queue state is accurate
        GlobalIdentifier key;
        __uint128_t entry_id;
    };

    //OK
    class EntranceController: public virtual EntranceControllerInterface{

        private:

            dg::pow2_cyclic_queue<EntranceControllerWaitingItem> tick_wait_queue;
            dg::pow2_cyclic_queue<EntranceEntry> entrance_entry_pq; //no exhausted container
            const size_t entrance_entry_pq_cap; 
            dg::unordered_unstable_map<GlobalIdentifier, __uint128_t> key_id_map; //no exhausted container
            __uint128_t id_ticker;
            const std::chrono::nanoseconds expiry_period;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> tick_sz_per_load;

        public:

            EntranceController(dg::pow2_cyclic_queue<EntranceControllerWaitingItem> tick_wait_queue,
                               dg::pow2_cyclic_queue<EntranceEntry> entrance_entry_pq,
                               size_t entrance_entry_pq_cap,
                               dg::unordered_unstable_map<GlobalIdentifier, __uint128_t> key_id_map,
                               __uint128_t id_ticker,
                               std::chrono::nanoseconds expiry_period,
                               std::unique_ptr<stdx::fair_atomic_flag> mtx,
                               stdx::hdi_container<size_t> tick_sz_per_load) noexcept: tick_wait_queue(std::move(tick_wait_queue)),
                                                                                       entrance_entry_pq(std::move(entrance_entry_pq)),
                                                                                       entrance_entry_pq_cap(entrance_entry_pq_cap),
                                                                                       key_id_map(std::move(key_id_map)),
                                                                                       id_ticker(id_ticker),
                                                                                       expiry_period(std::move(expiry_period)),
                                                                                       mtx(std::move(mtx)),
                                                                                       tick_sz_per_load(std::move(tick_sz_per_load)){}

            void tick(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION)); //exceeding clock + queue accuracy
                        std::abort();
                    }
                }

                std::binary_semaphore smp(0);
                bool need_wait;
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                {
                    stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                    auto now            = this->get_now();
                    size_t wait_offset  = sz; 

                    for (size_t i = 0u; i < sz; ++i){
                        if (this->entrance_entry_pq.size() == this->entrance_entry_pq_cap){
                            wait_offset = i; 
                            break;
                        }

                        auto entry      = EntranceEntry{};
                        entry.timestamp = now;
                        entry.key       = global_id_arr[i];
                        entry.entry_id  = this->id_ticker++;

                        dg::network_exception_handler::nothrow_log(this->entrance_entry_pq.push_back(entry));
                        this->key_id_map[global_id_arr[i]] = entry.entry_id;
                    }

                    if (wait_offset == sz)
                    {
                        need_wait = false;
                    }
                    else
                    {
                        need_wait = true;
                        dg::network_exception_handler::nothrow_log(this->tick_wait_queue.push_back(EntranceControllerWaitingItem
                        {
                            .id_arr         = std::next(global_id_arr, wait_offset),
                            .id_arr_sz      = sz - wait_offset,
                            .release_smp    = &smp
                        }));
                    }
                }

                if (need_wait)
                {
                    smp.acquire();
                }
            }

            //assume finite sorted queue of entrance_entry_pq
            //assume key_id_map points to the lastest GlobalIdentifier guy in the entrance_entry_pq

            void get_expired_id(GlobalIdentifier * output_arr, size_t& sz, size_t cap) noexcept{ //bad

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                auto bar_time   = std::chrono::steady_clock::now() - this->expiry_period;
                sz              = 0u;

                while (true){
                    if (sz == cap){
                        return;
                    }

                    if (this->entrance_entry_pq.empty()){
                        return;
                    }

                    if (this->entrance_entry_pq.front().timestamp > bar_time){ //bad
                        return;
                    }

                    EntranceEntry entry = this->entrance_entry_pq.front();
                    this->entrance_entry_pq.pop_front();
                    auto map_ptr        = this->key_id_map.find(entry.key); 

                    if constexpr(DEBUG_MODE_FLAG){
                        if (map_ptr == this->key_id_map.end()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    if (map_ptr->second == entry.entry_id){
                        output_arr[sz++] = entry.key;
                        this->key_id_map.erase(map_ptr);
                    }

                    this->fulfill_one_push_wait_queue();
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->tick_sz_per_load.value;
            }

        private:

            void fulfill_one_push_wait_queue() noexcept
            {
                if (this->tick_wait_queue.empty())
                {
                    return;
                }

                if (this->tick_wait_queue.front().id_arr_sz == 0u)
                {
                    std::abort();
                }

                auto entry      = EntranceEntry{};
                entry.timestamp = this->get_now();
                entry.key       = this->tick_wait_queue.front().id_arr[0];
                entry.entry_id  = this->id_ticker++;

                dg::network_exception_handler::nothrow_log(this->entrance_entry_pq.push_back(entry));
                this->key_id_map[entry.key] = entry.entry_id;
                
                this->tick_wait_queue.front() = EntranceControllerWaitingItem
                {
                    .id_arr         = std::next(this->tick_wait_queue.front().id_arr),
                    .id_arr_sz      = this->tick_wait_queue.front().id_arr_sz - 1,
                    .release_smp    = this->tick_wait_queue.front().release_smp
                };

                if (this->tick_wait_queue.front().id_arr_sz == 0u)
                {
                    this->tick_wait_queue.front().release_smp->release();
                    this->tick_wait_queue.pop_front();
                }
            }

            auto get_now() const noexcept -> std::chrono::time_point<std::chrono::steady_clock>{

                if (this->entrance_entry_pq.empty()){
                    return std::chrono::steady_clock::now();
                }

                return std::max(this->entrance_entry_pq.back().timestamp, std::chrono::steady_clock::now());
            }
    };

    //OK
    class RandomHashDistributedEntranceController: public virtual EntranceControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<EntranceControllerInterface>[]> base_arr;
            const size_t pow2_base_arr_sz;
            const size_t keyvalue_feed_cap;
            const size_t max_tick_per_load; 

        public:

            RandomHashDistributedEntranceController(std::unique_ptr<std::unique_ptr<EntranceControllerInterface>[]> base_arr,
                                                    size_t pow2_base_arr_sz,
                                                    size_t keyvalue_feed_cap,
                                                    size_t max_tick_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                        pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                        keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                        max_tick_per_load(max_tick_per_load){}

            void tick(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                auto feed_resolutor                 = InternalFeedResolutor{};
                feed_resolutor.dst                  = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feed_allocation_cost         = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feed_mem(feed_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feed_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_value     = dg::network_hash::hash_reflectible(global_id_arr[i]);
                    size_t partitioned_idx  = hashed_value & (this->pow2_base_arr_sz - 1u); //bad
                    auto feed_arg           = InternalFeedArgument{};
                    feed_arg.id             = global_id_arr[i];
                    feed_arg.failed_err_ptr = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void get_expired_id(GlobalIdentifier * output_arr, size_t& sz, size_t cap) noexcept{

                sz = 0u;
                size_t seed = dg::network_randomizer::randomize_int<size_t>() >> 1;  

                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
                    size_t rem_cap  = cap - sz;

                    if (rem_cap == 0u){
                        return;
                    }

                    size_t idx = (seed + i) & (this->pow2_base_arr_sz - 1u);
                    GlobalIdentifier * tmp_output_arr = std::next(output_arr, sz);
                    size_t tmp_output_arr_sz{};

                    this->base_arr[idx]->get_expired_id(tmp_output_arr, tmp_output_arr_sz, rem_cap);
                    sz += tmp_output_arr_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_tick_per_load;
            }
        
        private:

            struct InternalFeedArgument{
                GlobalIdentifier id;
                exception_t * failed_err_ptr;
            };

            struct InternalFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalFeedArgument>{

                std::unique_ptr<EntranceControllerInterface> * dst;

                void push(const size_t& idx, std::move_iterator<InternalFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id;
                    }

                    this->dst[idx]->tick(global_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].failed_err_ptr = exception_arr[i];
                        }
                    }
                }
            };
    };

    //OK
    class ExhaustionControlledEntranceController: public virtual EntranceControllerInterface{

        private:

            std::unique_ptr<EntranceControllerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledEntranceController(std::unique_ptr<EntranceControllerInterface> base,
                                                   std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                   std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                   executor(std::move(executor)),
                                                                                                                                   exhaustion_controller(std::move(exhaustion_controller)){}

            void tick(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept{

                GlobalIdentifier * first_id_ptr     = global_id_arr;
                GlobalIdentifier * last_id_ptr      = std::next(first_id_ptr, sz);
                exception_t * first_exception_ptr   = exception_arr;
                exception_t * last_exception_ptr    = std::next(exception_arr, sz);
                size_t sliding_window_sz            = sz;

                //let's try to grasp what's going on here
                //we are protected by range bro
                //assume we are to resolve every QUEUE_FULL, our state is correct at all time
                //we are to maintain the state of peeking the next sliding_window of QUEUE_FULL
                //such is we are to decay one state -> another state of lesser std::distance(first_id_ptr, last_id_ptr)

                auto task = [&, this]() noexcept{
                    this->base->tick(first_id_ptr, sliding_window_sz, first_exception_ptr);
                    size_t queueing_sz                          = std::count(first_exception_ptr, last_exception_ptr, dg::network_exception::QUEUE_FULL);
                    exception_t err                             = this->exhaustion_controller->update_waiting_size(queueing_sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    exception_t * first_retriable_exception_ptr = std::find(first_exception_ptr, last_exception_ptr, dg::network_exception::QUEUE_FULL);
                    exception_t * last_retriable_exception_ptr  = std::find_if(first_retriable_exception_ptr, last_exception_ptr, [](exception_t err){return err != dg::network_exception::QUEUE_FULL;});
                    sliding_window_sz                           = std::distance(first_retriable_exception_ptr, last_retriable_exception_ptr);
                    size_t relative_offset                      = std::distance(first_exception_ptr, first_retriable_exception_ptr);

                    std::advance(first_id_ptr, relative_offset);
                    std::advance(first_exception_ptr, relative_offset);

                    return !this->exhaustion_controller->is_should_wait() || first_id_ptr == last_id_ptr;
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(std::move(task));
                this->executor->exec(virtual_task);
            }

            void get_expired_id(GlobalIdentifier * output_arr, size_t& sz, size_t cap) noexcept{

                this->base->get_expired_id(output_arr, sz, cap);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    class VoidEntranceController: public virtual EntranceControllerInterface
    {
        public:

            void tick(GlobalIdentifier * global_id_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
            }

            void get_expired_id(GlobalIdentifier * output_arr, size_t& sz, size_t cap) noexcept
            {
                sz = 0u;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }
    };

    //the cyclic unordered node map philosophy of cache access and guarantee of lifetime is very important
    //such that when a node is enqueued at position sz, its lifetime is guaranteed to be at least sz
    //this does not mean that inbound gates and other measurements are not important, because it, first, increases the life expectancy overall, second, decrease memory usages in case of idle, even though we are using flash stream extension, which, we can safely assume that we are at 100% usage at all time, because the system is designed to be so
    //this, singlehandedly is the most important implementation of socket transportation

    //think of bandwidth and outdegree and preplanned, we need to specify the individual bandwidth in the instantiation of mailbox_impl1, and we'd need to make sure that all pipes at full capacity is a reasonable scenerio
    //we'd have to have 0 delay for all of the steps
    //this is the topic that we have discussed in a span of 2 years, we'd want to loosen the pipe when its busy and stricten the pipe when its not busy or we'd just constant ping our pipe at a reasonable velocity

    //OK
    class PacketAssembler: public virtual PacketAssemblerInterface{

        private:

            dg::cyclic_unordered_node_map<GlobalIdentifier, AssembledPacket> packet_map;
            size_t max_segment_sz_per_stream; 
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            PacketAssembler(dg::cyclic_unordered_node_map<GlobalIdentifier, AssembledPacket> packet_map,
                            size_t max_segment_sz_per_stream,
                            std::unique_ptr<stdx::fair_atomic_flag> mtx,
                            stdx::hdi_container<size_t> consume_sz_per_load) noexcept: packet_map(std::move(packet_map)),
                                                                                       max_segment_sz_per_stream(max_segment_sz_per_stream),
                                                                                       mtx(std::move(mtx)),
                                                                                       consume_sz_per_load(std::move(consume_sz_per_load)){}

            void assemble(std::move_iterator<PacketSegment *> segment_arr, size_t sz, std::expected<AssembledPacket, exception_t> * assembled_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);
                PacketSegment * base_segment_arr = segment_arr.base();

                for (size_t i = 0u; i < sz; ++i){
                    if (base_segment_arr[i].segment_sz == 0u){
                        assembled_arr[i] = this->make_empty_assembled_packet(0u,
                                                                             base_segment_arr[i].mm_integrity_value,
                                                                             base_segment_arr[i].has_mm_integrity_value);
                        continue;
                    }

                    auto map_ptr        = this->packet_map.find(base_segment_arr[i].id);
                    bool is_new_map_ptr = false;

                    if (map_ptr == this->packet_map.end()){
                        if (base_segment_arr[i].segment_sz > this->max_segment_sz_per_stream){
                            assembled_arr[i] = std::unexpected(dg::network_exception::SOCKET_STREAM_BAD_SEGMENT_SIZE);
                            continue;
                        }

                        std::expected<AssembledPacket, exception_t> waiting_pkt = this->make_empty_assembled_packet(base_segment_arr[i].segment_sz,
                                                                                                                    base_segment_arr[i].mm_integrity_value,
                                                                                                                    base_segment_arr[i].has_mm_integrity_value);

                        if (!waiting_pkt.has_value()){
                            assembled_arr[i] = std::unexpected(waiting_pkt.error());
                            continue;
                        }

                        auto [emplace_ptr, status]          = this->packet_map.try_emplace(base_segment_arr[i].id, std::move(waiting_pkt.value()));
                        dg::network_exception_handler::dg_assert(status);
                        map_ptr                             = emplace_ptr;
                        is_new_map_ptr                      = true;
                    }

                    exception_t err = this->internal_packet_segment_put(map_ptr->second, std::move(base_segment_arr[i]));

                    if (dg::network_exception::is_failed(err)){
                        if (is_new_map_ptr){ 
                            this->packet_map.erase(map_ptr);
                        }

                        assembled_arr[i] = std::unexpected(err);
                        continue;
                    }

                    if (map_ptr->second.collected_segment_sz == map_ptr->second.total_segment_sz){ 
                        assembled_arr[i] = std::move(map_ptr->second);
                        this->packet_map.erase(map_ptr);
                    } else{
                        assembled_arr[i] = std::unexpected(dg::network_exception::SOCKET_STREAM_SEGMENT_FILLING);
                    }
                }
            }

            void destroy(GlobalIdentifier * id_arr, size_t sz) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdx::xlock_guard<stdx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    auto map_ptr = this->packet_map.find(id_arr[i]);

                    if (map_ptr == this->packet_map.end()){
                        continue;
                    }

                    this->packet_map.erase(map_ptr);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load.value;
            }

        private:

            static auto internal_packet_segment_put(AssembledPacket& pkt,
                                                    PacketSegment&& pkt_segment) noexcept -> exception_t{

                if (pkt_segment.segment_idx >= pkt.data.size()){
                    return dg::network_exception::INDEX_OUT_OF_RANGE;
                }

                if (pkt.data[pkt_segment.segment_idx].has_value()){
                    return dg::network_exception::SOCKET_STREAM_DUPLICATE_SEGMENT;
                }

                pkt.data[pkt_segment.segment_idx]   = std::move(pkt_segment.buf);
                pkt.collected_segment_sz            += 1u;

                return dg::network_exception::SUCCESS;
            }

            auto make_empty_assembled_packet(size_t segment_sz,
                                             uint64_t mm_integrity_value,
                                             bool has_mm_integrity_value) noexcept -> std::expected<AssembledPacket, exception_t>{

                auto vec            = dg::network_exception::cstyle_initialize<internal_vector<std::optional<internal_segment_kernel_buffer>>>(segment_sz);

                if (!vec.has_value()){
                    return std::unexpected(vec.error());
                }

                size_t collected    = 0u;
                size_t total        = segment_sz;

                return AssembledPacket{.data                    = std::move(vec.value()),
                                       .collected_segment_sz    = collected,
                                       .total_segment_sz        = total,
                                       .mm_integrity_value      = mm_integrity_value,
                                       .has_mm_integrity_value  = has_mm_integrity_value};
            }
    };

    //OK
    class RandomHashDistributedPacketAssembler: public virtual PacketAssemblerInterface{

        private:

            std::unique_ptr<std::unique_ptr<PacketAssemblerInterface>[]> base_arr;
            const size_t pow2_base_arr_sz;
            const size_t keyvalue_feed_cap;
            const size_t consume_sz_per_load;

        public:

            RandomHashDistributedPacketAssembler(std::unique_ptr<std::unique_ptr<PacketAssemblerInterface>[]> base_arr,
                                                 size_t pow2_base_arr_sz,
                                                 size_t keyvalue_feed_cap,
                                                 size_t consume_sz_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                       pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                       keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                       consume_sz_per_load(consume_sz_per_load){}

            void assemble(std::move_iterator<PacketSegment *> segment_arr, size_t sz, std::expected<AssembledPacket, exception_t> * assembled_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                PacketSegment * base_segment_arr    = segment_arr.base();

                auto feed_resolutor                 = InternalAssembleFeedResolutor{};
                feed_resolutor.dst                  = this->base_arr.get(); 

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feed_allocation_cost         = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feed_mem(feed_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feed_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_value             = dg::network_hash::hash_reflectible(base_segment_arr[i].id);
                    size_t partitioned_idx          = hashed_value & (this->pow2_base_arr_sz - 1u);

                    auto feed_arg                   = InternalAssembleFeedArgument{}; //bad
                    feed_arg.segment                = std::move(base_segment_arr[i]);
                    feed_arg.fallback_segment_ptr   = std::next(base_segment_arr, i);
                    feed_arg.rs                     = std::next(assembled_arr, i); 

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, std::move(feed_arg));
                }
            }

            void destroy(GlobalIdentifier * id_arr, size_t sz) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto feed_resolutor                 = InternalDestroyFeedResolutor{};
                feed_resolutor.dst                  = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feed_allocation_cost         = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feed_mem(feed_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feed_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_value             = dg::network_hash::hash_reflectible(id_arr[i]);
                    size_t partitioned_idx          = hashed_value & (this->pow2_base_arr_sz - 1u);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, id_arr[i]);
                } 
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
        
        private:

            struct InternalAssembleFeedArgument{
                PacketSegment segment;
                PacketSegment * fallback_segment_ptr;
                std::expected<AssembledPacket, exception_t> * rs;
            };

            struct InternalAssembleFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalAssembleFeedArgument>{

                std::unique_ptr<PacketAssemblerInterface> * dst;

                void push(const size_t& idx, std::move_iterator<InternalAssembleFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalAssembleFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<PacketSegment[]> segment_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<AssembledPacket, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        segment_arr[i] = std::move(base_data_arr[i].segment);
                    }

                    this->dst[idx]->assemble(std::make_move_iterator(segment_arr.get()), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (rs_arr[i].has_value()){
                            *base_data_arr[i].rs = std::move(rs_arr[i]); //thru
                            continue;   
                        }

                        if (rs_arr[i].error() == dg::network_exception::SOCKET_STREAM_SEGMENT_FILLING){
                            *base_data_arr[i].rs = std::move(rs_arr[i]); //thru
                            continue;
                        }

                        //fallback, move semantic does not apply to failed exception_t
                        *base_data_arr[i].rs                    = std::move(rs_arr[i]);
                        *base_data_arr[i].fallback_segment_ptr  = std::move(segment_arr[i]);
                    }
                }
            };

            struct InternalDestroyFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, GlobalIdentifier>{

                std::unique_ptr<PacketAssemblerInterface> * dst;

                void push(const size_t& idx, std::move_iterator<GlobalIdentifier *> data_arr, size_t sz) noexcept{

                    this->dst[idx]->destroy(data_arr.base(), sz);
                }
            };
    };

    //OK
    class ExhaustionControlledPacketAssembler: public virtual PacketAssemblerInterface{

        private:

            std::unique_ptr<PacketAssemblerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledPacketAssembler(std::unique_ptr<PacketAssemblerInterface> base,
                                                std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                executor(std::move(executor)),
                                                                                                                                exhaustion_controller(std::move(exhaustion_controller)){}

            void assemble(std::move_iterator<PacketSegment *> segment_arr, size_t sz, std::expected<AssembledPacket, exception_t> * assembled_arr) noexcept{

                PacketSegment * base_segment_arr                                    = segment_arr.base();

                PacketSegment * first_segment_ptr                                   = base_segment_arr;
                PacketSegment * last_segment_ptr                                    = std::next(first_segment_ptr, sz);
                std::expected<AssembledPacket, exception_t> * first_assembled_ptr   = assembled_arr;
                std::expected<AssembledPacket, exception_t> * last_assembled_ptr    = std::next(first_assembled_ptr, sz);
                size_t sliding_window_sz                                            = sz;
                auto full_signal                                                    = std::expected<AssembledPacket, exception_t>(std::unexpected(dg::network_exception::QUEUE_FULL)); 

                auto task = [&, this]() noexcept{
                    this->base->assemble(std::make_move_iterator(first_segment_ptr), sliding_window_sz, first_assembled_ptr);
                    size_t queueing_sz  = std::count_if(first_assembled_ptr, last_assembled_ptr, [](const auto& e){return e.has_value() == false && e.error() == dg::network_exception::QUEUE_FULL;});
                    exception_t err     = this->exhaustion_controller->update_waiting_size(queueing_sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    std::expected<AssembledPacket, exception_t> * first_retriable_ptr   = std::find_if(first_assembled_ptr, last_assembled_ptr, [](const auto& e){return e.has_value() == false && e.error() == dg::network_exception::QUEUE_FULL;});
                    std::expected<AssembledPacket, exception_t> * last_retriable_ptr    = std::find_if(first_retriable_ptr, last_assembled_ptr, [](const auto& e){return e.has_value() == true || e.error() != dg::network_exception::QUEUE_FULL;});

                    sliding_window_sz                                                   = std::distance(first_retriable_ptr, last_retriable_ptr);
                    size_t relative_offset                                              = std::distance(first_assembled_ptr, first_retriable_ptr);

                    std::advance(first_segment_ptr, relative_offset);
                    std::advance(first_assembled_ptr, relative_offset);

                    return !this->exhaustion_controller->is_should_wait() || first_segment_ptr == last_segment_ptr;
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(task);
                this->executor->exec(virtual_task);
            }

            void destroy(GlobalIdentifier * id_arr, size_t sz) noexcept{

                this->base->destroy(id_arr, sz);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class BufferFIFOContainer: public virtual InBoundContainerInterface{

        private:

            dg::pow2_cyclic_queue<BufferContainerWaitingItem> waiting_item_vec;
            dg::pow2_cyclic_queue<internal_huge_kernel_buffer> buffer_vec;
            size_t buffer_vec_capacity;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            BufferFIFOContainer(dg::pow2_cyclic_queue<BufferContainerWaitingItem> waiting_item_vec,
                                dg::pow2_cyclic_queue<internal_huge_kernel_buffer> buffer_vec,
                                size_t buffer_vec_capacity,
                                std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                stdx::hdi_container<size_t> consume_sz_per_load) noexcept: waiting_item_vec(std::move(waiting_item_vec)),
                                                                                           buffer_vec(std::move(buffer_vec)),
                                                                                           buffer_vec_capacity(buffer_vec_capacity),
                                                                                           mtx(std::move(mtx)),
                                                                                           consume_sz_per_load(std::move(consume_sz_per_load)){}

            void push(std::move_iterator<internal_huge_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

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

            void pop(internal_huge_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

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
    class ExhaustionControlledBufferContainer: public virtual InBoundContainerInterface{

        private:

            std::unique_ptr<InBoundContainerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor;
            std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller;

        public:

            ExhaustionControlledBufferContainer(std::unique_ptr<InBoundContainerInterface> base,
                                                std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) noexcept: base(std::move(base)),
                                                                                                                                executor(std::move(executor)),
                                                                                                                                exhaustion_controller(std::move(exhaustion_controller)){}

            void push(std::move_iterator<internal_huge_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                internal_huge_kernel_buffer * buffer_arr_base        = buffer_arr.base();
                internal_huge_kernel_buffer * buffer_arr_first       = buffer_arr_base;
                internal_huge_kernel_buffer * buffer_arr_last        = std::next(buffer_arr_first, sz);
                exception_t * exception_arr_first   = exception_arr;
                exception_t * exception_arr_last    = std::next(exception_arr_first, sz);
                size_t sliding_window_sz            = sz;

                auto task = [&, this]() noexcept{
                    this->base->push(std::make_move_iterator(buffer_arr_first), sliding_window_sz, exception_arr_first);

                    size_t waiting_sz                   = std::count(exception_arr_first, exception_arr_last, dg::network_exception::QUEUE_FULL);
                    exception_t err                     = this->exhaustion_controller->update_waiting_size(waiting_sz);

                    if (dg::network_exception::is_failed(err)){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(err));
                    }

                    exception_t * retriable_arr_first   = std::find(exception_arr_first, exception_arr_last, dg::network_exception::QUEUE_FULL);
                    exception_t * retriable_arr_last    = std::find_if(retriable_arr_first, exception_arr_last, [](exception_t err){return err != dg::network_exception::QUEUE_FULL;});

                    size_t relative_offset              = std::distance(exception_arr_first, retriable_arr_first);
                    sliding_window_sz                   = std::distance(retriable_arr_first, retriable_arr_last);

                    std::advance(buffer_arr_first, relative_offset);
                    std::advance(exception_arr_first, relative_offset); 

                    return !this->exhaustion_controller->is_should_wait() || (buffer_arr_first == buffer_arr_last); //TODOs: we want to subscribe these guys to a load_balancer system
                };

                auto virtual_task = dg::network_concurrency_infretry_x::ExecutableWrapper<decltype(task)>(std::move(task));
                this->executor->exec(virtual_task);
            }

            void pop(internal_huge_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                this->base->pop(output_buffer_arr, sz, output_buffer_arr_cap);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class HashDistributedBufferContainer: public virtual InBoundContainerInterface{

        private:

            std::unique_ptr<std::unique_ptr<InBoundContainerInterface>[]> buffer_container_vec;
            size_t pow2_buffer_container_vec_sz;
            size_t consume_sz_per_load; 

        public:

            HashDistributedBufferContainer(std::unique_ptr<std::unique_ptr<InBoundContainerInterface>[]> buffer_container_vec,
                                           size_t pow2_buffer_container_vec_sz,
                                           size_t consume_sz_per_load) noexcept: buffer_container_vec(std::move(buffer_container_vec)),
                                                                                 pow2_buffer_container_vec_sz(pow2_buffer_container_vec_sz),
                                                                                 consume_sz_per_load(consume_sz_per_load){}

            void push(std::move_iterator<internal_huge_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

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

            void pop(internal_huge_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                sz = 0u; 
                size_t seed = dg::network_randomizer::randomize_int<size_t>() >> 1; 

                for (size_t i = 0u; i < this->pow2_buffer_container_vec_sz; ++i){
                    size_t remaining_cap = output_buffer_arr_cap - sz;
                    
                    if (remaining_cap == 0u){
                        return;
                    }

                    size_t idx = (seed + i) & (this->pow2_buffer_container_vec_sz - 1u);
                    internal_huge_kernel_buffer * tmp_output_buffer_arr = std::next(output_buffer_arr, sz);
                    size_t tmp_sz{};
                    this->buffer_container_vec[i]->pop(tmp_output_buffer_arr, tmp_sz, remaining_cap);
                    sz += tmp_sz;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->consume_sz_per_load;
            }
    };

    //OK
    class ReactingBufferContainer: public virtual InBoundContainerInterface,
                                   public virtual ContainerBusyAdapterInterface{

        private:

            std::unique_ptr<InBoundContainerInterface> base;
            std::unique_ptr<ComplexReactor> reactor;
            std::chrono::nanoseconds max_wait_time;
            size_t ease_threshold;
            size_t busy_threshold;

        public:

            ReactingBufferContainer(std::unique_ptr<InBoundContainerInterface> base,
                                    std::unique_ptr<ComplexReactor> reactor,
                                    std::chrono::nanoseconds max_wait_time,
                                    size_t ease_threshold,
                                    size_t busy_threshold) noexcept: base(std::move(base)),
                                                                     reactor(std::move(reactor)),
                                                                     max_wait_time(max_wait_time),
                                                                     ease_threshold(ease_threshold),
                                                                     busy_threshold(busy_threshold){}

            void push(std::move_iterator<internal_huge_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                this->base->push(buffer_arr, sz, exception_arr);
                size_t thru_sz = std::count(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);
                this->reactor->increment(thru_sz);
            }

            void pop(internal_huge_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->reactor->subscribe(this->max_wait_time);
                std::atomic_signal_fence(std::memory_order_seq_cst);
                this->base->pop(output_buffer_arr, sz, output_buffer_arr_cap);
                this->reactor->decrement(sz);
            }
            
            auto set_busy() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->busy_threshold);
                return dg::network_exception::SUCCESS;
            }

            auto set_ease() noexcept -> exception_t{

                this->reactor->set_wakeup_threshold(this->ease_threshold);
                return dg::network_exception::SUCCESS;
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //OK
    class FairInBoundBufferContainer: public virtual InBoundContainerInterface{

        private:

            dg::pow2_cyclic_queue<FairBufferContainerWaitingItem> push_waiting_item_vec;
            dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>> distribution_queue;
            dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_huge_kernel_buffer>> *, dg_binary_semaphore *>> waiting_queue;
            dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>> leftover_queue;
            std::unique_ptr<UnitAllocatorInterface<dg::vector<internal_huge_kernel_buffer>>> bufvec_allocator;
            std::unique_ptr<stdx::fair_atomic_flag> mtx;
            stdx::hdi_container<size_t> consume_sz_per_load;

        public:

            FairInBoundBufferContainer(dg::pow2_cyclic_queue<FairBufferContainerWaitingItem> push_waiting_item_vec,
                                       dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>> distribution_queue,
                                       dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_huge_kernel_buffer>> *, dg_binary_semaphore *>> waiting_queue,
                                       dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>> leftover_queue,
                                       std::unique_ptr<UnitAllocatorInterface<dg::vector<internal_huge_kernel_buffer>>> bufvec_allocator,
                                       std::unique_ptr<stdx::fair_atomic_flag> mtx,
                                       size_t consume_sz_per_load): push_waiting_item_vec(std::move(push_waiting_item_vec)),
                                                                    distribution_queue(std::move(distribution_queue)),
                                                                    waiting_queue(std::move(waiting_queue)),
                                                                    leftover_queue(std::move(leftover_queue)),
                                                                    bufvec_allocator(std::move(bufvec_allocator)),
                                                                    mtx(std::move(mtx)),
                                                                    consume_sz_per_load(stdx::hdi_container<size_t>{consume_sz_per_load}){}

            void push(std::move_iterator<internal_huge_kernel_buffer *> buffer_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (sz == 0u){
                    return;
                }

                internal_huge_kernel_buffer * base_buffer_arr = buffer_arr.base();
                std::expected<dg::vector<internal_huge_kernel_buffer>, exception_t> buffer_vec_tmp = this->bufvec_allocator->get();

                if (!buffer_vec_tmp.has_value()){
                    std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::RESOURCE_EXHAUSTION);
                    return;
                }

                dg::vector<internal_huge_kernel_buffer> buffer_vec = std::move(buffer_vec_tmp.value());

                buffer_vec.resize(sz);
                std::copy(std::make_move_iterator(base_buffer_arr),
                          std::next(std::make_move_iterator(base_buffer_arr), sz),
                          buffer_vec.begin());

                dg_binary_semaphore * releasing_smp = nullptr;
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

            void pop(internal_huge_kernel_buffer * output_buffer_arr, size_t& sz, size_t output_buffer_arr_cap) noexcept{

                std::optional<dg::vector<internal_huge_kernel_buffer>> str_vec(std::nullopt); 
                dg_binary_semaphore smp(0);
                
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
    class BufferContainerRedistributorWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<InBoundContainerInterface> fr_warehouse;
            std::shared_ptr<InBoundContainerInterface> to_warehouse;
            size_t fr_warehouse_get_cap;
            size_t to_warehouse_push_cap;
            size_t busy_threshold;

        public:

            BufferContainerRedistributorWorker(std::shared_ptr<InBoundContainerInterface> fr_warehouse,
                                               std::shared_ptr<InBoundContainerInterface> to_warehouse,
                                               size_t fr_warehouse_get_cap,
                                               size_t to_warehouse_push_cap,
                                               size_t busy_threshold) noexcept: fr_warehouse(std::move(fr_warehouse)),
                                                                                to_warehouse(std::move(to_warehouse)),
                                                                                fr_warehouse_get_cap(fr_warehouse_get_cap),
                                                                                to_warehouse_push_cap(to_warehouse_push_cap),
                                                                                busy_threshold(busy_threshold){}

            bool run_one_epoch() noexcept{

                dg::network_stack_allocation::NoExceptAllocation<internal_huge_kernel_buffer[]> recv_buf_arr(this->fr_warehouse_get_cap);
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

            struct InternalDeliveryResolutor: dg::network_producer_consumer::ConsumerInterface<internal_huge_kernel_buffer>{

                InBoundContainerInterface * dst;

                void push(std::move_iterator<internal_huge_kernel_buffer *> data_arr, size_t data_arr_sz) noexcept{

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

    //OK
    class NetworkStatusMonitorWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<NetworkBusyStatusRetrieverInterface> netstat_retriever;
            std::shared_ptr<NetworkBusyObserverInterface> busy_observer;
            std::chrono::nanoseconds break_time; 

        public:

            NetworkStatusMonitorWorker(std::shared_ptr<NetworkBusyStatusRetrieverInterface> netstat_retriever,
                                       std::shared_ptr<NetworkBusyObserverInterface> busy_observer,
                                       std::chrono::nanoseconds break_time) noexcept: netstat_retriever(std::move(netstat_retriever)),
                                                                                      busy_observer(std::move(busy_observer)),
                                                                                      break_time(break_time){}
            
            bool run_one_epoch() noexcept{

                using netstat_interface_t = NetworkBusyStatusRetrieverInterface;
                using observer_interface_t = NetworkBusyObserverInterface;

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

    //OK
    class ExpiryWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<PacketAssemblerInterface> packet_assembler;
            std::shared_ptr<EntranceControllerInterface> entrance_controller;
            std::shared_ptr<BlackListGateInterface> blacklist_gate;
            size_t packet_assembler_vectorization_sz;
            size_t expired_id_consume_sz;
            size_t busy_consume_sz;

        public:

            ExpiryWorker(std::shared_ptr<PacketAssemblerInterface> packet_assembler,
                         std::shared_ptr<EntranceControllerInterface> entrance_controller,
                         std::shared_ptr<BlackListGateInterface> blacklist_gate,
                         size_t packet_assembler_vectorization_sz,
                         size_t expired_id_consume_sz,
                         size_t busy_consume_sz) noexcept: packet_assembler(std::move(packet_assembler)),
                                                           entrance_controller(std::move(entrance_controller)),
                                                           blacklist_gate(std::move(blacklist_gate)),
                                                           packet_assembler_vectorization_sz(packet_assembler_vectorization_sz),
                                                           expired_id_consume_sz(expired_id_consume_sz),
                                                           busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept{

                size_t id_arr_cap   = this->expired_id_consume_sz;
                size_t id_arr_sz    = {};
                dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> id_arr(id_arr_cap);

                this->entrance_controller->get_expired_id(id_arr.get(), id_arr_sz, id_arr_cap);

                auto pa_feed_resolutor              = InternalPacketAssemblerDestroyFeedResolutor{};
                pa_feed_resolutor.dst               = this->packet_assembler.get(); 
                pa_feed_resolutor.blacklist_gate    = this->blacklist_gate.get();

                size_t trimmed_pa_feed_sz           = std::min(std::min(std::min(this->packet_assembler_vectorization_sz, this->packet_assembler->max_consume_size()), id_arr_sz), this->blacklist_gate->max_consume_size());
                size_t pa_feeder_allocation_cost    = dg::network_producer_consumer::delvrsrv_allocation_cost(&pa_feed_resolutor, trimmed_pa_feed_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> pa_feeder_mem(pa_feeder_allocation_cost);
                auto pa_feeder                      = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&pa_feed_resolutor, trimmed_pa_feed_sz, pa_feeder_mem.get()));

                for (size_t i = 0u; i < id_arr_sz; ++i){
                    dg::network_producer_consumer::delvrsrv_deliver(pa_feeder.get(), id_arr[i]);
                }

                return id_arr_sz >= this->busy_consume_sz;
            }

        private:

            struct InternalPacketAssemblerDestroyFeedResolutor: dg::network_producer_consumer::ConsumerInterface<GlobalIdentifier>{

                PacketAssemblerInterface * dst;
                BlackListGateInterface * blacklist_gate;

                void push(std::move_iterator<GlobalIdentifier *> id_arr, size_t id_arr_sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(id_arr_sz);

                    GlobalIdentifier * base_id_arr = id_arr.base();
                    this->dst->destroy(base_id_arr, id_arr_sz);
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    this->blacklist_gate->blacklist(base_id_arr, id_arr_sz, exception_arr.get());

                    for (size_t i = 0u; i < id_arr_sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };     
    };

    //OK
    class InBoundWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<PacketAssemblerInterface> packet_assembler;
            std::shared_ptr<InBoundGateInterface> inbound_gate;
            std::shared_ptr<InBoundGateInterface> blacklist_gate;
            std::shared_ptr<InBoundContainerInterface> inbound_container;
            std::shared_ptr<EntranceControllerInterface> entrance_controller;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device;
            std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base;
            size_t packet_assembler_vectorization_sz;
            size_t inbound_gate_vectorization_sz;
            size_t blacklist_gate_vectorization_sz;
            size_t entrance_controller_vectorization_sz;
            size_t inbound_container_vectorization_sz;
            size_t upstream_consume_sz;
            size_t busy_consume_sz;

        public:

            InBoundWorker(std::shared_ptr<PacketAssemblerInterface> packet_assembler,
                          std::shared_ptr<InBoundGateInterface> inbound_gate,
                          std::shared_ptr<InBoundGateInterface> blacklist_gate,
                          std::shared_ptr<InBoundContainerInterface> inbound_container,
                          std::shared_ptr<EntranceControllerInterface> entrance_controller,
                          std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device,
                          std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
                          size_t packet_assembler_vectorization_sz,
                          size_t inbound_gate_vectorization_sz,
                          size_t blacklist_gate_vectorization_sz,
                          size_t entrance_controller_vectorization_sz,
                          size_t inbound_container_vectorization_sz,
                          size_t upstream_consume_sz,
                          size_t busy_consume_sz) noexcept: packet_assembler(std::move(packet_assembler)),
                                                            inbound_gate(std::move(inbound_gate)),
                                                            blacklist_gate(std::move(blacklist_gate)),
                                                            inbound_container(std::move(inbound_container)),
                                                            entrance_controller(std::move(entrance_controller)),
                                                            infretry_device(std::move(infretry_device)),
                                                            base(std::move(base)),
                                                            packet_assembler_vectorization_sz(packet_assembler_vectorization_sz),
                                                            inbound_gate_vectorization_sz(inbound_gate_vectorization_sz),
                                                            blacklist_gate_vectorization_sz(blacklist_gate_vectorization_sz),
                                                            entrance_controller_vectorization_sz(entrance_controller_vectorization_sz),
                                                            inbound_container_vectorization_sz(inbound_container_vectorization_sz),
                                                            upstream_consume_sz(upstream_consume_sz),
                                                            busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept{

                size_t consuming_sz     = 0u;
                size_t consuming_cap    = this->upstream_consume_sz;

                dg::network_stack_allocation::NoExceptAllocation<internal_segment_kernel_buffer[]> buf_arr(consuming_cap);

                try
                {
                    for (size_t i = 0u; i < consuming_cap; ++i)
                    {
                        buf_arr[i].resize(base->max_content_size());
                    }
                }
                catch (...)
                {
                    return false;
                }

                this->recv(buf_arr.get(), consuming_sz, consuming_cap);

                auto et_feed_resolutor                      = InternalEntranceFeedResolutor{};
                et_feed_resolutor.entrance_controller       = this->entrance_controller.get();

                size_t trimmed_et_feed_cap                  = std::min(std::min(this->entrance_controller_vectorization_sz, consuming_sz), this->entrance_controller->max_consume_size());
                size_t et_feeder_allocation_cost            = dg::network_producer_consumer::delvrsrv_allocation_cost(&et_feed_resolutor, trimmed_et_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> et_feeder_mem(et_feeder_allocation_cost);
                auto et_feeder                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&et_feed_resolutor, trimmed_et_feed_cap, et_feeder_mem.get()));

                auto ib_feed_resolutor                      = InternalInBoundFeedResolutor{};
                ib_feed_resolutor.inbound_container         = this->inbound_container.get();

                size_t trimmed_ib_feed_cap                  = std::min(std::min(this->inbound_container_vectorization_sz, consuming_sz), this->inbound_container->max_consume_size());
                size_t ib_feeder_allocation_cost            = dg::network_producer_consumer::delvrsrv_allocation_cost(&ib_feed_resolutor, trimmed_ib_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ib_feeder_mem(ib_feeder_allocation_cost);
                auto ib_feeder                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&ib_feed_resolutor, trimmed_ib_feed_cap, ib_feeder_mem.get())); 

                auto ps_feed_resolutor                      = InternalPacketSegmentFeedResolutor{};
                ps_feed_resolutor.packet_assembler          = this->packet_assembler.get();
                ps_feed_resolutor.entrance_feeder           = et_feeder.get();
                ps_feed_resolutor.inbound_feeder            = ib_feeder.get();
                ps_feed_resolutor.infretry_device           = this->infretry_device.get();

                size_t trimmed_ps_feed_cap                  = std::min(std::min(this->packet_assembler_vectorization_sz, consuming_sz), this->packet_assembler->max_consume_size());
                size_t ps_feeder_allocation_cost            = dg::network_producer_consumer::delvrsrv_allocation_cost(&ps_feed_resolutor, trimmed_ps_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> ps_feeder_mem(ps_feeder_allocation_cost);
                auto ps_feeder                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&ps_feed_resolutor, trimmed_ps_feed_cap, ps_feeder_mem.get()));  

                auto gt_feed_resolutor                      = InternalInBoundGateFeedResolutor{};
                gt_feed_resolutor.inbound_gate              = this->inbound_gate.get();
                gt_feed_resolutor.downstream_feeder         = ps_feeder.get();                    

                size_t trimmed_gt_feed_cap                  = std::min(std::min(this->inbound_gate_vectorization_sz, consuming_sz), this->inbound_gate->max_consume_size());
                size_t gt_feeder_allocation_cost            = dg::network_producer_consumer::delvrsrv_allocation_cost(&gt_feed_resolutor, trimmed_gt_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> gt_feeder_mem(gt_feeder_allocation_cost);
                auto gt_feeder                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&gt_feed_resolutor, trimmed_gt_feed_cap, gt_feeder_mem.get()));

                auto bl_feed_resolutor                      = InternalInBoundGateFeedResolutor{};
                bl_feed_resolutor.inbound_gate              = this->blacklist_gate.get();
                bl_feed_resolutor.downstream_feeder         = gt_feeder.get();

                size_t trimmed_bl_feed_cap                  = std::min(std::min(this->blacklist_gate_vectorization_sz, consuming_sz), this->blacklist_gate->max_consume_size());
                size_t bl_feeder_allocation_cost            = dg::network_producer_consumer::delvrsrv_allocation_cost(&bl_feed_resolutor, trimmed_bl_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> bl_feeder_mem(bl_feeder_allocation_cost);
                auto bl_feeder                              = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&bl_feed_resolutor, trimmed_bl_feed_cap, bl_feeder_mem.get()));

                for (size_t i = 0u; i < consuming_sz; ++i){
                    std::expected<PacketSegment, exception_t> pkt = deserialize_packet_segment(std::move(buf_arr[i]));

                    if (!pkt.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(pkt.error()));
                        continue;
                    }

                    dg::network_producer_consumer::delvrsrv_deliver(bl_feeder.get(), std::move(pkt.value()));
                }

                return consuming_sz >= this->busy_consume_sz;
            }
        
        private:

            void recv(internal_segment_kernel_buffer * arr, size_t& sz, size_t cap) noexcept{

                dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<void>[]> addr_arr(cap);
                dg::network_stack_allocation::NoExceptAllocation<size_t[]> capacity_arr(cap);
                dg::network_stack_allocation::NoExceptAllocation<size_t[]> sz_arr(cap);

                for (size_t i = 0u; i < cap; ++i){
                    addr_arr[i]     = arr[i].data();
                    capacity_arr[i] = arr[i].size();
                    sz_arr[i]       = 0u;
                }

                this->base->recv(addr_arr.get(), capacity_arr.get(), sz_arr.get(), sz, cap);

                for (size_t i = 0u; i < sz; ++i){
                    arr[i].resize(sz_arr[i]);
                }
            }

            struct InternalEntranceFeedResolutor: dg::network_producer_consumer::ConsumerInterface<GlobalIdentifier>{

                EntranceControllerInterface * entrance_controller;

                void push(std::move_iterator<GlobalIdentifier *> id_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    this->entrance_controller->tick(id_arr.base(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::SOCKET_STREAM_LEAK));
                        }
                    }
                }
            };

            struct InternalInBoundFeedResolutor: dg::network_producer_consumer::ConsumerInterface<internal_huge_kernel_buffer>{

                InBoundContainerInterface * inbound_container;

                void push(std::move_iterator<internal_huge_kernel_buffer *> data_arr, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    this->inbound_container->push(data_arr, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            struct InternalPacketSegmentFeedResolutor: dg::network_producer_consumer::ConsumerInterface<PacketSegment>{

                PacketAssemblerInterface * packet_assembler;
                dg::network_producer_consumer::DeliveryHandle<GlobalIdentifier> * entrance_feeder;
                dg::network_producer_consumer::DeliveryHandle<internal_huge_kernel_buffer> * inbound_feeder;
                dg::network_concurrency_infretry_x::ExecutorInterface * infretry_device;

                void push(std::move_iterator<PacketSegment *> data_arr, size_t sz) noexcept{

                    PacketSegment * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<AssembledPacket, exception_t>[]> assembled_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id; 
                    }

                    this->packet_assembler->assemble(std::make_move_iterator(base_data_arr), sz, assembled_arr.get());

                    std::atomic_signal_fence(std::memory_order_seq_cst);

                    for (size_t i = 0u; i < sz; ++i){
                        dg::network_producer_consumer::delvrsrv_deliver(this->entrance_feeder, global_id_arr[i]);
                    }

                    for (size_t i = 0u; i < sz; ++i){
                        if (assembled_arr[i].has_value()){
                            std::expected<internal_huge_kernel_buffer, exception_t> buf = std::unexpected(dg::network_exception::EXPECTED_NOT_INITIALIZED);

                            auto resolutor = [&]() noexcept{
                                buf = internal_integrity_assembled_packet_to_buffer(static_cast<AssembledPacket&&>(assembled_arr[i].value())); 

                                if (buf.has_value())
                                {
                                    return true;
                                }

                                if (buf.error() != dg::network_exception::RESOURCE_EXHAUSTION)
                                {
                                    return true;
                                }

                                return false;
                            };

                            dg::network_concurrency_infretry_x::ExecutableWrapper virtual_resolutor(resolutor);
                            this->infretry_device->exec(virtual_resolutor);

                            if (!buf.has_value()){
                                dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(buf.error()));
                            } else{
                                dg::network_producer_consumer::delvrsrv_deliver(this->inbound_feeder, std::move(buf.value()));
                            }
                        } else{
                            if (assembled_arr[i].error() != dg::network_exception::SOCKET_STREAM_SEGMENT_FILLING){
                                dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(assembled_arr[i].error()));
                            }
                        }
                    }
                }
            };

            struct InternalInBoundGateFeedResolutor: dg::network_producer_consumer::ConsumerInterface<PacketSegment>{

                InBoundGateInterface * inbound_gate;
                dg::network_producer_consumer::DeliveryHandle<PacketSegment> * downstream_feeder;

                void push(std::move_iterator<PacketSegment *> data_arr, size_t sz) noexcept{

                    PacketSegment * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<GlobalIdentifier[]> global_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        global_id_arr[i] = base_data_arr[i].id;
                    }

                    this->inbound_gate->thru(global_id_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                            continue;
                        }

                        dg::network_producer_consumer::delvrsrv_deliver(this->downstream_feeder, std::move(base_data_arr[i]));
                    }
                }  
            };
    };

    //OK
    class MailBox: public virtual dg::network_kernel_mailbox_impl1::core::MailboxInterface{

        private:

            dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec;
            std::unique_ptr<PacketizerInterface> packetizer;
            std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base;
            std::shared_ptr<InBoundContainerInterface> inbound_container;
            std::shared_ptr<OutBoundRuleInterface> outbound_rule;
            size_t transmission_vectorization_sz;

        public:

            MailBox(dg::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec,
                    std::unique_ptr<PacketizerInterface> packetizer,
                    std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
                    std::shared_ptr<InBoundContainerInterface> inbound_container,
                    std::shared_ptr<OutBoundRuleInterface> outbound_rule,
                    size_t transmission_vectorization_sz) noexcept: daemon_vec(std::move(daemon_vec)),
                                                                    packetizer(std::move(packetizer)),
                                                                    base(std::move(base)),
                                                                    inbound_container(std::move(inbound_container)),
                                                                    outbound_rule(std::move(outbound_rule)),
                                                                    transmission_vectorization_sz(transmission_vectorization_sz){}

            void send(MailBoxArgument * data_arr, size_t sz, exception_t * exception_arr) noexcept{

                MailBoxArgument * base_data_arr = data_arr;

                auto feed_resolutor             = InternalFeedResolutor{};
                feed_resolutor.dst              = this->base.get(); 

                size_t trimmed_mailbox_feed_sz  = std::min(std::min(this->transmission_vectorization_sz, sz * this->packetizer->max_segment_count()), this->base->max_consume_size());
                size_t feeder_allocation_cost   = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_mailbox_feed_sz);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_mailbox_feed_sz, feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    std::expected<bool, exception_t> outbound_thru_status = this->outbound_rule->thru(base_data_arr[i].to);

                    if (!outbound_thru_status.has_value()){
                        exception_arr[i] = outbound_thru_status.error();
                        continue;
                    }

                    if (!outbound_thru_status.value()){
                        exception_arr[i] = dg::network_exception::SOCKET_STREAM_BAD_OUTBOUND_RULE;
                        continue;
                    }

                    std::expected<internal_segment_kernel_buffer, exception_t> buf          = dg::network_exception::cstyle_initialize<internal_segment_kernel_buffer>(std::string_view(static_cast<const char *>(base_data_arr[i].content),
                                                                                                                                                                                        base_data_arr[i].content_sz)); //
                    
                    if (!buf.has_value()){
                        exception_arr[i] = buf.error();
                        continue;
                    }

                    std::expected<internal_vector<PacketSegment>, exception_t> segment_vec  = this->packetizer->packetize(std::move(buf.value()));

                    if (!segment_vec.has_value()){
                        exception_arr[i] = segment_vec.error();
                        continue;
                    }

                    for (size_t j = 0u; j < segment_vec.value().size(); ++j)
                    {
                        InternalFeedArgument arg
                        {
                            .segment    = std::move(segment_vec.value()[j]),
                            .err        = std::next(exception_arr, i)
                        };

                        dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), base_data_arr[i].to, std::move(arg));
                    }
                }
            }

            void recv(std::add_pointer_t<void> * output_arr,
                      size_t * output_cap_arr,
                      size_t * output_sz_arr,
                      size_t& output_arr_sz,
                      size_t output_arr_cap) noexcept{

                dg::network_stack_allocation::NoExceptAllocation<internal_huge_kernel_buffer[]> buffer_arr(output_arr_cap);                
                this->inbound_container->pop(buffer_arr.get(), output_arr_sz, output_arr_cap);

                for (size_t i = 0u; i < output_arr_sz; ++i)
                {
                    size_t trimmed_buffer_sz    = std::min(buffer_arr[i].size(), output_cap_arr[i]);
                    output_sz_arr[i]            = trimmed_buffer_sz;

                    std::copy(buffer_arr[i].begin(), std::next(buffer_arr[i].begin(), trimmed_buffer_sz), static_cast<char *>(output_arr[i]));
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }

            auto max_content_size() const noexcept -> size_t{

                return this->packetizer->max_packet_size();
            }

        private:

            struct InternalFeedArgument
            {
                PacketSegment segment;
                exception_t * err;
            };

            struct InternalFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<Address, InternalFeedArgument>{

                dg::network_kernel_mailbox_impl1::core::MailboxInterface * dst;

                void push(const Address& to, std::move_iterator<InternalFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalFeedArgument * base_data_arr    = data_arr.base();
                    size_t arr_cap                          = sz;
                    size_t arr_sz                           = 0u;

                    dg::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arg_arr(arr_cap);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(arr_cap);
                    dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<exception_t>[]> exception_ptr_arr(arr_cap);
                    dg::network_stack_allocation::NoExceptAllocation<internal_segment_kernel_buffer[]> segment_arr(arr_cap);

                    for (size_t i = 0u; i < sz; ++i){
                        std::expected<internal_segment_kernel_buffer, exception_t> serialized = serialize_packet_segment(static_cast<PacketSegment&&>(base_data_arr[i].segment));

                        if (!serialized.has_value()){
                            *base_data_arr[i].err = serialized.error();
                            continue;
                        }

                        segment_arr[arr_sz]                 = std::move(serialized.value());
                        mailbox_arg_arr[arr_sz].content     = segment_arr[arr_sz].data();
                        mailbox_arg_arr[arr_sz].content_sz  = segment_arr[arr_sz].size();
                        mailbox_arg_arr[arr_sz].to          = to;
                        exception_ptr_arr[arr_sz]           = base_data_arr[i].err;

                        arr_sz                              += 1;
                    }

                    this->dst->send(mailbox_arg_arr.get(), arr_sz, exception_arr.get());

                    for (size_t i = 0u; i < arr_sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *exception_ptr_arr[i] = exception_arr[i];
                        }
                    }
                }
            };

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

        static auto get_no_exhaustion_controller() -> std::unique_ptr<ExhaustionControllerInterface>{

            return std::make_unique<NoExhaustionController>();
        }

        static auto get_empty_outbound_rule() -> std::unique_ptr<OutBoundRuleInterface>{

            return std::make_unique<EmptyOutBoundRule>();
        }

        static auto get_incremental_packet_id_generator(Address factory_addr) -> std::unique_ptr<PacketIDGeneratorInterface>{

            uint64_t random_counter = dg::network_randomizer::randomize_int<uint64_t>();
            return std::make_unique<PacketIDGenerator>(std::move(factory_addr), random_counter);
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

        static auto get_random_packet_id_generator(Address factory_addr) -> std::unique_ptr<PacketIDGeneratorInterface>{

            return std::make_unique<RandomPacketIDGenerator>(factory_addr);
        }

        static auto get_temporal_absolute_timeout_inbound_gate(size_t map_capacity, 
                                                               std::chrono::nanoseconds abs_timeout_dur,
                                                               size_t ticking_clock_resolution = 1u,
                                                               size_t max_consume_decay_factor = 2u) -> std::unique_ptr<InBoundGateInterface>{

            const size_t MIN_MAP_CAPACITY                       = size_t{1};
            const size_t MAX_MAP_CAPACITY                       = size_t{1} << 40;
            const std::chrono::nanoseconds MIN_ABS_TIMEOUT_DUR  = std::chrono::nanoseconds(1u);
            const std::chrono::nanoseconds MAX_ABS_TIMEOUT_DUR  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

            if (std::clamp(map_capacity, MIN_MAP_CAPACITY, MAX_MAP_CAPACITY) != map_capacity){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(abs_timeout_dur, MIN_ABS_TIMEOUT_DUR, MAX_ABS_TIMEOUT_DUR) != abs_timeout_dur){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_max_consume_sz = map_capacity >> max_consume_decay_factor;
            size_t max_consume_sz           = std::max(size_t{1}, tentative_max_consume_sz); 
            auto abstimeout_map             = temporal_finite_unordered_map<GlobalIdentifier, std::chrono::time_point<std::chrono::steady_clock>>(map_capacity); 

            return std::make_unique<TemporalAbsoluteTimeoutInBoundGate>(std::move(abstimeout_map),
                                                                        abs_timeout_dur,
                                                                        ticking_clock_resolution,
                                                                        stdx::make_unique_fair_atomic_flag(),
                                                                        stdx::hdi_container<size_t>{max_consume_sz});
        }

        static auto get_random_hash_distributed_inbound_gate(std::vector<std::unique_ptr<InBoundGateInterface>> inbound_gate_vec,
                                                             size_t keyvalue_feed_cap = DEFAULT_KEYVALUE_FEED_SIZE) -> std::unique_ptr<InBoundGateInterface>{

            const size_t MIN_KEYVALUE_FEED_CAP  = size_t{1};
            const size_t MAX_KEYVALUE_FEED_CAP  = size_t{1} << 25;

            if (!stdx::is_pow2(inbound_gate_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(keyvalue_feed_cap, MIN_KEYVALUE_FEED_CAP, MAX_KEYVALUE_FEED_CAP) != keyvalue_feed_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto inbound_gate_arr       = std::make_unique<std::unique_ptr<InBoundGateInterface>[]>(inbound_gate_vec.size());
            size_t inbound_gate_arr_sz  = inbound_gate_vec.size();
            size_t max_consume_sz       = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < inbound_gate_vec.size(); ++i){
                if (inbound_gate_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                max_consume_sz      = std::min(max_consume_sz, inbound_gate_vec[i]->max_consume_size());
                inbound_gate_arr[i] = std::move(inbound_gate_vec[i]);
            }

            return std::make_unique<RandomHashDistributedInBoundGate>(std::move(inbound_gate_arr),
                                                                      inbound_gate_arr_sz,
                                                                      keyvalue_feed_cap,
                                                                      max_consume_sz);
        }

        static auto get_void_inbound_gate() -> std::unique_ptr<InBoundGateInterface>
        {
            return std::make_unique<VoidInboundGate>();
        }

        static auto get_temporal_blacklist_gate(size_t set_cap,
                                                size_t rehash_sz,
                                                size_t reliability_decay_factor,
                                                size_t max_consume_decay_factor = 2u) -> std::unique_ptr<BlackListGateInterface>{
            
            const size_t MIN_SET_CAP    = size_t{1};
            const size_t MAX_SET_CAP    = size_t{1} << 40;

            if (std::clamp(set_cap, MIN_SET_CAP, MAX_SET_CAP) != set_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto blacklist_set              = temporal_switching_bloom_filter<GlobalIdentifier>(set_cap, rehash_sz, reliability_decay_factor);
            size_t tentative_max_consume_sz = set_cap >> max_consume_decay_factor;
            size_t max_consume_sz           = std::max(size_t{1}, tentative_max_consume_sz); 

            return std::make_unique<TemporalBlackListGate>(std::move(blacklist_set),
                                                           stdx::make_unique_fair_atomic_flag(),
                                                           stdx::hdi_container<size_t>{max_consume_sz});
        }

        static auto get_random_hash_distributed_blacklist_gate(std::vector<std::unique_ptr<BlackListGateInterface>> blacklist_gate_vec,
                                                               size_t keyvalue_feed_cap = DEFAULT_KEYVALUE_FEED_SIZE) -> std::unique_ptr<BlackListGateInterface>{
            
            const size_t MIN_KEYVALUE_FEED_CAP  = size_t{1};
            const size_t MAX_KEYVALUE_FEED_CAP  = size_t{1} << 25;

            if (!stdx::is_pow2(blacklist_gate_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(keyvalue_feed_cap, MIN_KEYVALUE_FEED_CAP, MAX_KEYVALUE_FEED_CAP) != keyvalue_feed_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto blacklist_gate_arr         = std::make_unique<std::unique_ptr<BlackListGateInterface>[]>(blacklist_gate_vec.size());
            size_t blacklist_gate_arr_sz    = blacklist_gate_vec.size();
            size_t max_consume_sz           = std::numeric_limits<size_t>::max();

            for (size_t i = 0u; i < blacklist_gate_vec.size(); ++i){
                if (blacklist_gate_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                max_consume_sz          = std::min(max_consume_sz, blacklist_gate_vec[i]->max_consume_size());
                blacklist_gate_arr[i]   = std::move(blacklist_gate_vec[i]);
            }

            return std::make_unique<RandomHashDistributedBlackListGate>(std::move(blacklist_gate_arr),
                                                                        blacklist_gate_arr_sz,
                                                                        keyvalue_feed_cap,
                                                                        max_consume_sz);
        }

        static auto get_void_blacklist_gate() -> std::unique_ptr<BlackListGateInterface>
        {
            return std::make_unique<VoidBlackListGate>();
        }

        static auto get_packetizer(Address factory_addr, 
                                   size_t segment_bsz,
                                   size_t max_packet_bsz,
                                   bool has_integrity_transmit) -> std::unique_ptr<PacketizerInterface>{

            const size_t MIN_SEGMENT_BYTE_SZ    = size_t{1};
            const size_t MAX_SEGMENT_BYTE_SZ    = size_t{1} << 30;  
            const size_t MIN_MAX_PACKET_BSZ     = 0u;
            const size_t MAX_MAX_PACKET_BSZ     = size_t{1} << 40; 

            if (std::clamp(segment_bsz, MIN_SEGMENT_BYTE_SZ, MAX_SEGMENT_BYTE_SZ) != segment_bsz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(segment_bsz)){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(max_packet_bsz, MIN_MAX_PACKET_BSZ, MAX_MAX_PACKET_BSZ) != max_packet_bsz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(max_packet_bsz)){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<Packetizer>(get_random_packet_id_generator(factory_addr),
                                                stdx::ulog2(segment_bsz),
                                                max_packet_bsz,
                                                has_integrity_transmit);
        }  

        static auto get_entrance_controller(size_t tick_wait_queue_cap,
                                            size_t queue_cap,
                                            std::chrono::nanoseconds expiry_period,
                                            size_t max_consume_decay_factor = 2u) -> std::unique_ptr<EntranceControllerInterface>{
            
            const size_t MIN_TICK_WAIT_QUEUE_CAP                = size_t{1};
            const size_t MAX_TICK_WAIT_QUEUE_CAP                = size_t{1} << 30;
            const size_t MIN_QUEUE_CAP                          = size_t{1};
            const size_t MAX_QUEUE_CAP                          = size_t{1} << 30;

            const std::chrono::nanoseconds MIN_EXPIRY_PERIOD    = std::chrono::nanoseconds(1);
            const std::chrono::nanoseconds MAX_EXPIRY_PERIOD    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            
            if (std::clamp(tick_wait_queue_cap, MIN_TICK_WAIT_QUEUE_CAP, MAX_TICK_WAIT_QUEUE_CAP) != tick_wait_queue_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(queue_cap, MIN_QUEUE_CAP, MAX_QUEUE_CAP) != queue_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(expiry_period, MIN_EXPIRY_PERIOD, MAX_EXPIRY_PERIOD) != expiry_period){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t upqueue_cap              = stdx::ceil2(queue_cap);
            size_t tentative_max_consume_sz = upqueue_cap >> max_consume_decay_factor;
            size_t max_consume_sz           = std::max(size_t{1}, tentative_max_consume_sz);
            auto tick_wait_queue            = dg::pow2_cyclic_queue<EntranceControllerWaitingItem>(stdx::ulog2(stdx::ceil2(tick_wait_queue_cap)));
            auto entrance_entry_pq          = dg::pow2_cyclic_queue<EntranceEntry>(stdx::ulog2(upqueue_cap));
            auto key_id_map                 = dg::unordered_unstable_map<GlobalIdentifier, __uint128_t>{};

            return std::make_unique<EntranceController>(std::move(tick_wait_queue),
                                                        std::move(entrance_entry_pq),
                                                        upqueue_cap,
                                                        std::move(key_id_map),
                                                        __uint128_t{0u},
                                                        expiry_period,
                                                        stdx::make_unique_fair_atomic_flag(),
                                                        stdx::hdi_container<size_t>{max_consume_sz});
        }

        static auto get_random_hash_distributed_entrance_controller(std::vector<std::unique_ptr<EntranceControllerInterface>> base_vec,
                                                                    size_t keyvalue_feed_cap    = DEFAULT_KEYVALUE_FEED_SIZE) -> std::unique_ptr<EntranceControllerInterface>{

            const size_t MIN_KEYVALUE_FEED_CAP  = size_t{1};
            const size_t MAX_KEYVALUE_FEED_CAP  = size_t{1} << 25;

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(keyvalue_feed_cap, MIN_KEYVALUE_FEED_CAP, MAX_KEYVALUE_FEED_CAP) != keyvalue_feed_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_arr           = std::make_unique<std::unique_ptr<EntranceControllerInterface>[]>(base_vec.size());
            size_t base_arr_sz      = base_vec.size();
            size_t max_consume_sz   = std::numeric_limits<size_t>::max(); 

            for (size_t i = 0u; i < base_arr_sz; ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                max_consume_sz  = std::min(max_consume_sz, base_vec[i]->max_consume_size());
                base_arr[i]     = std::move(base_vec[i]);
            }

            size_t trimmed_feed_cap = std::min(max_consume_sz, keyvalue_feed_cap);

            return std::make_unique<RandomHashDistributedEntranceController>(std::move(base_arr),
                                                                             base_arr_sz,
                                                                             trimmed_feed_cap,
                                                                             max_consume_sz);
        }

        static auto get_exhaustion_controlled_entrance_controller(std::unique_ptr<EntranceControllerInterface> base,
                                                                  std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                                  std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<EntranceControllerInterface>{
            
            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (executor == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExhaustionControlledEntranceController>(std::move(base),
                                                                            std::move(executor),
                                                                            std::move(exhaustion_controller));
        }

        static auto get_void_entrance_controller() -> std::unique_ptr<EntranceControllerInterface>
        {
            return std::make_unique<VoidEntranceController>();
        }

        static auto get_packet_assembler(size_t packet_map_cap,
                                         size_t max_segment_sz_per_stream,
                                         size_t max_consume_decay_factor = 2u) -> std::unique_ptr<PacketAssemblerInterface>{

            const size_t MIN_PACKET_MAP_CAP             = size_t{1};
            const size_t MAX_PACKET_MAP_CAP             = size_t{1} << 30;
            const size_t MIN_MAX_SEGMENT_SZ_PER_STREAM  = size_t{1};
            const size_t MAX_MAX_SEGMENT_SZ_PER_STREAM  = size_t{1} << 30; 

            if (std::clamp(packet_map_cap, MIN_PACKET_MAP_CAP, MAX_PACKET_MAP_CAP) != packet_map_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(max_segment_sz_per_stream, MIN_MAX_SEGMENT_SZ_PER_STREAM, MAX_MAX_SEGMENT_SZ_PER_STREAM) != max_segment_sz_per_stream){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t tentative_max_consume_sz = packet_map_cap >> max_consume_decay_factor;
            size_t max_consume_sz           = std::max(size_t{1}, tentative_max_consume_sz);
            size_t packet_map_bucket_cap    = dg::cyclic_unordered_node_map<GlobalIdentifier, AssembledPacket>::ceil_size_to_capacity(packet_map_cap);
            auto packet_map                 = dg::cyclic_unordered_node_map<GlobalIdentifier, AssembledPacket>(packet_map_bucket_cap);
 
            return std::make_unique<PacketAssembler>(std::move(packet_map),
                                                    max_segment_sz_per_stream,
                                                    stdx::make_unique_fair_atomic_flag(),
                                                    stdx::hdi_container<size_t>{max_consume_sz});
        }

        static auto get_random_hash_distributed_packet_assembler(std::vector<std::unique_ptr<PacketAssemblerInterface>> base_vec,
                                                                 size_t keyvalue_feed_cap = DEFAULT_KEYVALUE_FEED_SIZE){
            
            const size_t MIN_KEYVALUE_FEED_CAP  = size_t{1};
            const size_t MAX_KEYVALUE_FEED_CAP  = size_t{1} << 25;

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(keyvalue_feed_cap, MIN_KEYVALUE_FEED_CAP, MAX_KEYVALUE_FEED_CAP) != keyvalue_feed_cap){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t max_consume_sz   = std::numeric_limits<size_t>::max(); 
            auto base_arr           = std::make_unique<std::unique_ptr<PacketAssemblerInterface>[]>(base_vec.size());
            size_t base_arr_sz      = base_vec.size(); 

            for (size_t i = 0u; i < base_arr_sz; ++i){
                if (base_vec[i] == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                max_consume_sz  = std::min(max_consume_sz, base_vec[i]->max_consume_size());
                base_arr[i]     = std::move(base_vec[i]);
            }

            size_t trimmed_feed_cap = std::min(keyvalue_feed_cap, max_consume_sz);

            return std::make_unique<RandomHashDistributedPacketAssembler>(std::move(base_arr),
                                                                          base_arr_sz,
                                                                          trimmed_feed_cap,
                                                                          max_consume_sz);
        }

        static auto get_exhaustion_controlled_packet_assembler(std::unique_ptr<PacketAssemblerInterface> base,
                                                               std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                               std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<PacketAssemblerInterface>{
            
            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (executor == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (exhaustion_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExhaustionControlledPacketAssembler>(std::move(base),
                                                                         std::move(executor),
                                                                         std::move(exhaustion_controller));

        }

        static auto get_buffer_fifo_container(size_t buffer_capacity,
                                              size_t push_concurrency_sz,
                                              size_t consume_factor = 4u) -> std::unique_ptr<InBoundContainerInterface>{
            
            const size_t MIN_BUFFER_CAPACITY    = 1u;
            const size_t MAX_BUFFER_CAPACITY    = size_t{1} << 25;
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
            size_t upqueue_cap                  = stdx::ceil2(buffer_capacity);

            return std::make_unique<BufferFIFOContainer>(dg::pow2_cyclic_queue<BufferContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                         dg::pow2_cyclic_queue<internal_huge_kernel_buffer>(stdx::ulog2(upqueue_cap)),
                                                         upqueue_cap,
                                                         stdx::make_unique_fair_atomic_flag(),
                                                         stdx::hdi_container<size_t>{consume_sz});
        }

        static auto get_exhaustion_controlled_buffer_container(std::unique_ptr<InBoundContainerInterface> base,
                                                               std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> executor,
                                                               std::shared_ptr<ExhaustionControllerInterface> exhaustion_controller) -> std::unique_ptr<InBoundContainerInterface>{
            
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

        static auto get_reacting_buffer_container(std::unique_ptr<InBoundContainerInterface> base,
                                                  size_t reacting_threshold,
                                                  size_t concurrent_subscriber_cap,
                                                  std::chrono::nanoseconds wait_time,
                                                  size_t ease_threshold = 1u,
                                                  size_t busy_threshold = 1024u) -> std::unique_ptr<InBoundContainerInterface>{

            const size_t MIN_REACTING_THRESHOLD             = size_t{1};
            const size_t MAX_REACTING_THRESHOLD             = size_t{1} << 30;
            const std::chrono::nanoseconds MIN_WAIT_TIME    = std::chrono::nanoseconds{1}; 
            const std::chrono::nanoseconds MAX_WAIT_TIME    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes{1});
            const size_t MIN_EASE_THRESHOLD                 = size_t{1};
            const size_t MAX_EASE_THRESHOLD                 = size_t{1} << 30;
            const size_t MIN_BUSY_THRESHOLD                 = size_t{1};
            const size_t MAX_BUSY_THRESHOLD                 = size_t{1} << 30; 

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(reacting_threshold, MIN_REACTING_THRESHOLD, MAX_REACTING_THRESHOLD) != reacting_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(wait_time, MIN_WAIT_TIME, MAX_WAIT_TIME) != wait_time){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(ease_threshold, MIN_EASE_THRESHOLD, MAX_EASE_THRESHOLD) != ease_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_threshold, MIN_BUSY_THRESHOLD, MAX_BUSY_THRESHOLD) != busy_threshold){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ReactingBufferContainer>(std::move(base),
                                                             get_complex_reactor(reacting_threshold, concurrent_subscriber_cap),
                                                             wait_time,
                                                             ease_threshold,
                                                             busy_threshold);
        }

        static auto get_fair_inbound_buffer_container(size_t distribution_queue_sz,
                                                      size_t waiting_queue_sz,
                                                      size_t leftover_queue_sz,
                                                      size_t push_concurrency_sz,
                                                      size_t unit_sz) -> std::unique_ptr<InBoundContainerInterface>{
            
            const size_t MIN_DISTRIBUTION_QUEUE_SZ  = size_t{1};
            const size_t MAX_DISTRIBUTION_QUEUE_SZ  = size_t{1} << 20;
            const size_t MIN_WAITING_QUEUE_SZ       = size_t{1};
            const size_t MAX_WAITING_QUEUE_SZ       = size_t{1} << 20;
            const size_t MIN_LEFTOVER_QUEUE_SZ      = size_t{1};
            const size_t MAX_LEFTOVER_QUEUE_SZ      = size_t{1} << 20;
            const size_t MIN_UNIT_SZ                = size_t{1};
            const size_t MAX_UNIT_SZ                = size_t{1} << 20; 
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

            if (std::clamp(unit_sz, MIN_UNIT_SZ, MAX_UNIT_SZ) != unit_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(push_concurrency_sz, MIN_PUSH_CONCURRENCY_SZ, MAX_PUSH_CONCURRENCY_SZ) != push_concurrency_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            size_t queue_cap = distribution_queue_sz + waiting_queue_sz + leftover_queue_sz; 

            return std::make_unique<FairInBoundBufferContainer>(dg::pow2_cyclic_queue<FairBufferContainerWaitingItem>(stdx::ulog2(stdx::ceil2(push_concurrency_sz))),
                                                                dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>>(stdx::ulog2(stdx::ceil2(distribution_queue_sz))),
                                                                dg::pow2_cyclic_queue<std::pair<std::optional<dg::vector<internal_huge_kernel_buffer>> *, dg_binary_semaphore *>>(stdx::ulog2(stdx::ceil2(waiting_queue_sz))),
                                                                dg::pow2_cyclic_queue<dg::vector<internal_huge_kernel_buffer>>(stdx::ulog2(stdx::ceil2(leftover_queue_sz))),
                                                                get_default_lifo_unit_allocator<dg::vector<internal_huge_kernel_buffer>>(queue_cap),
                                                                stdx::make_unique_fair_atomic_flag(),
                                                                unit_sz);
        }


        static auto get_random_hash_distributed_buffer_container(std::vector<std::unique_ptr<InBoundContainerInterface>> base_vec) -> std::unique_ptr<InBoundContainerInterface>{

            const size_t MIN_BASE_VEC_SZ            = size_t{1};
            const size_t MAX_BASE_VEC_SZ            = size_t{1} << 20;

            if (std::clamp(base_vec.size(), MIN_BASE_VEC_SZ, MAX_BASE_VEC_SZ) != base_vec.size()){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (!stdx::is_pow2(base_vec.size())){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            auto base_vec_up        = std::make_unique<std::unique_ptr<InBoundContainerInterface>[]>(base_vec.size());
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

        static auto get_inbound_redistributor_worker(std::shared_ptr<InBoundContainerInterface> fr_warehouse,
                                                     std::shared_ptr<InBoundContainerInterface> to_warehouse,
                                                     size_t fr_warehouse_get_cap,
                                                     size_t to_warehouse_push_cap,
                                                     size_t busy_threshold) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{
                                            
            const size_t MIN_FR_WAREHOUSE_GET_CAP   = 1u;
            const size_t MAX_FR_WAREHOUSE_GET_CAP   = size_t{1} << 25;
            const size_t MIN_TO_WAREHOUSE_PUSH_CAP  = 1u;
            const size_t MAX_TO_WAREHOUSE_PUSH_CAP  = size_t{1} << 25;
            const size_t MIN_BUSY_THRESHOLD         = 0u;
            const size_t MAX_BUSY_THRESHOLD         = size_t{1} << 25;

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

            return std::make_unique<BufferContainerRedistributorWorker>(std::move(fr_warehouse),
                                                                        std::move(to_warehouse),
                                                                        fr_warehouse_get_cap,
                                                                        to_warehouse_push_cap,
                                                                        busy_threshold);
        } 

        static auto get_expiry_worker(std::shared_ptr<PacketAssemblerInterface> packet_assembler,
                                      std::shared_ptr<EntranceControllerInterface> entrance_controller,
                                      std::shared_ptr<BlackListGateInterface> blacklist_gate,
                                      size_t packet_assembler_vectorization_sz,
                                      size_t consumption_sz,
                                      size_t busy_consumption_sz) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{
            
            const size_t MIN_PACKET_ASSEMBLER_VECTORIZATION_SZ  = 1u;
            const size_t MAX_PACKET_ASSEMBLER_VECTORIZATION_SZ  = size_t{1} << 25; 
            const size_t MIN_CONSUMPTION_SZ                     = 1u;
            const size_t MAX_CONSUMPTION_SZ                     = size_t{1} << 25;
            const size_t MIN_BUSY_CONSUMPTION_SZ                = 0u;
            const size_t MAX_BUSY_CONSUMPTION_SZ                = size_t{1} << 25;

            if (packet_assembler == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (entrance_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (blacklist_gate == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(packet_assembler_vectorization_sz, MIN_PACKET_ASSEMBLER_VECTORIZATION_SZ, MAX_PACKET_ASSEMBLER_VECTORIZATION_SZ) != packet_assembler_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(consumption_sz, MIN_CONSUMPTION_SZ, MAX_CONSUMPTION_SZ) != consumption_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_consumption_sz, MIN_BUSY_CONSUMPTION_SZ, MAX_BUSY_CONSUMPTION_SZ) != busy_consumption_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<ExpiryWorker>(std::move(packet_assembler), std::move(entrance_controller), std::move(blacklist_gate), 
                                                  packet_assembler_vectorization_sz,
                                                  consumption_sz,
                                                  busy_consumption_sz);
        }

        static auto get_inbound_worker(std::shared_ptr<PacketAssemblerInterface> packet_assembler,
                                       std::shared_ptr<InBoundGateInterface> inbound_gate,
                                       std::shared_ptr<InBoundGateInterface> blacklist_gate,
                                       std::shared_ptr<InBoundContainerInterface> inbound_container,
                                       std::shared_ptr<EntranceControllerInterface> entrance_controller,
                                       std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device,
                                       std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
                                       size_t packet_assembler_vectorization_sz,
                                       size_t inbound_gate_vectorization_sz,
                                       size_t blacklist_gate_vectorization_sz,
                                       size_t entrance_controller_vectorization_sz,
                                       size_t inbound_container_vectorization_sz,
                                       size_t consumption_sz,
                                       size_t busy_consumption_sz) -> std::unique_ptr<dg::network_concurrency::WorkerInterface>{

            const size_t MIN_PACKET_ASSEMBLER_VECTORIZATION_SZ      = 1u;
            const size_t MAX_PACKET_ASSEMBLER_VECTORIZATION_SZ      = size_t{1} << 25;
            const size_t MIN_INBOUND_GATE_VECTORIZATION_SZ          = 1u;
            const size_t MAX_INBOUND_GATE_VECTORIZATION_SZ          = size_t{1} << 25;
            const size_t MIN_BLACKLIST_GATE_VECTORIZATION_SZ        = 1u;
            const size_t MAX_BLACKLIST_GATE_VECTORIZATION_SZ        = size_t{1} << 25;
            const size_t MIN_ENTRANCE_CONTROLLER_VECTORIZATION_SZ   = 1u;
            const size_t MAX_ENTRANCE_CONTROLLER_VECTORIZATION_SZ   = size_t{1} << 25;
            const size_t MIN_INBOUND_CONTAINER_VECTORIZATION_SZ     = 1u;
            const size_t MAX_INBOUND_CONTAINER_VECTORIZATION_SZ     = size_t{1} << 25;
            const size_t MIN_CONSUMPTION_SZ                         = 1u;
            const size_t MAX_CONSUMPTION_SZ                         = size_t{1} << 25;
            const size_t MIN_BUSY_CONSUMPTION_SZ                    = 0u;
            const size_t MAX_BUSY_CONSUMPTION_SZ                    = size_t{1} << 25;

            if (packet_assembler == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_gate == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (blacklist_gate == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (entrance_controller == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (infretry_device == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(packet_assembler_vectorization_sz, MIN_PACKET_ASSEMBLER_VECTORIZATION_SZ, MAX_PACKET_ASSEMBLER_VECTORIZATION_SZ) != packet_assembler_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(inbound_gate_vectorization_sz, MIN_INBOUND_GATE_VECTORIZATION_SZ, MAX_INBOUND_GATE_VECTORIZATION_SZ) != inbound_gate_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(blacklist_gate_vectorization_sz, MIN_BLACKLIST_GATE_VECTORIZATION_SZ, MAX_BLACKLIST_GATE_VECTORIZATION_SZ) != blacklist_gate_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(entrance_controller_vectorization_sz, MIN_ENTRANCE_CONTROLLER_VECTORIZATION_SZ, MAX_ENTRANCE_CONTROLLER_VECTORIZATION_SZ) != entrance_controller_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(inbound_container_vectorization_sz, MIN_INBOUND_CONTAINER_VECTORIZATION_SZ, MAX_INBOUND_CONTAINER_VECTORIZATION_SZ) != inbound_container_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(consumption_sz, MIN_CONSUMPTION_SZ, MAX_CONSUMPTION_SZ) != consumption_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(busy_consumption_sz, MIN_BUSY_CONSUMPTION_SZ, MAX_BUSY_CONSUMPTION_SZ) != busy_consumption_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<InBoundWorker>(std::move(packet_assembler),
                                                   std::move(inbound_gate),
                                                   std::move(blacklist_gate),
                                                   std::move(inbound_container),
                                                   std::move(entrance_controller),
                                                   std::move(infretry_device),
                                                   std::move(base),
                                                   packet_assembler_vectorization_sz,
                                                   inbound_gate_vectorization_sz,
                                                   blacklist_gate_vectorization_sz,
                                                   entrance_controller_vectorization_sz,
                                                   inbound_container_vectorization_sz,
                                                   consumption_sz,
                                                   busy_consumption_sz);
        }

        static auto get_flash_streamx_mailbox(std::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec,
                                              std::unique_ptr<PacketizerInterface> packetizer,
                                              std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
                                              std::shared_ptr<InBoundContainerInterface> inbound_container,
                                              std::shared_ptr<OutBoundRuleInterface> outbound_rule,
                                              size_t transmission_vectorization_sz) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

            const size_t MIN_TRANSMISSION_VECTORIZATION_SZ  = 1u;
            const size_t MAX_TRANSMISSION_VECTORIZATION_SZ  = size_t{1} << 25; 

            if (packetizer == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (base == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (inbound_container == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (outbound_rule == nullptr){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            if (std::clamp(transmission_vectorization_sz, MIN_TRANSMISSION_VECTORIZATION_SZ, MAX_TRANSMISSION_VECTORIZATION_SZ) != transmission_vectorization_sz){
                dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
            }

            return std::make_unique<MailBox>(to_dg_vector(std::move(daemon_vec)),
                                             std::move(packetizer),
                                             std::move(base),
                                             std::move(inbound_container),
                                             std::move(outbound_rule),
                                             transmission_vectorization_sz);
        }
    };

    struct Config{
        Address factory_addr;
        uint32_t packetizer_segment_bsz;
        uint32_t packetizer_max_bsz;
        bool packetizer_has_integrity_transmit;

        uint32_t gate_controller_ato_component_sz;
        uint32_t gate_controller_ato_map_capacity;
        std::chrono::nanoseconds gate_controller_ato_dur;
        uint32_t gate_controller_ato_keyvalue_feed_cap;
        bool gate_controller_ato_is_voided; 

        uint32_t gate_controller_blklst_component_sz;
        uint32_t gate_controller_blklst_bloomfilter_cap;
        uint32_t gate_controller_blklst_bloomfilter_rehash_sz;
        uint32_t gate_controller_blklst_bloomfilter_reliability_decay_factor;
        uint32_t gate_controller_blklst_keyvalue_feed_cap; 
        bool gate_controller_blklst_is_voided; 

        uint32_t latency_controller_tick_wait_queue_cap;
        uint32_t latency_controller_component_sz;
        uint32_t latency_controller_queue_cap;
        std::chrono::nanoseconds latency_controller_expiry_period;
        uint32_t latency_controller_keyvalue_feed_cap;
        bool latency_controller_has_exhaustion_control; 
        bool latency_controller_is_voided;

        uint32_t packet_assembler_component_sz;
        uint32_t packet_assembler_map_cap;
        uint32_t packet_assembler_max_segment_per_stream;
        uint32_t packet_assembler_keyvalue_feed_cap;
        bool packet_assembler_has_exhaustion_control; 

        uint32_t inbound_container_component_sz;
        uint32_t inbound_container_cap;
        uint32_t inbound_container_push_concurrent_sz;
        bool inbound_container_has_exhaustion_control;
        bool inbound_container_has_react_pattern;
        uint32_t inbound_container_react_sz;
        uint32_t inbound_container_subscriber_cap;
        std::chrono::nanoseconds inbound_container_react_latency;
        bool inbound_container_has_redistributor;
        size_t inbound_container_redistributor_distribution_queue_sz;
        size_t inbound_container_redistributor_waiting_queue_sz;
        size_t inbound_container_redistributor_concurrent_sz;
        size_t inbound_container_redistributor_push_concurrent_sz;
        size_t inbound_container_redistributor_unit_sz;

        uint32_t expiry_worker_count;
        uint32_t expiry_worker_packet_assembler_vectorization_sz;
        uint32_t expiry_worker_consume_sz;
        uint32_t expiry_worker_busy_consume_sz;

        uint32_t inbound_worker_count;
        uint32_t inbound_worker_packet_assembler_vectorization_sz;
        uint32_t inbound_worker_inbound_gate_vectorization_sz;
        uint32_t inbound_worker_blacklist_gate_vectorization_sz;
        uint32_t inbound_worker_latency_controller_vectorization_sz;
        uint32_t inbound_worker_inbound_container_vectorization_sz;
        uint32_t inbound_worker_consume_sz;
        uint32_t inbound_worker_busy_consume_sz;
        uint32_t inbound_redistributor_worker_suck_cap;
        uint32_t inbound_redistributor_worker_push_cap;
        uint32_t inbound_redistributor_worker_busy_threshold; 

        uint32_t mailbox_transmission_vectorization_sz; 

        std::shared_ptr<OutBoundRuleInterface> outbound_rule;
        std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device;
        std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base; //alright people might not like std::unique_ptr<> for throwing reasons, yet it is practice of extensibility to make this std::unique<>, we have to break practice for now
    };

    struct ConfigMaker{

        private:

            static auto make_packetizer(Config config) -> std::unique_ptr<PacketizerInterface>{

                return ComponentFactory::get_packetizer(config.factory_addr, config.packetizer_segment_bsz, config.packetizer_max_bsz, config.packetizer_has_integrity_transmit);
            }

            static auto make_ato_gate_controller(Config config) -> std::unique_ptr<InBoundGateInterface>{
                
                if (config.gate_controller_ato_is_voided)
                {
                    return ComponentFactory::get_void_inbound_gate();
                }

                if (config.gate_controller_ato_component_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.gate_controller_ato_component_sz == 1u){
                    return ComponentFactory::get_temporal_absolute_timeout_inbound_gate(config.gate_controller_ato_map_capacity, 
                                                                                        config.gate_controller_ato_dur);
                }

                std::vector<std::unique_ptr<InBoundGateInterface>> inbound_gate_vec{};

                for (size_t i = 0u; i < config.gate_controller_ato_component_sz; ++i){
                    inbound_gate_vec.push_back(ComponentFactory::get_temporal_absolute_timeout_inbound_gate(config.gate_controller_ato_map_capacity,
                                                                                                            config.gate_controller_ato_dur));
                }

                return ComponentFactory::get_random_hash_distributed_inbound_gate(std::move(inbound_gate_vec), config.gate_controller_ato_keyvalue_feed_cap);
            }

            static auto make_blklst_gate_controller(Config config) -> std::unique_ptr<BlackListGateInterface>{

                if (config.gate_controller_blklst_is_voided)
                {
                    return ComponentFactory::get_void_blacklist_gate();
                }

                if (config.gate_controller_blklst_component_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.gate_controller_blklst_component_sz == 1u){
                    return ComponentFactory::get_temporal_blacklist_gate(config.gate_controller_blklst_bloomfilter_cap,
                                                                         config.gate_controller_blklst_bloomfilter_rehash_sz,
                                                                         config.gate_controller_blklst_bloomfilter_reliability_decay_factor);
                }

                std::vector<std::unique_ptr<BlackListGateInterface>> blklst_gate_vec{};

                for (size_t i = 0u; i < config.gate_controller_blklst_component_sz; ++i){
                    blklst_gate_vec.push_back(ComponentFactory::get_temporal_blacklist_gate(config.gate_controller_blklst_bloomfilter_cap,
                                                                                            config.gate_controller_blklst_bloomfilter_rehash_sz,
                                                                                            config.gate_controller_blklst_bloomfilter_reliability_decay_factor));
                }

                return ComponentFactory::get_random_hash_distributed_blacklist_gate(std::move(blklst_gate_vec), config.gate_controller_blklst_keyvalue_feed_cap);
            }

            static auto make_packet_assembler(Config config) -> std::unique_ptr<PacketAssemblerInterface>{

                if (config.packet_assembler_component_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.packet_assembler_component_sz == 1u){
                    return ComponentFactory::get_packet_assembler(config.packet_assembler_map_cap,
                                                                  config.packet_assembler_max_segment_per_stream);   
                }

                std::vector<std::unique_ptr<PacketAssemblerInterface>> pktasmblr_vec{};

                for (size_t i = 0u; i < config.packet_assembler_component_sz; ++i){
                    pktasmblr_vec.push_back(ComponentFactory::get_packet_assembler(config.packet_assembler_map_cap,
                                                                                   config.packet_assembler_max_segment_per_stream));
                }

                return ComponentFactory::get_random_hash_distributed_packet_assembler(std::move(pktasmblr_vec), config.packet_assembler_keyvalue_feed_cap);
            }

            static auto make_inbound_container(Config config) -> std::unique_ptr<InBoundContainerInterface>{

                if (config.inbound_container_component_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.inbound_container_has_exhaustion_control){
                    if (config.inbound_container_has_react_pattern){
                        if (config.inbound_container_component_sz == 1u){
                            return ComponentFactory::get_exhaustion_controlled_buffer_container(ComponentFactory::get_reacting_buffer_container(ComponentFactory::get_buffer_fifo_container(config.inbound_container_cap,
                                                                                                                                                                                            config.inbound_container_push_concurrent_sz),
                                                                                                                                                config.inbound_container_react_sz,
                                                                                                                                                config.inbound_container_subscriber_cap,
                                                                                                                                                config.inbound_container_react_latency),
                                                                                                config.infretry_device,
                                                                                                ComponentFactory::get_no_exhaustion_controller());
                        } else{
                            auto inbound_container_vec = std::vector<std::unique_ptr<InBoundContainerInterface>>(config.inbound_container_component_sz);
                            std::generate(inbound_container_vec.begin(), inbound_container_vec.end(), std::bind_front(ComponentFactory::get_buffer_fifo_container, config.inbound_container_cap, config.inbound_container_push_concurrent_sz, 4u)); //

                            return ComponentFactory::get_exhaustion_controlled_buffer_container(ComponentFactory::get_reacting_buffer_container(ComponentFactory::get_random_hash_distributed_buffer_container(std::move(inbound_container_vec)),
                                                                                                                                                config.inbound_container_react_sz,
                                                                                                                                                config.inbound_container_subscriber_cap,
                                                                                                                                                config.inbound_container_react_latency),
                                                                                                config.infretry_device,
                                                                                                ComponentFactory::get_no_exhaustion_controller());
                        }
                    } else{
                        if (config.inbound_container_component_sz == 1u){
                            return ComponentFactory::get_exhaustion_controlled_buffer_container(ComponentFactory::get_buffer_fifo_container(config.inbound_container_cap,
                                                                                                                                            config.inbound_container_push_concurrent_sz),
                                                                                                config.infretry_device,
                                                                                                ComponentFactory::get_no_exhaustion_controller());
                        } else{
                            auto inbound_container_vec = std::vector<std::unique_ptr<InBoundContainerInterface>>(config.inbound_container_component_sz);
                            std::generate(inbound_container_vec.begin(), inbound_container_vec.end(), std::bind_front(ComponentFactory::get_buffer_fifo_container, config.inbound_container_cap, config.inbound_container_push_concurrent_sz, 4u)); //

                            return ComponentFactory::get_exhaustion_controlled_buffer_container(ComponentFactory::get_random_hash_distributed_buffer_container(std::move(inbound_container_vec)),
                                                                                                config.infretry_device,
                                                                                                ComponentFactory::get_no_exhaustion_controller());
                        }
                    }
                } else{
                    if (config.inbound_container_has_react_pattern){
                        if (config.inbound_container_component_sz == 1u){
                            return ComponentFactory::get_reacting_buffer_container(ComponentFactory::get_buffer_fifo_container(config.inbound_container_cap, config.inbound_container_push_concurrent_sz),
                                                                                   config.inbound_container_react_sz,
                                                                                   config.inbound_container_subscriber_cap,
                                                                                   config.inbound_container_react_latency);
                        } else{
                            auto inbound_container_vec = std::vector<std::unique_ptr<InBoundContainerInterface>>(config.inbound_container_component_sz);
                            std::generate(inbound_container_vec.begin(), inbound_container_vec.end(), std::bind_front(ComponentFactory::get_buffer_fifo_container, config.inbound_container_cap, config.inbound_container_push_concurrent_sz, 4u)); //

                            return ComponentFactory::get_reacting_buffer_container(ComponentFactory::get_random_hash_distributed_buffer_container(std::move(inbound_container_vec)),
                                                                                   config.inbound_container_react_sz,
                                                                                   config.inbound_container_subscriber_cap,
                                                                                   config.inbound_container_react_latency);
                        }
                    } else{
                        if (config.inbound_container_component_sz == 1u){
                            return ComponentFactory::get_buffer_fifo_container(config.inbound_container_cap, config.inbound_container_push_concurrent_sz);
                        } else{
                            auto inbound_container_vec = std::vector<std::unique_ptr<InBoundContainerInterface>>(config.inbound_container_component_sz);
                            std::generate(inbound_container_vec.begin(), inbound_container_vec.end(), std::bind_front(ComponentFactory::get_buffer_fifo_container, config.inbound_container_cap, config.inbound_container_push_concurrent_sz, 4u)); //

                            return ComponentFactory::get_random_hash_distributed_buffer_container(std::move(inbound_container_vec));
                        }
                    }
                }
            }

            static auto make_fair_inbound_container(Config config) -> std::unique_ptr<InBoundContainerInterface>{

                if (!config.inbound_container_has_redistributor){
                    return nullptr;
                }
                
                return ComponentFactory::get_fair_inbound_buffer_container(config.inbound_container_redistributor_distribution_queue_sz,
                                                                           config.inbound_container_redistributor_waiting_queue_sz,
                                                                           config.inbound_container_redistributor_concurrent_sz,
                                                                           config.inbound_container_redistributor_push_concurrent_sz,
                                                                           config.inbound_container_redistributor_unit_sz);
            }

            static auto make_latency_controller(Config config) -> std::unique_ptr<EntranceControllerInterface>{

                if (config.latency_controller_is_voided)
                {
                    return ComponentFactory::get_void_entrance_controller();
                }

                if (config.latency_controller_component_sz == 0u){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (config.latency_controller_has_exhaustion_control){
                    if (config.latency_controller_component_sz == 1u){
                        return ComponentFactory::get_exhaustion_controlled_entrance_controller(ComponentFactory::get_entrance_controller(config.latency_controller_tick_wait_queue_cap,
                                                                                                                                         config.latency_controller_queue_cap,
                                                                                                                                         config.latency_controller_expiry_period),
                                                                                               config.infretry_device,
                                                                                               ComponentFactory::get_no_exhaustion_controller());
                    } else{
                        std::vector<std::unique_ptr<EntranceControllerInterface>> entrance_controller_vec(config.latency_controller_component_sz);
                        auto gen = std::bind_front(ComponentFactory::get_entrance_controller, config.latency_controller_tick_wait_queue_cap, config.latency_controller_queue_cap, config.latency_controller_expiry_period, 2u);
                        std::generate(entrance_controller_vec.begin(), entrance_controller_vec.end(), gen);

                        return ComponentFactory::get_exhaustion_controlled_entrance_controller(ComponentFactory::get_random_hash_distributed_entrance_controller(std::move(entrance_controller_vec),
                                                                                                                                                                 config.latency_controller_keyvalue_feed_cap),
                                                                                               config.infretry_device,
                                                                                               ComponentFactory::get_no_exhaustion_controller());
                    }
                } else{
                    if (config.latency_controller_component_sz == 1u){
                        return ComponentFactory::get_entrance_controller(config.latency_controller_tick_wait_queue_cap,
                                                                         config.latency_controller_queue_cap,
                                                                         config.latency_controller_expiry_period);
                    } else{
                        std::vector<std::unique_ptr<EntranceControllerInterface>> entrance_controller_vec(config.latency_controller_component_sz);
                        auto gen = std::bind_front(ComponentFactory::get_entrance_controller, config.latency_controller_tick_wait_queue_cap, config.latency_controller_queue_cap, config.latency_controller_expiry_period, 2u);
                        std::generate(entrance_controller_vec.begin(), entrance_controller_vec.end(), gen);

                        return ComponentFactory::get_random_hash_distributed_entrance_controller(std::move(entrance_controller_vec),
                                                                                                 config.latency_controller_keyvalue_feed_cap);
                    }
                }
            }

            static auto make_mailbox_x(std::shared_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface> base,
                                       std::shared_ptr<OutBoundRuleInterface> outbound_rule,
                                       std::unique_ptr<PacketizerInterface> packetizer,
                                       std::unique_ptr<InBoundGateInterface> inbound_gate,
                                       std::unique_ptr<BlackListGateInterface> blacklist_gate,
                                       std::unique_ptr<PacketAssemblerInterface> packet_assembler,
                                       std::unique_ptr<InBoundContainerInterface> inbound_container,
                                       std::unique_ptr<InBoundContainerInterface> fair_inbound_container,
                                       std::unique_ptr<EntranceControllerInterface> entrance_controller,
                                       size_t expiry_worker_count,
                                       size_t expiry_worker_packet_assembler_vectorization_sz,
                                       size_t expiry_worker_consume_sz,
                                       size_t expiry_worker_busy_consume_sz,
                                       size_t inbound_worker_count,
                                       size_t inbound_worker_packet_assembler_vectorization_sz,
                                       size_t inbound_worker_inbound_gate_vectorization_sz,
                                       size_t inbound_worker_blacklist_gate_vectorization_sz,
                                       size_t inbound_worker_entrance_controller_vectorization_sz,
                                       size_t inbound_worker_inbound_container_vectorization_sz,
                                       size_t inbound_worker_consume_sz,
                                       size_t inbound_worker_busy_consume_sz,
                                       size_t inbound_redistributor_worker_suck_cap,
                                       size_t inbound_redistributor_worker_push_cap,
                                       size_t inbound_redistributor_worker_busy_threshold,
                                       bool has_inbound_redistribution,
                                       size_t mailbox_transmission_vectorization_sz,
                                       std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

                const size_t MIN_EXPIRY_WORKER_COUNT        = 1u;
                const size_t MAX_EXPIRY_WORKER_COUNT        = 1024u; 
                const size_t MIN_INBOUND_WORKER_COUNT       = 1u;
                const size_t MAX_INBOUND_WORKER_COUNT       = 1024u;

                if (base == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (packetizer == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (inbound_gate == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (blacklist_gate == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (packet_assembler == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (inbound_container == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (entrance_controller == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(expiry_worker_count, MIN_EXPIRY_WORKER_COUNT, MAX_EXPIRY_WORKER_COUNT) != expiry_worker_count){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(inbound_worker_count, MIN_INBOUND_WORKER_COUNT, MAX_INBOUND_WORKER_COUNT) != inbound_worker_count){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                if (infretry_device == nullptr){
                    dg::network_exception::throw_exception(dg::network_exception::INVALID_ARGUMENT);
                }

                std::shared_ptr<InBoundGateInterface> inbound_gate_sp                   = std::move(inbound_gate);
                std::shared_ptr<BlackListGateInterface> blacklist_gate_sp               = std::move(blacklist_gate);
                std::shared_ptr<PacketAssemblerInterface> packet_assembler_sp           = std::move(packet_assembler);
                std::shared_ptr<InBoundContainerInterface> inbound_container_sp         = std::move(inbound_container);
                std::shared_ptr<InBoundContainerInterface> fair_inbound_container_sp    = std::move(fair_inbound_container);
                std::shared_ptr<InBoundContainerInterface> enduser_inbound_container_sp = {};
                std::shared_ptr<EntranceControllerInterface> entrance_controller_sp     = std::move(entrance_controller);
                std::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec   = {};

                if (has_inbound_redistribution){
                    enduser_inbound_container_sp = fair_inbound_container_sp;
                } else{
                    enduser_inbound_container_sp = inbound_container_sp;
                }

                if (has_inbound_redistribution){
                    auto worker = ComponentFactory::get_inbound_redistributor_worker(inbound_container_sp,
                                                                                     fair_inbound_container_sp,
                                                                                     inbound_redistributor_worker_suck_cap,
                                                                                     inbound_redistributor_worker_push_cap,
                                                                                     inbound_redistributor_worker_busy_threshold);

                    auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker)));
                    daemon_vec.emplace_back(std::move(daemon_handle));            
                }

                for (size_t i = 0u; i < inbound_worker_count; ++i){
                    auto worker = ComponentFactory::get_inbound_worker(packet_assembler_sp, inbound_gate_sp, blacklist_gate_sp, 
                                                                       inbound_container_sp, entrance_controller_sp, infretry_device,
                                                                       base,
                                                                       inbound_worker_packet_assembler_vectorization_sz,
                                                                       inbound_worker_inbound_gate_vectorization_sz,
                                                                       inbound_worker_blacklist_gate_vectorization_sz,
                                                                       inbound_worker_entrance_controller_vectorization_sz,
                                                                       inbound_worker_inbound_container_vectorization_sz,
                                                                       inbound_worker_consume_sz,
                                                                       inbound_worker_busy_consume_sz);

                    auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker)));
                    daemon_vec.emplace_back(std::move(daemon_handle));
                }

                for (size_t i = 0u; i < expiry_worker_count; ++i){
                    auto worker = ComponentFactory::get_expiry_worker(packet_assembler_sp, entrance_controller_sp, blacklist_gate_sp,
                                                                      expiry_worker_packet_assembler_vectorization_sz,
                                                                      expiry_worker_consume_sz,
                                                                      expiry_worker_busy_consume_sz);

                    auto daemon_handle  = dg::network_exception_handler::throw_nolog(dg::network_concurrency::daemon_saferegister(dg::network_concurrency::IO_DAEMON, std::move(worker)));
                    daemon_vec.emplace_back(std::move(daemon_handle));
                }

                return ComponentFactory::get_flash_streamx_mailbox(std::move(daemon_vec),
                                                                   std::move(packetizer),
                                                                   base,
                                                                   enduser_inbound_container_sp,
                                                                   outbound_rule,
                                                                   mailbox_transmission_vectorization_sz);
            }

        public:

            static auto make(Config config) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

                return make_mailbox_x(config.base,
                                      config.outbound_rule,
                                      make_packetizer(config),
                                      make_ato_gate_controller(config),
                                      make_blklst_gate_controller(config),
                                      make_packet_assembler(config),
                                      make_inbound_container(config),
                                      make_fair_inbound_container(config),
                                      make_latency_controller(config),
                                      config.expiry_worker_count,
                                      config.expiry_worker_packet_assembler_vectorization_sz,
                                      config.expiry_worker_consume_sz,
                                      config.expiry_worker_busy_consume_sz,
                                      config.inbound_worker_count,
                                      config.inbound_worker_packet_assembler_vectorization_sz,
                                      config.inbound_worker_inbound_gate_vectorization_sz,
                                      config.inbound_worker_blacklist_gate_vectorization_sz,
                                      config.inbound_worker_latency_controller_vectorization_sz,
                                      config.inbound_worker_inbound_container_vectorization_sz,
                                      config.inbound_worker_consume_sz,
                                      config.inbound_worker_busy_consume_sz,
                                      config.inbound_redistributor_worker_suck_cap,
                                      config.inbound_redistributor_worker_push_cap,
                                      config.inbound_redistributor_worker_busy_threshold,
                                      config.inbound_container_has_redistributor,
                                      config.mailbox_transmission_vectorization_sz,
                                      config.infretry_device);
            }
    };

    extern auto get_empty_outbound_rule() -> std::unique_ptr<OutBoundRuleInterface>{

        return ComponentFactory::get_empty_outbound_rule();
    } 

    extern auto spawn(Config config) -> std::unique_ptr<dg::network_kernel_mailbox_impl1::core::MailboxInterface>{

        return ConfigMaker::make(config);
    }
}

#endif
