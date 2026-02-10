//I have to say it, getting the framework done was ... definitely a very very hard thing to do

//latency
//thruput

//latency + thruput is theoretically done as close to the book as possible

//disaster recovery (offload to the caller, all packets received are heavily retried until success, callers responsibility is to reasonably chunk and timeout)
//shooting range of packet (cover 100% range, malicious input)

//normal operatable window of operation (works OK most of the time as long as reasonable nozzle size)
//point is the representational state transfer is not easy to implmment, with so many disaster factors that could corrupt the packets or duplicate the packets outside of the duplication detection window
//we solved that by using dedicated ID for each request