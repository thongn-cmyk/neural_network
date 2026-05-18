#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include "network_exception.h"

int main()
{
    try
    {
        dg_sock::network_exception::throw_exception(dg_sock::network_exception::RUNTIME_SOCKETIO_ERROR);
    }
    catch (...)
    {
        std::cout << static_cast<size_t>(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
    }
}