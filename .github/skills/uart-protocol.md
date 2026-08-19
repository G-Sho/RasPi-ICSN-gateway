# UART protocol

## Current message format

The current implementation expects UART frames of the form:

```text
RX:<sender_mac>|<data_length>|<base64_payload>\n
TX:<destination_mac>|<base64_payload>\n
```

## Packet parsing

`PacketParser` decodes the serialized payload into the structured `CommunicationData` layout used by the gateway logic:

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

## Implementation guidance

- Keep UART message framing consistent with the existing receiver/parser logic.
- Do not reinterpret the serialized format as a CEFORE protocol message.
- Treat the UART side as a transport boundary to the ESP32/ICSN side, not as the CEFORE network state machine.
