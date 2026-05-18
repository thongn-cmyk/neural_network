#ifndef __DEVIATION_PROJECTION_INGESTION_AID_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <deviation_projection_client/deviation_projection_client.h>
#include "model.h"
#include <stdexcept>
#include <exception>
#include <data_loader/source_loader/multisource_loader.h>
#include <fire_bandwidth_control/generic_firer.h>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <deviation_projection_ingestion_aid_client/deviation_projection_ingestion_aid_client.h>

namespace deviation_projection_ingestion_aid
{
    class PiecewiseBuilder
    {
        private:

            std::optional<Remote> _worker_remote;
            std::optional<ClientRemote> _client_remote;
            std::optional<data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig> _data_loader_config;
            std::optional<fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig> _firer_config;
            connectivity_subsystem::MasterConfiguration _connection_config;

            using self = PiecewiseBuilder;

            static auto get_default_connection_config() -> connectivity_subsystem::MasterConfiguration
            {
                return connectivity_subsystem::MasterConnection::default_master_configuration();
            }

        public:

            PiecewiseBuilder(): _worker_remote(std::nullopt),
                                _client_remote(std::nullopt),
                                _data_loader_config(std::nullopt),
                                _firer_config(std::nullopt),
                                _connection_config(get_default_connection_config()){}

            auto worker_remote(const Remote& arg) -> self&
            {
                this->_worker_remote = arg;

                return *this;
            }

            auto client_remote(const Remote& remote, uint64_t client_id) -> self&
            {
                this->_client_remote = ClientRemote
                {
                    .remote     = remote,
                    .client_id  = client_id
                };

                return *this;
            }

            auto data_loader_config(const data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig& arg) -> self&
            {
                this->_data_loader_config = arg;

                return *this;
            }

            auto firer_config(const fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig& arg) -> self&
            {
                this->_firer_config = arg;

                return *this;
            }

            auto connection_config(const connectivity_subsystem::MasterConfiguration& arg) -> self&
            {
                this->_connection_config = arg;

                return *this;
            }

            auto build() -> PiecewiseArgument
            {
                if (!this->_worker_remote.has_value())
                {
                    throw std::invalid_argument("bad worker remote, null");
                }

                if (!this->_client_remote.has_value())
                {
                    throw std::invalid_argument("bad client remote, null");
                }

                if (!this->_data_loader_config.has_value())
                {
                    throw std::invalid_argument("bad data loader config, null");
                }

                if (!this->_firer_config.has_value())
                {
                    throw std::invalid_argument("bad firer config, null");
                }

                return PiecewiseArgument
                {
                    .worker_remote      = this->_worker_remote.value(),
                    .client_remote      = this->_client_remote.value(),
                    .data_loader_config = this->_data_loader_config.value(),
                    .firer_config       = this->_firer_config.value(),
                    .connection_config  = this->_connection_config
                };
            }
    };

    class ClientTrainingDataPiecewiseIngestor
    {
        private:

            std::vector<PiecewiseArgument> piecewise_argument_vec;
            dg_sock::network_rest_frame::client::retry_policy_t retry_policy;
            std::chrono::nanoseconds sleep_dur;

            using self = ClientTrainingDataPiecewiseIngestor;

            static inline const dg_sock::network_rest_frame::client::retry_policy_t DEFAULT_RETRY_POLICY    = dg_sock::network_rest_frame::client::RequestRetryMachineFactory<>::EXPONENTIAL_HARD;
            static inline const std::chrono::nanoseconds DEFAULT_SLEEP_DUR                                  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));

        public:

            ClientTrainingDataPiecewiseIngestor(): piecewise_argument_vec(),
                                                   retry_policy(DEFAULT_RETRY_POLICY),
                                                   sleep_dur(DEFAULT_SLEEP_DUR){}

            auto add(const PiecewiseArgument& arg) -> self&
            {
                this->piecewise_argument_vec.push_back(arg);

                return *this;
            }

            template <class ...Args>
            auto set_sleep_dur(std::chrono::duration<Args...> dur) -> self&
            {
                std::chrono::nanoseconds casted_dur = std::chrono::duration_cast<std::chrono::nanoseconds>(dur);

                if (casted_dur < std::chrono::nanoseconds(0))
                {
                    throw std::invalid_argument("bad sleep dur, negative");
                }

                this->sleep_dur = casted_dur;

                return *this;
            }

            void run(common_exception::CancellationTokenInterface& cancellation_token)
            {
                using namespace deviation_projection_ingestion_aid_client;

                std::vector<std::unique_ptr<APIClient>> api_client_vec{};

                for (const auto& piecewise_argument: this->piecewise_argument_vec)
                {
                    api_client_vec.push_back(std::make_unique<APIClient>(piecewise_argument.worker_remote,
                                                                         piecewise_argument.connection_config));


                    api_client_vec.back()->set_retry_policy(this->retry_policy);
                    api_client_vec.back()->set_unique_request(true);

                    api_client_vec.back()->run(this->to_run_payload(piecewise_argument))->wait();
                }

                while (true)
                {
                    if (cancellation_token.is_canceled())
                    {
                        common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                    }

                    std::vector<bool> is_completed_vec  = this->wait_all(this->get_is_completed(api_client_vec));
                    bool is_completed                   = std::find(is_completed_vec.begin(), is_completed_vec.end(), false) == is_completed_vec.end();

                    if (is_completed)
                    {
                        this->wait_all(this->get_result(api_client_vec));
                        return;
                    }

                    std::this_thread::sleep_for(this->sleep_dur);
                }
            }

        private:

            template <class T>
            auto wait_all(const std::vector<std::shared_ptr<Promise<T>>>& promise_vec) -> std::vector<T>
            {
                std::vector<T> rs_vec{};

                for (const auto& promise: promise_vec)
                {
                    rs_vec.push_back(promise->wait());
                }

                return rs_vec;
            }

            auto get_is_completed(const std::vector<std::unique_ptr<deviation_projection_ingestion_aid_client::APIClient>>& api_client_vec) -> std::vector<std::shared_ptr<Promise<bool>>>
            {
                std::vector<std::shared_ptr<Promise<bool>>> rs_vec{};

                for (const auto& api_client: api_client_vec)
                {
                    rs_vec.push_back(api_client->is_completed());
                }

                return rs_vec;
            }

            auto get_result(const std::vector<std::unique_ptr<deviation_projection_ingestion_aid_client::APIClient>>& api_client_vec) -> std::vector<std::shared_ptr<Promise<stdx::fancy_void>>>
            {
                std::vector<std::shared_ptr<Promise<stdx::fancy_void>>> rs_vec{};

                for (const auto& api_client: api_client_vec)
                {
                    rs_vec.push_back(api_client->get_result());
                }

                return rs_vec;
            }

            auto to_run_payload(const PiecewiseArgument& arg) -> deviation_projection_ingestion_aid_client::RunPayload
            {
                return
                {
                    .data_loader_config = arg.data_loader_config,
                    .server_sink_vec    = {deviation_projection_ingestion_aid_client::ServerSink{.remote = arg.client_remote.remote, .client_id = arg.client_remote.client_id}},
                    .token_firer_config = arg.firer_config
                };
            }
    };
}

#endif