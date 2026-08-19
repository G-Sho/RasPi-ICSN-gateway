# CEFORE interface

## Purpose

`CeforeInterface` wraps the CEFORE client API used by the gateway to register prefixes, send Interest packets, send Data objects, and read pending CEFORE traffic.

## Current model

The implementation follows the client-style lifecycle used by the repository:

1. `cef_log_init2()`
2. `cef_frame_init()`
3. `cef_client_init()`
4. `cef_client_connect()`
5. `cef_client_read()` in the polling loop

## Important details

- CEFORE registration calls use `cef_client_prefix_reg()` and the URI-to-name conversion helpers.
- `receive()` polls the CEFORE client socket with a timeout rather than blocking indefinitely.
- `parseInterest()` converts CEFORE name payloads back into gateway-visible URIs.

## Guidance

- Keep CEFORE-side behavior documented as external to the gateway’s internal local routing state.
- Do not assume that local NAME registration implies that the gateway owns NDN routing decisions.
- Preserve the current callback-driven Interest handling pattern unless the issue explicitly changes the runtime API.
