#import "@preview/abbr:0.3.1"

#abbr.make(
  ("RDCP", "ROLORAN Disaster Communication Protocol"),
  ("MERLIN", "Messaging-System mit regionaler LoRa-Infrastruktur"),
  ("CIRE", "CITIZEN REPORT"),
)

#set heading(numbering: "1.")
#let heading = [#text(size: 20pt)[RDCP Emergency Button Manual]]

#set page(
  paper: "a4",
  margin: (top: 30mm, bottom: 25mm),
  header: place(dx: 0%, dy: 30%)[#text(fill: white)[#heading]],
  background: image("resources/background.svg", fit: "cover"),
  footer: context [
    #set align(right)
    #counter(page).display("1/1", both: true)
  ]
)


#let unibw_medium_gray = rgb("dadada")
#let unibw_light_gray = rgb("e3e3e3")
#let unibw_dark_gray = rgb("9d9d9c")
#let unibw_txt_gray = rgb("646363")
#let unibw_orange = rgb("ed6e00")

#[
  #page(
    header: place(dx: 0%, dy: 30%)[#text(fill: white)[#heading]],
    background: image("resources/background.svg", fit: "cover"),
  )[
    #v(5%)
    #set align(center)
    #text(size: 5em)[RDCP Emergency Button Manual]

    #image("images/closed-button.jpg", width: 90%)
  ]
  #pagebreak()
]

#set par(
  justify: true
)

#outline()

#abbr.list(columns: 1)

= Introduction
This manual is meant as a companion document to the existing documentation of the #abbr.a[RDCP] Emergency Button.
The Emergency Button is part of the #abbr.a[MERLIN].

The Emergency Button is essentially a #abbr.a[MERLIN]-Messenger with only the capability to send one specific message type: the #abbr.a[CIRE].
If pressed, it wirelessly sends out a predefined emergency report to be transported via #abbr.a[MERLIN].
It is meant to be used when writing a fully fledged report via a #abbr.a[MERLIN]-Base or #abbr.a[MERLIN]-Messenger would take to much time or if they cannot be physically operated anymore.

= Build Instructions
Following the open architecture of the #abbr.a[MERLIN] project, you can build your own RDCP Emergency Button.
The build instructions allow you to replicate the buttons as used by the project.
You are, however, free to adapt the project to your own needs, e.g., by using a different case.
In this case you may need to adapt the following steps.

== Required Parts and Tools

You will need the following parts:
#table(
  columns: (auto, auto, auto),
  table.header([Component],[Quantity],[Info]),
  [Button Case], [1], [The project uses specific mushroom push buttons#footnote[The concrete product used for the prototypes and featured in the images: https://intratec-shop.de/grobhandtaster-pilztaster-notaus-ip65-pilzschalter-notausschalter-schalter-grobhandtaster-druck/JSSL5103] for which the 3D models were optimized.],
  [Heltec V4], [1], [Microcontroller with integrated LoRa radio. Including appropriate antenna.],
  [XY-3606], [1], [Stepdown converter for converting 12V received via DC jack to 5V.],
  [Passive Piezo Buzzer], [1], [Used for audio feedback],
  [USB-C to 4 Wire Jack], [1], [Jack to connect USB pins of Heltec V4 to.],
  [DC5521 Jack], [1], [Jack for 12V main power supply.],
  [M3 x 8 Screws], [2], [For securing the inlay to the case.],
  [Filament], [n/a], [Filament of your choosing to print the inlay. The project used PLA.],
  [Electric wire (ideally 24 AWG)], [n/a], [Cabling],
  [Terminal Block], [2], [Optional: Can be used as shared mass/5V]
)

You will also need access the following tools:

#table(
  columns: (auto, auto),
  table.header([Tool],[Info]),
  [3D Printer], [To print the inlay parts with your filament.],
  [Screwdriver Set], [Case screws, M3 screws, XY-3606 screws, etc.],
  [Soldering Station], [For soldering cables, etc.],
  [Step Drill (10 - 14mm)], [Drilling holes into the case for the two jacks.]
)

== 3D Printing Parts
There are four parts the need to be printed.

#grid(
  columns: (auto, auto, auto, auto),
  gutter: 2em,
  figure(
    image("images/ground-plate.jpg", height: 3cm),
    caption: [Ground plate]
  ),
  figure(
    image("images/inlay.jpg", height: 3cm),
    caption: [Inlay]
  ),
  figure(
    image("images/clamp-1.jpg", height: 3cm),
    caption: [Clamp 1]
  ),
  figure(
    image("images/clamp-2.jpg", height: 3cm),
    caption: [Clamp 2]
  )
)

Ground plate and inlay can be printed without supporting structures, while clamp 1 and clamp 2 are best printed with support enforcers only.
The pins on the clamps likely require a little amount filing to get them to smoothly fit into the receptacles of the inlay.
They do not need to stick to the receptacles by themselves, as they will be held in place by the lid of the button.
If building more than one button its generally recommended to print each part multiple time in parallel.
Parts can be combined as long as they use the same plastic and print settings (e.g. printing clamp 1 and 2 together is generally fine).
These four parts (ground plate, inlay, clamp 1 and 2) will later be assembled as shown in the 3D render.

#grid(
  columns: (1fr, 1fr),
  figure(
    image("images/inlay-grid.jpg")
  ),
  figure(
    image("images/assembled-3d-small.png")
  )
)


== Assembly Steps

+ Unscrew the lid and open the button.

  #grid(
    columns: (auto, auto),
    align: (center, center),
    gutter: 1em,
    image("images/closed-button.jpg", height: 4cm),
    image("images/opened-button.jpg", height: 4cm),
  )

+ Identify the two points for the holes of the jacks by ensuring that the text on the backside of the button is upright.
  The two holes shall be made on the top and left insets when the opening of the case is facing away from you.
  Use a step drill to create a hole for the USB-C to 4 Wire Jack on the top and a hole for the DC5521 Jack on the left side.
  Note that the jacks may require holes of slightly different diameters.

  #grid(
    columns: (auto, auto, auto),
    rows: (4cm),
    gutter: 1em,
    image("images/backside-orientation-small.jpg"),
    image("images/screw.jpg"),
    image("images/interior-1.jpg"),
  )

  After creating the necessary holes you may insert first the ground plate and afterward the inlay.
  Fixate the inlay with two screws.
  Afterward, insert the outlets through the new holes.

+ Perform the electrical wiring according to the following schematic.

  #[
    #set align(center)
    #image("images/button-interior.svg", width: 80%)
  ]

  Instead of connecting all mass to the XY-3606 step down converter you may alternatively use a terminal block, same for the two 5V connections.

  Its recommended to first prepare and solder the step down converter and the Heltec V4 separately.
  #grid(
    columns: (auto, auto),
    rows: (4cm),
    gutter: 1em,
    image("images/step-down-conv.jpg"),
    image("images/wired-esp.jpg"),
  )

  After preparing both components, insert them into the respective insets of the inlay.
  Connect all necessary wires.
  Finally, secure the two components via the 3D-printed clamps.
  #grid(
    columns: (auto, auto),
    rows: (4cm),
    gutter: 1em,
    image("images/electrics.jpg"),
    image("images/interior-2.jpg"),
  )

  The last two remaining wires should be connected to the switch on the lid.
  By screwing down the lid, the clamps are at the same time pushed down by lid and are thus firmly held in place.

= Usage

This section explains how an assembled unit may be utilized.

#box(fill: unibw_orange, inset: 1em, radius: (1em))[
  #set text(fill: white)

  #underline([Warning])

  The Heltec V4 board supports power supply via either 5V pins or USB#footnote[https://resource.heltec.cn/download/WiFi_LoRa_32_V4/datasheet/WiFi_LoRa_32_V4.2.0.pdf].
  The button exposes the 5V pins via the DC5521 jack and the USB via the USB-C jack.
  Hence, the same restrictions apply to these two jacks.
]

== Flashing
Flashing is performed via the USB-C port.
Note that while appearing as a physical USB-C port, the 4 wire variant used does not actually constitute a valid USB-C interface but is rather a legacy interface.
Using USB-C to USB-C cables will thus fail.
Instead, utilize a USB-C to USB-A cable and connect the USB-A end to a computer.

The firmware for the button is build and distributed as a PlatformIO project.
Assuming that PlatformIO is already installed and no other USB CDC devices are connected you may issue the following command inside the root directory of this project:
```
pio run -e heltec_wifi_lora_32_V4 --target upload --upload-port /dev/ttyACM0
```
This command builds and subsequently flashes the current codebase.
You may checkout any commit/branch if you require a specific firmware version.

Sometimes Heltec V4 do not allow flashing.
In this case you need to access the microcontroller and physically hold the PROG button pressed while triggering a reboot through a short press on RST.
After performing this once it should be flashable afterwards.



== Provisioning
Provisioning is performed via the USB-C port.
See the restrictions from the previous section.

The easiest way is to use an operational #abbr.a[MERLIN]-HQ.
The #abbr.a[MERLIN]-HQ has a dedicated provisioning setting for emergency buttons.
It can also provide the correct crypto material for the given infrastructure.

For manual provisioning, e.g., during testing you may utilize the ROLORAN-Terminal application to connect to the button.
Then copy the following initscript while replacing any placeholders `XXXX` with the concrete values.

```
+SET RADIO NUM 1
+SET CHANNEL NUM 2
+SET CHANNEL LORA 00 869.525 125 7 5 12 22 15 1 2
+SET CHANNEL LORA 01 869.525 125 7 5 12 22 15 1 2
+SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 0
+SET RADIO INTERFACE 0 0 0 1 0 -1 -1 -1 -1
+SET RDCP ADDR XXXX
+SET RDCP ENTRYPOINT XXXX
+SET RDCP HQAESKEY XXX
+SET RDCP HQPUBKEY XXX
+RADIOINIT
+SLEEP WAKEUPPIN 7

```


== Installation
Installation just requires the connection of a 12V DC5521 plug to the corresponding jack on the button.
The button should indicate successful boot through a short beeping sequence.
Allow 5 seconds after boot for finalizing its setup.
Now the button is ready and may be pressed anytime to fire an #abbr.a[RDCP] #abbr.a[CIRE] message via the configured #abbr.a[MERLIN]-Base.

The pictures below show the installation in a #abbr.a[MERLIN]-Base in Neuhaus.
#grid(
  columns: (auto, auto),
  gutter: 2em,
  image("images/merlin-base-open.jpg"),
  image("images/merlin-base-closed.jpg"),
)

#set align(bottom)
#box(fill: unibw_light_gray, radius: 1em, inset: 2em)[
  = Disclaimer and Contact
  The ROLORAN project of which MERLIN is a part of is financed by the European Union as part of the NextGenerationEU program.

  All contents of this document are governed by the License which can be found in the project repository.

  Contact information:
  #grid(
    columns: (auto, auto),
    gutter: 1em,
    [Website], [#link("https://go.unibw.de/roloran")[https://go.unibw.de/roloran]],
    [Mail], [#link("mailto:projekt-roloran@unibw.de")[projekt-roloran\@unibw.de]]
  )
]
