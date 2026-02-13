#include <iostream>
#include "dg_buf.h"

int main()
{
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<size_t>>> test_data =
    {
        {"test_field1", 
            {
                {"test_field1_1", {11, 111}}, 
                {"test_field1_2", {12, 122}},
                {"test_field1_3", {13, 133}}
            }
        },
        {"test_field2", 
            {
                {"test_field2_1", {21, 211}},
                {"test_field2_2", {22, 222}},
                {"test_field_2_3", {23, 233}}
            }
        },
        {"test_field3", 
            {
                {"test_field3_1", {31, 311}},
                {"test_field3_2", {32, 322}},
                {"test_field3_3", {33, 333}}
            }
        }
    };

    std::string streamable{};

    auto serialized = dg::dgbuf::stl_to_dgbuf::serializer{}.serialize(std::move(test_data), streamable);
    serialized = dg::dgbuf::autosetter::AutoSetter{}.auto_set(serialized, streamable.data());

    std::cout << serialized["test_field1"]["test_field1_1"][1] << std::endl;

    if (serialized == serialized)
    {

    }

    if (serialized != serialized)
    {

    }

    for (const auto [field0, rhs]: serialized)
    {
        for (const auto [field1, data]: rhs)
        {
            for (const auto e: serialized[field0][field1])
            {
                std::cout << std::string_view(field0) << "<>" << std::string_view(field1) << "<>" << e << std::endl;
            }
        }
    }
}