# Measurement and latency

## Current behavior

`MainController` writes a file named `latency_log.csv` when the gateway measures transit time across a CEFORE-issued Interest and the returned sensor Data value.

The CSV contains:

- unix timestamp in microseconds
- content name
- chunk number
- latency in microseconds

## Scope

This measurement is a gateway-side diagnostic feature. It is not a general-purpose network emulation or a production telemetry system.

## Guidance

When documenting this behavior, describe it as a local measurement artifact for evaluating gateway latency rather than as a CEFORE protocol metric.
