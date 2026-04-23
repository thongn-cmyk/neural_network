#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <matrix_broker_server/starter.h>

int main()
{
    matrix_broker_server::start_server();
}