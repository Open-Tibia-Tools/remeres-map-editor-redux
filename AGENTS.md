# Agent Development Guide & Rules

## Project Context

**Remere's Map Editor (RME) Redux** is a high-performance, tile-based map editor for Open Tibia servers, built with modern C++ (C++20/C++23), wxWidgets, OpenGL 4.6, and NanoVG.

* **Concise, Modern UX**: Delivers a streamlined, responsive, and distraction-free workflow for map creators. UI interactions must remain snappy and intuitive.
* **Peak Rendering & Viewport Performance**: Maps contain millions of tiles, items, and creatures. The editor relies on high-throughput batched rendering (OpenGL 4.6 SpriteBatch), spatial indexing, and zero-stutter real-time panning/zooming.
* **Cache Locality & Data-Oriented Design (DOD)**: Because millions of elements are traversed every frame, cache locality, contiguous memory buffers (flat vectors, SoA), and predictable memory access patterns are mandatory. Avoid pointer chasing and deep polymorphic hierarchies in performance-critical paths.
* **Architecture Reference**: For an exhaustive system overview, file-by-file catalog, and data structures, refer to [source/util/agents.md](file:///e:/Github/remeres-map-editor-redux/source/util/agents.md).

---

## Core Development Rules

### 1. Enforce Single Responsibility Principle (SRP)
- Every class, module, and function must have one single, well-defined responsibility.
- **Opportunistic Refactoring**: If touched code violates SRP, all edits must account for refactoring the affected area into SRP (split monolithic classes, extract multi-purpose routines, decouple UI/logic/data).

### 2. Enforce Data-Oriented Design (DOD) & Cache Locality
- Design data structures for optimal cache utilization, contiguous memory layout, and batch processing (e.g., flat contiguous vectors, Struct-of-Arrays where appropriate) rather than deep pointer-chasing object graphs.
- **Opportunistic Refactoring**: If touched code violates DOD, refactor the touched data structures and access patterns towards DOD.

### 3. Dedicated Feature Branches & PR Workflow
- Every plan, task, or bugfix implementation must be executed on a separate, dedicated git branch (e.g., `feature/...`, `fix/...`, `refactor/...`) cut from `main`.
- **Mandatory in Plans**: Every implementation plan must explicitly include creating and checking out a new dedicated branch as its first execution step.
- Never commit work directly to `main`—all changes must be isolated so they can be cleanly submitted and merged via Pull Request.

### 4. Commit Frequently
- Make small, atomic, and logically coherent commits on the feature branch.
- Never bundle unrelated changes across subsystems into a single commit.

### 5. Clean Code & Dead Code Elimination
- Code must remain clean, readable, and properly formatted.
- Before committing, always check for and eliminate dead code, unused functions/parameters, obsolete includes, and temporary debug statements. Never leave commented-out dead code.

### 6. Apply YAGNI (You Aren't Gonna Need It)
- Implement only what is directly needed to solve the task at hand.
- Avoid speculative generalizations, premature abstractions, or forward-looking hooks for hypothetical use cases.

### 7. Reuse Before Implementing
- Before creating a new mechanism or utility, search the codebase to see if it is already implemented (e.g., existing map saving/loading, spatial lookups, parsers, or math helpers).
- Reuse, extend, or extract from existing code rather than reinventing functionality. Consult [source/util/agents.md](file:///e:/Github/remeres-map-editor-redux/source/util/agents.md) for existing subsystems.

### 8. Modern C++ (C++20 & C++23)
- Maximize the use of modern C++ features supported by the toolchain:
  - Concepts and constraints (`<concepts>`, `requires` clauses).
  - Ranges and views (`std::ranges`, `std::views`).
  - Formatting via `std::format`.
  - Non-owning views (`std::span`, `std::string_view`) over raw pointer/length pairs.
  - Compile-time evaluation (`constexpr`, `consteval`).
