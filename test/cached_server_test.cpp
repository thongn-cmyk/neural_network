#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <functional>
#include <random>
#include <utility>
#include <algorithm>
#include <chrono>
#include <internal_rest/network_rest_frame.h>
#include <global_config/rest_config.h>
#include <connectivity_subsystem/connectivity_subsystem.h>
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
#include <data_loader/hex_encoder/hex_encoder.h>
#include <main_service/main_service.h>
#include <serializer/trivial_serializer.h>
#include <resource_disposer/resource_disposer.h>

//You are right, I have not found a stable solution for large size messages with unexpected connection drops
//Because we are coding system. We are expecting that our network is up and running 100% at all times, the otherwise just cannot be implemented without some kind of derangement

//What I have not been able to implement yet is fixed outdegree and fixed outbound bandwidth
//as we have already observed, best bandwidth or the ideal number "unacknowledged packet count" lies probably around 1 << 10 -> 1 << 16
//so with fixed bandwidth for each of the outdegree, which is 1 << 8 -> 1 << 14, we can 100% guarantee that our packets are delivered in a timely manner, but that's out of the scope of our system, because most of the time, we only do 1-1 heavy transfer and just some calls to synchronize   
//but the implementation I would guess is we'd have some kind of congestion, or an unusual amount of in-transit memory, which we'd have to make sure by synchronization at the caller points, so master-slave is a mandatory, not an optional choice in this case

//So ideally in our case, this operation is a scope operation with on-demand spawns, and a kernel termination as a job cleanup guarantee
//I just cannot see it otherwise 

using namespace dg_sock::network_rest_frame::model;
using ResponseInterface = dg_sock::network_rest_frame::client::ResponseInterface;

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

void init_concurrency()
{
    using namespace dg_sock::network_concurrency;

    std::cout << "initializing concurrency...\n";

    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < 12u; ++i)
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
        .num_kernel_inbound_worker = 4,
        .num_process_inbound_worker = 1,
        .num_outbound_worker = 4,
        .num_kernel_rescue_worker = 0,
        .num_retry_worker = 1,

        .inbound_socket_concurrency_sz = 4,
        .outbound_socket_concurrency_sz = 4,
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

class Dictionary
{
    private:

        std::unordered_map<dg_sock::string, size_t> counter_map;
        std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
    
    public:

        Dictionary(): counter_map(),
                      mtx(fair_mutex::make_unique_fair_atomic_flag()){}

        auto add(const dg_sock::string& e) -> bool
        {
            fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

            this->counter_map[e] += 1;
            return true;
        }

        auto get(const dg_sock::string& e) -> size_t
        {
            fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

            return this->counter_map[e];
        }

        void clear() noexcept
        {
            fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

            this->counter_map.clear();
        }
};

class CachedServerTest: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
{
    private:

        std::shared_ptr<Dictionary> dictionary;

    public:

        static inline constexpr std::string_view RESOLVABLE_PATH = "cached_server_test";

        using Request   = dg_sock::network_rest_frame::model::Request;
        using Response  = dg_sock::network_rest_frame::model::Response;

        CachedServerTest(std::shared_ptr<Dictionary> dictionary): dictionary(std::move(dictionary)){}

        auto handle(const Request& request) -> Response
        {
            bool rs = this->dictionary->add(request.payload);

            return Response
            {
                .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(rs)
            };
        }
};

void initialize_resource()
{
    init_basic();
    init_mailbox();
    init_rest_server();
    init_rest_client();

    std::cout << "initializing connectivity subsystem...\n";
    connectivity_subsystem::init();

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

auto get_requestee_url() -> ResourceAddress
{
    return ResourceAddress
    {
        .remote_addr    = get_local_remote().addr,
        .resource_addr  = dg_sock::string("cached_server_test"),
        .channel        = get_local_remote().channel
    };
}

auto get_requestor() -> Address
{
    return get_local_remote().addr;
}

auto get_designated_request_id() -> request_id_t
{
    request_id_t rs{};
    dg_sock::network_rest_frame::client_instance::get_instance()->get_designated_request_id(1u, &rs);

    static request_id_t previous_rs{};

    if (trivial_serializer::reflectible_is_equal(rs, previous_rs))
    {
        std::cout << "designated request id went wrong\n";
        std::abort();
    }

    previous_rs = rs;
    return rs;
}

auto randomize_request() -> ClientRequest
{
    std::string tmp = randomize_string(randomize_int(size_t{1} << 4));
    // std::string tmp = "hello";

    return ClientRequest
    {
        .requestee_url                  = get_requestee_url(),
        .requestor                      = get_requestor(),
        .payload                        = dg_sock::string(tmp),
        .payload_serialization_format   = {},
        .client_timeout_dur             = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)),
        .server_abs_timeout             = std::nullopt,
        .designated_request_id          = get_designated_request_id()
    };
}

auto randomize_unique_request() -> std::vector<ClientRequest>
{
    const size_t UNIQUE_REQUEST_RANGE   = size_t{1} << 2;
    ClientRequest request               = randomize_request();
    const size_t unique_request_sz      = randomize_int(UNIQUE_REQUEST_RANGE);
    std::vector<ClientRequest> rs       = {};

    for (size_t i = 0u; i < unique_request_sz; ++i)
    {
        rs.push_back(request);
    }

    return rs;
}

template <class T>
auto shuffle_vector(const std::vector<T>& arg) -> std::vector<T>
{
    static auto randomizer = std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};

    auto tmp = arg;
    std::shuffle(tmp.begin(), tmp.end(), randomizer);

    return tmp;
}

auto randomize_client_request_vec() -> std::vector<ClientRequest>
{
    size_t request_sz = randomize_int(size_t{1} << 6);
    std::vector<ClientRequest> rs{};

    for (size_t i = 0u; i < request_sz; ++i)
    {
        if (randomize_int(2) == 0u)
        {
            std::vector<ClientRequest> same_id_request = randomize_unique_request();
            rs.insert(rs.end(), same_id_request.begin(), same_id_request.end());
        }
        else
        {
            rs.push_back(randomize_request());
        }
    }

    return shuffle_vector(rs);
}

auto get_expected_dictionary(const std::vector<ClientRequest>& client_request_vec) -> std::unordered_map<dg_sock::string, size_t>
{
    std::unordered_map<dg_sock::string, std::unordered_set<dg_sock::string>> counter_map{};
    std::unordered_map<dg_sock::string, size_t> result_map{};

    for (const auto& client_request: client_request_vec)
    {
        counter_map[client_request.payload].insert(dg::network_compact_serializer::serialize<dg_sock::string>(client_request.designated_request_id));
        // result_map[client_request.payload] += 1;
    }

    for (const auto& [key, counter_set]: counter_map)
    {
        result_map[key] = counter_set.size();
    }

    return result_map;
}

void run_one_test(const std::shared_ptr<Dictionary>& dictionary)
{
    std::vector<ClientRequest> client_request_vec                   = randomize_client_request_vec();
    std::unordered_map<dg_sock::string, size_t> expected_dictionary = get_expected_dictionary(client_request_vec);
    std::vector<std::shared_ptr<ResponseInterface>> response_vec    = {};

    // std::cout << "requesting > " << client_request_vec.size() << "\n";

    for (auto& client_request: client_request_vec)
    {
        auto tmp = dg_sock::network_rest_frame::client_instance::get_instance()->request(std::move(client_request));

        if (!tmp.has_value())
        {
            std::cout << "mayday, bad request!\n";
            std::abort();
        }

        response_vec.push_back(std::move(tmp.value()));
    }

    for (const auto& response: response_vec)
    {
        response->response();
    }

    for (const auto& [key, counter]: expected_dictionary)
    {
        if (dictionary->get(key) != counter)
        {
            std::cout << "payload test," << std::string_view(key) << ",\n";

            std::cout << "mayday, mismatched counter value\n";
            std::cout << dictionary->get(key) << "<>" << counter << "\n";

            std::abort();
        }
    }

    dictionary->clear();
}

class TestWorker: public virtual concurrency_base::WorkerInterface
{
    public:

        auto run_one_epoch() noexcept -> bool
        {
            const size_t TEST_SZ    = size_t{1} << 20;
            const size_t COUT_SZ    = size_t{1} << 4;

            std::cout << "__BEGIN_DEVIATION_PROJECTION_INGESTION_AID_CLIENT_TEST__\n";

            std::cout << "starting cached server...\n";

            std::shared_ptr<Dictionary> dictionary = std::make_shared<Dictionary>();
            dg_sock::network_rest_frame::server_instance::hook(CachedServerTest::RESOLVABLE_PATH, std::make_unique<CachedServerTest>(dictionary));

            for (size_t i = 0u; i < TEST_SZ; ++i)
            {
                run_one_test(dictionary);

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