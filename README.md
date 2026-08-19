# Raspberry Pi - ICSN Gateway

This repository contains a Raspberry Pi C++ gateway that connects an ESP32-based ICSN network to a CEFORE/NDN network through a UART bridge and a CEFORE client interface.

## Overview

The gateway acts as a forwarding boundary between two domains:

- ICSN / ESP32 side: sensor data arrives over UART and is forwarded as Interest or Data traffic
- CEFORE / NDN side: the gateway registers prefixes, receives Interests from cefnetd, and publishes sensor content to the NDN network

The current implementation is a runtime service, not a general-purpose simulator or a lab-specific topology. The source of truth is the code under [src](src) and the components declared in [include](include).

## System position

```text
ESP32 / ICSN node <--UART--> Raspberry Pi gateway <--CEFORE API--> cefnetd <--NDN--> CEFORE network
```

The role of this repository is limited to the gateway logic itself:

- parse UART packets from the ESP32 side
- map ICSN content names to/from CEFORE names
- learn routing entries from packet metadata
- forward Interests to the ICSN side using the gateway FIB
- publish sensor payloads to CEFORE and cache recent values locally

## Core responsibilities

| Component | Responsibility |
|---|---|
| `UARTReceiver` | Reads and transmits UART data to and from the ESP32 bridge |
| `PacketParser` | Parses serialized ICSN frames into structured sensor data |
| `CeforeInterface` | Connects to `cefnetd` and sends/receives CEFORE Interest/Data messages |
| `NameMapper` | Adds/removes scheme components and strips CEFORE-specific suffixes |
| `GatewayFIB` | Stores routing entries and performs LPM-style prefix lookup |
| `MainController` | Coordinates UART callbacks, Interest handling, PIT/CS behavior, and latency logging |

## Data flow

### ICSN -> CEFORE

1. An ESP32 sensor sends a DATA packet over UART.
2. `UARTReceiver` delivers the raw payload to `MainController`.
3. `PacketParser` decodes the payload into a structured `SensorData` record.
4. The gateway learns the sender MAC from the packet source and saves the corresponding FIB entry.
5. `NameMapper` converts the ICSN name into a CEFORE URI.
6. `MainController` stores the payload in the local CS and publishes it through `CeforeInterface::sendData()`.

### CEFORE -> ICSN

1. `cefnetd` delivers a CEFORE Interest to the gateway.
2. `CeforeInterface::parseInterest()` converts the name back to an ICSN content name.
3. `MainController::onInterest()` checks the PIT and CS states before forwarding.
4. `GatewayFIB::lookup()` selects the next-hop MAC for the content name using the current gateway-local FIB state.
5. `UARTReceiver` sends the Interest to the ESP32 node over UART.

## Current implementation details

### UART protocol

The UART payload format is still implemented as a small framed message carrying sender MAC, length, and Base64 payload:

```text
RX:<sender_mac>|<data_length>|<base64_payload>\n
TX:<destination_mac>|<base64_payload>\n
```

The layout is handled in the UART receiver and is consumed by the packet parser before routing.

### ICSN packet structure

The payload encoded over UART is modeled as a packed binary structure:

```cpp
struct __attribute__((packed)) CommunicationData {
    char signalCode[10];
    uint8_t hopCount;
    char contentName[100];
    char content[20];
    uint32_t counter;
    uint8_t hmac[32];
};
```

The gateway interprets `signalCode` values such as `DATA` and `INTEREST` as part of the ICSN flow.

### Name mapping

`NameMapper` performs the current name conversions used by the gateway:

- adds a `ccnx:/` scheme when publishing to CEFORE
- removes the scheme when handling CEFORE Interests
- strips CEFORE-added TLV-like suffix components before deriving the underlying ICSN content name

### Gateway-local FIB, PIT, and CS

These state tables are gateway-local and are not the same as CEFORE internal PIT/CS state:

- `GatewayFIB`: stores content-name to downstream MAC mappings and resolves by longest-prefix match
- `PIT`: suppresses duplicate Interests for the same content during a short timeout window and queues chunk numbers for ordered replay
- `CS`: stores a short-lived copy of the latest content so delayed Interests can be served immediately without an additional ESP32 round trip

### Measurement/logging

`MainController` writes a CSV file named `latency_log.csv` to record:

- unix timestamp in microseconds
- content name
- chunk number
- latency in microseconds

This log is a gateway-side measurement artifact for observing response latency during CEFORE/ICSN round trips.

## Build and run

Detailed instructions are in [BUILD.md](BUILD.md).

Typical build flow:

```bash
mkdir -p build
cd build
cmake -DCEFORE_ROOT=/path/to/cefore ..
make -j$(nproc)
```

Typical startup:

```bash
sudo ./gateway /dev/serial0 115200 ./config/test_fib.conf
```

The process expects `cefnetd` to be running before the gateway initializes its CEFORE connection.

## Project structure

- [src](src): runtime implementation
- [include](include): public headers and shared data structures
- [config](config): sample runtime configuration
- [docs](docs): additional design or reference material
- [BUILD.md](BUILD.md): build and dependency notes
- [OPERATION_GUIDE.md](OPERATION_GUIDE.md): runtime validation steps
- [raspi-gateway-design.md](raspi-gateway-design.md): design-level overview

## Dependencies

- CEFORE / `libcefore`
- CMake 3.10+
- C++17 compiler
- OpenSSL libraries
- pthread and dl support

## Notes

- This repository documents the current implementation, not a speculative future design.
- Public docs should avoid personal environment values, local absolute paths, and lab-specific topology details.
- Functional code changes are intentionally out of scope for documentation-only cleanup work.

## License

See [LICENSE](LICENSE).

## Related documents

- [BUILD.md](BUILD.md)
- [OPERATION_GUIDE.md](OPERATION_GUIDE.md)
- [raspi-gateway-design.md](raspi-gateway-design.md)
- [CMakeLists.txt](CMakeLists.txt)
- [CEFORE official site](https://cefore.net/)
