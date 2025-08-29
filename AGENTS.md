# Agent Instructions

This repository contains `mnvkd`, a C server-authoring toolkit built around a virtual kernel with coroutine, actor, service and application frameworks.

## Testing

- To run the project's tests with debug settings, use:
  - `./debug.sh bmake test`
- To run the tests with release/optimized settings, use:
  - `./release.sh bmake test`
- The project can also be built using CMake:
  - `cmake -S . -B build`
  - `cmake --build build`
  - `ctest --test-dir build` (if tests are enabled)

## Project Overview

Key areas of the project include:

- **Core modules**: Objects follow a three-file pattern described in [design.md#Object-Oriented C Style](design.md#object-oriented-c-style):
  `vk_<name>.h` exposes public interfaces with opaque types, `vk_<name>_s.h` defines the full structs, and `vk_<name>.c` provides the implementations. Examples include threading (`vk_thread.c`), networking (`vk_socket.c`), services (`vk_service.c`), and the main entry point (`vk_main.c`).
- **Tests and examples**: Files prefixed with `vk_test_` provide usage examples and unit tests.
- **Build scripts**: `debug.sh` and `release.sh` set up compilation flags for development or optimized builds.
- **Build systems**: Both BSD Make (invoked via `bmake`) and CMake are supported.

Always ensure tests pass before committing changes.
