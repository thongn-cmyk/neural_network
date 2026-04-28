#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <deviation_projection_ingestion_aid_server/client_box.h>
#include <deviation_projection_ingestion_aid_server/controller.h>
#include <deviation_projection_ingestion_aid_server/starter.h>

int main()
{
    deviation_projection_ingestion_aid_server::start_server();
    deviation_projection_ingestion_aid_server::stop_server();
}