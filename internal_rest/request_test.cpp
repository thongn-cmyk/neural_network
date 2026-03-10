#include <iostream>
#include "network_concurrency.h"
#include <string>
#include <thread>
#include <mutex>
#include "network_kernel_mailbox_impl1.h"
#include "network_kernel_mailbox_impl1_flash_stream_x.h"
#include "network_kernel_mailbox_impl1_channel_x.h"
#include <random>

//alright, we'd test requests today
//basically we've done everything we could to achieve low latency, we'd need jumbo frames or friends to achieve higher bandwidth

static inline constexpr size_t SERVER_IN_CHANNEL    = 1234;
static inline constexpr size_t CLIENT_IN_CHANNEL    = 12345;
static inline constexpr size_t RECV_CHANNEL_MSG_CAP = size_t{1} << 16;

void init_basic()
{
    init_concurrency();

    std::cout << "initializing cron subsystem\n"; 
    cron_subsystem::init();

    std::cout << "initializing network_cron\n";
    dg_sock::network_cron::init();

    std::cout << "initializing stack allocation\n";
    dg_sock::network_stack_allocation::init();
    
    std::cout << "initializing network allocation\n";
    dg_sock::network_allocation::init();

    std::cout << "initializing network randomizer\n";
    dg_sock::network_randomizer::init();

    std::filesystem::path tmp_file = std::filesystem::temp_directory_path() / "request_test.txt";

    std::cout << "initializing logging subsystem\n";
    dg_sock::network_log::init(tmp_file);
}

void init_mailbox()
{
    auto [retry_device_up, destructor] = dg_sock::network_concurrency_infretry_x::get_infretry_machine(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1000))); 
    std::shared_ptr<dg_sock::network_concurrency_infretry_x::ExecutorInterface> retry_device = std::move(retry_device_up);

    std::cout << "initializing base socket memory\n";

    dg_sock::network_kernel_mailbox_impl1::allocation::init({.total_mempiece_count = 1 << 20,
                                                             .mempiece_sz = 1 << 10,
                                                             .affined_refill_sz = 1 << 8,
                                                             .affined_mem_vec_capacity = 1 << 8,
                                                             .affined_free_vec_capacity = 1 << 8});

    std::cout << "initializing base socket configuration\n";

    auto base_socket_config = dg_sock::network_kernel_mailbox_impl1::Config
    {
        .num_kernel_inbound_worker = 1,
        .num_process_inbound_worker = 1,
        .num_outbound_worker = 1,
        .num_kernel_rescue_worker = 0,
        .num_retry_worker = 1,

        .inbound_socket_concurrency_sz = 1,
        .outbound_socket_concurrency_sz = 1,
        .sin_fam = AF_INET,
        .comm = SOCK_DGRAM,
        .protocol = 0,
        .host_ip = {.ip = dg_sock::network_kernel_mailbox_impl1::utility::ipv4_std_formatted_str_to_compact("127.0.0.1").value()},
        .host_port_inbound = 5000,
        .host_port_outbound = 5001,

        .is_void_retransmission_controller = false,
        .retransmission_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2)),
        .retransmission_concurrency_sz = 2,
        .retransmission_queue_cap = 1 << 16,
        .retransmission_user_queue_cap = 1 << 14,
        .retransmission_packet_cap = 20,
        .retransmission_idhashset_cap = 1 << 24,
        .retransmission_ticking_clock_resolution = 1 << 10,
        .retransmission_has_react_pattern = false,
        .retransmission_react_sz = 1 << 8,
        .retransmission_react_queue_cap = 1 << 8,
        .retransmission_user_push_concurrency_sz = 1024,
        .retransmission_retriable_push_concurrency_sz = 1024,
        .retransmission_unit_sz = 1024,
        .retransmission_react_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10)),
        .retransmission_has_exhaustion_control = true,

        .inbound_buffer_concurrency_sz = 8,
        .inbound_buffer_container_cap = 1 << 14,
        .inbound_buffer_has_react_pattern = true,
        .inbound_buffer_react_sz = 1 << 6,
        .inbound_buffer_react_queue_cap = 1 << 12,
        .inbound_buffer_push_concurrency_sz = 1024,
        .inbound_buffer_react_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10)),
        .inbound_buffer_has_fair_distribution = true,
        .inbound_buffer_has_fair_redistribution = false,
        .inbound_buffer_fair_distribution_queue_cap = 1 << 14,
        .inbound_buffer_fair_waiting_queue_cap = 1 << 10,
        .inbound_buffer_fair_leftover_queue_cap = 1 << 10,
        .inbound_buffer_fair_push_concurrency_sz = 1024,
        .inbound_buffer_fair_unit_sz = 1 << 12,
        .inbound_buffer_has_exhaustion_control = true,

        .inbound_packet_concurrency_sz = 8,
        .inbound_packet_container_cap = 1 << 14,
        .inbound_packet_has_react_pattern = true,
        .inbound_packet_react_sz = 1 << 10,
        .inbound_packet_react_queue_cap = 1 << 12,
        .inbound_packet_push_concurrency_sz = 1024,
        .inbound_packet_react_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10)),
        .inbound_packet_has_fair_distribution = true,
        .inbound_packet_has_fair_redistribution = false,
        .inbound_packet_fair_packet_queue_cap = 1 << 14,
        .inbound_packet_fair_waiting_queue_cap = 1 << 10,
        .inbound_packet_fair_leftover_queue_cap = 1 << 10,
        .inbound_packet_fair_push_concurrency_sz = 1024,
        .inbound_packet_fair_unit_sz = 1 << 12,
        .inbound_packet_has_exhaustion_control = true, 

        .inbound_idhashset_concurrency_sz = 8,
        .inbound_idhashset_cap = 1 << 18,

        .worker_inbound_buffer_fair_container_fr_warehouse_get_cap = 1 << 12,
        .worker_inbound_buffer_fair_container_to_warehouse_push_cap = 1 << 12,
        .worker_inbound_buffer_fair_container_busy_threshold = 0u,

        .worker_inbound_fair_packet_fr_warehouse_get_cap = 1 << 12,
        .worker_inbound_fair_packet_to_warehouse_push_cap = 1 << 12,
        .worker_inbound_fair_packet_busy_threshold = 0u,

        .worker_inbound_buffer_accumulation_sz = 1 << 12,
        .worker_inbound_packet_consumption_cap = 1 << 12,
        .worker_inbound_packet_busy_threshold_sz = 1,
        .worker_rescue_packet_sz_per_transmit = 1 << 6,
        .worker_kernel_rescue_dispatch_threshold = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(4)),
        .worker_kernel_rescue_disaster_sleep_dur = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)),
        .worker_retransmission_consumption_cap = 1000,
        .worker_retransmission_busy_threshold_sz = 1,
        .worker_outbound_packet_consumption_cap = 10,
        .worker_outbound_packet_busy_threshold_sz = 1,

        .mailbox_inbound_cap = size_t{1} << 16,
        .mailbox_outbound_cap = size_t{1} << 16,
        .traffic_reset_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)),

        .outbound_transmit_frequency = uint32_t{1} << 18,

        .outbound_container_request_packet_container_cap = 1 << 14,
        .outbound_container_ack_packet_container_cap = 1 << 14,
        .outbound_container_krescue_packet_container_cap = 1 << 14,
        .outbound_container_waiting_queue_capacity = 1 << 14,
        .outbound_container_leftover_queue_capacity = 1 << 10,
        .outbound_container_unit_sz = 1 << 10,
        .outbound_container_push_concurrency_sz = 1024,
        .outbound_container_has_exhaustion_control = false,

        .inbound_tc_has_borderline_per_inbound_worker = true,
        .inbound_tc_peraddr_cap = uint32_t{1} << 20,
        .inbound_tc_global_cap = uint32_t{1} << 20,
        .inbound_tc_addrmap_cap = uint32_t{1} << 20,
        .inbound_tc_side_cap = uint32_t{1} << 20,
        .inbound_tc_is_voided = true,

        .outbound_tc_has_borderline_per_outbound_worker = true,
        .outbound_tc_border_line_sz = uint32_t{1} << 20,
        .outbound_tc_peraddr_cap = uint32_t{1} << 20,
        .outbound_tc_global_cap = uint32_t{1} << 20,
        .outbound_tc_addrmap_cap = uint32_t{1} << 20,
        .outbound_tc_side_cap = uint32_t{1} << 20,
        .outbound_tc_is_voided = true,

        .natip_controller = dg_sock::network_kernel_mailbox_impl1::get_default_natip_controller(std::make_unique<IPSiever>(), std::make_unique<IPSiever>(), 1024, 1024),
        .retry_device = retry_device
    };

    auto stream_socket_config = dg_sock::network_kernel_mailbox_impl1_flash_stream_x::Config
    {
        .factory_addr = {.ip = dg_sock::network_kernel_mailbox_impl1::utility::ipv4_std_formatted_str_to_compact("127.0.0.1").value(),
                         .port = 5001},
        .packetizer_segment_bsz = size_t{1} << 0,
        .packetizer_max_bsz = size_t{1} << 20,
        .packetizer_has_integrity_transmit = true,

        .gate_controller_ato_component_sz = 1,
        .gate_controller_ato_map_capacity = size_t{1} << 16,
        .gate_controller_ato_dur = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)),
        .gate_controller_ato_keyvalue_feed_cap = size_t{1} << 10,
        .gate_controller_ato_is_voided = true,

        .gate_controller_blklst_component_sz = 1,
        .gate_controller_blklst_bloomfilter_cap = size_t{1} << 20,
        .gate_controller_blklst_bloomfilter_rehash_sz = 4,
        .gate_controller_blklst_bloomfilter_reliability_decay_factor = 10,
        .gate_controller_blklst_keyvalue_feed_cap = size_t{1} << 10,
        .gate_controller_blklst_is_voided = true, 

        .latency_controller_tick_wait_queue_cap = size_t{1} << 10,
        .latency_controller_component_sz = 1,
        .latency_controller_queue_cap = size_t{1} << 20,
        .latency_controller_expiry_period = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(100)),
        .latency_controller_keyvalue_feed_cap = size_t{1} << 10,
        .latency_controller_has_exhaustion_control = true,
        .latency_controller_is_voided = true,

        .packet_assembler_component_sz = 16,
        .packet_assembler_map_cap = size_t{1} << 20,
        .packet_assembler_max_segment_per_stream = size_t{1} << 11,
        .packet_assembler_keyvalue_feed_cap = size_t{1} << 10,
        .packet_assembler_has_exhaustion_control = true,

        .inbound_container_component_sz = 16,
        .inbound_container_cap = size_t{1} << 20,
        .inbound_container_push_concurrent_sz = size_t{1} << 10,
        .inbound_container_has_exhaustion_control = true,
        .inbound_container_has_react_pattern = true,
        .inbound_container_react_sz = size_t{1} << 10,
        .inbound_container_subscriber_cap = size_t{1} << 10,
        .inbound_container_react_latency = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100)),
        .inbound_container_has_only_redistributor = false,
        .inbound_container_has_redistributor = true,
        .inbound_container_redistributor_distribution_queue_sz = size_t{1} << 20,
        .inbound_container_redistributor_waiting_queue_sz = size_t{1} << 20,
        .inbound_container_redistributor_concurrent_sz = size_t{1} << 20,
        .inbound_container_redistributor_push_concurrent_sz = size_t{1} << 10,
        .inbound_container_redistributor_unit_sz = size_t{1} << 6,

        .expiry_worker_count = 1,
        .expiry_worker_packet_assembler_vectorization_sz = size_t{1} << 10,
        .expiry_worker_consume_sz = size_t{1} << 10,
        .expiry_worker_busy_consume_sz = 1,

        .inbound_worker_count = 1,
        .inbound_worker_packet_assembler_vectorization_sz = size_t{1} << 10,
        .inbound_worker_inbound_gate_vectorization_sz = size_t{1} << 10,
        .inbound_worker_blacklist_gate_vectorization_sz = size_t{1} << 10,
        .inbound_worker_latency_controller_vectorization_sz = size_t{1} << 10,
        .inbound_worker_inbound_container_vectorization_sz = size_t{1} << 10,
        .inbound_worker_consume_sz = size_t{1} << 10,
        .inbound_worker_busy_consume_sz = 1,
        .inbound_redistributor_worker_suck_cap = size_t{1} << 10,
        .inbound_redistributor_worker_push_cap = size_t{1} << 10,
        .inbound_redistributor_worker_busy_threshold = 1,

        .mailbox_transmission_vectorization_sz = size_t{1} << 10,

        .outbound_rule = dg_sock::network_kernel_mailbox_impl1_flash_stream_x::get_empty_outbound_rule(),
        .infretry_device = retry_device,
        .base = nullptr
    };

    auto channel_socket_config = dg_sock::network_kernel_mailbox_impl1::channel_x::Config
    {
        .channel_kind_map       = {{SERVER_IN_CHANNEL, dg_sock::network_kernel_mailbox_impl1_channel_x::PreconfiguratedStickyChannelContainer::THOUSAND_CHANNEL_CODEX},
                                   {CLIENT_IN_CHANNEL, dg_sock::network_kernel_mailbox_impl1_channel_x::PreconfiguratedStickyChannelContainer::THOUSAND_CHANNEL_CODEX}}, // <uint32_t, channel_t>

        .channel_msg_cap_map    = {{SERVER_IN_CHANNEL, RECV_CHANNEL_MSG_CAP},
                                   {CLIENT_IN_CHANNEL, RECV_CHANNEL_MSG_CAP}}, // <uint32_t, uint64_t>

        .assorter_worker_sz     = 1
    };

    std::cout << "initializing socket\n";

    dg_sock::network_kernel_mailbox::init(
    {
        .base_config    = base_socket_config,
        .stream_config  = stream_socket_config,
        .channel_config = channel_socket_config
    });
}

void init_rest_server()
{

}

void init_rest_client()
{

}

void test_rest_client()
{

}

int main()
{
    init_basic();
    init_mailbox();
    init_rest_server();
    init_rest_client();
    test_rest_client();
}