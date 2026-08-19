# Build and operations

## Build assumptions

The project is configured in [CMakeLists.txt](CMakeLists.txt) to search for CEFORE with a configurable `CEFORE_ROOT` value. The default is `/usr/local` if no override is provided.

## Operational requirements

- `cefnetd` must be running before the gateway starts
- the gateway expects a valid UART device and baud rate
- the optional FIB config file is loaded when supplied at startup

## Documentation guidance

Keep examples grounded in the actual command-line arguments of the project and avoid referencing personal lab topology, local mount paths, or one-off experimental hardware arrangements.
