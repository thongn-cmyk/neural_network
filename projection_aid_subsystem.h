#ifndef __PROJECTION_AID_SUBSYSTEM_H__
#define __PROJECTION_AID_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "float_def.h"

namespace projection_aid_subsystem
{
    using namespace float_def;

    struct Remote
    {
        std::string ipv6;
        uint16_t port;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ipv6, port);
        }

        template <class Reflector>
        void dg_reflect(const Refelctor& reflector)
        {
            reflector(ipv6, port);
        }
    };

    class ClientBox
    {
        private:

            std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_data;
            std::vector<generic_matrix_factory::GenericMatrixResource> matrix_resource_vec;
            std::unique_ptr<matrix_deviation_calculator::MatrixDeviationCalculatorInterface> deviation_calculator;

        public:

            ClientBox(): training_data(),
                         matrix_resource_vec(),
                         deviation_calculator(){}

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                if (inp == nullptr)
                {
                    throw std::invalid_argument("bad input matrix, null");
                }

                if (out == nullptr)
                {
                    throw std::invalid_argument("bad output matrix, null");
                }

                this->training_data.push_back({inp, out});
            }

            void clear_training_data() noexcept
            {
                this->training_data.clear();
            }

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                this->matrix_resource_vec = matrix_resource_vec;
            }
            
            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                this->deviation_calculator = generic_matrix_deviation_calculator_factory::Factory::get_from_generic_codex(kind);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                if (this->deviation_calculator == nullptr)
                {
                    throw std::runtime_error("incomplete setup, missing deviation calculator");
                }

                std::vector<mdc_float_t> rs_vec{};

                for (const auto& matrix_resource: this->matrix_resource_vec)
                {
                    std::unique_ptr<the_matrix::MatrixInterface> the_matrix = this->wrap_matrix(generic_matrix_factory::GenericMatrixLoader{}.load_resource(matrix_resource));

                    std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec{};
                    std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec{};
                    std::vector<std::shared_ptr<tensor_model::Matrix>> expected_vec{};

                    for (const auto& [inp, out]: this->training_data)
                    {
                        inp_vec.push_back(inp);
                        expected_vec.push_back(out);
                    }

                    out_vec = the_matrix->project(inp_vec);

                    rs_vec.push_back(this->deviation_calculator->get_deviation(stdx::zip(out_vec, expected_vec)));
                }

                return rs_vec;
            }
    };

    class ConnectionBoundClientBox
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            ClientBox base;
            bool was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::SlaveConnection>(connection_config)),
                                                                                                           base(),
                                                                                                           was_explicitly_destroyed(false),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.add_training_data(inp, out);
            }

            void clear_training_data() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.clear_training_data();
            }

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.set_matrix_resource(matrix_resource_vec);
            }

            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.set_deviation_calculator(kind);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->base.get();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (std::exchange(this->was_explicitly_destroyed, true))
                {
                    return;
                }

                this->connection->close();
            }

            auto is_closed() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->was_explicitly_destroyed;
            }

            auto is_alive() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->connection->is_alive();
            }
    };

    //I was thinking about a relay system, for master to talk to masters, build a heap of communication, essentially to increase bandwidth + reduce latency of the broadcasting matrices (its only 1 or 2 MB)
    //it's complicated how we should build this
    //we'll solve that tomorrow, I think that is two_distinct_responsibilities, but not sure of the reduction process, it's somewhat coupled into this projection aid subssytem, so it's best to just ... solve it all right here

    //I have spent 1 year to tune the REST controller to 10GB/s, it's complicated
    //but it's actually about non-waiting and reactive response
    //the latency is probably ~10-20ms but we'd want to shorten the tree of requests tmr, which we'd discuss in depth
    //I dont really want to talk about who owns what, what is in filesystem or RAM
    //it's a filesystem and a management system that is out of the scope of this project
    //we'd try to fix the session and the connectivity instead of being paranoid about things, worst case, save it and re-ingest the data from files

    class ClientManager: public virtual cron_subsystem::UpdatableInterface
    {
        private:

            std::unordered_map<uint64_t, std::shared_ptr<ConnectionBoundClientBox>> client_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            uint64_t id_counter;

        public:

            ClientManager(): client_map(),
                             mtx(fair_mutex::make_unique_fair_atomic_flag()),
                             id_counter(0u){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                std::shared_ptr<ConnectionBoundClientBox> obj = std::make_shared<ConnectionBoundClientBox>(connection_config);

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    uint64_t nxt_id             = this->id_counter;
                    this->client_map[nxt_id]    = std::move(obj);
                    this->id_counter            = nxt_id + 1u;

                    return nxt_id;
                }
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (auto map_ptr = this->client_map.find(client_box_id); map_ptr != this->client_map.end())
                {
                    return map_ptr->second;
                }

                return nullptr;
            }

            void close_client_box(uint64_t client_box_id) noexcept
            {
                std::shared_ptr<ConnectionBoundClientBox> tmp{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (auto map_ptr = this->client_map.find(client_box_id); map_ptr != this->client_map.end())
                    {
                        tmp = std::move(map_ptr->second);
                        this->client_map.erase(map_ptr);
                    }
                    else
                    {
                        tmp = nullptr;
                    }
                }

                if (tmp != nullptr)
                {
                    tmp->close();
                }
            }

            void update()
            {
                std::vector<std::pair<uint64_t, std::shared_ptr<ConnectionBoundClientBox>>> client_vec{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                    std::copy(this->client_map.begin(), this->client_map.end(), std::back_inserter(client_vec));
                }

                std::unordered_set<uint64_t> bad_client_id_set{};

                for (const auto& [client_id, client_instance]: client_vec)
                {
                    bool is_bad_client;

                    try
                    {
                        is_bad_client = client_instance->is_closed() || !client_instance->is_alive();
                    }
                    catch (...)
                    {
                        is_bad_client = true;
                    }

                    if (is_bad_client)
                    {
                        bad_client_id_set.insert(client_id);
                        client_instance->close();
                    }
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                    decltype(this->client_map) new_client_map{};

                    for (const auto& [client_id, client_instance]: this->client_map)
                    {
                        if (!bad_client_id_set.contains(client_id))
                        {
                            new_client_map.insert({client_id, client_instance});
                        }
                    }

                    this->client_map = std::move(new_client_map);
                }
            }
    };

    class SelfObservedClientManager
    {
        private:

            std::shared_ptr<ClientManager> base;
            std::shared_ptr<void> cron_obj;

        public:

            static inline const std::chrono::nanoseconds CROM_DURATION = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

            SelfObservedClientManager()
            {
                this->base      = std::make_shared<ClientManager>();
                this->cron_obj  = cron_subsystem::register_periodic_cronjob(this->base, CRON_DURATION);
            }

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                return this->base->open_client_box(connection_config);
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return this->base->get_client_box(client_box_id);
            }

            void close_client_box(uint64_t client_box_id) noexcept
            {
                this->base->close_client_box(client_box_id);
            }
    };

    struct OpenClientRequest
    {
        connectivity_subsystem::SlaveConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_config);
        }
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, projection_aid_subsystem::exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct CloseClientRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct CloseClientResponse
    {
        projection_aid_subsystem::exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct AddTrainingDataRequest
    {
        uint64_t client_id;
        std::vector<tensor_model::tensor_std_float_t> input_logit_vec;
        std::vector<uint64_t> input_logit_vec_shape;
        std::vector<tensor_model::tensor_std_float_t> output_logit_vec;
        std::vector<uint64_t> output_logit_vec_shape;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id,
                      input_logit_vec, input_logit_vec_shape,
                      output_logit_vec, output_logit_vec_shape);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id,
                      input_logit_vec, input_logit_vec_shape,
                      output_logit_vec, output_logit_vec_shape);
        }
    };

    struct AddTrainingDataResponse
    {
        projection_aid_subsystem::exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct SetMatrixResourceRequest
    {
        uint64_t client_id;
        std::vector<generic_matrix_factory::GenericMatrixResource> matrix_resource_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, matrix_resource_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, matrix_resource_vec);
        }
    };

    struct SetMatrixResourceResponse
    {
        projection_aid_subsystem::exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct SetDeviationCalculatorRequest
    {
        uint64_t client_id;
        generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, kind);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, kind);
        }
    };

    struct SetDeviationCalculatorResponse
    {
        projection_aid_subsystem::exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct GetDeviationRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct GetDeviationResponse
    {
        std::expected<mdc_float_t, projection_aid_subsystem::exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    class OpenClientResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            OpenClientResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                OpenClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<OpenClientRequest>(request.content);
                OpenClientResponse semantic_response;

                try
                {
                    uint64_t client_box_id = this->client_manager->open_client_box(semantic_request.connection_config);
                    
                    semantic_response = OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (const std::invalid_argument& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                catch (const std::exception& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(INTERNAL_SERVER_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                
                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class CloseClientResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            CloseClientResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                CloseClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<CloseClientRequest>(request.content);
                this->client_manager->close_client_box(semantic_request.client_id);
                CloseClientResponse semantic_response
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class AddTrainingDataResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            AddTrainingDataResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                AddTrainingDataRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<AddTrainingDataRequest>(request.content);
                AddTrainingDataResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = AddTrainingDataResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        std::shared_ptr<tensor_model::Matrix> inp = tensor_matrix_operation::make_matrix_from_flat_vec(stdx::to_castable_vector_initializer(semantic_request.input_logit_vec_shape),
                                                                                                                       semantic_request.input_logit_vec);

                        std::shared_ptr<tensor_model::Matrix> out = tensor_matrix_operation::make_matrix_from_flat_vec(stdx::to_castable_vector_initializer(semantic_request.output_logit_vec_shape),
                                                                                                                       semantic_request.output_logit_vec);

                        client_box->add_training_data(inp, out);

                        semantic_Response = AddTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::dg_dgstd_serialization_identifier()
                };
            }
    };

    class SetMatrixResourceResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            SetMatrixResourceResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                SetMatrixResourceRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<SetMatrixResourceRequest>(request.content);
                SetMatrixResourceResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = SetMatrixResourceResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_matrix_resource(semantic_request.matrix_resource_vec);

                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())  
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::dg_dgstd_serialization_identifier()
                };
            }
    };

    class SetDeviationCalculatorResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            SetDeviationCalculatorResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                SetDeviationCalculatorRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<SetDeviationCalculatorRequest>(request.content);
                SetDeviationCalculatorResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = SetDeviationCalculatorResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_deviation_calculator(semantic_request.kind);

                        semantic_response = SetDeviationCalculatorResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = SetDeviationCalculatorResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = SetDeviationCalculatorResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::dg_dgstd_serialization_identifier()
                };
            }
    };

    class GetDeviationResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;
        
        public:

            GetDeviationResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetDeviationRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<GetDeviationRequest>(request.content);
                GetDeviationResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = GetDeviationResponse
                    {
                        .result = std::unexpected(CLIENT_NOT_FOUND_ERROR_CODE),
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = client_box->get(),
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(RUNTIME_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    void init()
    {
        ClientManagerSingleton::initialize();

        internal_rest_controller::hook(RestConfiguration::get_open_client_url(), std::make_unique<OpenClientResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_close_client_url(), std::make_unique<CloseClientResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_add_training_data_url(), std::make_unique<AddTrainingDataResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_set_matrix_resource_resolver_url(), std::make_unique<SetMatrixResourceResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_set_deviation_calculator_resolver_url(), std::make_unique<SetDeviationCalculatorResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_get_deviation_resolver_url(), std::make_unique<GetDeviationResolver>(ClientManagerSingleton::get()));
    }

    void deinit() noexcept
    {
        internal_rest_controller::unhook(RestConfiguration::get_get_deviation_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_deviation_calculator_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_matrix_resource_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_add_training_data_url());
        internal_rest_controller::unhook(RestConfiguration::get_close_client_url());
        internal_rest_controller::unhook(RestConfiguration::get_open_client_url());

        ClientManagerSingleton::deinitialize();
    }

    //----

    class APIClient
    {
        private:

            internal_rest_controller::RequestClient client;

        public:

            APIClient(): client(){}

            auto set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine)
            {
                this->client.set_retry_policy(std::move(retry_machine));
            }

            auto open_client_box(const Remote& remote,
                                 const connectivity_subsystem::SlaveConnectionConfiguration& connection_config) -> uint64_t
            {
                internal_rest_controller::Url url = RestConfiguration::get_open_client_url(remote);
                OpenClientRequest request
                {
                    .connection_config = connection_config
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                OpenClientResponse response = dg::network_compact_serializer::dgstd_deserialize<OpenClientResponse>(response.content);

                if (!response.result.has_value())
                {
                    if (response.result.error() == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(response.err_verbal_description);
                    }

                    throw std::runtime_error(response.err_verbal_description);
                }
            }

            void close_client_box(const Remote& remote, uint64_t client_id)
            {

            }

            auto add_training_data_2(const Remote& remote, uint64_t client_id,
                                     const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                internal_rest_controller::Url url = RestConfiguration::get_add_training_data_url(remote);
                AddTrainingDataRequest request
                {
                    .client_id                  = client_id,
                    .input_logit_vec            = this->matrix_to_flat_logit_vec(inp),
                    .input_logit_vec_shape      = this->get_matrix_shape(inp),
                    .output_logit_vec           = this->matrix_to_flat_logit_vec(out),
                    .output_logit_vec_shape     = this->get_matrix_shape(out)
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                auto resolutor = [](const internal_rest_controller::ClientResponse& response)
                {
                    if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                    {
                        throw std::runtime_error("unexpected serialization format");
                    }

                    AddTrainingDataResponse response = dg::network_compact_serializer::dgstd_deserialize<AddTrainingDataResponse>(response.content);

                    if (response.result != SUCCESS)
                    {
                        if (response.result == INVALID_ARGUMENT_ERROR_CODE)
                        {
                            throw std::invalid_argument(response.err_verbal_description);
                        }

                        throw std::runtime_error(response.err_verbal_description);
                    }
                };

                return this->client.set_request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            void add_training_data(const Remote& remote, uint64_t client_id,
                                   const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                this->add_training_data_2(remote, client_id, inp, out)->wait();
            }

            void clear_training_data(const Remote& remote, uint64_t client_id)
            {

            }

            void set_matrix_resource_2(const Remote& remote, uint64_t client_id,
                                       const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                internal_rest_controller::Url url = RestConfiguration::get_set_matrix_resource_url(remote);
                SetMatrixResourceRequest request
                {
                    .client_id = client_id,
                    .matrix_resource_vec = matrix_resource_vec
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();
                
                auto resolutor = [](const internal_rest_controller::ClientResponse& response)
                {
                    if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                    {
                        throw std::runtime_error("unexpected serialization format");
                    }

                    SetMatrixResourceResponse response = dg::network_compact_serializer::dgstd_deserialize<SetMatrixResourceResponse>(response.content);

                    if (response.result != SUCCESS)
                    {
                        if (response.result == INVALID_ARGUMENT_ERROR_CODE)
                        {
                            throw std::invalid_argument(response.err_verbal_description);
                        }

                        throw std::runtime_error(response.err_verbal_description);
                    }
                };

                return this->client.set_request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            void set_matrix_resource(const Remote& remote, uint64_t client_id,
                                     const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                this->set_matrix_resource_2(remote, client_id, matrix_resource_vec)->wait();
            }

            void set_deviation_calculator(const Remote& remote, uint64_t client_id,
                                          generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {

            }

            auto get(const Remote& remote, uint64_t client_id) -> std::vector<mdc_float_t>
            {

            }
    };

    class APIClient_2
    {
        private:

            APIClient base;
            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            bool was_explicitly_closed;
            Remote remote;
            uint64_t client_id;

        public:

            APIClient_2(const Remote& remote,
                        const connectivity_subsystem::MasterConfiguration& config)
            {
                this->base                  = APIClient();

                auto tmp_config             = config;
                tmp_config.slave_count      = 1u;

                std::unique_ptr<connectivity_subsystem::MasterConnection connection = std::make_unique<connectivity_subsystem::MasterConnection>(tmp_config);

                this->client_id             = this->base.open_client_box(remote, connection->get_slave_configuration());
                this->connection            = std::move(connection);
                this->was_explicitly_closed = false;
                this->remote                = remote;
            }

            ~APIClient_2() noexcept
            {
                this->close();
            }

            auto set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine) -> APIClient_2&
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_retry_policy(std::move(retry_machine));

                return *this;
            }

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.add_training_data(this->remote, this->client_id, inp, out);
            }

            auto add_training_data_2(const std::sahred_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.add_training_data_2(this->remote, this->client_id, inp, out);
            }

            void clear_training_data()
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.clear_training_data(this->remote, this->client_id);
            }

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_matrix_resource(this->remote, this->client_id, matrix_resource_vec);
            }

            auto set_matrix_resource_2(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                if (!this->can_operate(0))
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.set_matrix_resource_2(matrix_resource_vec);
            }

            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_deviation_calculator(this->remote, this->client_id, this->kind);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.get(this->remote, this->client_id);
            }

            void close() noexcept
            {
                if (std::exchange(this->was_explicitly_closed, true))
                {
                    return;
                }

                try
                {
                    this->base.close_client_box(this->remote, this->client_id);
                }
                catch (...){}

                this->connection->close();
            }
        
        private:
            
            auto can_operate() -> bool
            {
                if (!this->connection->is_alive())
                {
                    return false;
                }

                if (this->was_explicitly_closed)
                {
                    return false;
                }

                return true;
            }
    };

    class TrainingDataIterableInterface
    {
        public:

            virtual ~TrainingDataIterableInterface() = default;
            virtual auto next() -> std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>> = 0;
            virtual auto has_next() -> bool = 0;
    };

    class UniformDataIngestor
    {
        private:

            std::shared_ptr<TrainingDataIterableInterface> training_data;
            std::vector<std::shared_ptr<APIClient_2>> client_vec;
            size_t pipe_sz;

        public:

            auto set_data_source(const std::shared_ptr<TrainingDataIterableInterface>& training_data) -> UniformDataIngestor&
            {
                this->training_data = training_data;
            }

            auto set_client_vector(const std::vector<std::shared_ptr<APIClient_2>>& client_vec) -> UniformDataIngestor&
            {
                this->client_vec = client_vec;
            }

            auto set_pipe_size() -> UniformDataIngestor&
            {
                this->pipe_sz = pipe_sz;
            }

            void run()
            {
                if (this->training_data == nullptr)
                {
                    throw std::runtime_error("bad training data instance, null");
                }

                if (this->client_vec.empty())
                {
                    throw std::runtime_error("bad client vector, empty");
                }

                if (this->pipe_sz == 0u)
                {
                    throw std::runtime_error("bad pipe size, 0");
                }

                std::deque<std::unique_ptr<internal_rest_controller::Promise<void>>> promise_vec{};
                size_t counter = 0u;

                while (this->training_data->has_next())
                {
                    auto [inp, out]     = this->training_data->next();
                    size_t client_idx   = counter % this->client_vec.size();

                    if (this->client_vec[client_idx] == nullptr)
                    {
                        throw std::runtime_error("bad client instance, null");
                    }

                    if (promise_vec.size() == this->pipe_sz)
                    {
                        promise_vec.front()->wait();
                        promise_vec.pop_front();
                    }

                    promise_vec.push_back(this->client_vec[client_idx]->add_training_data_2(inp, out));
                    counter += 1;
                }

                for (const auto& promise: promise_vec)
                {
                    promise->wait();
                }
            }
    };

    class TrainingSession
    {
        public:

            auto set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> TrainingSession&
            {
                return *this;
            }

            template <class FloatType = float_def::std_float_t>
            auto get() -> std::vector<FloatType>
            {
                return {};
            }
    };

    class TrainingSessionFactory
    {
        public:

            static inline constexpr uint8_t RETRY_POLICY_MINIMUM_EFFORT = 0u;
            static inline constexpr uint8_t RETRY_POLICY_MEDIUM_EFFOT   = 1u;
            static inline constexpr uint8_t RETRY_POLICY_MAXIMUM_EFFORT = 2u;

        public:

            auto set_matrix_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t calculator_kind) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_matrix_deviation_reducer(generic_matrix_deviation_calculator_factory::matrix_deviation_reducer_t reducer_kind) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_remote(const std::vector<std::string>& ipv6_addr_vec) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_training_data(const std::shared_ptr<TrainingDataIterableInterface>& training_data_iterable) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_connection_timeout(std::chrono::nanoseconds dur) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_session_timeout(std::chrono::nanoseconds dur) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_concurrency_size(size_t sz) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_retry_policy(uint8_t retry_policy) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto get() -> std::unique_ptr<TrainingSession>
            {
                return {};
            }
    };
}

#endif