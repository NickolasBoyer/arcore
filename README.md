# Arduino hardware cores

## Introduction

This is a fork of the core Arduino core hardware libraries.

The goal is to have a simple base to customize the core libraries to add or modify core functionality, without
needing to clone the entire Arduino repository.

Currently the only addition is support for MIDI-USB for the Arduino Leonardo and similar boards, but this should
serve as a good base for other modifications to the cores libraries. Let me know if you do anything interesting with this!

This fork^ has been forked by me and i've added support for various functions found in the teensy library, currently only all the output functions have been added but i will be providing the rest of the input functions (handles etc.) too shortly.

## Installation and Usage

Version 1.5.4 (or greater) of the Arduino IDE is required to build projects with this library.

1. Clone this repository.
2. On Linux, run `./install.sh`. The script creates a symlink in ~/Arduino/hardware to the hardware folder in repository. On Windows or Mac you have
   to do this manually (you may copy the hardware folder there instead of creating a symlink).
3. Launch the Arduino IDE (only tested on 1.5.7, will not work pre-1.5.x).
4. Under Tools -> Board, select Arduino Leonardo (arcore).
5. Upload your sketch as usual.


### Update 2014-12-18

The folder structure has changed. If you previously used this project, you will have
to run `./install.sh` again.

## MIDI-USB support

This fork currently contains a basic version of USB-MIDI support. It it still under development, and the API should be considered unstable.

The implementation currently has a single IN and a single OUT endpoint. It can be used together with CDC and HID (as a composite USB device),
 or standalone (by modifying USBDesc.h).

Currently there are no high-level API's. However, the MIDI event format is very simple to read and write (see examples below).
For more info on the MIDI-USB event format, see the official [USB-MIDI specification][2].


### Send a note-on and note-off every two seconds

```cpp
// First parameter is the note number (48 = middle C).
// Second parameter is the velocity (64 = normal, 127 = fastest).
// Third parameter is the Channel (0 = 1)
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
void loop() {
  usbMIDI.sendNoteOn(48, 64, 0);   // Channel 0, middle C, normal velocity
  delay(500);

  usbMIDI.sendNoteOff(48, 64, 0);   // Channel 0, middle C, normal velocity
  delay(1500);
  
  usbMIDI.sendControlChange(10, 65, 0); // Set the value of controller 10 on channel 0 to 65
}

void setup() {

}
```

### Read and echo MIDI messages back to the PC (old not working like this anymore)

```cpp
void loop() {
   while(MIDIUSB.available() > 0) {  // Repeat while notes are available to read.
      MIDIEvent e;
      e = MIDIUSB.read();
      if(e.type == 0x09 || e.type == 0x08) { // Only echo note-on and note-off
         MIDIEvent response;
         response.type = e.type; // Same event type (note-on/note-off)
         response.m1 = e.m1;     // Same event type and channel
         response.m2 = (e.m2 + 12) % 128;  // Shift by one octave
         response.m3 = e.m3;     // Same velocity
      }
      MIDIUSB.write(e);
      MIDIUSB.flush();
   }
}

void setup() {

}
```


## Supported Boards

This is only officially tested on the Arduino Leonardo. Users have also reported it working on the Arduino Micro and Freeduino Leonardo.

Supporting more boards should be as simple as copying the configuration over from the official boards.txt to the one in this repository,
 with minor changes to build.core and build.variant.

ARM (sam) boards might require more work.

## Host support

This should work out-of-the-box on recent versions of Linux and Mac OS.

On Windows 7 or later, it might be required to install the Arduino drivers, especially when using it in compound USB mode.

Windows XP does not support compound devices, but should still work as a plain MIDI device (might also need drivers). Compound mode can be disabled by editing USBDesc.h.

Users have reported success with iPads. The main requirement here is that the device should not request a high current (as it does by default). A different board configuration, `Arduino Leonardo (arcore, iPad compatible)` is provided for this.


 [1]: https://github.com/arduino/Arduino/tree/ide-1.5.x/hardware/arduino/avr/cores
 [2]: http://www.usb.org/developers/docs/devclass_docs/midi10.pdf
