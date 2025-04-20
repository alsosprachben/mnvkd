## 12-Factor vs Actor

### Cloud Functions: A 12-Factor Device

Cloud functions emerged as a key primitive of [the 12-Factor application model](https://12factor.net/)—treating computation as ephemeral logic with no memory of the past. Through their immutability, they became ephemeral: functions without identity, state, or persistence. Immutable things are often associated with eternality—but in finite systems, immutability means disposability. Without memory, the function cannot remain.

They offered scalability through statelessness, enabling horizontal distribution. But to achieve this, they externalized context, coordination, and observability.

The result: functions that are easy to run, but difficult to compose.

---

### Serverless and the Isolation Paradox

Function-as-a-Service platforms like AWS Lambda started by provisioning short-lived services with boilerplate wrappers around user logic. They isolated *services* that run functions—not the function logic itself.

Performance optimizations pushed isolation boundaries closer to the function—toward the idea of **local actors**.

V8 Isolates and WASM mimic this idea—they provide spatial locality, but not temporal locality. They still rely on *foreign interfaces*, *runtime boundaries*, and *external I/O coordination*.

**DataVec changes this:** actors are not virtual guests—they are memory-native, isolatable or cooperative, and able to control locality precisely.

---

### From Logical Functions to Ontological Actors

Cloud functions represent logic, not presence. They are stateless, ephemeral, and depend on orchestration to simulate continuity.

**Actors are different:** they are persistent, composable, introspectable entities—[*ontological objects*](https://spatiotemporal.io/hammer.html#ontologicalobjects) in memory space.

| Model        | Phase 1  | Phase 2  |
|--------------|----------|----------|
| Logical      | Map      | Reduce   |
| Ontological  | Scatter  | Gather   |

**DataVec lets you tune isolation and locality per actor—without virtual machines, without foreign calls, and without glue.**

---

### Composition Failure

**“Do not distribute without necessity.” — Occam’s Scale**

Stateless functions are easy to scale—but hard to compose. Each function becomes a node in a distributed graph—held together by glue: queues, retries, schedulers, and logs. But graph traversal is not free: in general, it's factorial in complexity. The more possible paths through a distributed function graph, the harder the system is to compose, test, and maintain.

What starts as simplicity becomes proliferation. The more modular the function, the more infrastructure it needs to coordinate.

Map/Reduce was a logical breakthrough for parallelism. But when used as a general architecture, it leads to fragmentation—shattering systems into symbolic pieces that must be gathered, ordered, and reasoned about externally.

**Occam’s Scale:** Distribute only when locality fails. Each map that is composed incurs a latency cost. Map less, and reduce more.

The real cost of orchestration isn't technical—it's conceptual. We’ve mistaken motion for progress.

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

### Where the Industry Is Going

Major platforms are trending toward structured, local-first execution—but they’re dragging along old architecture.

**Durable Objects → external DBs  
AWS Step Functions → orchestration glue  
Temporal → workflow wrappers**

Apple got locality right by making the device the system of record. iCloud stores app state as local SQLite files and syncs changes—but the state lives on-device.

Most cloud platforms don’t have a persistent client device. Their services are stateless, ephemeral, and rely on coordination to simulate continuity.

**DataVec brings the missing piece:** persistent, memory-resident actors that give services their own locality—no client required.

| Model           | Locality Source              | Coordination         | Observability         |
|-----------------|-----------------------------|----------------------|----------------------|
| Typical Cloud   | None (Stateless)            | Orchestration Layers | External Telemetry   |
| Apple iCloud    | Device Storage (e.g. SQLite)| Sync Framework       | Built-in             |
| DataVec         | Memory-Native Actors        | None (Inherent)      | Embedded & Native    |

**Locality isn’t a constraint—it’s the path forward for performance, reliability, and privacy.**

---

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

### We Built This Because We Had To

**1. High-Scale, Low-Latency Demands (OpenRTB):**  
As an ad-tech technical co-founder, Ben built a real-time bidder that processed 12 billion JSON requests per day—peaking at 1M QPS—on commodity VMs. We had no cookie-based filtering, so every bid was stateful. One exchange even asked how we outperformed their custom Java HFT stack on bare metal. The answer? Structural locality and zero glue.

**2. Cloud Cost Burnout (Postmortem):**  
Later, Ben met Tony working on a project that was bought off a company that had gone bankrupt from cloud costs—millions lost to Lambda, RDS, and orchestration overhead. They didn’t need microservices; they needed structure. That’s when I realized the real problem: modern infra forces you to rent complexity, not run software.

**And one deeper frustration:**  
Why is it so hard to write a custom protocol handler like `inetd`—and have it scale? Abstractions don’t have to add overhead. DataVec was built so developers can write high-performance services from day one—without switching languages or rewriting everything just to go from prototype to production.

---

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