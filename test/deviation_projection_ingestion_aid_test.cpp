#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <deviation_projection_server/starter.h>
#include <deviation_projection_client/deviation_projection_client.h>
#include <iostream>
#include <functional>
#include <random>
#include <utility>
#include <algorithm>
#include <chrono>
#include <internal_rest/network_rest_frame.h>
#include <global_config/rest_config.h>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <coroutine_subsystem/coroutine_x.h>
#include <iostream>
#include <internal_rest/network_concurrency.h>
#include <string>
#include <thread>
#include <mutex>
#include <internal_rest/network_kernel_mailbox_impl1.h>
#include <internal_rest/network_kernel_mailbox_impl1_flash_stream_x.h>
#include <internal_rest/network_kernel_mailbox_impl1_channel_x.h>
#include <random>
#include <internal_rest/network_rest_frame.h>
#include <logging_subsystem/logging_subsystem.h>
#include <coroutine_subsystem/coroutine_x.h>
#include <deviation_projection_ingestion_aid_server/starter.h>
#include <deviation_projection_ingestion_aid_client/deviation_projection_ingestion_aid_client.h>
#include <data_loader/hex_encoder/hex_encoder.h>
#include <main_service/main_service.h>
#include <deviation_projection_ingestion_aid/deviation_projection_ingestion_aid.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward_to_deviation.h>
#include <cuda_management/host_service.h>

static inline constexpr char DELIM_CHAR = ',';
static inline constexpr char EOR_CHAR   = '\0';

class IPSiever: public virtual dg_sock::network_kernel_mailbox_impl1::external_interface::IPSieverInterface{

    public:

        auto thru(dg_sock::network_kernel_mailbox_impl1::model::Address) noexcept -> std::expected<bool, exception_t>{

            return true;
        }
};

static inline constexpr size_t CLIENT_IN_CHANNEL    = 5000;
static inline constexpr size_t RECV_CHANNEL_MSG_CAP = size_t{1} << 16;

using Remote = dg_sock::network_rest_frame::model::Remote;

void init_concurrency()
{
    using namespace dg_sock::network_concurrency;

    std::cout << "initializing concurrency...\n";

    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < 4u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_UNIT_DAEMON});
    }

    for (size_t i = 0u; i < 4u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_STREAM_DAEMON});
    }

    for (size_t i = 0u; i < 1u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_CHANNEL_DAEMON});
    }

    for (size_t i = 0u; i < 1u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = REST_SERVER_DAEMON});
    }

    for (size_t i = 0u; i < 4u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = REST_CLIENT_DAEMON});   
    }

    // for (size_t i = 0u; i < 1u; ++i)
    // {
    //     worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = UPDATE_DAEMON});
    // }

    for (size_t i = 0u; i < 1u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = COROUTINE_DAEMON});
    }

    for (size_t i = 0u; i < 1u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = CRON_TICK_DAEMON});
    }

    for (size_t i = 0u; i < 1u; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = CRON_WORK_DAEMON});
    }

    for (size_t i = 0u; i < 1; ++i)
    {
        worker_info_vec.push_back(
        {
            .cpu_id = std::nullopt,
            .daemon = RESOURCE_DISPOSER_DAEMON
        });
    }

    for (size_t i = 0u; i < 10; ++i)
    {
        worker_info_vec.push_back({
            .cpu_id = std::nullopt,
            .daemon = COMMON_ONFLY_POOL
        });
    }

    for (size_t i = 0u; i < 1; ++i)
    {
        worker_info_vec.push_back({
            .cpu_id = std::nullopt,
            .daemon = COROUTINE_DAEMON
        });
    }

    dg_sock::network_concurrency::init({worker_info_vec});
}

void init_basic()
{
    init_concurrency();

    std::cout << "initializing cron subsystem...\n"; 
    cron_subsystem::init();

    std::cout << "initializing coroutine...\n";
    coroutine_x::init(true, false, false);

    std::cout << "initializing network randomizer...\n";
    dg_sock::network_randomizer::init();

    std::cout << "initializing stack allocation...\n";
    dg_sock::network_stack_allocation::init();
    
    std::cout << "initializing network allocation...\n";
    dg_sock::network_allocation::init();

    // std::cout << "initializing network_cron\n";
    // dg_sock::network_cron::init();

    std::filesystem::path tmp_file = std::filesystem::temp_directory_path() / "request_test.txt";

    std::cout << "initializing logging subsystem...\n";
    dg_sock::network_log::init(tmp_file);

    std::filesystem::path tmp_file2 = std::filesystem::temp_directory_path() / "request_test_2.txt";
    logging_subsystem::init(tmp_file2);
}

void init_mailbox()
{
    static auto [retry_device_up, destructor] = dg_sock::network_concurrency_infretry_x::get_infretry_machine(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1000))); 

    std::shared_ptr<dg_sock::network_concurrency_infretry_x::ExecutorInterface> retry_device = std::move(retry_device_up);

    std::cout << "initializing base socket memory...\n";

    dg_sock::network_kernel_mailbox_impl1::allocation::init({.total_mempiece_count = 1 << 20,
                                                             .mempiece_sz = 1 << 10,
                                                             .affined_refill_sz = 1 << 8,
                                                             .affined_mem_vec_capacity = 1 << 8,
                                                             .affined_free_vec_capacity = 1 << 8});

    dg_sock::network_kernel_mailbox_impl1_flash_stream_x::init_memory({.total_mempiece_count = 1 << 20,
                                                                       .mempiece_sz = 1 << 10,
                                                                       .affined_refill_sz = 1 << 8,
                                                                       .affined_mem_vec_capacity = 1 << 8,
                                                                       .affined_free_vec_capacity = 1 << 8});

    std::cout << "initializing base socket configuration...\n";

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
        .retransmission_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)),
        .retransmission_concurrency_sz = 2,
        .retransmission_queue_cap = 1 << 16,
        .retransmission_user_queue_cap = 1 << 14,
        .retransmission_packet_cap = 30,
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
        .packetizer_segment_bsz = size_t{1} << 7,
        .packetizer_max_bsz = size_t{1} << 20,
        .packetizer_has_integrity_transmit = false,

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
        .inbound_container_has_only_redistributor = true,
        .inbound_container_has_redistributor = false,
        .inbound_container_redistributor_distribution_queue_sz = size_t{1} << 1,
        .inbound_container_redistributor_waiting_queue_sz = size_t{1} << 10,
        .inbound_container_redistributor_concurrent_sz = size_t{1} << 10,
        .inbound_container_redistributor_push_concurrent_sz = size_t{1} << 10,
        .inbound_container_redistributor_unit_sz = size_t{1} << 0,

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

    auto channel_socket_config = dg_sock::network_kernel_mailbox_impl1_channel_x::Config //
    {
        .channel_kind_map       = {{global_config::rest_config::HIGH_AVAILABILITY_CHANNEL, dg_sock::network_kernel_mailbox_impl1_channel_x::PreconfiguratedStickyChannelContainer::THOUSAND_CHANNEL_CODEX},
                                   {CLIENT_IN_CHANNEL, dg_sock::network_kernel_mailbox_impl1_channel_x::PreconfiguratedStickyChannelContainer::THOUSAND_CHANNEL_CODEX}}, // <uint32_t, channel_t>

        .channel_msg_cap_map    = {{global_config::rest_config::HIGH_AVAILABILITY_CHANNEL, RECV_CHANNEL_MSG_CAP},
                                   {CLIENT_IN_CHANNEL, RECV_CHANNEL_MSG_CAP}}, // <uint32_t, uint64_t>

        .assorter_worker_sz     = 1
    };

    std::cout << "initializing socket...\n";

    dg_sock::network_kernel_mailbox::init(
    {
        .base_config    = base_socket_config,
        .stream_config  = stream_socket_config,
        .channel_config = channel_socket_config
    });
}

void init_rest_server()
{
    using namespace dg_sock::network_rest_frame::model;

    dg_sock::network_rest_frame::server_instance::BuilderConfig config
    {
        .cache_each_capacity                                        = size_t{1} << 10,
        .cache_response_capacity                                    = size_t{1} << 16,
        .cache_concurrency_sz                                       = size_t{1} << 4,
        
        .cache_unique_write_set_each_capacity                       = size_t{1} << 10,
        .cache_unique_write_set_concurrency_sz                      = size_t{1} << 4,

        .recv_channel_counter_map                                   = {{global_config::rest_config::HIGH_AVAILABILITY_CHANNEL, 1u}},
        .send_channel                                               = CLIENT_IN_CHANNEL,

        .cache_unique_write_traffic_controller_elemental_thru_cap   = size_t{1} << 20,
        .cache_unique_write_traffic_controller_concurrency_sz       = size_t{1} << 4,
        .cache_unique_write_traffic_controller_reset_duration       = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)),
    };

    std::cout << "initializing REST server...\n";
    dg_sock::network_rest_frame::server_instance::init(config);

    // std::cout << "hooking REST resolver\n"; 
    // dg_sock::network_rest_frame::server_instance::hook(HELLO_WORLD_PATH, std::make_shared<HelloResolver>());
}

void init_rest_client()
{
    std::cout << "initializing REST client...\n";

    std::unique_ptr<dg_sock::network_rest_frame::client::RestControllerInterface> controller = dg_sock::network_rest_frame::client_instance::SolutionBuilder{}.set_recv_channel(CLIENT_IN_CHANNEL)
                                                                                                                                                              .get();


    using Address = dg_sock::network_kernel_mailbox_impl1::model::Address;

    Address addr = 
    {
        .ip     = dg_sock::network_kernel_mailbox_impl1::utility::ipv4_std_formatted_str_to_compact("127.0.0.1").value(),
        .port   = 5000
    };

    dg_sock::network_rest_frame::client_instance::init(std::move(controller), addr);
}

void initialize_resource()
{
    init_basic();
    init_mailbox();
    init_rest_server();
    init_rest_client();

    std::cout << "initializing connectivity subsystem...\n";
    connectivity_subsystem::init();

    std::cout << "starting deviation projection server...\n";
    deviation_projection_server::start_server();

    std::cout << "starting deviation projection ingestion aid server...\n";
    deviation_projection_ingestion_aid_server::start_server();

    std::cout << "initializing resource disposer...\n";
    resource_disposer::init();

    std::cout << "initializing main service...\n";
    main_service::init();
}

auto randomize_int(size_t sz) -> size_t
{
    if (sz == 0u)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return randomizer () % sz;
}

auto randomize_string(size_t sz) -> std::string
{
    std::string rs{};

    for (size_t i = 0u; i < sz; ++i)
    {
        rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(randomize_int(256))));
    }

    return rs;
}

auto generate_random_token() -> std::string
{
    size_t STR_SZ_RANGE = size_t{1} << 4;

    return randomize_string(randomize_int(STR_SZ_RANGE));
}

auto generate_random_token_vec() -> std::vector<std::string>
{
    size_t TOKEN_VEC_SZ_RANGE   = size_t{1} << 4;
    size_t TOKEN_VEC_SZ         = randomize_int(TOKEN_VEC_SZ_RANGE);
    std::vector<std::string> rs = {};

    for (size_t i = 0u; i < TOKEN_VEC_SZ; ++i)
    {
        rs.push_back(generate_random_token());
    }

    return rs;
}

auto hex_encode_token(const std::string& inp) -> std::string
{
    return data_loader::hex_encoder::hex_encode(inp);
}

auto hex_decode_token(const std::string& inp) -> std::string
{
    return data_loader::hex_encoder::hex_decode(inp);
}

auto join(const std::vector<std::string>& data, char c) -> std::string
{
    if (data.empty())
    {
        return {};
    }

    std::string rs = data[0];

    for (size_t i = 1u; i < data.size(); ++i)
    {
        rs += c;
        rs += data[i];
    }

    return rs;
}

auto randomize_delim_config() -> data_loader::stream_reader::DelimitedStreamReaderConfig
{
    return
    {
        .delim_char     = DELIM_CHAR,
        .eor_char       = EOR_CHAR,
        .max_token_sz   = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                   : std::optional<uint64_t>(std::numeric_limits<uint64_t>::max())
    };
}

auto randomize_external_delim_config() -> data_loader::stream_reader::ExternalDelimitedStreamReaderConfig
{
    return data_loader::stream_reader::to_external_delimited_stream_reader_config(randomize_delim_config());
}

auto randomize_file_loader_config(const std::string& file_path) -> data_loader::file_source::FileLoaderConfig
{
    return 
    {
        .delim_config               = randomize_external_delim_config(),
        .local_file_path            = file_path,
        .read_ahead_buffer_sz_hint  = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4)),
        .unit_byte_sz_hint          = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4))
    };
}

auto randomize_external_file_loader_config(const std::string& file_path) -> data_loader::file_source::ExternalFileLoaderConfig
{
    return data_loader::file_source::to_external_file_loader_config(randomize_file_loader_config(file_path));
}

auto randomize_generic_reader_config(const std::string& file_path) -> data_loader::generic_source::GenericReaderConfig
{
    return
    {
        .source = randomize_external_file_loader_config(file_path)
    };
}

auto randomize_external_generic_reader_config(const std::string& file_path) -> data_loader::generic_source::ExternalGenericReaderConfig
{
    return data_loader::generic_source::to_external_generic_reader_config(randomize_generic_reader_config(file_path));
}

auto randomize_normal_retry_config() -> data_loader::retryer_device::normal_device::RetryConfig
{
    return
    {
        .base_wait_time             = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)),
        .exponential_base           = randomize_int(2) + 2,
        .max_retry_count            = randomize_int(size_t{1} << 2),
        .retryable_exception_vec    = std::nullopt
    };
}

auto randomize_external_normal_retry_config() -> data_loader::retryer_device::normal_device::ExternalRetryConfig
{
    return data_loader::retryer_device::normal_device::to_external_retry_config(randomize_normal_retry_config());
}

auto randomize_generic_retry_config() -> data_loader::retryer_device::generic_device::GenericRetryConfig
{
    return
    {
        .config = randomize_external_normal_retry_config()
    };
}

auto randomize_external_generic_retry_config() -> data_loader::retryer_device::generic_device::ExternalGenericRetryConfig
{
    return data_loader::retryer_device::generic_device::to_external_generic_retry_config(randomize_generic_retry_config());
}

auto randomize_source_transaction_broker_config(const std::string& file_path) -> data_loader::source_loader::broker::SourceTransactionBrokerConfig
{
    return
    {
        .source_config  = randomize_external_generic_reader_config(file_path),
        .retry_config   = randomize_external_generic_retry_config()
    };
}

auto randomize_external_source_transaction_broker_config(const std::string& file_path) -> data_loader::source_loader::broker::ExternalSourceTransactionBrokerConfig
{
    return data_loader::source_loader::broker::to_external_source_transaction_broker_config(randomize_source_transaction_broker_config(file_path));
}

auto randomize_wait_loader_config(const std::string& file_path) -> data_loader::source_loader::wait_loader::WaitLoaderConfig
{
    return
    {
        .tx_sz          = randomize_int(size_t{1} << 4) + 1,
        .broker_config  = randomize_external_source_transaction_broker_config(file_path)
    };
}

auto randomize_external_wait_loader_config(const std::string& file_path) -> data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig
{
    return data_loader::source_loader::wait_loader::to_external_wait_loader_config(randomize_wait_loader_config(file_path));
}

auto randomize_config(const std::string& file_path) -> data_loader::source_loader::generic_loader::GenericLoaderConfig
{
    return
    {
        .config = randomize_external_wait_loader_config(file_path)
    };
}

auto randomize_external_config(const std::string& file_path) -> data_loader::source_loader::generic_loader::ExternalGenericLoaderConfig
{
    return data_loader::source_loader::generic_loader::to_external_generic_loader_config(randomize_config(file_path));
}

auto randomize_multisource_config(const std::string& file_path) -> data_loader::source_loader::multisource_loader::MultisourceLoaderConfig
{
    return
    {
        .config_vec = {randomize_external_config(file_path)}
    };
}

auto randomize_external_multisource_config(const std::string& file_path) -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
{
    return data_loader::source_loader::multisource_loader::to_external_multisource_loader_config(randomize_multisource_config(file_path));
}

auto join_hex_token(const std::vector<std::string>& token_vec) -> std::string
{
    if (token_vec.empty())
    {
        return "";
    }

    return join(token_vec, DELIM_CHAR) + EOR_CHAR;
}

auto randomize_tx_hint_size() -> uint64_t
{
    return randomize_int(size_t{1} << 4);
}

auto randomize_hex_token_file() -> std::string
{
    std::vector<std::string> org_vec    = generate_random_token_vec();
    std::vector<std::string> token_vec  = {};

    for (const auto& str: org_vec)
    {
        token_vec.push_back(hex_encode_token(str));
    }

    std::string file_output = join_hex_token(token_vec);
    std::string FILE_PATH   = "/home/ubuntu/SeriousBillionDollarProject/src/test/data_loader_file_source_test.bin";

    {
        std::ofstream out_file(FILE_PATH, std::ios::binary);
        out_file.write(file_output.data(), file_output.size());
    }

    return FILE_PATH;
}

auto remove_file_path(const std::string& fp)
{
    std::filesystem::remove(fp);
}

auto get_random_temporal_firer_config() -> fire_bandwidth_control::temporal_firer::TemporalFirerConfig
{
    return 
    {
        .window_population  = static_cast<uint64_t>(randomize_int(size_t{1} << 2)),
        .window_dur         = std::chrono::nanoseconds(randomize_int(size_t{1} << 20))
    };
}

auto get_random_external_temporal_firer_config() -> fire_bandwidth_control::temporal_firer::ExternalTemporalFirerConfig
{
    return fire_bandwidth_control::temporal_firer::to_external_temporal_firer_config(get_random_temporal_firer_config());
}

auto get_random_generic_firer_config() -> fire_bandwidth_control::generic_firer::GenericFirerConfig
{
    return
    {
        .config = get_random_external_temporal_firer_config()
    };
}

auto get_random_external_generic_firer_config() -> fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig
{
    return fire_bandwidth_control::generic_firer::to_external_generic_firer_config(get_random_generic_firer_config());
}

auto get_local_remote() -> Remote
{
    return Remote
    {
        .addr       = {
            .ip     = dg_sock::network_kernel_mailbox_impl1::utility::ipv4_std_formatted_str_to_compact("127.0.0.1").value(),
            .port   = 5000
        },

        .channel    = global_config::rest_config::HIGH_AVAILABILITY_CHANNEL
    };
}

auto get_random_ingestion_aid_run_payload(const std::string& fp, const deviation_projection_client::ClientRemote& client_remote) -> deviation_projection_ingestion_aid_client::RunPayload
{
    return
    {
        .data_loader_config = randomize_external_multisource_config(fp),
        .server_sink_vec    = {deviation_projection_ingestion_aid_client::ServerSink{.remote = client_remote.remote, .client_id = client_remote.client_id}},
        .token_firer_config = get_random_external_generic_firer_config()  
    };
}

void wait_client(deviation_projection_ingestion_aid_client::APIClient * client)
{
    while (true)
    {
        if (client->is_completed()->wait())
        {
            client->get_result()->wait();
            return;
            // std::this_thread::sleep_for(std::chrono::seconds(100));
            // std::abort();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void test_ingestion_aid()
{
    using namespace deviation_projection_ingestion_aid;

    deviation_projection_client::APIClient client(get_local_remote());
    std::string fp = randomize_hex_token_file();

    PiecewiseArgument arg = PiecewiseBuilder{}.worker_remote(get_local_remote())
                                              .client_remote(client.get_client_remote().remote, client.get_client_remote().client_id)
                                              .data_loader_config(randomize_external_multisource_config(fp))
                                              .firer_config(get_random_external_generic_firer_config())
                                              .build();

    auto cancellation_token = common_exception::CancellationToken();
    ClientTrainingDataPiecewiseIngestor{}.add(arg).run(cancellation_token);

    remove_file_path(fp);
}

void run_one_test()
{
    test_ingestion_aid();
}

class TestWorker: public virtual concurrency_base::WorkerInterface
{
    public:

        auto run_one_epoch() noexcept -> bool
        {
            const size_t TEST_SZ    = size_t{1} << 10;
            const size_t COUT_SZ    = size_t{1} << 0;

            std::cout << "__BEGIN_DEVIATION_PROJECTION_INGESTION_AID_CLIENT_TEST__\n";

            for (size_t i = 0u; i < TEST_SZ; ++i)
            {
                run_one_test();

                if (i % COUT_SZ == 0u)
                {
                    std::cout << i << "/" << TEST_SZ << "\n";
                }
            }

            std::cout << "__END_DEVIATION_PROJECTION_INGESTION_AID_CLIENT_TEST__\n";

            // common_exception::throw_exception(common_exception::OPERATION_GRACEFUL_TERMINATION_ERROR);
            std::abort();

            return true;
        }
};

void run_test()
{
    std::cout << "initializing resource...\n";
    initialize_resource();

    std::cout << "registering daemon...\n";
    auto handle = concurrency_base::daemon_saferegister(concurrency_base::COROUTINE_DAEMON, std::make_unique<TestWorker>());

    if (!handle.has_value())
    {
        std::cout << "mayday, cannot register Test daemon\n";
        std::abort();
    }

    std::cout << "subscribing main...\n";
    main_service::main_subscribe();
}

int main()
{
    run_test();
}