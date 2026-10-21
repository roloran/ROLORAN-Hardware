# AGENTS.md

## Architecture

Architecture documentation lives in `docs/architecture/`.

When modifying component boundaries, interfaces, protocols,
or dependencies, consult the relevant architecture documentation.

Do not silently deviate from the documented architecture.
If implementation requires an architectural change, point this
out before implementing it.

## Build and validation

- Build with `pio run`.
- Treat compiler warnings as issues to investigate.
- When tests are added, document and run their command here.
- Changes involving hardware behavior should state what requires validation on a physical Heltec WiFi LoRa 32 V4 device.

## Project structure

- Public headers live in `include/`.
- Firmware implementation lives in `src/`.
- Bundled third-party or locally maintained libraries live in `lib/`.
- Documentation lives in `docs/`.
- Do not modify generated artifacts when their source is available.

## Hardware and compatibility

- The target is the Heltec WiFi LoRa 32 V4, currently configured through the PlatformIO V3 board definition with an ESP32-S3 override.
- Preserve pin assignments, radio configuration, serial protocol compatibility, and provisioning behavior unless a change is explicitly requested.
- Highlight changes that affect wiring, power management, persistent settings, radio behavior, or provisioning.

## Validation

- Run `pio run` after firmware changes.
- Clearly identify behavior that could not be tested without physical hardware.
