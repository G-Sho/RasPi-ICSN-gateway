# Architecture

## Main components

- `UARTReceiver`: serial interface to the ESP32 bridge
- `PacketParser`: converts payloads into structured sensor data
- `CeforeInterface`: wraps CEFORE client calls and callback registration
- `NameMapper`: converts names between ICSN and CEFORE forms
- `GatewayFIB`: stores routing entries and supports longest-prefix lookup
- `MainController`: orchestrates the runtime logic and gateway state

## Runtime model

The current code uses a callback-driven UART receive path plus a polling loop for CEFORE receive processing. The gateway does not rely on a single monolithic event queue; instead, each subsystem owns a single responsibility.

## Important constraints

- `GatewayFIB` is routing state in the local gateway process.
- The CEFORE PIT and CS are external to this repo and should not be described as the gateway’s own internal state.
- The gateway does not own the full sensor network topology; it only forwards based on local name-to-MAC mapping and the present request state.
