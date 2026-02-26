//I think it's OK now that we use global string for the stream packet

//because the lifetime of the string is within the scope of the request_handler (assume that our waiting_queue is 0), which is only a handful of max 100MB / handler
//and we did do synchronization or self-pace at the rest controller, so there should not be a transmission issue

//just make sure that we do have bandwidth for connections and do synchronization for each of the campaign to make sure that we are fair

//to be honest, normally I would agree that we shouldn't be isolating cores, but this is time-sensitive and our rest controller is our product
//we just want to split bandwidth and make sure that we are staying connected 100% of the time, we actually have yet to talk about fairness because we do synchronization and we hold unique reference over the entire training stack
//we would have to make sure we can work both ways (either splitting cores or not)