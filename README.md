# ROLORAN Hardware

In the spirit of open source hardware (OSH), this repository contains the
electronic, mechanical, and embedded-software design files for hardware used
by ROLORAN, with a particular focus on the MERLIN infrastructure.

## Repository contents

- [`MERLIN-Circuit-Board/`](MERLIN-Circuit-Board/) contains the KiCad
  schematics and PCB layouts for the `LoRaDualv1_1` dual-radio board and the
  `LoRaQuadV2` quad-radio board.
- [`MERLIN-Base/`](MERLIN-Base/) contains FreeCAD sources and printable STL
  parts for housing electronics and mounting the `LoRaQuadV2` board inside a
  MERLIN Base.
- [`RDCP-Modem/`](RDCP-Modem/) contains a printable three-piece enclosure for
  an RDCP modem based on the `LoRaQuadV2` board.
- [`MERLIN-CIRE-Button/`](MERLIN-CIRE-Button/) contains the hardware model,
  firmware, and assembly documentation for the dedicated RDCP emergency
  button, also known as the Big Red Button.
- [`MERLIN-Assist/`](MERLIN-Assist/) contains the enclosure and firmware for
  a three-button assisted-living prototype that can send emergency, OK, and
  test reports through MERLIN.

Each directory has its own README with further information about the relevant
designs, components, build process, or usage.

## Tools and file formats

The repository includes:

- KiCad projects for schematics and PCB layouts
- FreeCAD source models and STL files for mechanical parts
- PlatformIO/Arduino firmware for ESP32-based Heltec LoRa boards
- Typst and PDF assembly documentation for the MERLIN CIRE Button

These projects include prototypes and installation-specific designs. Review
and validate the electrical, mechanical, radio, and safety requirements before
manufacturing or deploying any hardware. In particular, MERLIN Assist is a
proof of concept and is not certified for medical or safety-critical use.

## License

This repository is licensed under the
[GNU General Public License version 2](LICENSE).
