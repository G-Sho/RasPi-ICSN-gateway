# Overview

## Purpose

This repository implements the Raspberry Pi-side gateway for an ICSN/ESP32 environment connected to a CEFORE-based NDN system.

## Current scope

The gateway is responsible for:

- receiving UART traffic from an ESP32 bridge
- parsing ICSN DATA / INTEREST payloads
- learning content-name to MAC mappings in `GatewayFIB`
- forwarding CEFORE Interests to the ICSN side
- publishing sensor payloads to CEFORE
- maintaining a small local PIT and CS for gateway-local latency and duplicate suppression

## Role boundaries

This repository does not implement the sensor node firmware or the CEFORE daemon itself. It connects to that external system through a client interface and leaves the actual NDN network behavior to `cefnetd` and CEFORE.
