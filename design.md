# mnvkd

## Scheduling Process

 - `vk_pipe` - IO Pipe
 - `vk_pipelog` - pipe events
 - `vk_map` - mmap mapping
 - `vk_maplog` - mmap queue
 - `vk_maplist` - mmap list
 - `vk_mapstack` - mmap stack frames
 - `vk_mapsock` - mmap socket splice
 - `vk_mapfile` - mmap file splice
 - `vk_maptls` - mmap libtls splice
 - `vk_state` - state machine coroutines
 - `vk_sched` - dispatch queue
 - `vk_mesh_http` - HTTP/1.1 gateway
 - `vk_mesh_http2` - HTTP/2.0 gateway
 - `vk_mesh_fcgi` - FastCGI gateway
 - `vk_mesh_grpc` - gRPC gateway

# concurrency model

- Consumer interfaces can be synchronous (request, response) or asynchronous (request and wait)
- Producer interfaces can be synchronous (request, response) with or without reentrance (with a request queue), or asynchronous (request becomes a future)
- Interfaces should be synchronous unless parallelism is needed.
- coroutines are made reentrant with a per-coroutine queue. 

# Resource Concurrency Interface

Queue internal to compute resources. Block external to compute resources. For example, golang uses fixed sized queues as channels between internal resource goroutines, but uses a single event loop for blocking on network resources, which are external.

# One Dynamic Queue

You only need one dynamic queue in a particular path. All the rest can be fixed if you block externally, to regulate black-pressure. This is the one piece missing from golang, although it is easy to implement using auto-growing slices, except for the realloc() overhead.

# Memory Layout

Each coroutine gets a heap of fixed pages backed by a single memory mapping. Within the heap, page-aligned, fixed-sized array allocations of a particular type may be allocated, until the heap pages are exhausted. 
