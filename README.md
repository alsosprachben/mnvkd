# M:N Virtual Kernel Daemon

## Big Data as Small Data

### Horizontal Layers of Abstraction
To reduce the size of computational problems, software frameworks are often comprised fo horizontal layers of abstraction. Data passes vertically through each layer, each layer processing all data, but at different stages of the process. Therefore, a "software stack" implements a "data pipeline". The use of a pipeline increases throughput, but the complexity of using many layers also increases latency.

### Mapping and Reducing
When "big data" is involved, a single big data process becomes comprised of many big data processes, one for each layer. The way to deal with a big data problem is to make it a small data problem. To do this, a "distributed system" is developed, where each layer is paritioned, data is "mapped" to each partition, then "reduced" on the other side.

### Vertical Layers of Abstraction
Ideally, data should be mapped once at the beginning, then reduced once at the end, in contiguous vertical layers of abstraction, but often distributed systems end up unnecessarily re-reducing and re-mapping in between each layer. This architecture is often the effect of a distributed system inheriting the design of a non-distributed system, which started out with horizontal layers of abstraction. Therefore, a distributed system needs to be designed from the beginning with vertical layers of abstraction. 

### Locality of Reference
With proper vertical layer partitioning, the horizontal layers can become local again. This completely changes the algorithmic conditions. No need to store every variable in a giant hash table. No need for a service mesh to manage horizontal connections beween systems. Applications are small and efficient again, but exist within a modern horizontal resource backplane.

### Operating Systems Manage Resources
