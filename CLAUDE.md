# OpenCrawlEngine

A local, single-player role-playing game engine driven by a language-model game
master. The engine runs on the player's machine: it talks to an
OpenAI-compatible endpoint (the player supplies their own key), keeps all state
in a local SQLite database, and renders through a Dear ImGui desktop frontend.
There is no server, account system, or telemetry — only the model endpoint.

This file orients work in the repository. It favors specifics over generalities;
when something here disagrees with the code, trust the code and fix this file.

## Repository layout

```
libs/        C11 infrastructure libraries (stable C ABIs), one job each:
             json, http, llm, agent, secrets, store
engine/      C++17 headless game library (namespace oce::) — model, rules, the
             game-master tool layer, persistence. Links no graphics.
app/         C++17 frontend — GLFW + OpenGL + Dear ImGui (app/ui) and the
             entry point (app/src/main.cpp).
cmake/       Dependency and helper modules.
scripts/     Developer utilities.
```

Each module carries its own `CMakeLists.txt` and is added to the build once it
exists, so the tree compiles at every step.

## Build & run

```
cmake -S . -B build -G Ninja
cmake --build build
./build/bin/OpenCrawlEngine
```

Headless (no graphics — builds the libraries, engine, and tests only):

```
cmake -S . -B build-headless -G Ninja -DOCE_BUILD_UI=OFF
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
```

Debug builds enable AddressSanitizer and UBSan by default (`-DOCE_SANITIZERS=OFF`
to turn them off).

## Architecture

- **Infrastructure (C11, `libs/`).** Small single-purpose libraries with stable
  C ABIs: JSON, HTTP + streaming, an OpenAI-compatible chat client, the agent
  turn loop, a secret store, and persistence. Each returns status codes (never
  asserts or throws in release), uses single-owner non-thread-safe handles, and
  is unit-tested offline.
- **Engine (C++17, `engine/`).** The game itself: value-type model structs,
  deterministic rule systems (dice, combat, skill checks, leveling, items,
  assets, world state), and persistence. It links no graphics and is fully
  testable headless with a mock model backend and an in-memory store.
- **Game master via tool calls.** The model drives the game by calling
  registered tools (apply stat changes, add an item, start combat, set a skill
  check, and so on). Narrative is streamed assistant text; mechanics are tool
  calls. Every tool validates and clamps its arguments before mutating state and
  returns a small result the model can react to. The engine — never the model —
  owns dice outcomes.
- **Threading.** One process, two threads. The UI thread owns the window and
  renders a snapshot of game state copied under a brief lock. A single worker
  thread owns the agent/LLM/HTTP handles and runs the blocking streaming turn;
  streamed tokens and tool mutations are published under the same lock.
  Cancellation is a polled flag. The UI thread never makes a network call.

## Conventions

- **Languages.** C11 for `libs/` (stable ABI); C++17 for `engine/` and `app/`.
- **Files.** `snake_case`. C library files and public symbols carry the `oce_`
  prefix (`oce_llm.h`, `oce_llm_chat_stream`). C++ lives in namespace `oce`
  (and nested, e.g. `oce::ui`).
- **Naming.** Types `PascalCase`; functions and methods `snake_case`; member
  variables trailing underscore (`window_`); constants `kName`. `#pragma once`
  in every header.
- **Headers** open with a short banner: purpose, ownership, lifetime, and
  threading expectations.
- **Memory.** RAII and `std::unique_ptr` for ownership in C++; explicit
  `init`/`shutdown` pairs with "who allocates, who frees" documented in C. Raw
  pointers are borrowed, never owned.
- **Errors.** Status-code enums in C; a `Result`-style return or status codes in
  C++. No exceptions across the C ABI.
- **Managers** are single-owner and non-copyable (deleted copy constructor), own
  one responsibility, and are reached through narrow APIs — no global
  singletons, no god objects. Dependencies are injected (callbacks/handles);
  composition over inheritance.
- **Comments** explain intent, not mechanics, and appear only where they earn
  their place.

## Project standards

This is open-source software; committed artifacts are held to that bar.

- Implement from understanding. Write fresh, clear code; do not paste in
  third-party source.
- Keep the codebase product-neutral: no marketing, business, enterprise, or
  service/subscription framing anywhere — code, comments, docs, or commit
  messages. (The in-fiction economy — taverns, shops, and other holdings the
  player can own — is game content and is unrelated.)
- Validate and bound all model-supplied input before it touches game state. Keep
  secrets in memory only and zeroized on release; parameterize all SQL; cap
  stream sizes.
- Local, uncommitted developer notes may live in `.claude/local/` (git-ignored).

## Decision shapes

| Situation | Default move |
| :--- | :--- |
| Bug fix, small in-scope addition, behavior-preserving refactor | Proceed; keep it focused. |
| New dependency, new public ABI, schema change, threading change | Propose first; these are load-bearing. |
| Anything touching secrets, input validation, or SQL | Treat as security-bearing; review before merging. |
| Build is red (compile, test, or sanitizer) | Stop and make it green before continuing. |

## Skills & verification

Project skills live in `.claude/skills/`. The gate before calling work done is
`oce-verify` (headless build + `ctest`). Run `oce-clean-check` before committing.
Never mark a feature complete unless the build, tests, and sanitizers are green.

## Tone

Concise and peer-to-peer. Ground statements in `file:line`. State what changed
and why; skip narration of process. Prefer a recommendation over a survey of
options.
