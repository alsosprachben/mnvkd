## 12-Factor vs Actor

### The Twelve-Factor Trap

> "Do not scatter without necessity." — Occam’s Scale

Twelve-Factor apps modularized logic and improved scalability—but by scattering context into services, pipelines, and logs.

What began as clean decomposition becomes coordination overhead, where state, behavior, and observability are externalized.

```
Twelve-Factor Overhead:
  ├─ Env vars
  ├─ Logs & metrics
  ├─ Meshes & queues
  └─ Coordination logic
```

---

## 12-Factor vs Actor

### Actors Reclaimed

Actors were designed for locality: self-contained units of state, logic, and messaging.

But many frameworks today—Akka, Temporal, Durable Objects—externalize core behavior, reducing actors to scattered services.

```
Actor Models:
┌─ Traditional: [ Actor ]
│   ├─ Behavior
│   ├─ Local State
│   └─ Internal Observability
└─ Modern: [ Stateless Actor ]
    ├─ Remote DB
    ├─ External Event Bus
    └─ Logging Pipeline
```

**If everything is external—what's left of the actor?**

---

## 12-Factor versus Actor

### DataVec: Actors Done Right ("Statelessful")

> "Statelessful": Map Less, Reduce More.

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

## 12-Factor versus Actor

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

## Philosophy

### In Praise of Idleness

> “There are two kinds of work: first, altering the position of matter at or near the earth’s surface;  
> second, telling other people to do so.”  
> —Bertrand Russell, *In Praise of Idleness* (1932)

Modern cloud infrastructure is dominated by the second kind—not computation, but coordination. Orchestration. Messaging. Retrying. Logging. Managing.

But the goal of a system isn’t to move—it’s to rest. Pipelines exist to bring data to its optimal resting point—not to keep it in motion forever.

**DataVec is built to minimize movement by maximizing structure.**  
No layers of delegation. No glue-filled pipelines. Just actors, in memory, doing the work directly.

*In Praise of Idleness* isn’t about doing nothing—it’s about designing systems that no longer have to work so hard.

And the market is starting to catch on.

---

## The Market

### Where the Industry Is Going

Major platforms are trending toward structured, local-first execution—but they’re dragging along old architecture.

**Durable Objects → external DBs  
AWS Step Functions → orchestration glue  
Temporal → workflow wrappers**

Apple got locality right by making the device the system of record. iCloud stores app state as local SQLite files and syncs changes—but the state lives on-device.

Most cloud platforms don’t have a persistent client device. Their services are stateless, ephemeral, and rely on coordination to simulate continuity.

**DataVec brings the missing piece:** persistent, memory-resident actors that give services their own locality—no client required.

| Model           | Locality Source                | Coordination         | Observability        |
|-----------------|-------------------------------|----------------------|---------------------|
| Typical Cloud   | None (Stateless)               | Orchestration Layers | External Telemetry   |
| Apple iCloud    | Device Storage (e.g. SQLite)   | Sync Framework       | Built-in            |
| DataVec         | Memory-Native Actors           | None (Inherent)      | Embedded & Native   |

**Locality isn’t a constraint—it’s the path forward for performance, reliability, and privacy.**

---

## The Market

### The Real Opportunity

The cloud created a coordination economy: middleware, observability stacks, and orchestration tools ballooned into billion-dollar industries.

- $70B+ in edge/serverless forecasted by 2032
- $20B+ annual spend on observability tools
- Countless teams burned out writing glue

But these are symptoms of structural absence:

- No memory model → external state
- No locality → I/O-bound coordination
- No introspection → telemetry engineering

**DataVec solves the cause—not just the symptoms.**

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
- No more reduce-map cycles to rehydrate scattered state—each actor is self-contained and always live. Structural locality replaces coordination as the primitive.

```
Full-Stack Integration:
  ├─ Threading Framework (coroutine + I/O aggregation)
  ├─ Actor Kernel (soft real-time, memory-safe)
  ├─ Service Framework (protocol-native, multiplexed)
  └─ Application Framework (WinterTC-ready)
```

**This isn’t another server stack. It’s the layer beneath them all—and it’s already running.**

---

## The Solution

### From High-Level Code to Ontological Systems

LLMs excel at manipulating symbols—functions, logic, syntax—but struggle with ontological concerns: values in space, identity in time, structure in memory.

Immutable abstractions from lambda calculus map well to stateless services, but not to actors with stateful presence and mutable structure.

**DataVec bridges the gap:** high-level code (JS, Python) is AI-translated into memory-anchored C actors—each an ontological object: persistent, composable, and introspectable.

```
Code Flow:
Dev Code → AI Assist → C Actors → State Machines → mnvkd Runtime
```

These actors are not mere functions. They live in memory, evolve deterministically, and integrate structure, state, and behavior as one.

**Write in symbols. Deploy as values. Scale as structure.**

---

## The Solution

### Efficient Enough to Sell as a Service

**DataVec uses just 104 KB per actor**—with full observability and live state in memory, not on disk.

- 20,000 actors on a 2 GB VM = ~$10/month
- $0.0005/actor/month (flat rate)
- $0.005 per million invocations

```
Traditional Stack:
  ├─ Proxy + Orchestrator
  ├─ DB + Cache
  ├─ Telemetry Glue
  └─ Runtime Tax

DataVec Stack:
  └─ Actor Runtime (RAM)
     ├─ Heap = State
     ├─ mmap() = Persistence
     └─ Logs = Embedded
```

**It runs lean enough to be sold profitably—no cold starts, no orchestration waste.**

---

## The Solution

### We Built This Because We Had To

**1. High-Scale, Low-Latency Demands (OpenRTB):**  
As an ad-tech technical co-founder, Ben built a real-time bidder that processed 12 billion JSON requests per day—peaking at 1M QPS—on commodity VMs. We had no cookie-based filtering, so every bid was stateful. One exchange even asked how we outperformed their custom Java HFT stack on bare metal. The answer? Structural locality and zero glue.

**2. Cloud Cost Burnout (Postmortem):**  
Later, Ben met Tony working on a project that was bought off a company that had gone bankrupt from cloud costs—millions lost to Lambda, RDS, and orchestration overhead. They didn’t need microservices; they needed structure. That’s when I realized the real problem: modern infra forces you to rent complexity, not run software.

**And one deeper frustration:**  
Why is it so hard to write a custom protocol handler like `inetd`—and have it scale? Abstractions don’t have to add overhead. DataVec was built so developers can write high-performance services from day one—without switching languages or rewriting everything just to go from prototype to production.

---

## The Solution

### The Team

**Ben**  
Platform-Obsessed Generalist. Expert at vertically-efficient servers. 18 data science patents in ad tech. Real-Time services, SDR, and embedded experience. Previous technical co-founder experience.

**Tony**  
Prolific App Developer, and Generalist. Expert at rapid prototyping, distributed system design, and leading teams. Deep experience in NextJS and related ecosystem.

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

## The Solution

### From Prototype to Ecosystem

**DataVec is already running**: 300k QPS/core with 104 KB actors, live state, and zero glue. The core is proven. Now it’s time to grow.

We’re raising a **$1.5M pre-seed** to accelerate adoption, polish the developer experience, and launch on both AWS Marketplace and self-hosted deployments.

```
What’s Done:
  └─ Runtime Core (mnvkd)

In Progress:
  ├─ Self-hosted Launch (TLS, Fetch API, WinterTC APIs)
  └─ Core Types & Memory Model (mostly done)

What’s Next:
  ├─ Managed Service Launch
  ├─ NextJS Integration & AI DevX
  ├─ Developer Experience Layer (CLI, SDKs, interop)
  └─ Parallel: Protocol Libraries (Redis, SQLite, Vector DBs)
```

**Initial team:** Ben (CTO/dev), Tony (CEO/dev), plus a generalist principal engineer to drive throughput and composability.

**This is the moment to reshape the stack—cleanly, natively, and composably.**

---

## The Pitch

### Flat-Rate Economics: Fast, Lean, and Predictable

**DataVec is so efficient, it breaks the cloud pricing model.**  
Whether flat-rate or usage-based, it delivers margin where others can’t.

- **Flat Rate:** $0.0005 per actor per month (104 KB RAM, 2 GB VM = $10/month for 20,000 actors)
- **Usage Rate (illustrative):** $0.005 per 1M invocations (based on 300k QPS/core)

```
Two Views of Cost:

[Memory-based Pricing]
  ├─ 104 KB per actor
  ├─ 20,000 actors on 2 GB VM
  └─ ~$10/month = $0.0005/actor/month

[Usage-based Comparison]
  ├─ 1 core = 300k QPS
  ├─ ~25B invocations/month
  └─ $0.005 per 1M (with healthy margin)

Comparison:
  ├─ Lambda: $0.60 per 1M
  ├─ Fastly / Durable Objects: $0.50–$1 per 1M
  └─ DataVec: $0.005 per 1M (estimated, 100x cheaper)
```

**You don’t have to choose. DataVec wins on cost, scale, and simplicity.**  
For the first time, structured compute is margin-positive at cloud scale.

---

## The Pitch

### Composable Locality: Built-In Control

**With DataVec, structure is flexible—without overhead.**

Developers choose exactly how actors interact: isolated like serverless functions, or fused into zero-copy execution chains. No scaffolding. No compromise.

```
Locality Spectrum:
  [Isolated Actor]
    └─ Own heap
    └─ mprotect() isolation
    
  [Cooperative Micro-Process]
    └─ Shared heap
    └─ Coroutine execution

  [Message-Passing Cluster]
    └─ vk_socket ring buffers
    └─ Shared or isolated views

  [Fused Driver Chain]
    └─ Zero-copy logic handoff
    └─ Tight in-memory cooperation
```

**Structure on your terms. Efficiency at every scale.**

---

## The End

### Thank You

This is the runtime for the next era—where structure is native, locality is the platform, and overhead is no longer a given.

**Let’s condense the cloud—and make it rain.**

To get in touch:

- **Email:** [founders@datavec.io](mailto:founders@datavec.io)
- **Site:** [https://datavec.io](https://datavec.io)
- **GitHub:** [mnvkd on GitHub](https://github.com/alsosprachben/mnvkd)