#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <allocation_base/global_allocator.h>
#include <allocation_base/stack_allocator.h>

int main()
{
    // connectivity_subsystem::init();
    allocation_base::global_allocator::init();
    allocation_base::global_allocator::deinit();

    allocation_base::stack_allocator::NoExceptAllocation<size_t[]> allocation(2);

    // connectivity_subsystem::MasterConnection connection({});
    // connectivity_subsystem::SlaveConnection connection2({});

}