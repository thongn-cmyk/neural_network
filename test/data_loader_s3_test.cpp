#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <data_loader/source/s3_source/s3_source.h>
#include <stdint.h>
#include <stdlib.h>

//I guess that we'd have to implement all the data loaders this morning
//it's complicated

//the number one reason that I can't spawn different threads for requests is flow-control
//because if we have finite threads, and resources, we'd expect that one request lifetime would encapsulate all that it spawns or required
//otherwise we are not flow-control accurate

//I know that you might think that we should not CAP the threads, then we'd have capacity problems in our code
//then you'd say that we should wait for the threads, and control the threads at the spawn and close, then we'd have deadlock

//see, it's super very complicated how we should implement this in a standard manner such that our flow must be perfect

int main()
{

}