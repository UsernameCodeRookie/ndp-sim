# NDP-SIM Architectural Specification

## 1. Scope and Objectives
The NDP-SIM project provides an event-driven simulation environment for near-data processing architectures, emphasizing a clean separation between hardware primitives and software operators. This specification formalizes the architecture, timing semantics, configuration stack, and verification strategy of the framework as implemented in this repository.

Primary objectives are:
- Model heterogeneous compute fabrics (pipelined ALUs, processing elements, systolic arrays) under a unified event-scheduled clocking discipline.
- Provide reusable communication and memory abstractions that capture realistic flow control, latency, and bandwidth constraints.
- Support hardware/software co-design through an operator layer capable of binding to simulated accelerators while retaining CPU fallbacks.
- Deliver tooling for configuration bitstream composition and architectural parameter management.

## 2. System Overview
### 2.1 Layered Simulation Stack
```
┌────────────────────────────────────────────────────────┐
│ Application / Operator Layer (src/operators, tests)    │
├────────────────────────────────────────────────────────┤
│ Accelerator Primitives (TPU, LSU, pipelines, PE)       │
├────────────────────────────────────────────────────────┤
│ Communication & Timing Fabric (ports, connections,     │
│ ready-valid handshakes, tick scheduling)               │
├────────────────────────────────────────────────────────┤
│ Event-Driven Kernel (event.h, scheduler.h)             │
├────────────────────────────────────────────────────────┤
│ Configuration & Bitstream Toolchain (config/,          │
│ bitstream/, docs/)                                     │
└────────────────────────────────────────────────────────┘
```

### 2.2 Architectural Model
- **Discrete time:** All components advance on scheduler-managed ticks. Latency is encoded as integer cycle counts.
- **Message passing:** Components communicate via `Architecture::Port` instances carrying polymorphic `DataPacket`s.
- **Configurability:** Python utilities generate bit-level configuration payloads aligned with the simulated hardware datapath.

## 3. Event-Driven Simulation Kernel (`src/event.h`, `src/scheduler.h`)
- **Event taxonomy:** `EventType` enumerates compute, memory, communication, and custom events. The base `Event` class encapsulates timestamps, priorities, and cancellation semantics.
- **Scheduling semantics:** `EventScheduler` maintains a priority queue sorted by time then priority. `run`, `runFor`, and `runUntil` drive the simulation horizon. Lambda and periodic events allow lightweight scheduling without bespoke subclasses.
- **Determinism:** Events cannot be scheduled in the past; warnings are emitted for violations. Verbose tracing provides deterministic replay aid.
- **Periodic execution:** `PeriodicEvent` reissues itself with preserved execution counts, enabling clock-like behavior for components without hand coding loops.

## 4. Component Abstractions (`src/component.h`, `src/tick.h`)
- **Component base class:** Stores a symbolic name, scheduler reference, enable flag, and a registry of named ports. Lifecycle hooks (`initialize`, `reset`) are exposed for derived classes.
- **Ticking components:** `TickingComponent` represents clocked blocks. Upon `start`, it schedules self-rescheduling lambda events spaced by the configured period, mirroring synchronous digital logic.
- **Pipelined execution:** `PipelineComponent` (e.g., ALU pipelines) manages stage-local state, stall and flush controls, and stage-specific transformation functions. Statistics such as occupancy and stall counts are tracked for analysis.

## 5. Communication Fabric (`src/port.h`, `src/connection.h`, `src/connections/ready_valid.h`)
- **Port semantics:** Ports may be input, output, or bidirectional. Writes place shared pointers to `DataPacket`; reads consume them, enabling back-pressure modeling.
- **Data packets:** `DataPacket` holds a timestamp and validity bit plus virtual cloning, allowing heterogeneous payloads (`IntDataPacket`, `ALUDataPacket`, `MemoryRequestPacket`, etc.).
- **Connections:** `Connection` objects aggregate source and destination ports and enforce propagation latency. `TickingConnection` periodically invokes `propagate` without additional user code.
- **Handshake modeling:** `ReadyValidConnection` implements elastic buffering with ready/valid semantics, internal FIFOs, stall accounting, and optional pipeline latency to capture realistic flow control.

## 6. Processing Subsystems (`src/components`)
### 6.1 Pipelined ALU (`components/alu.h`, `components/pipeline.h`)
- Three-stage pipeline (decode, execute, writeback) parameterized by lambda functions.
- Rich ALU opcode set (arithmetic, logical, shifts, comparisons, saturation). Static helpers provide symbolic names and glyphs for debugging.
- Stage-level verbose logging and statistics enable latency/pipeline hazard studies.

### 6.2 Processing Element (`components/pe.h`)
- Integrates register file, instruction queue with depth-controlled back pressure, and ALU execution.
- Ports: instruction/data ingress, ready/valid handshake, result egress.
- Queue occupancy statistics, register dumps, and utilization metrics aid microarchitectural evaluation.

### 6.3 Event Libraries (`src/events`)
- `ComputeEvent` and `MemoryAccessEvent` demonstrate pluggable behaviors atop the scheduler, showcasing extensibility for domain-specific stimuli.

## 7. Memory Subsystem (`components/lsu.h`, `components/dram.h`)
- **Memory banks:** `MemoryBank` simulates multi-cycle SRAMs with explicit state machines, latency counters, and response acknowledgment.
- **Load-store unit:** Unified LSU handles scalar/vector accesses, queueing, per-bank arbitration, and ready/valid interfacing. Statistics capture stall cycles and completed operations.
- **DRAM integration:** When compiled with `USE_DRAMSIM3`, `DRAMsim3Wrapper` delegates requests to a cycle-accurate third-party DRAM model, translating callbacks into completed transactions.
- **Direct access primitives:** `LoadStoreUnit::directRead` and `directWrite` bypass port-level flow control for configuration and bootstrapping flows.

## 8. Systolic Array TPU (`components/tpu.h`, `components/precision.h`)
- **MAC grid:** Template-based `SystolicArrayTPU` instantiates a square grid of `MACUnit`s. Each MAC maintains precision-specific accumulators and exposes hooks for instrumentation.
- **Precision traits:** `Int32PrecisionTraits` and `Float32PrecisionTraits` define value/accumulator types, encoding/decoding logic, and stringification, enabling mixed-precision experimentation.
- **Memory sharing:** A dedicated LSU instance services array memory operations, supporting both SRAM-style banks and optional DRAMsim3 backends.
- **Control surface:** Public API exposes MAC handles, memory block transfers, accumulator resets, and verbose tracing for algorithm developers.

## 9. Operator Framework (`src/operators`)
- **Execution abstraction:** `OperatorBase` manages backend selection (`CPU`, `TPU`, `AUTO`), TPU binding, and verbose reporting, promoting software/hardware co-design without recompilation.
- **Tensor utilities:** `Tensor` and `TensorShape` encapsulate multidimensional data with randomization helpers for testing.
- **GEMM operator (`operators/gemm.h`):** Implements tiled and naive CPU kernels alongside a TPU-accelerated path that exercises memory primitives and MAC orchestration. Tile scheduling is parameterized through `TileConfig` and `TileIterator`.
- **Correctness harness:** `verifyGEMM` cross-checks results, providing tolerance-based validation for floating point paths.

## 10. Configuration and Bitstream Toolchain (`bitstream/`, `config/`, `config/utils/`)
- **Bit-level primitives:** `Bit` (arbitrary-width bitvectors) supports concatenation, slicing, arithmetic, and serialization, serving as the backbone of configuration word assembly.
- **Graph mapping:** `bitstream/index.py` introduces deferred node indexing (`NodeIndex`, `Connect`) to decouple naming from final placement, coordinating with `NodeGraph` registry.
- **Config modules:** Abstract base classes in `bitstream/config/base.py` and Python modules in `config/component_config/` outline how architectural parameters map to packed bitfields. Utility functions (e.g., `pack_field_decimal`, `concat_bits_high_to_low`) ensure consistent endianness.
- **Generator stubs:** `config/config_generator.py` sketches merge order and file loading for assembling complete configuration images, establishing integration points for future automation.

## 11. Repository Layout Highlights
- `src/`: C++ simulation kernel, components, operators, and headers for consumers.
- `tests/`: GoogleTest-based regression suite covering GEMM correctness, backend consistency, and error handling.
- `docs/UNIFIED_ARCHITECTURE.md`: Supplemental design rationale for unified TPU/LSU integration and DRAMsim3 toggling.
- `third_party/`: Vendor libraries (DRAMsim3, GoogleTest) vendored for reproducible builds.
- `build/`: CMake-generated artifacts (kept out of version control best practices but present here for reference).

## 12. Build and Toolchain Integration
- **Configuration:** `cmake -S . -B build` configures the project and fetches GoogleTest via `FetchContent`.
- **Compilation:** `cmake --build build` produces simulation binaries (e.g., GEMM examples, TPU demos) and unit tests.
- **Optional macros:** Define `USE_DRAMSIM3` at compile time to enable off-chip memory modeling. Additional include paths must reference `third_party/DRAMsim3` headers.

## 13. Verification Strategy (`tests/test_gemm_operator.cpp`)
- Validates TPU-accelerated GEMM against CPU implementations across sizes (2×2 up to 64×64) and tiling configurations.
- Exercises error paths (dimension mismatch, non-matrix tensors) to ensure defensive programming.
- Uses environment-controlled verbosity (`GEMM_TEST_VERBOSE`) for deterministic trace capture.

## 14. Extensibility Guidelines
- **New components:** Derive from `Component` or `TickingComponent`, register ports in the constructor, and schedule recurring behavior via `start`.
- **Custom communication patterns:** Extend `Connection` to implement arbitration policies or multi-cast semantics while reusing scheduler integration from `TickingConnection`.
- **Additional operators:** Subclass `OperatorBase`, implement CPU and TPU execution paths, and reuse tensor helpers for data preparation.
- **Configuration modules:** Populate `FIELD_MAP` entries in Python modules to map JSON/YAML specifications into bitstreams, leveraging provided packing utilities.

## 15. Future Work
- Complete the configuration generator pipeline to emit deployable bitstreams directly from high-level specs.
- Expand the operator library (e.g., convolution, activation functions) while reusing TPU primitives.
- Introduce stochastic or trace-driven event sources to evaluate workload-dependent behaviors.
- Integrate performance counters into the scheduler for full-system throughput/latency profiling.

## 16. Glossary
- **NDP:** Near-Data Processing—computation close to memory to reduce data movement.
- **TPU:** Tensor Processing Unit—here, a configurable systolic array accelerator.
- **LSU:** Load-Store Unit handling memory transactions for compute arrays.
- **MAC:** Multiply-Accumulate unit, fundamental to systolic arrays.
- **Ready/Valid Handshake:** Flow control protocol ensuring data transfer only when both producer (valid) and consumer (ready) agree.

---
This specification mirrors the current implementation state of the `event-driven` branch (Owner: `UsernameCodeRookie`, Repository: `ndp-sim`) and should serve as the authoritative reference for contributors extending the simulator, tooling, or architectural models.
