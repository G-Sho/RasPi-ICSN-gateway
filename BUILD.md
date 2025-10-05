# Build Instructions

## Prerequisites

### Required Libraries

1. **CEFORE** (libcefore)
   ```bash
   # Install CEFORE from source or package manager
   # See: https://cefore.net/
   ```

2. **Build Tools**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git
   ```

## Build Steps

1. Clone the repository
   ```bash
   git clone <repository-url>
   cd RasPi-ICSN-gateway
   ```

2. Create build directory
   ```bash
   mkdir build
   cd build
   ```

3. Configure with CMake
   ```bash
   cmake ..
   ```

4. Build
   ```bash
   make
   ```

5. Install (optional)
   ```bash
   sudo make install
   ```

## Running

### Basic Usage

```bash
sudo ./gateway /dev/serial0 115200
```

### Command Line Arguments

- `argv[1]`: UART device path (default: `/dev/serial0`)
- `argv[2]`: Baudrate (default: `115200`)

### Example

```bash
# Using default settings
sudo ./gateway

# Custom UART device and baudrate
sudo ./gateway /dev/ttyUSB0 115200
```

## UART Configuration on Raspberry Pi

1. Enable UART on GPIO pins
   ```bash
   sudo raspi-config
   # Navigate to: Interface Options -> Serial Port
   # Disable login shell over serial: No
   # Enable serial port hardware: Yes
   ```

2. Edit `/boot/config.txt`
   ```bash
   sudo nano /boot/config.txt
   ```

   Add or modify:
   ```
   enable_uart=1
   dtoverlay=disable-bt
   ```

3. Reboot
   ```bash
   sudo reboot
   ```

## Troubleshooting

### CEFORE not found

If CMake cannot find CEFORE:
```bash
# Specify CEFORE install path
cmake -DCEFORE_INCLUDE=/path/to/cefore/include -DCEFORE_LIB=/path/to/cefore/lib ..
```

### UART permission denied

```bash
sudo usermod -a -G dialout $USER
# Logout and login again
```

### cefnetd not running

Make sure cefnetd is running before starting the gateway:
```bash
sudo cefnetd
```
