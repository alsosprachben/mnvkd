## 12-Factor versus Actor

### The Twelve-Factor Trap: Modularity via Scattering

#### Occam's Scale: "Do not scatter without necessity."

Twelve-Factor apps transformed modularity and scalability—at the hidden cost of scattering logic, state, and observability into externalized pipelines.

**Why does scaling inevitably spiral into complexity?**  
**Are we scattering essential context without necessity?**

```
Clean Modularity (Twelve-Factor Apps)
  │
  ▼
Hidden Overhead:
  ├─ Env var loading
  ├─ Log aggregation
  ├─ Service meshes
  ├─ State serialization
  └─ Constant coordination
```

---

### How Actors Became 12-Factor

Actors were originally designed to **avoid scattering**: logic, state, and messaging naturally co-located for simplicity and performance.

Yet modern implementations—AWS Lambda, Cloudflare Durable Objects, Temporal workflows, Akka clusters—externalize actor state, effectively making them just another type of 12-factor component.

```
Modern "12-Factor Actor":
  [Stateless Actor]
  ├─ External DB/Cache
  ├─ External Queue/Event Bus
  └─ External Observability (Logs/Metrics)
```

**If actors become stateless and externalized, are they still actors—or just more scattering?**

---

### The Actor Model Was Always Anti-12-Factor

The original actor paradigm emphasized **structural locality**—state, logic, and observability integrated naturally within each actor.

True actors encapsulate state internally, communicate via messages, and maintain intrinsic observability. No external scattering, no unnecessary overhead.

```
True Actor Model:
  [Actor]
  ├─ Logic (Behavior)
  ├─ State (Local Persistence)
  └─ Observability (Intrinsic)
```

**Actors were never meant to externalize state—actors were always about structural locality.**

---

### DataVec: Actors Done Right ("Statelessful")

#### "Statelessful": Map Less, Reduce More.

DataVec embraces the true actor paradigm, creating actors as **ontological objects**: a single, coherent memory space that integrates logic, state, and observability intrinsically.

```
Ontological Actor (DataVec):
  ├─ Logic (Composable behaviors)
  ├─ State (Local, Structured)
  └─ Observability (Embedded, Deterministic)
```

"Statelessful" means isolating state appropriately, sharing context efficiently, and minimizing redundant external mappings.

**No scattering. No externalization. No unnecessary overhead.**

---

### The Platform for WinterTC and Beyond

DataVec’s actor runtime (`mnvkd`) is not just efficient—it’s foundational. It enables structured, composable platforms like [WinterTC](https://wintertc.org) (cloud function specification) to be built atop **true actors** rather than 12-factor scaffolding.

Each actor is a memory-scoped process with deterministic behavior and embedded observability. Services like Redis, SQLite, and even custom vector databases can be implemented as actors with native in-memory messaging.

```
WinterTC-Compatible Platform:
  ├─ Built from local, structured actors
  ├─ Zero glue, zero serialization
  ├─ Composable over HTTP/2 or native APIs
  └─ Efficient introspection without scraping
```

**The cloud solved scale by scattering.  
DataVec solves structure by restoring locality.**

---

## The Market

### The Market Is Moving Toward Us

Edge and serverless platforms are trending toward structured, local-first computing—but they’re building on the wrong foundation.

Cloudflare, AWS, Vercel, and others are converging on **WinterTC-style runtimes** that resemble the browser: localized APIs, scoped execution, lightweight isolation.

But they still treat actors like 12-factor components:

```
Durable Objects → External Storage
AWS Step Functions → Orchestration Glue
Temporal → Workflow-as-a-Service

All still externalize logic, state, or both.
```

**They’re chasing locality while still scattering execution.  
DataVec gives them the foundation they’re missing.**

---

### Strategic Insight: Small Data Wins

While the industry raced to externalize everything into the cloud, Apple quietly took the opposite path—and won.

- **iOS services run on local, structured state**—often just SQLite files.  
- **iCloud acts as a syncing fabric, not a database**—state lives *on the device*.  
- Their model is **small-data per user**, not centralized big data.

Apple didn’t reject scale—they localized it. And in doing so, they achieved speed, reliability, and simplicity at massive scale.

```
Cloud Model: External State → Coordination → Observability
Apple Model: Local State → Sync → Native Observability
```

**This isn't behind—it’s ahead. Big data is actually made of small-data problems. And locality wins.**

---

### What Others Are Trying — and Why It’s Not Enough

Everyone sees the problem. But the solutions so far are **patches on a 12-factor foundation**—not structural fixes.

- **Cloudflare Durable Objects**: per-object state, but backed by external storage and sync logic.  
- **AWS Step Functions**: orchestration glue that increases latency and complexity.  
- **Temporal**: stateful workflows, but still rely on distributed backends and coordination.

These tools all retrofit **structure on top of scattering**. They solve symptoms, not causes.

```
[Function] → [Queue] → [State Store] → [Function]
↑ Log Glue   ↑ Orchestration   ↑ Retry Logic
```

**DataVec starts where these solutions stop:  
Local structure. Embedded observability. True actor composition.**

---

### Why Now: The Economic Turn

The era of infinite funding and infrastructure sprawl is over. Efficiency is no longer optional—it’s strategic.

- **Budgets are tighter.** Runway matters more than revenue multiples.  
- **Teams are leaner.** Infrastructure must be simpler to reason about and maintain.  
- **Efficiency, composability, and insight** are now value drivers—not afterthoughts.

Most stacks were built to chase growth. Few were built to last.

```
Old Model: Scale → Glue → Complexity → Overspend
New Model: Structure → Locality → Clarity → Efficiency
```

**DataVec was designed for this moment:  
A structural foundation that replaces layers, not just builds more.  
Local-first, introspectable, radically efficient.**

---

### The Opportunity

Infrastructure today is bloated with coordination layers—glue code, observability tooling, orchestration logic, and state bridges.

- **$70B+** projected for serverless and edge compute by 2032  
- **$20B+** spent annually on observability, tracing, and logging tools  
- Billions more spent on integration middleware—queues, proxies, orchestrators, brokers  

These costs are symptoms. They reflect what the stack is missing:

- No structural memory model—so state is externalized  
- No shared locality—so coordination becomes I/O-bound  
- No introspection—so debugging becomes telemetry engineering  

```
The Cost of Missing Structure:
  ├─ Performance loss
  ├─ Developer burnout
  ├─ Vendor lock-in
  └─ Infinite glue
```

**DataVec unlocks the second 10×—by removing the overhead others normalize.**

---

## The Solution

### What We're Building

**DataVec is for servers what SQLite is for databases.**  
A vertically integrated execution model—not a framework, not a function runner.

At its core is `mnvkd`, a memory-anchored C runtime that:

- Implements an M:N:1 scheduler with stackless coroutines and isolated micro-heaps  
- Uses actor-based microprocesses for fault isolation—no threads, no kernel processes  
- Runs services like `inetd`—but faster, safer, scoped per connection  
- Embeds observability—no external glue, no log scraping, no orchestration spaghetti  

```
Full-Stack Integration:
  ├─ Threading Framework (coroutine + I/O aggregation)
  ├─ Actor Kernel (soft real-time, memory-safe)
  ├─ Service Framework (protocol-native, multiplexed)
  └─ Application Framework (WinterTC-ready)
```

**This isn’t another server stack. It’s the layer beneath them all—and it’s already running.**

---

### The Team

We're systems thinkers, platform engineers, and runtime architects.

- Built for low-level efficiency and high-level developer ergonomics  
- Decades of experience in C, runtime internals, and distributed infrastructure  
- Deep intuition for composability, observability, and system safety  

This is not a pivot. It’s the system we were always building toward.  
From the memory model to the protocol layer—we own the whole stack.

```
Core Values:
  ├─ Structure-first
  ├─ Efficiency by design
  ├─ Developer-oriented
  └─ Composable by principle
```

**This isn’t just a runtime—it’s our operating philosophy.**

---

### The Roadmap

- **Prototype Complete:** `mnvkd` running at 300k QPS/core in internal benchmarks  
- **Self-Hosted Launch:** Native TLS, Fetch API, and WinterTC runtime parity  
- **Managed Service Launch:** NextJS and WinterTC integration with optional AI-assisted JS→C translation  
- **Service Protocol Library:** Actor-based microservices for drop-in replacements:  
  - **Redis:** Text-keyed Judy array with RESP protocol  
  - **SQLite:** Embedded per-connection micro-thread  
  - **Vector DB:** Per-tenant memory-optimized spatial layout  

```
Platform Timeline:
  ├─ Core runtime → ✓
  ├─ Edge-ready interface → In Progress
  ├─ Microservice protocol kit → Next
  └─ DevX and SDK layers → Funding Enabled
```

**This is a platform, not just a product—an ecosystem ready to be composed from the ground up.**

---

### Let's Build It Right

The cloud solved scale.  
**DataVec solves structure.**

We’re raising a **$1.5M pre-seed round** to bring DataVec to early adopters and complete the core platform.

- Finalize developer SDKs, CLI tools, and safe interop layers  
- Embed observability and introspection into the actor runtime  
- Integrate with edge-native AI, local-first SaaS, and structured pipelines  
- Grow the team: systems engineer, developer advocate, and design contributor  

```
Funding Use:
  ├─ Product polish & DX
  ├─ Ecosystem integration
  ├─ Community-building
  └─ Strategic hires
```

**If you're betting on edge-native platforms, structural simplicity, and local-first design—we're your runtime.**

---

## The Pitch

### Efficient Enough to Sell as a Service

Most platforms build abstraction layers that eat margin: cold starts, orchestration, logging glue, state rehydration.

**DataVec runs so efficiently that we can host structured compute as a service—and still make a profit.**

```
Traditional Serverless = Cost Sink:
  ├─ Cold start latency
  ├─ Overhead from orchestration
  ├─ External storage IOPS
  └─ Runtime tax (GC, threads, logs)

DataVec = Structured Margin:
  ├─ 104 KB per actor (measured)
  ├─ mmap() persistence, zero-copy
  ├─ No runtime tax
  └─ Built-in observability
```

---

### Compute That Pays for Itself

Each actor heap is page-aligned and lives in memory—not in a cold pool or replay log. That means:

- No rehydration cost  
- No idle instance burn  
- No infrastructure gymnastics  

A single VM with 2 GB RAM can run up to **20,000 live actors** at full observability and still stay under budget.

```
1 actor = 104 KB
20,000 actors = ~2 GB
Cost = ~$10/month VM

That’s $0.0005 per actor per month.
```

---

### Others Scale by Charging You More

Platforms like Lambda@Edge, Fastly, and Durable Objects charge per invocation, per GB-s, per region.

They are expensive because **their model is expensive to run**.

```
Lambda@Edge:
  ├─ $0.60 per 1M calls
  ├─ Cold starts
  └─ External DB for state

DataVec:
  ├─ $0.005 per 1M invocations
  ├─ No cold starts
  └─ Live heap = live state
```

---

### Profit-Positive Platform Economics

DataVec doesn’t depend on usage fees to subsidize infrastructure. Its default mode of operation is margin-efficient:

- 1 core = 300,000 invocations/sec  
- 1 GB RAM = 10,000+ live actors  
- Persistence is just memory pages  

**This is the first edge-native platform where structured compute is cheap enough to sell as a flat-rate service.**

---

### Visual Cost Stack: Others vs DataVec

Serverless providers hide margin loss in a tower of services. DataVec collapses the stack into a single, efficient layer:

```
Traditional Stack:
  ├─ CDN Proxy
  ├─ Function Runtime
  ├─ Logging Pipeline
  ├─ State DB + Cache
  ├─ Observability Service
  └─ Retry & Orchestration Logic

DataVec Stack:
  └─ Actor Runtime (in RAM)
   ├─ Heap = State
   ├─ Embedded Logging
   ├─ Built-in Observability
   └─ mmap() = Persistence
```

**We didn’t optimize the stack—we removed it.**

---

### Composable Locality: Isolation vs Cooperation

DataVec actors scale across a full spectrum of execution models, from completely isolated functions to tightly bound cooperative drivers:

```
Actor Composition Spectrum:
  [Isolated Function]
  └─ Own heap
  └─ Memory masked via mprotect()

  [Micro-Process with Threads]
  └─ Shared heap
  └─ Cooperative coroutines

  [Multi-Process Cluster]
  └─ Ring buffer messaging (vk_socket)
  └─ Shared or isolated memory windows

  [Fused Driver Chain]
  └─ Fully cooperative, zero-copy logic flow
  └─ Execution passes without queues
```

**Developers control locality, isolation, and execution sharing—down to the page.**

---

## The End

### Thank You

This is the runtime for the next era—where structure is native, locality is the platform, and overhead is no longer a given.

To get in touch:

- **Email:** [founders@datavec.io](mailto:founders@datavec.io)  
- **Site:** [https://datavec.io](https://datavec.io)  
- **GitHub:** [mnvkd on GitHub](https://github.com/alsosprachben/mnvkd)  

**Let’s condense the cloud—and make it rain.**
