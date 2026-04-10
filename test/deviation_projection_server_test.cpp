#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <deviation_projection_server/starter.h>

int main()
{
    deviation_projection_server::start_server();
    deviation_projection_server::stop_server();
}