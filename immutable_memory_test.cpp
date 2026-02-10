#define STRONG_MEMORY_ORDERING_FLAG true

#include "immutable_memory.h"
#include "cu_immutable_memory.h"

int main()
{
    immutable_memory::Factory::get_normal_immutable_memory_cache({}, {}, {}, {}, {});

    cu_immutable_memory::init({}, {});
    cu_immutable_memory::get_cu_memspan(cu_immutable_memory::acquire_memory({}).value());
    cu_immutable_memory::get_cu_memspan(cu_immutable_memory::cache_n_acquire_memory({}, {}));
    cu_immutable_memory::deinit();
}