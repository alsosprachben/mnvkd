#mnvkd

##Scheduling Process

	- `vk_pipe` - IO Pipe - `vk_pipelog` - pipe events - `vk_map` - mmap mapping - `vk_maplog` -
	mmap queue - `vk_maplist` - mmap list - `vk_mapstack` - mmap stack frames - `vk_mapsock` -
	mmap socket splice - `vk_mapfile` - mmap file splice - `vk_maptls` - mmap libtls splice - `vk_state` -
	state machine coroutines - `vk_sched` - dispatch queue - `vk_mesh_http` - HTTP / 1.1 gateway - `vk_mesh_http2` -
	HTTP / 2.0 gateway - `vk_mesh_fcgi` - FastCGI gateway - `vk_mesh_grpc` - gRPC gateway

#concurrency model

	- Consumer interfaces can be synchronous(request, response) or
    asynchronous(request and wait) - Producer interfaces can be synchronous(request, response) with or without reentrance (with a request queue), or asynchronous (request becomes a future)
- Interfaces should be synchronous unless parallelism is needed.
- coroutines are made reentrant with a per-coroutine queue.

#Resource Concurrency Interface

Queue internal to compute resources. Block external to compute resources. For example, golang uses fixed sized queues as channels between internal resource goroutines, but uses a single event loop for blocking on network resources, which are external.

#One Dynamic Queue

You only need one dynamic queue in a particular path. All the rest can be fixed if you block externally, to regulate black-pressure. This is the one piece missing from golang, although it is easy to implement using auto-growing slices, except for the realloc() overhead.

#Memory Layout

Each coroutine gets a heap of fixed pages backed by a single memory mapping. Within the heap, page-aligned, fixed-sized array allocations of a particular type may be allocated, until the heap pages are exhausted. 




## Design

### Horizontal Layers of Abstraction
To reduce the size of computational problems, software frameworks are often comprised of horizontal layers of abstraction. Data passes vertically through each layer, each layer processing all data, but at different stages of the process. Therefore, a "software stack" implements a "data pipeline". The use of a pipeline increases throughput, but the complexity of using many layers also increases latency.

### Mapping and Reducing: Big Data as Small Data
When "big data" is involved, a single big data process becomes comprised of many big data processes, one for each layer. The way to deal with a big data problem is to make it a small data problem. To do this, a "distributed system" is developed, where each layer is partitioned, data is "mapped" to each partition, then "reduced" on the other side. This terminology comes from the `map()` and `reduce()` functions common in the domain of functional programming. Sometimes this is called splitting and merging, or scattering and gathering. 

### Vertical Layers of Abstraction
Ideally, data should be mapped once at the beginning, then reduced once at the end, in contiguous vertical layers of abstraction, but often distributed systems end up unnecessarily re-reducing and re-mapping in between each layer. Such an architecture is often a symptom of a distributed system that unfortunately inherited the design of a non-distributed system that started out with horizontal layers of abstraction. Therefore, it can be beneficial for a distributed system to be designed from the beginning with vertical layers of abstraction. 

### Locality of Reference
With proper vertical layer partitioning, the horizontal layers can become local again. This completely changes the algorithmic conditions. The need for slow and costly cloud methods disappears:
  - No need for complex distributed hash tables just to store variables.
  - No need for complex service meshes to manage horizontal connections between systems.
  - No need for complex data lakes with complex batch processing latencies. 
  - No need for complex inter-lake journals with complex data pipeline management. 
  - No need for complex function dispatching "serverless" systems. 

Applications are small and efficient again, but exist within a modern horizontal resource backplane supporting local access, and this "locality of reference" mitigates the latency problem of distributed data pipelines.

### Platforms Manage Resources
In a computing system, the operating system's task is to manage the physical resources. The task of a platform's standard library is to interface with the operating system to expose the physical resources to design patterns, by implementing common algorithms optimized for resource management.

For example, the standard library implements various object containers, like sets, tables, lists, and queues, and provides interfaces for performing input and output with those containers. This explains why container implementations differ greatly across different platforms: the differences of resource management opinions. For example:
 - A platform that has garbage collection has containers with external references, often mixing object types in a single container, because the type information is associated with the object storage.
 - However, a platform with manually managed memory often uses internal references, and a container only supporting a single type, because the container is comprised of element references, and a fixed-sized head object.
 - A platform with stack-based threads needs a thread-aware memory allocator to deal with memory transactions.
 - However, an event-based platform with futures shares all memory, and all memory transactions are within a single thread.

### Frameworks Invert Control
In a typical functional computing environment, dependent platform code is linked as external functions called from application functions. The application is completely in control over its dependencies, and needs configuration changes to use different dependencies.

Rather than acting as a dependent library, a framework can invert control so that rather than being called by applications, the framework instead calls into applications, and the applications have no direct control over the framework. This simplifies the interface with the application, and makes it easier to unify configuration across applications.
