//today we'd work on major major changes

//first is the compute resource dedicated for the socket, we might want to try, second is the updatable has to be from cron_subsystem.h
//third is huge requests >= 64 KB
//fourth is memory has to be allocated on the fly thouugh it has capacity

//when I spent so much time to think about how the flash_stream_x could be written, it all comes down to lifetime, and dynamic lifetime by using cyclic_unordered_map queue
//the answer is so perfect when the buffer is <= 64KB but not otherwise, the otherwise needs improvision of lifetime and compromision of lifetime that is yet to be thoroughly thought

//we have reached 100% transmission rate on every test if only correct pipe size is configurated or correct worker size is configurated or both, this is a major change in terms of what it means to be fair in an extreme transmission scenerio
//and this is the foundation of what to be built next in terms of fixed virtual network and virtual transmission edges
//maybe an extension of request to do security + friends, but we'd use this as an internal service, external facing should be another rest controller inside the code

//what's hard is that this REST controller works perfectly fine if it has < 64KB requests, making sure that every inbound requests lifetime is just and fair (we think in terms of inbound request lifetime)
//but for > 1MB or 10MB, we'd need to throttle the speed to make sure that it's still fair (or we have two instances, just to make sure that it's clean)

//what I tried to explain is that two concurrent rest_frame would solve the problem, it's again ... shared pipe, what ???
//because if the request size is reasonable on the fast pipe (1 request < 100 bytes), we can make sure that the waiting time (or lifetime) is significanly lower, I mean a lot lower

//when I think about the bottleneck of AWS and server NIC, we can think of the transportation layer is a free-way kind of road, where the bandwidth is still bottlenecked by the pipe size, but much larger than that of country-road or highway-road
//in the application logics, which is our rest_frame, we'd have to hand-configure that (one for stream which is min(args...) * 80% / concurrent_user, one for ping and healthcheck(min(args...) * 20% / concurrent_user))

//for security, I guess we'd just use our method of encoding, we'd need to do hand-shake, extension + etc.