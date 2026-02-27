#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include "connectivity_subsystem.h"

int main()
{
    connectivity_subsystem::init();

    connectivity_subsystem::MasterConnection connection({});
    connectivity_subsystem::SlaveConnection connection2({});

    //it's complicated but channel should be processed serially and do hand-to-hand delivery

    //when I think of channel, I think of independent deliveries based on sizes, such is that < 1KB should be processing 1 million requests/ second
    //> 1MB should be processing 1 thousand requests/ second

    //but the scope of that is too big, so we'd have to linger on that for a while
    //I guess that we do have to drop packets on channel, and leave the responsibility of inbound_container size to the timeout_controller
    //because the otherwise is not exactly a disaster but not a way to do things

    //and we'd have to reorganize the code, I'm thinking that we'd have to sub-logic the matrix and pull that as an independent system, though we also run the code here locally, ideally
    //we'd re-organize the code tomorrow

    //I'm thinking of the system as: projection of deviation (only holds the REST_Controller + open connection + project deviation + close connection)
    //                               course master (responsible for steering the course and holds unique reference to the entire binary tree stack of the projection slaves)
    //                               or course master would have to recursively call another course master (this is cleaner logic)

    //ServiceProvider OneStopSystem to link all of the services into one controller, to spawn slaves + spawn master, ingest, train and query data

    //matrix as a service: a single component to do matrix serialization / deserialization (injected both into the projector (maybe) or client-facing service (mandatory))

    //as a client, I want: data, matrix recommendation, data training, and data query
    //we'd have to build that pipeline from A -> Z this week or next week
}