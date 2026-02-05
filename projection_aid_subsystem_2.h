#ifndef __PROJECTION_AID_SUBSYSTEM_2_H__
#define __PROJECTION_AID_SUBSYSTEM_2_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "float_def.h"
#include "projection_aid_subsystem.h"

namespace projection_aid_subsystem_2
{
    static inline const std::string DATA_RESOURCE_SERIALIZATION_KIND = dg::network_compact_serializer::get_dgstd_serialization_identifier();

    struct DataSourceRule
    {
        std::vector<uint64_t> inp_shape;
        std::vector<tensor_model::tensor_std_float_t> inp_vec;
        std::vector<uint64_t> out_shape;
        std::vector<tensor_model::tensor_std_float_t> out_vec;
    };

    class ClientBoxInterface
    {            
        public:

            virtual ~ClientBoxInterface() noexcept = 0;

            virtual void set_training_data_source(const data_source::DataSourceConfiguration& data_source_config) = 0;
            virtual void run_data_ingestion() = 0;
            virtual auto is_data_ingestion_completed() = 0;
            virtual void clear_training_data() = 0;

            virtual void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) = 0;
            virtual void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind) = 0;
            virtual void set_deviation_reduction_method(generic_matrix_deviation_calculator_factory::matrix_deviation_reduction_t kind) = 0;

            virtual auto get() -> std::vector<mdc_float_t> = 0;
    };

    //this is pretty complicated, but we'd try to do it like this, we'd provide a data source to pull from, and a parsing rule
    //what this really means is that we'd have a client to implement a client_box_interface and invoke some kind of crazy recursive function
    //I'll be back to implement this tmr, this requires some heavy thinking

    //it's not unintentional that we want uniform responsibilities for all of our nodes
    //it's proven better and more robust such way to code

    class BaseClientBox: public virtual ClientBoxInterface
    {
        private:

            std::shared_ptr<projection_aid_subsystem::ThreadSafe_APIClient_2> api_client;
            std::shared_ptr<projection_aid_subsystem::UniformDataIngestor> data_ingestor;
        
        
    };

    class MasterClientBox: public virtual ClientBoxInterface
    {
        private:

            std::vector<std::unique_ptr<ClientBoxInterface>> client_box_vec;

        public:

            MasterClientBox(std::vector<std::unique_ptr<ClientBoxInterface>> client_box_vec) noexcept: client_box_vec(std::move(client_box_vec)){}
    };
}

#endif