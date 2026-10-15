# MERLIN Circuit Boards

This directory contains the KiCad design sources for LoRa circuit boards used
by the MERLIN infrastructure. The designs combine controller hardware with
multiple LoRa transceiver modules and expose the radio connections through
SMA connectors.

## Board designs

Each design consists of a KiCad project (`.kicad_pro`), schematic
(`.kicad_sch`), and PCB layout (`.kicad_pcb`). Open the corresponding
`.kicad_pro` file to work with a design.

- **LoRaDualv1_1** is a dual-radio design built around ESP32 development
  boards and E22 LoRa modules. Its schematic was created with KiCad 8.
- **LoRaQuadV2** is a quad-radio design with additional controller, power,
  monitoring, and interface circuitry. Its schematic was created with
  KiCad 9.

The `LoRaQuadV2` board is used by other hardware in this repository,
including the printable enclosures in [`../MERLIN-Base/`](../MERLIN-Base/)
and [`../RDCP-Modem/`](../RDCP-Modem/).

## Manufacturing

The checked-in files are editable design sources. Before manufacturing a
board, review the schematic, PCB layout, footprints, design rules, and the
requirements of the selected fabricator. Generate production artifacts such
as Gerber files, drill files, bills of materials, and placement files from
the selected KiCad project as needed.
