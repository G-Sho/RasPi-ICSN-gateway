# Forwarding state

## Gateway FIB

`GatewayFIB` is the gateway’s local forwarding table. It stores name-to-MAC mappings and resolves the best match using a longest-prefix match approach backed by a fixed-size LRU cache.

## PIT

`MainController::pit_` holds pending Interest state keyed by the ICSN content name. Entries track:

- the last forward timestamp
- a FIFO queue of pending chunk numbers

This is used to avoid sending duplicate ICSN Interests while a matching request is still in flight.

## CS

`MainController::cs_` stores a short-lived recent content value keyed by content name. It is used to serve delayed or repeated requests immediately without revisiting the ESP32 side.

## Guardrails

- These state structures are gateway-local and should not be conflated with the CEFORE daemon’s own PIT/CS.
- The gateway does not implement CEFORE routing itself; it simply forwards with local knowledge and the current request stream.
