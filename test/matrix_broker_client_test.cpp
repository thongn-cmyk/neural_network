#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

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
#include <matrix_broker_server/starter.h>
#include <matrix_broker_client/matrix_broker_client.h>
#include <matrix/generic_matrix_factory.h>
#include <matrix/tensor_factory.h>
#include <connection_handshake_server/starter.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward.h>
#include <taylor_matrix/cuda_matrix/tensor_matrix_forward_to_deviation.h>
#include <cuda_management/host_service.h>
#include <stl_extension/semantic_mapper.h>

class IPSiever: public virtual dg_sock::network_kernel_mailbox_impl1::external_interface::IPSieverInterface{

    public:

        auto thru(dg_sock::network_kernel_mailbox_impl1::model::Address) noexcept -> std::expected<bool, exception_t>{

            return true;
        }
};

namespace dg_sock::network_compact_serializer
{
    consteval auto get_dgstd_serialization_identifier() -> const char *
    {
        return "dgstd_1";
    }
}
//alright, we'd test requests today
//basically we've done everything we could to achieve low latency, we'd need jumbo frames or friends to achieve higher bandwidth

static inline constexpr size_t CLIENT_IN_CHANNEL    = 12345;
static inline constexpr size_t RECV_CHANNEL_MSG_CAP = size_t{1} << 16;

void init_concurrency()
{
    using namespace dg_sock::network_concurrency;

    std::cout << "initializing concurrency\n";

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

    dg_sock::network_concurrency::init({worker_info_vec});
}

void init_basic()
{
    init_concurrency();

    std::cout << "initializing cron subsystem\n"; 
    cron_subsystem::init();

    std::cout << "initializing coroutine\n";
    coroutine_x::init(true, false, false);

    std::cout << "initializing network randomizer\n";
    dg_sock::network_randomizer::init();

    std::cout << "initializing stack allocation\n";
    dg_sock::network_stack_allocation::init();
    
    std::cout << "initializing network allocation\n";
    dg_sock::network_allocation::init();

    // std::cout << "initializing network_cron\n";
    // dg_sock::network_cron::init();

    std::filesystem::path tmp_file = std::filesystem::temp_directory_path() / "request_test.txt";

    std::cout << "initializing logging subsystem\n";
    dg_sock::network_log::init(tmp_file);

    std::filesystem::path tmp_file2 = std::filesystem::temp_directory_path() / "request_test_2.txt";
    logging_subsystem::init(tmp_file2);
}

void init_mailbox()
{
    static auto [retry_device_up, destructor] = dg_sock::network_concurrency_infretry_x::get_infretry_machine(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1000))); 

    std::shared_ptr<dg_sock::network_concurrency_infretry_x::ExecutorInterface> retry_device = std::move(retry_device_up);

    std::cout << "initializing base socket memory\n";

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
        .packetizer_segment_bsz = size_t{1} << 0,
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

    std::cout << "initializing socket\n";

    dg_sock::network_kernel_mailbox::init(
    {
        .base_config    = base_socket_config,
        .stream_config  = stream_socket_config,
        .channel_config = channel_socket_config
    });
}

struct HelloMessage
{
    std::string hello_msg;
    std::chrono::time_point<std::chrono::utc_clock> when;

    template <class Reflector>
    void dg_reflect(const Reflector& reflector) const
    {
        reflector(hello_msg, when);
    }

    template <class Reflector>
    void dg_reflect(const Reflector& reflector)
    {
        reflector(hello_msg, when);
    }
};

class HelloResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
{
    public:

        auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
        {
            if (std::string_view(request.payload_serialization_format) != dg_sock::network_compact_serializer::get_dgstd_serialization_identifier())
            {
                return 
                {
                    .response                       = {},
                    .response_serialization_format  = {},
                    .err_code                       = dg_sock::network_exception::INVALID_ARGUMENT
                };
            }

            HelloMessage hello_msg  = dg_sock::network_compact_serializer::dgstd_deserialize<HelloMessage>(request.payload);

            hello_msg.hello_msg     = hello_msg.hello_msg + "<> acked";
            hello_msg.when          = std::chrono::utc_clock::now();

            return
            {
                .response                       = dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string>(hello_msg),
                .response_serialization_format  = dg_sock::string(dg_sock::network_compact_serializer::get_dgstd_serialization_identifier()),
                .err_code                       = dg_sock::network_exception::SUCCESS
            };
        }
};

static inline const std::string_view HELLO_WORLD_PATH = "hello_world_path";

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

    std::cout << "initializing REST server\n";
    dg_sock::network_rest_frame::server_instance::init(config);

    std::cout << "hooking REST resolver\n"; 
    dg_sock::network_rest_frame::server_instance::hook(HELLO_WORLD_PATH, std::make_shared<HelloResolver>());
}

void init_rest_client()
{
    std::cout << "initializing REST client\n";

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

void init_rest_frame_resource()
{
    init_basic();
    init_mailbox();
    init_rest_server();
    init_rest_client();
}

void init_handshake_server_resource()
{
    connection_handshake_server::start_server();
}

void init_matrix_broker_server_resource()
{
    matrix_broker_server::start_server();
}

void init_resource()
{
    init_rest_frame_resource();
    init_handshake_server_resource();
    init_matrix_broker_server_resource();
}

using Remote = dg_sock::network_rest_frame::model::Remote;

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

void test_matrix_broker()
{
    const size_t VECTOR_SZ_RANGE        = size_t{1} << 10;

    auto client = matrix_broker_client::APIClient(get_local_remote());

    for (size_t i = 0u; i < VECTOR_SZ_RANGE; ++i)
    {
        auto rs                             = client.broke_matrix("taylor_host_matrix", matrix_broker_client::MATRIX_ENTROPY_LOW, i)->wait();

        if (!std::holds_alternative<matrix_broker_client::FixedProjectionArgument>(rs.projection_argument.projection_argument))
        {
            std::cout << "mayday, unexpected projection argument\n";
            std::abort();
        }

        std::vector<size_t> matrix_shape    = std::get<matrix_broker_client::FixedProjectionArgument>(rs.projection_argument.projection_argument).inp_matrix_shape;
        auto matrix_projector               = generic_matrix_factory::GenericMatrixLoader{}.load_resource(generic_matrix_factory::GenericMatrixExternalizer{}.to_internal(stdx::to_automap_object(rs.matrix_resource)));

        auto matrix                         = tensor_factory::make_matrix_from_shape_vec(matrix_shape);
        auto rs_2                           = matrix_projector->project({matrix});

        std::cout << rs_2.size() << "<size>\n";
    }
}

int main()
{
    std::cout << "__BEGIN_MATRIX_BROKER_CLIENT_TEST__\n";

    std::cout << "initializing resource...\n";
    init_resource();

    std::cout << "testing matrix broker...\n";
    test_matrix_broker();

    std::cout << "__END_MATRIX_BROKER_CLIENT_TEST__\n";
}