#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include "network_allocation.h"
#include "network_log.h"
#include "network_producer_consumer.h"
#include "network_stack_allocation.h"
#include "network_randomizer.h"
#include "network_compact_serializer.h"
#include "network_compact_trivial_serializer.h"
#include "network_kernel_allocator.h"
#include "network_kernel_allocator_singleton.h"
#include "network_kernel_buffer.h"
#include "network_std_container.h"

int main()
{
    auto test_1 = dg_sock::network_allocation::make_unique<int[]>(10);
    auto test_2 = dg_sock::network_allocation::make_unique<float>(1.1);

    dg_sock::network_kernel_buffer::kernel_string str{};
}