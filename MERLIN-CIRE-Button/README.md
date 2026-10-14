# MERLIN CIRE Button

The MERLIN CIRE Button is a dedicated emergency-reporting device for the
MERLIN network. It is also referred to as the RDCP Emergency Button, the
"Big Red Button" (BRB), or the "Roter Knopf". Pressing the button sends a
predefined RDCP `CITIZEN REPORT` (CIRE) over LoRa, allowing a user to report
an emergency without composing a message on a MERLIN Base or MERLIN
Messenger.

The device is intended to be installed in or near a MERLIN Base. Its firmware
is derived from the RDCP-Modem implementation and provides visual and audible
feedback while the report is transmitted and acknowledged.

## Repository contents

- [`MERLIN-CIRE-Button-hardware/`](MERLIN-CIRE-Button-hardware/) contains the
  FreeCAD model for the printable internal inlays and component mounts.
- [`MERLIN-CIRE-Button-firmware/`](MERLIN-CIRE-Button-firmware/) contains the
  PlatformIO firmware project, provisioning information, and serial command
  reference.

## Documentation

For firmware setup, configuration, provisioning, and device-specific serial
commands, see the
[firmware README](MERLIN-CIRE-Button-firmware/README.md).

For the required components, 3D-printing guidance, electrical assembly,
flashing, provisioning, and usage instructions, see the
[RDCP Emergency Button manual](MERLIN-CIRE-Button-firmware/docs/manual.pdf).
The manual's editable Typst source is available in
[`docs/manual.typ`](MERLIN-CIRE-Button-firmware/docs/manual.typ).

The design is open and may be adapted to other enclosures or component
layouts. Such changes may require corresponding adjustments to the printable
parts, wiring, and firmware pin assignments.
