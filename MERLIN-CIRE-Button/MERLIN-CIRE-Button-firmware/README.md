# First prototype demonstrator for "Roter Knopf in Neuhaus"

The "Roter Knopf" (or "Big Red Button (BRB)") is behaves like a mobile MERLIN-Messenger and has only a single task:
fire an RDCP-CIRE if the red button is pressed.
It is meant to be installed inside/close to a MERLIN-Base and indicate a severe emergency at that location.

## Background 

- Based on current RDCP-Modem implementation 
- Pulls in some dependencies from ROLODECK implementation 
- Mostly hardcoded RDCP device configuration for demonstration purposes only

## Building 

- Made for Heltec LoRa32 v3.2 devices
- see src/rdcp-modem-plugin.cpp for initscript and hardware wiring 
- flash firmware, prepare initscript, restart; then it's ready to use

## Provisioning

The MERLIN HQ software explicitly supports provisioning BRBs via their serial interface.
Alternatively manual provisioning can be done like this:
~~~
+SET RADIO NUM 1
+SET CHANNEL NUM 2
+SET CHANNEL LORA 00 869.525 125 7 5 12 22 15 1 2
+SET CHANNEL LORA 01 869.525 125 7 5 12 22 15 1 2
+SET RADIO SX1262 0 0 1 11 10 09 08 14 13 12 -1 -1 0
+SET RADIO INTERFACE 0 0 0 1 0 -1 -1 -1 -1
+SET RDCP ADDR C018
+SET RDCP ENTRYPOINT 0200
+SET RDCP HQAESKEY <XXX>
+SET RDCP HQPUBKEY <XXX>
+RADIOINIT
+SLEEP WAKEUPPIN 7
~~~

Remember to replace <XXX> with real cryptomaterial obtained for your environment.

## TODOs for contributors

- ~~Decide about used components (e.g., display) and workflow from users' perspective~~ 
- ~~Proper configuration of settings (entry point, crypto material, ...) via serial commands, as in: not hard-coded~~ 
- ~~Handle these devices in MERLIN HQ software properly (not provisioned like ROLODECKs)~~
- Build proper hardware and housing 
- Specify and implement interfaces on the MERLIN-Basis side (e.g., show status on E-Ink display)
- ~~Implement energy saving mechanisms (e.g., light or deep sleep with wake-up on DIO1 and button press; might have to use different pins than first prototype)~~
- Better integrate the dependencies (i.e., ROLODECK/RDCP-Relay crypto libraries; RDCP v0.4 message crafting/parsing) into RDCP-Modem

Difficulty rating: Suitable for first-time contributors

## Commands for use over Serial/UART

The supported commands for the BRB (Big-Red-Button) correspond mostly to those of the RDCP-Modem (on whose code this is built).
This project does however, also provide a few additional commands.

### Melody Subcommands
Some HW configurations of the BRB may have the ability to play sounds through a passive buzzer.
Passive buzzers can only produce beep tones, however, through chaining it is possible to play little melodies.
The buzzer has a fixed number of melodies, designated for different purpose, e.g., to signal a "success".
All melodies can have at most `MELODY_LEN` tones (currently 8).
Default melodies are baked into the code and can be interacted with through the following commands.

- `MELODY SETTONE a b c d e f` Defines the tone `b` in melody `a`. The tone is set to frequency `c` in Hz, plays for `d` milliseconds and is silent afterward for `e` milliseconds. Example: `MELODY MODIFY 3 4 1000 120 50`

- `MELODY PLAY a` plays melody `a` once. Example: `MELODY PLAY 0`

- `MELODY TEST` plays a sequence of tones with increasing frequency. Useful to estimate resonance frequency of the buzzer (expected at roughly 2000 Hz). Example: `MELODY TEST`

- `MELODY VOLUME a` sets the volume (specifically the duty cycle of the PWM) for all tones. Values between `0` (mute) and `128` (full volume) are possible. Beware, its not a linear scale. Example: `MELODY VOLUME 64`
