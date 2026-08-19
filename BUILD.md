# Build guide

This repository builds a Raspberry Pi gateway executable that links against CEFORE libraries and runs as a local service bridging UART-based ICSN traffic and CEFORE-controlled NDN traffic.

## Prerequisites

- CEFORE installed and available on the target system
- CMake 3.10 or newer
- C++17 compiler
- OpenSSL development libraries
- pthread / dl support

The project expects a CEFORE installation under a standard prefix such as `/usr/local`, or a custom prefix specified at configure time.

## Install CEFORE

Follow the CEFORE installation instructions from the CEFORE project. The build uses the `cefore` library and expects the headers and shared library to be available under the configured CEFORE root.

## Configure the build

From the repository root:

```bash
mkdir -p build
cd build
cmake -DCEFORE_ROOT=/path/to/cefore ..
```

If `CEFORE_ROOT` is not specified, the project defaults to `/usr/local` as shown in [CMakeLists.txt](CMakeLists.txt).

## Build

```bash
make -j$(nproc)
```

The resulting binary is named `gateway` and is produced at:

```bash
./gateway
```

## Run

Before starting the gateway, ensure that `cefnetd` is already running.

```bash
sudo ./gateway /dev/serial0 115200 ./config/test_fib.conf
```

Argument summary:

1. UART device path (for example `/dev/serial0`)
2. UART baud rate (for example `115200`)
3. Optional initial FIB config file path

Example:

```bash
sudo ./gateway /dev/ttyUSB0 115200 ./config/test_fib.conf
```

## UART configuration

The gateway expects the Raspberry Pi serial interface to be available on the selected UART device. Common examples are:

```bash
ls -l /dev/serial0
ls -l /dev/ttyAMA0
ls -l /dev/ttyUSB0
```

If the serial port is not accessible, add the current user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
```

Then log out and log back in before retrying.

## CEFORE lookup troubleshooting

If CMake cannot find the CEFORE library, specify the installation root explicitly:

```bash
cmake -DCEFORE_ROOT=/opt/cefore ..
```

This is the expected configuration path for the project and matches the logic in [CMakeLists.txt](CMakeLists.txt).

## Operational notes

The gateway does not replace `cefnetd`; it connects to it through the `CeforeInterface` layer. In practice:

- `cefnetd` must be running before the gateway starts
- the gateway registers the relevant relevant prefixes using the CEFORE client API
- the gateway forwards ICSN requests to the UART side and publishes CEFORE Data through the same runtime loop

## Related documents

- [README.md](README.md)
- [OPERATION_GUIDE.md](OPERATION_GUIDE.md)
- [raspi-gateway-design.md](raspi-gateway-design.md)
- [CMakeLists.txt](CMakeLists.txt)
