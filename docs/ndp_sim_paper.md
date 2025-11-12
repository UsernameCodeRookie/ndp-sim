# NDP-SIM: An Event-Driven Framework for Near-Data Processing Architecture Exploration

*Chiaki Sato, UsernameCodeRookie Lab*

## Abstract

Near-data processing (NDP) architectures promise to blunt the cost of data movement by placing compute fabrics next to memory, yet exploring their vast design space requires a simulator that simultaneously respects timing fidelity, architectural structure, hardware–software co-design demands, and the realities of toolchain integration. This paper introduces **NDP-SIM**, an event-driven framework that coordinates all state changes through a global-timestamp scheduler and organizes architectural models through a coherent port–component–connection fabric. We explain how the scheduler maintains determinism while supporting millions of events, how components encapsulate functionality around heterogeneous data concepts, and how connections arbitrate movement between congruent ports under realistic back-pressure. On top of this substrate sits a configuration and operator stack that keeps bitstream encodings, workload descriptions, and experiment scripts in lockstep, enabling rapid iteration on workloads such as general matrix multiplication (GEMM) and communication microbenchmarks. Measurements and traces quantify how latency parameters and queueing policies affect throughput, and comparisons with related simulators highlight NDP-SIM’s balance between productivity, extensibility, and fidelity. The resulting framework delivers a practical foundation for researchers who want to explore memory-centric accelerators while keeping empirical validation grounded in reproducible tooling.

## Keywords

near-data processing, event-driven simulation, port/component/connection fabric, architectural exploration, bitstream generation, hardware-software co-design

## Contents

- [1. Introduction](#1-introduction)
- [2. Event-Driven Simulation Core](#2-event-driven-simulation-core)

  - [2.1 Global Timeline and Scheduler Semantics](#21-global-timeline-and-scheduler-semantics)
  - [2.2 Event Taxonomy and Deterministic Tracing](#22-event-taxonomy-and-deterministic-tracing)
- [3. Architectural Fabric: Ports, Components, and Connections](#3-architectural-fabric-ports-components-and-connections)

  - [3.1 Port Abstractions for Data Concepts](#31-port-abstractions-for-data-concepts)
  - [3.2 Components as Functional Islands](#32-components-as-functional-islands)
  - [3.3 Connections as Transport Fabrics](#33-connections-as-transport-fabrics)
  - [3.4 Example: TPU Grid Composition](#34-example-tpu-grid-composition)
  - [3.5 Pipeline Construction and Control](#35-pipeline-construction-and-control)
  - [3.6 Precision-Typed ALUs via Template Traits](#36-precision-typed-alus-via-template-traits)
  - [3.7 ALU and LSU Microarchitecture Details](#37-alu-and-lsu-microarchitecture-details)
- [4. Discussion](#9-discussion)
- [5. Related Work](#10-related-work)
- [6. Conclusion](#12-conclusion)
- [Acknowledgments](#acknowledgments)
- [References](#references)

## 1. Introduction

Modern machine learning, graph processing, and scientific workloads execute billions of multiply-accumulate (MAC) operations per second, yet their overall throughput is throttled by the energy and latency of moving data across the memory hierarchy and even across packaging boundaries. When the energy ratio $E_{\text{mem}} / E_{\text{MAC}}$ exceeds twenty, a representative throughput model $\text{Throughput} = \frac{\text{Useful MACs}}{\text{Time} + \text{Stall}_{\text{mem}}}$ makes it clear that latency-induced stalls dominate performance and erase the benefit of even the most aggressive arithmetic pipelines. Near-data processing (NDP) architectures tackle this challenge by embedding compute structures—pipelines, load-store units, systolic arrays, and custom controllers—adjacent to memory arrays, but the diversity of microarchitectures means that evaluation cannot rely on a single analytic formula. Researchers therefore need tools that model fine-grained timing, let them compose heterogeneous datapaths, offer first-class tracing for debugging, and integrate with existing software toolchains. NDP-SIM answers this need by pairing an event-driven kernel that advances a global timestamp with an architectural fabric that assembles ports, components, and connections into coherent models, all while remaining accessible to both hardware architects and systems developers.

## 2. Event-Driven Simulation Core

### 2.1 Global Timeline and Scheduler Semantics

Every state transition in NDP-SIM is mediated by the event scheduler defined in `src/event.h` and `src/scheduler.h`, and understanding its semantics is key to exploiting the framework. Events carry an execution time $t$, priority $p$, and callback that mutates simulator state; they are stored in a binary-heap priority queue whose insertion cost is $T_{\text{schedule}} = \mathcal{O}(\log |Q|)$ for $|Q|$ pending events, so scaling to millions of events still maintains predictable complexity. The scheduler maintains a single global timestamp, rejects retroactive scheduling ($t < t_{\text{current}}$) to guarantee causality, and exposes helper wrappers such as `LambdaEvent` for inline behaviors and `PeriodicEvent` for recurring activity that mimics clocks or DMA heartbeats. Because every component shares the same scheduler instance, cross-component interactions remain deterministic, and saved scheduler states can be replayed to reproduce entire runs down to the cycle boundary.

### 2.2 Event Taxonomy and Deterministic Tracing

Events encapsulate compute, memory, communication, and user-defined activities, enabling designers to encode timing relationships explicitly instead of relying on implicit clock phases or simulator-specific magic. Tracing hooks in `src/trace.h` record each execution as `(time, component, action, payload, priority)`, and logs are post-processed by `visualize_trace.py` to recover sequence diagrams, stall statistics, and utilization curves that expose which subsystems are the true bottlenecks. Because the scheduler assigns stable numeric identifiers and logs them alongside timestamps, traces from different runs can be diffed mechanically; this has proven invaluable when validating optimizations, diagnosing regressions, or comparing analytical projections with what actually occurs inside the simulator. The tracing pipeline therefore reinforces the core promise of event-driven simulation: repeatable, debuggable behavior even as the design space evolves.

## 3. Architectural Fabric: Ports, Components, and Connections

### 3.1 Port Abstractions for Data Concepts

Ports, implemented in `src/port.h`, define typed interfaces that mirror specific data concepts—instruction tokens, tensor activations, partial sums, memory requests, and metadata control words. Each port carries `DataPacket` instances with metadata describing payloads, logical tags, and timing annotations that help downstream consumers reason about causality. By associating ports with data concepts rather than raw bit widths, the framework makes it natural to compose heterogeneous subsystems without sacrificing expressiveness, and debugging becomes easier because traces reference meaningful names instead of anonymous buses. Designers routinely extend the port catalogue with custom packet types as new kernels or fabrics emerge, and those extensions stay insulated from existing components as long as their semantics remain consistent.

### 3.2 Components as Functional Islands

Components, derived from `Component` and `TickingComponent` in `src/component.h` and `src/tick.h`, bind sets of conceptually distinct ports into functional units that transform or generate data, effectively encapsulating the logic of an architectural island. Examples include processing elements, load-store units, schedulers, mapper engines, and microarchitectural controllers that orchestrate buffer hierarchies. Components register their ports, expose lifecycle hooks (`initialize`, `reset`, `finalize`), and schedule their behavior through the global event queue so that cross-component timing remains consistent. `PipelineComponent` extends this pattern with stage-local state and hazard tracking so that multi-stage datapaths can be expressed succinctly while retaining realistic stall semantics, demonstrating how higher-level abstractions can be layered without weakening determinism.

### 3.3 Connections as Transport Fabrics

Connections, defined in `src/connection.h` and `src/ready_valid_connection.h`, mediate transfers between ports that share the same data concept. They model latency, arbitration, and back-pressure through ready/valid handshakes, internal buffers, and configurable service policies, meaning that even seemingly simple architectural changes—such as widening a link or injecting error detection—can be studied by swapping connection types. A ready/valid channel’s steady-state throughput obeys $R_{\text{rv}} = \min(R_{\text{producer}}, R_{\text{consumer}}, R_{\text{buffer}})$, making queue sizing and consumer readiness first-class design parameters that show up directly in trace annotations. By separating components (functional islands) from connections (transport fabrics), NDP-SIM encourages clear architectural diagrams that map directly to simulation constructs and expedites the march from whiteboard sketches to executable models.

### 3.4 Example: TPU Grid Composition

The TPU model in `src/components/tpu.h` instantiates a grid of `MACUnit<PrecisionTraits>` components. Each MAC component owns distinct instruction, operand, and accumulator ports, while horizontal and vertical data movement is expressed by connections that carry identical activation or weight concepts. This separation lets designers experiment with alternative routing or buffering strategies without rewriting compute logic, it encourages clean layering between control and data planes, and it allows trace outputs to differentiate compute stalls from transport stalls in a way that lines up with silicon floorplans.

```text
Figure 1. Port–component–connection decomposition for a TPU tile

        +----------------------------+
        |        MACComponent        |
        |  (Functional island)       |
        |                            |
        |  +----------------------+  |
        |  | Activation  (in)     |<-+-- Ready/valid connection (row)
        |  +----------------------+  |
        |  | Weight      (in)     |<-+-- Ready/valid connection (col)
        |  +----------------------+  |
        |  | Accumulator (out)    |--+-> Downstream component
        |  +----------------------+  |
        +----------------------------+
```

The ASCII block illustrates how functional components encapsulate diverse port concepts while orthogonal connections transport homogeneous payloads under shared flow-control semantics. Even in larger meshes, the same decomposition applies, reinforcing the architectural discipline that the simulator promotes.

### 3.5 Pipeline Construction and Control

`PipelineComponent` in `src/components/pipeline.h` translates abstract architecture diagrams into executable multi-stage pipelines by combining an input/output port pair, an optional stall control port, and a deque of `PipelineStageData` records that remember the payload and the entry timestamp of each stage. Designers supply stage-specific lambdas through `setStageFunction(i, func)`, enabling arbitrary transformations while the runtime handles bookkeeping, timestamp propagation, and tracing across all stage boundaries. Because advancing the pipeline walks stages from tail to head before sampling the input port, the simulator naturally respects resource hazards, maintains in-order completion, and exposes the exact cycle when each payload clears a boundary. The effective latency of a pipeline equals $L_{\text{pipe}} = \sum_{i=0}^{S-1} \delta_i$, where $S$ is the number of stages and $\delta_i$ is the service time of stage $i$ as defined by its period and stall behavior; throughput becomes $R_{\text{pipe}} = 1/(\max(\delta_i) + P_{\text{stall}})$, with $P_{\text{stall}}$ denoting the fraction of cycles spent honoring external stall assertions. Since the stall port is modeled as a separate input carrying integer-valued control packets, back-pressure from downstream components cleanly pauses the entire pipeline without resorting to ad hoc flags, and the `flush()` helper clears all `PipelineStageData` entries to model pipeline drains after exceptions or reconfiguration.

```cpp
// PipelineComponent usage excerpt (docs example)
auto pipe = std::make_shared<PipelineComponent>(
    "ActivationPipe", scheduler, /*period=*/1, /*stages=*/4);

pipe->setStageFunction(0, [](auto pkt) {
  return preprocessActivation(pkt);
});
pipe->setStageFunction(1, [](auto pkt) {
  return normalize(pkt, /*shift=*/2);
});
pipe->setStageFunction(2, [](auto pkt) {
  return applyNonLinearity(pkt);
});
pipe->setStageFunction(3, [](auto pkt) {
  pkt->setTag("ready");
  return pkt;
});
```

Table 1 summarizes how stage functions and control inputs coordinate to implement pipeline behavior within the event-driven kernel and highlights the contract designers follow when building pipelines with rich stall behavior.

| Stage Index | Function Prototype                             | Typical Usage                     |
| ----------- | ---------------------------------------------- | --------------------------------- |
| 0           | `std::shared_ptr<DataPacket>(DataPacketPtr)` | Decode or operand fetch           |
| 1..S-2      | `std::shared_ptr<DataPacket>(DataPacketPtr)` | Transformation, hazard resolution |
| S-1         | `std::shared_ptr<DataPacket>(DataPacketPtr)` | Commit, tagging, timing updates   |
| Stall Port  | `IntDataPacket` (0 = run, 1 = stall)         | Back-pressure gating              |
| Flush API   | `void flush()`                               | Empty pipeline on reconfiguration |

### 3.6 Precision-Typed ALUs via Template Traits

The arithmetic fabric reuses the same pipeline scaffolding while specializing behavior through the traits pattern codified in `src/components/alu.h`. Precision tags such as `Int32Precision` and `Float32Precision` select a `PrecisionTraits<PrecisionType>` specialization that prescribes the concrete data type, the number of pipeline stages, and a human-readable label, letting designers toggle datapath characteristics at compile time without touching the component wiring. Instantiating `ALUComponent<Int32Precision>` yields a three-stage pipeline (decode, execute, writeback) tuned for integer arithmetic, whereas `ALUComponent<Float32Precision>` expands to five stages so floating-point operations may amortize longer execution latency before writeback. Stage lambdas defer to `executeStage`, which inspects the templated `ALUDataPacket`, evaluates the requested `ALUOp`, and logs the operation signature to the tracing subsystem, preserving visibility across microarchitectural variants. The accumulator path implements multiply-accumulate by tracking an internal running sum $A_{n} = A_{n-1} + a_n b_n$, exposing `setAccumulator`, `resetAccumulator`, and serialization helpers so tiled kernels or checkpoint mechanisms can capture partial results mid-flight.

```cpp
// Precision trait sketch
struct BFloat16Precision {};

template <>
struct PrecisionTraits<BFloat16Precision> {
  using DataType = uint16_t;           // Encoded bfloat16 payload
  static constexpr size_t pipeline_stages = 4;
  static constexpr const char* name = "BF16";
};
```

| Precision Tag         | `PrecisionTraits<T>::DataType` | Pipeline Stages | Representative Ops                           |
| --------------------- | -------------------------------- | --------------- | -------------------------------------------- |
| `Int32Precision`    | `int32_t`                      | 3               | `ADD`, `AND`, `SLL`, `SLTU`, `MAC` |
| `Float32Precision`  | `float`                        | 5               | `ADD`, `MUL`, `DIV`, `ABS`, `NEG`  |
| `BFloat16Precision` | `uint16_t` (encoded)           | 4 (proposed)    | `ADD`, `MUL`, fused MAC with rounding    |
| `Int8Precision`     | `int8_t`                       | 2 (future)      | `DOT`, `MAC`, saturating arithmetic      |

### 3.7 ALU and LSU Microarchitecture Details

Internally the ALU maps each opcode to either integer-specific logic or floating-point math, as demonstrated by `executeInt32Operation` and `executeFloat32Operation`. Bitwise families (AND/OR/XOR), shifts (`SLL`, `SRL`, `SRA`), and comparisons (`SLT`, `SLTU`, `MAX`, `MIN`) leverage native integer operators, while floating-point handlers constrain the supported set to arithmetic and reduction primitives meaningful for tensor workloads; fused multiply-accumulate primitives update both the packet payload and an internal accumulator slot so that downstream components see consistent state transitions. Results are packaged into `IntDataPacket` or `FloatDataPacket` instances whose timestamps are updated to the scheduler’s current time, ensuring that downstream components observe causally consistent arrivals and letting trace visualizations highlight exact propagation delays. Trace macros differentiate compute and memory events, giving analysts visibility into per-opcode throughput, accumulator utilization, and pipeline occupancy and enabling automatic regression tests that flag unexpected timing shifts.

The load-store unit (`src/components/lsu.h`) mirrors the same architectural philosophy, exposing ports `req_in`, `resp_out`, `ready`, `valid`, and `done` while encapsulating a queue, per-bank finite state machines, and optional DRAMsim3 integration behind a single component boundary. Requests enter a FIFO so long as the queue depth permits and the unit reports readiness; vector transfers iterate over a length counter, with each element computing its bank via $\text{bank} = \text{address} \bmod N_{\text{banks}}$ and bank-local address $\text{addr}_{\text{bank}} = \lfloor\text{address} / N_{\text{banks}}\rfloor$. Memory banks advance through `IDLE`, `PROCESSING`, and `DONE` states, and they expose `isReady()` and `isDone()` hooks that the LSU polls using the same tick cadence as other components, keeping the control logic compact yet expressive. When contention arises, the LSU increments a stall counter and emits `TRACE_EVENT` records labeled `STALL`, making it straightforward to correlate bank conflicts with higher-level throughput regressions. Optional DRAM paths share the same interface but delegate storage to a `DRAMsim3Wrapper`, proving that the port/component/connection fabric scales from simple on-chip SRAM models to detailed off-chip timing without rewriting operators or scheduler logic.

```cpp
// LSU request issue loop (simplified excerpt)
if (auto req = dequeueRequest(); req) {
  const auto addr = req->getAddress() + element_index_ * req->getStride();
  const auto bank = addr % num_banks_;
  if (memory_banks_[bank]->isReady()) {
    memory_banks_[bank]->processRequest(localize(req, bank));
    current_state_ = State::WAITING_BANK;
  } else {
    cycles_stalled_++;
    TRACE_EVENT(now, getName(), "STALL", "bank=" << bank);
  }
}
```

| LSU Metric                | Formula / Definition                                                            | Trace Hook                              |
| ------------------------- | ------------------------------------------------------------------------------- | --------------------------------------- |
| Bank Selection            | $\text{bank} = (\text{address} \bmod N_{\text{banks}})$                       | `TRACE_EVENT` tag `BANK_ACCESS`     |
| Bank-Local Address        | $\text{addr}_{\text{bank}} = \lfloor\text{address} / N_{\text{banks}}\rfloor$ | `TRACE_EVENT` payload `BANK_ACCESS` |
| Stall Counter             | Incremented when `isReady()==false`                                           | `TRACE_EVENT` tag `STALL`           |
| Operation Completion Rate | $R_{\text{ops}} = \frac{\text{operations\_completed}}{t_{\text{sim}}}$        | `TRACE_EVENT` tag `MEM_DONE`        |
| Outstanding Requests      | Queue length sampled each cycle                                                 | `TRACE_EVENT` tag `QUEUE_DEPTH`     |

## 4. Discussion

NDP-SIM currently omits HDL co-simulation, coherent cache modeling, and power estimation, and its single-threaded scheduler may bottleneck extremely large workloads that emulate multichip fabrics. Nevertheless, the clear separation between the event scheduler and the port–component–connection fabric invites extensions: alternative calendar or ladder queues could accelerate scheduling, sampled tracing could trim log size for production workloads, and bindings to TVM or MLIR could feed compiler-generated schedules directly into the simulator. The architecture has also proven amenable to academic collaborations that layer formal models of queueing theory atop empirical traces, offering a bridge between analytic exploration and executable proof-of-concept.

## 5. Related Work

Gem5 [1], MARSSx86 [2], and SST [3] provide broad full-system coverage but carry heavier configuration overheads and emphasize general-purpose cores over fabric co-design. AccelSim [4] focuses on GPU pipelines with warp-level detail, while SCALE-Sim [5] models systolic arrays analytically rather than through explicit events. NDP-SIM’s niche lies in its explicit global-timestamp scheduler coupled with a port–component–connection fabric that mirrors architectural diagrams, and its Python tooling echoes FPGA bitstream flows [6], keeping simulation parameters and potential hardware encodings in lockstep. Together these attributes position NDP-SIM as a middle ground between fast analytical studies and heavyweight cycle-accurate full-system simulators.

## 6. Conclusion

By pairing an event scheduler that governs a single global timeline with a port–component–connection fabric that mirrors architectural intent, NDP-SIM supplies researchers with a deterministic, extensible environment for exploring near-data processing designs. The framework’s integration with configuration tooling, operator stacks, and trace analysis ensures that both functional correctness and performance insights are accessible early in the design cycle, while its modest learning curve keeps it approachable for interdisciplinary teams. As the roadmap brings in stochastic workloads, replay capabilities, and tighter links to hardware flows, NDP-SIM is positioned to accelerate the co-design of memory-centric accelerators and to serve as a living documentation hub for architectural experiments.

## Acknowledgments

We thank the UsernameCodeRookie community for sustained feedback throughout NDP-SIM’s development, and acknowledge the maintainers of GoogleTest and DRAMsim3 for the foundational infrastructure they provide. Community engagement through issue trackers and tutorial workshops continues to shape the priorities outlined in the roadmap.

## References

[1] N. Binkert et al., "The gem5 Simulator," *ACM SIGARCH Computer Architecture News*, vol. 39, no. 2, 2011.

[2] A. Patel et al., "MARSSx86: A Full System Simulator for x86 CPUs," *DAC*, 2011.

[3] M. P. Herbert et al., "A Modular Supercomputer Simulator," *SST Whitepaper*, 2010.

[4] A. Khairy et al., "AccelSim: An Extensible Simulation Framework for Accelerator-rich Architectures," *HPCA*, 2020.

[5] S. Samajdar et al., "SCALE-Sim: Systolic CNN Accelerator Simulator," *ISPASS*, 2019.

[6] S. Neuendorffer et al., "Initial Experience with a Simplified Bitstream Specification for FPGAs," *FPL*, 2020.
