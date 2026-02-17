# DECT Communicator

A BLE-enabled accessory that provides SMS-style messaging over DECT NR+. This project is in its early stages of development.

## Overview

DECT Communicator uses the DECT NR+ radio standard to send and receive short messages between devices. A BLE interface allows mobile devices to connect and interact with the communicator as an accessory.

## Architecture

The firmware is built on the nRF Connect SDK (NCS) v3.1.1 and Zephyr RTOS, targeting Nordic Semiconductor hardware with DECT NR+ modem support.

The application is organized into two layers:

- **Data Layer** - Handles low-level DECT NR+ modem interaction including transmission, reception, and carrier configuration.
- **Protocol Layer** - Manages message framing, fragmentation, and reassembly on top of the data layer.

## Building

This project uses the [nRF Connect SDK](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html) build system. Helper scripts are provided to automate setup and building.

```sh
./scripts/init_project.sh  # Install dependencies and initialize the project
./scripts/build.sh          # Build the firmware
./scripts/flash.sh          # Flash the firmware to the device
```

## License

MIT License. See [LICENSE](LICENSE) for details.
