# Copilot instructions for RasPi-ICSN-gateway

## Project role

This repository is a Raspberry Pi gateway that bridges an ESP32-based ICSN network to a CEFORE-backed NDN environment. The gateway is the system boundary between UART-based sensor traffic and CEFORE traffic.

## Source of truth

Treat the current implementation in `src/` and `include/` as the authoritative specification. Do not document speculative or historical behavior as if it were current runtime behavior.

## Scope constraints

- Do not change runtime behavior as part of documentation work.
- Do not redesign UART payload encoding unless the issue explicitly requires it.
- Do not claim future features as implemented behavior.
- Do not include personal machine paths, local lab topology, or real MAC addresses in public docs.

## Architecture rules

- Keep the gateway role distinct from ESP32 bridge nodes, sensor nodes, and `cefnetd`.
- Explain wired routing as gateway-local state, not CEFORE-internal state.
- Distinguish `GatewayFIB`, `PIT`, and `CS` from the CEFORE protocol state machine.
- Describe the current callback-driven and polling-driven flow accurately.

## Documentation expectations

- Prefer direct references to current files and headers.
- When describing the protocol, use example values and generic names rather than lab-specific details.
- Keep README and build docs aimed at repository users.
- Keep developer and skill docs focused on architecture rules, invariants, and current implementation constraints.

## Required validation before completion

- Check that examples and commands match the actual build configuration.
- Check that the docs align with `CMakeLists.txt` and the runtime source files.
- Verify that no functional changes were introduced while editing docs.
