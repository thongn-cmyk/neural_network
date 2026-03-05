
#ifndef __SOCK_STREAM_TEST_PROGRAM__
#define __SOCK_STREAM_TEST_PROGRAM__

#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include "network_concurrency.h"
#include <string>
#include <thread>
#include <mutex>
#include "network_kernel_mailbox_impl1.h"
#include "network_kernel_mailbox_impl1_flash_stream_x.h"
#include "network_kernel_mailbox_impl1_channel_x.h"
#include <random>

static inline constexpr size_t RECV_CHANNEL         = 1234;
static inline constexpr size_t RECV_CHANNEL_MSG_CAP = size_t{1} << 16;
static inline constexpr size_t CHANNEL_BURST_SZ     = size_t{1} << 8;

class TestWorker: public virtual dg_sock::network_concurrency::WorkerInterface
{
    private:

        dg_sock::network_kernel_mailbox_impl1_channel_x::ChannelMailboxInterface * sock;
        std::binary_semaphore * smp;

    public:

        TestWorker(dg_sock::network_kernel_mailbox_impl1_channel_x::ChannelMailboxInterface * sock,
                   std::binary_semaphore * smp) noexcept: sock(sock),
                                                          smp(smp){}

        bool run_one_epoch() noexcept
        {
            const size_t TEST_SZ = size_t{1} << 20u;

            size_t packet_sz    = 8u;
            size_t packet_count = size_t{1} << 10;

            std::vector<std::shared_ptr<void>> resource_vec = {};
            std::vector<std::string> recv_vec{};
            std::vector<dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument> mailbox_arg_vec = make_mailbox_arg_vec(packet_sz, packet_count, 5000, resource_vec);

            for (size_t i = 0u; i < TEST_SZ; ++i)
            {
                size_t iterative_packet_count = randomize_packet_count() % packet_count + 1u; 

                std::cout << "test packet_sz > " << packet_sz << " packet_count > " << iterative_packet_count << std::endl;  

                this->strong_send(mailbox_arg_vec.data(), iterative_packet_count);
                this->strong_recv(iterative_packet_count, packet_sz, recv_vec);

                // sorted_print(recv_vec);
                // assert_equal(mailbox_arg_vec, recv_vec);

                std::cout << "test > " << i << "/" << TEST_SZ << std::endl;
                // resource_vec = {};

                // std::this_thread::sleep_for(std::chrono::seconds(10));
            }

            smp->release();

            return true;
        }

    private:

        void sorted_print(const std::vector<std::string>& vec)
        {
            std::vector<std::string> cpy_vec = vec;

            std::sort(cpy_vec.begin(), cpy_vec.end());

            for (const auto& e: cpy_vec)
            {
                std::cout << "value > " << e << std::endl;
            }
        }

        void assert_equal(const std::vector<dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument>& mailbox_arg_vec,
                          const std::vector<std::string>& recv_vec)
        {
            std::unordered_map<std::string, size_t> counter{};
            
            for (const auto& e: mailbox_arg_vec)
            {
                counter[std::string(std::string_view(static_cast<const char *>(e.content), std::next(static_cast<const char *>(e.content), e.content_sz)))] += 1u;
            }

            for (const auto& e: recv_vec)
            {
                size_t& value = counter[e];
                
                if (value == 0u)
                {
                    std::cout << "mayday counter" << std::endl;
                    std::abort();
                }

                value -= 1u;

                if (value == 0u)
                {
                    counter.erase(e);
                }
            }

            if (!counter.empty())
            {
                std::cout << "mayday counter not empty" << std::endl;
                std::abort();
            }
        }

        void strong_send(dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument * arg_arr, size_t sz)
        {
            static std::vector<dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument> tmp_vec{};
            
            tmp_vec.resize(sz);
            std::copy(arg_arr, std::next(arg_arr, sz), tmp_vec.begin());

            static std::vector<exception_t> exception_vec{};
            exception_vec.resize(sz);

            while (true)
            {
                this->sock->send(RECV_CHANNEL, tmp_vec.data(), tmp_vec.size(), exception_vec.data());
                size_t nxt_sz = 0u;

                for (size_t i = 0u; i < tmp_vec.size(); ++i)
                {
                    if (dg_sock::network_exception::is_failed(exception_vec[i]))
                    {
                        std::swap(tmp_vec[i], tmp_vec[nxt_sz++]);
                    }
                    else
                    {
                        // for (char c: std::string_view(static_cast<const char *>(tmp_vec[i].content), std::next(static_cast<const char *>(tmp_vec[i].content), tmp_vec[i].content_sz)))
                        // {
                        //     bool is_numeric = c >= '0' && c <= '9'; 

                        //     if (!is_numeric)
                        //     {
                        //         std::cout << "mayday 1" << std::endl;
                        //         std::cout << std::string_view(static_cast<const char *>(tmp_vec[i].content), std::next(static_cast<const char *>(tmp_vec[i].content), tmp_vec[i].content_sz)) << std::endl;
                        //         std::abort();
                        //     }
                        // }
                    }
                }

                if (nxt_sz == 0u)
                {
                    break;
                }

                tmp_vec.resize(nxt_sz);
            }            
        }
        
        void strong_recv(size_t sz,
                         size_t segment_sz,
                         std::vector<std::string>& buf_vec)
        {
            // std::vector<std::string> buf_vec(sz);

            buf_vec.resize(sz);

            for (auto& e: buf_vec)
            {
                e.resize(segment_sz);
            }

            static std::vector<dg_sock::string> recv_arr{};
            recv_arr.resize(sz);

            size_t actual_recv_sz = 0u; 

            while (actual_recv_sz != sz)
            {
                size_t tmp_recv_sz{}; 
                sock->recv(RECV_CHANNEL, std::next(recv_arr.data(), actual_recv_sz), tmp_recv_sz, sz - actual_recv_sz);

                for (size_t i = 0u; i < tmp_recv_sz; ++i)
                {
                    buf_vec[i + actual_recv_sz] = std::string_view(recv_arr[i + actual_recv_sz]);
                }

                actual_recv_sz += tmp_recv_sz;
                std::cout << "actual > " << actual_recv_sz << "<> expected > " << sz << std::endl;
            }
        }

        auto randomize_packet_size() -> size_t
        {
            static auto random_device       = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
            const size_t PACKET_SZ_RANGE    = size_t{1} << 7;

            return random_device() % PACKET_SZ_RANGE + 1u;
        }

        auto randomize_packet_count() -> size_t
        {
            static auto random_device       = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
            const size_t PACKET_COUNT_RANGE = size_t{1} << 16;

            return random_device() % PACKET_COUNT_RANGE + 1u;
        }
        
        auto make_str(size_t packet_sz, std::vector<std::shared_ptr<void>>& resource_container) -> char *
        {
            static auto random_device = std::bind(std::uniform_int_distribution<char>{}, std::mt19937_64{static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) }); 

            if (packet_sz == 0u)
            {
                return nullptr;
            }

            std::shared_ptr<char[]> buf = std::make_unique<char[]>(packet_sz);
            char * rs = buf.get();
            std::generate(rs, std::next(rs, packet_sz), std::ref(random_device));

            resource_container.push_back(buf);

            return rs;
        }

        auto make_incremental_str(size_t packet_sz, std::vector<std::shared_ptr<void>>& resource_container) -> char *
        {
            static std::atomic<size_t> incrementor = 0u;

            std::shared_ptr<char[]> buf = std::make_unique<char[]>(packet_sz);
            char * rs = buf.get();

            std::string incrementor_str = std::to_string(incrementor++);
            size_t trailing_zero_sz     = packet_sz - incrementor_str.size(); 

            std::fill(rs, std::next(rs, trailing_zero_sz), '0');
            std::copy(incrementor_str.begin(), incrementor_str.end(), std::next(rs, trailing_zero_sz));

            for (char c: std::string_view(buf.get(), std::next(buf.get(), packet_sz)))
            {
                bool is_numeric = c >= '0' && c <= '9'; 

                if (!is_numeric)
                {
                    std::cout << "mayday 12" << std::endl;
                    std::cout << std::string_view(buf.get(), std::next(buf.get(), packet_sz)) << std::endl;
                    std::abort();
                }
            }

            resource_container.push_back(buf);

            return rs;
        }

        auto make_mailbox_arg_vec(size_t packet_sz, size_t packet_count, uint16_t port,
                                  std::vector<std::shared_ptr<void>>& resource_container) -> std::vector<dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument>
        {
            dg_sock::network_kernel_mailbox_impl1::model::Address self_addr = {};
            self_addr.ip = dg_sock::network_kernel_mailbox_impl1::model::IP{.ip = dg_sock::network_kernel_mailbox_impl1::utility::ipv4_std_formatted_str_to_compact("127.0.0.1").value()};
            self_addr.port = port;

            std::vector<dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument> rs{};

            for (size_t i = 0u; i < packet_count; ++i)
            {
                dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument arg = {};

                arg.to          = self_addr;
                arg.content     = make_incremental_str(packet_sz, resource_container);
                arg.content_sz  = packet_sz;

                rs.push_back(std::move(arg));
            }

            return rs;
        }
};

class IPSiever: public virtual dg_sock::network_kernel_mailbox_impl1::external_interface::IPSieverInterface{

    public:

        auto thru(dg_sock::network_kernel_mailbox_impl1::model::Address) noexcept -> std::expected<bool, exception_t>{

            return true;
        }
};

void init_concurrency()
{
    using namespace dg_sock::network_concurrency;

    const size_t WORKER_SZ = 8u;
    std::vector<WorkerInformation> worker_info_vec{};

    for (size_t i = 0u; i < WORKER_SZ; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_UNIT_DAEMON});
    }

    for (size_t i = 0u; i < WORKER_SZ; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_STREAM_DAEMON});
    }

    for (size_t i = 0u; i < WORKER_SZ; ++i)
    {
        worker_info_vec.push_back(WorkerInformation{.cpu_id = std::nullopt, .daemon = MAILBOX_CHANNEL_DAEMON});
    }

    dg_sock::network_concurrency::init({worker_info_vec});
}

int main()
{
    //I wish there was an answer that does not involve wait() + hardware affinity
    //it just seems to me that we cannot generalize for other operating systems or other use cases

    //we'd test 1GB/s bandwidth today and move on to other tasks

    {
        init_concurrency();
        cron_subsystem::init();

        std::cout << "initializing network_allocation" << std::endl;
        dg_sock::network_allocation::init();

        std::cout << "initializing network_randomizer" << std::endl;
        dg_sock::network_randomizer::init();

        std::filesystem::path tmp_file = std::filesystem::temp_directory_path() / "sock_stream_test.txt";

        std::cout << "initializing logging_subsystem" << std::endl;
        dg_sock::network_log::init(tmp_file);

        {
            std::cout << "initializing stack allocation" << std::endl;
            dg_sock::network_stack_allocation::init();

            auto [retry_device_up, destructor] = dg_sock::network_concurrency_infretry_x::get_infretry_machine(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1000))); 
            std::shared_ptr<dg_sock::network_concurrency_infretry_x::ExecutorInterface> retry_device = std::move(retry_device_up);

            std::cout << "making socket ..." << std::endl;

            std::cout << "initializing unit mailbox allocation" << std::endl;
            dg_sock::network_kernel_mailbox_impl1::allocation::init({.total_mempiece_count = 1 << 20,
                                                                .mempiece_sz = 1 << 10,
                                                                .affined_refill_sz = 1 << 8,
                                                                .affined_mem_vec_capacity = 1 << 8,
                                                                .affined_free_vec_capacity = 1 << 8});

            std::cout << "initializing stream mailbox allocation" << std::endl;
            dg_sock::network_kernel_mailbox_impl1_flash_stream_x::init_memory(
                                                            {.total_mempiece_count = 1 << 20,
                                                            .mempiece_sz = 1 << 12,
                                                            .affined_refill_sz = 1 << 8,
                                                            .affined_mem_vec_capacity = 1 << 8,
                                                            .affined_free_vec_capacity = 1 << 8});

            std::cout << "initializing base sock" << std::endl;
            auto sock = dg_sock::network_kernel_mailbox_impl1::spawn(dg_sock::network_kernel_mailbox_impl1::Config
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
                .retransmission_queue_cap = 1 << 12,
                .retransmission_user_queue_cap = 1 << 10,
                .retransmission_packet_cap = 10,
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
                .inbound_buffer_has_fair_redistribution = true,
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
                .inbound_packet_has_fair_redistribution = true,
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

                .mailbox_inbound_cap = size_t{1} << 4,
                .mailbox_outbound_cap = size_t{1} << 4,
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

                .outbound_tc_has_borderline_per_outbound_worker = true,
                .outbound_tc_border_line_sz = uint32_t{1} << 20,
                .outbound_tc_peraddr_cap = uint32_t{1} << 20,
                .outbound_tc_global_cap = uint32_t{1} << 20,
                .outbound_tc_addrmap_cap = uint32_t{1} << 20,
                .outbound_tc_side_cap = uint32_t{1} << 20,
                .natip_controller = dg_sock::network_kernel_mailbox_impl1::get_default_natip_controller(std::make_unique<IPSiever>(), std::make_unique<IPSiever>(), 1024, 1024),
                .retry_device = retry_device
            });

            std::cout  << "initializing stream sock" << std::endl;
            auto sock2 = dg_sock::network_kernel_mailbox_impl1_flash_stream_x::spawn(dg_sock::network_kernel_mailbox_impl1_flash_stream_x::Config
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
                .base = std::move(sock)
            });

            std::cout << "initializing channel sock" << std::endl;
            auto sock3 = dg_sock::network_kernel_mailbox_impl1_channel_x::SolutionBuilder{}.set_base(std::move(sock2))
                                                                                           .add_channel(RECV_CHANNEL, dg_sock::network_kernel_mailbox_impl1_channel_x::PreconfiguratedStickyChannelContainer::MILLION_CHANNEL_CODEX, RECV_CHANNEL_MSG_CAP)
                                                                                           .set_worker_size(1)
                                                                                           .build();

            std::cout << "made socket ..." << std::endl;

            std::binary_semaphore smp(0u);

            auto test_worker = std::make_unique<TestWorker>(sock3.get(), &smp);
            // auto hello_world_worker = std::make_unique<HelloWorldWorker>(sock.get());

            dg_sock::network_concurrency::daemon_register(dg_sock::network_concurrency::MAILBOX_STREAM_DAEMON, std::move(test_worker));
            // dg_sock::network_concurrency::daemon_register(dg_sock::network_concurrency::IO_DAEMON, std::move(hello_world_worker));

            smp.acquire();

            std::cout << "test done" << std::endl;
            std::abort();
        }
    }
}

#endif