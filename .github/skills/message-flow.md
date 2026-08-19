# Message flow

## ICSN -> CEFORE

1. The ESP32 sends a DATA packet over UART.
2. `UARTReceiver` receives the message and passes it to `MainController` through the RX callback.
3. `PacketParser` extracts `signalCode`, `contentName`, and the payload.
4. `GatewayFIB` learns the sender MAC and stores a content-name to next-hop route.
5. `MainController` builds the CEFORE URI via `NameMapper` and publishes the value through `CeforeInterface::sendData()`.
6. The gateway also saves a short-lived content cache entry in its local CS.

## CEFORE -> ICSN

1. `cefnetd` sends an Interest to the gateway through `CeforeInterface`.
2. `MainController::onInterest()` normalizes the URI and checks local PIT/CS state.
3. If no viable cache hit is available, the gateway uses `GatewayFIB::lookup()` to determine which MAC to forward to.
4. The gateway serializes an ICSN `INTEREST` packet over UART and forwards it to the target node.

## Summary

The gateway is not the origin of the network protocol; it is the translator and forwarder between the ICSN device side and the CEFORE transport side.
