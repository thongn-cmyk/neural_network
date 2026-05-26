#ifndef __MONEY_SOLUTION_SOLUTION_SERVER_CLIENT_BOX_H__
#define __MONEY_SOLUTION_SOLUTION_SERVER_CLIENT_BOX_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "model.h"
#include <mutex_extension/fair_mutex.h>
#include <atomic>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <connection_based_manager/connection_based_manager.h>
#include <stl_extension/semantic_mapper.h>

namespace stock_solution_server
{
    class ClientBox
    {
        private:

            std::unique_ptr<stock_solution::SolutionProduct> solution_product;
        
        public:

            ClientBox(): solution_product(){}

            void set_solution(const ExternalSolutionData& solution_data)
            {
                this->solution_product  = std::make_unique<stock_solution::SolutionProduct>
                (
                    stdx::semantic_map<stock_solution::ExternalSolutionData>(solution_data)
                );
            }

            auto get_recommendation(const MarketData& market_data,
                                    std::chrono::time_point<std::chrono::utc_clock> forecast_timepoint,
                                    std::optional<size_t> top_k) -> Actionables
            {
                if (this->solution_product == nullptr)
                {
                    throw std::invalid_argument("bad solution product, null");
                }

                //... interruptability

                std::vector<stock_solution::Actionable> actionable_vec = this->solution_product->load_data(market_data.ticker_data_vec)
                                                                                                .predict(forecast_timepoint);

                Actionables rs{};

                for (const auto& actionable: actionable_vec)
                {
                    rs.actionable_vec.push_back(this->actionable_semantic_map(actionable));
                }

                return this->get_top_k(rs, top_k);
            }

        private:

            auto actionable_semantic_map(const stock_solution::Actionable& actionable) -> Actionable
            {

            }

            auto get_top_k(const Actionables& actionables,
                           std::optional<size_t> top_k) -> Actionables
            {
                if (!top_k.has_value())
                {
                    return actionables;
                }

                auto cmp_func = [](const Actionable& lhs, const Actionable& rhs)
                {
                    if (std::isnan(lhs.norm_confident_score))
                    {
                        std::abort();
                    }

                    if (std::isnan(rhs.norm_confident_score))
                    {
                        std::abort();
                    }

                    return lhs.norm_confident_score >= rhs.norm_confident_score;
                };

                Actionables rs      = actionables;
                algorithm_extension::make_heap(rs.actionable_vec.begin(), rs.actionable_vec.end(), cmp_func);
                auto first          = algorithm_extension::top_k(rs.actionable_vec.begin(), rs.actionable_vec.end(), top_k.value(), cmp_func);
                rs.actionable_vec   = std::vector<Actionable>(first, rs.actionable_vec.end());

                return rs;
            }
    };

    class ConnectionBoundClientBox: public virtual connection_based_manager::HealthcheckableInterface
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            std::unique_ptr<ClientBox> base;
            std::unique_ptr<std::atomic<bool>> was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::ThreadSafeSlaveConnection>(connection_config)),
                                                                                                           base(std::make_unique<ClientBox>()),
                                                                                                           was_explicitly_destroyed(std::make_unique<std::atomic<bool>>(false)),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void set_solution(const ExternalSolutionData& solution_data)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_solution(solution_data);
            }

            auto get_recommendation(const MarketData& market_data,
                                    std::chrono::time_point<std::chrono::utc_clock> forecast_timepoint,
                                    std::optional<size_t> top_k) -> Actionables
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                return this->base->get_recommendation(market_data,
                                                      forecast_timepoint,
                                                      top_k);
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
            }

            auto is_alive() -> bool
            {
                return !this->was_explicitly_destroyed->load(std::memory_order_relaxed) && this->connection->is_alive();
            }
    };

    class ClientBoxManager
    {
        private:

            std::unique_ptr<connection_based_manager::ManagerInterface> base;

        public:

            ClientBoxManager(): base(std::make_unique<connection_based_manager::ClientManager>()){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                this->base->add(std::make_shared<ConnectionBoundClientBox>(connection_config));
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return std::dynamic_pointer_cast<ConnectionBoundClientBox>(this->base->get(client_box_id));
            }

            void close_client_box(uint64_t client_box_id)
            {
                this->base->close(client_box_id);
            }
    };
}

#endif