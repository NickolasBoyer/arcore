
#include "Platform.h"
#include "USBAPI.h"
#include "USBDesc.h"
#include "MIDIUSB.h"
#include <avr/wdt.h>

#if defined(USBCON)
#ifdef MIDI_ENABLED

#define WEAK __attribute__ ((weak))

#define CLASS_AUDIO 0x01
#define SUBCLASS_AUDIO_CONTROL 0x01
#define SUBCLASS_MIDISTREAMING 0x03

#define TYPE_CS_INTERFACE 0x24
#define SUBTYPE_HEADER 0x01
#define SUBTYPE_MIDI_IN_JACK 0x02
#define SUBTYPE_MIDI_OUT_JACK 0x03
#define JACK_EMBEDDED 0x01
#define JACK_EXTERNAL 0x02

#define JACK1 0x01
#define JACK2 0x02
#define JACK3 0x03
#define JACK4 0x04



void watchdogReset() {
    // Reset after 120ms
    *(uint16_t *)0x0800 = 0x7777;
    wdt_enable(WDTO_120MS);
}


extern const ACDescriptor _acInterface PROGMEM;
const ACDescriptor _acInterface =
{
	D_INTERFACE(AC_INTERFACE,0,CLASS_AUDIO,SUBCLASS_AUDIO_CONTROL,0),
	{9, TYPE_CS_INTERFACE, SUBTYPE_HEADER, 0x0100, 9, 0x01, MIDI_INTERFACE}
};

int WEAK AC_GetInterface(u8* interfaceNum)
{
	interfaceNum[0] += 1;	// uses 1
	return USB_SendControl(TRANSFER_PGM,&_acInterface,sizeof(_acInterface));
}

extern const MIDIDescriptor _midiInterface PROGMEM;
const MIDIDescriptor _midiInterface =
{
	D_INTERFACE(MIDI_INTERFACE,MIDI_ENDPOINT_COUNT,CLASS_AUDIO,SUBCLASS_MIDISTREAMING,0),
	{7, TYPE_CS_INTERFACE, SUBTYPE_HEADER, 0x0100, 34},
	{6, TYPE_CS_INTERFACE, SUBTYPE_MIDI_IN_JACK, JACK_EMBEDDED, JACK1, 0x00},
	{9, TYPE_CS_INTERFACE, SUBTYPE_MIDI_OUT_JACK, JACK_EMBEDDED, JACK2, 1, JACK3, 1, 0x00},
    {6, TYPE_CS_INTERFACE, SUBTYPE_MIDI_IN_JACK, JACK_EXTERNAL, JACK3, 0x00},
    {9, TYPE_CS_INTERFACE, SUBTYPE_MIDI_OUT_JACK, JACK_EXTERNAL, JACK4, 1, JACK1, 1, 0x00},
    {
        D_ENDPOINT(USB_ENDPOINT_OUT(MIDI_ENDPOINT_OUT), USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
        { 5, 0x25, 0x01, 0x01, JACK1 }
    },
    { 
        D_ENDPOINT(USB_ENDPOINT_IN(MIDI_ENDPOINT_IN), USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
        { 5, 0x25, 0x01, 0x01, JACK2 }
    }
};


int WEAK MIDI_GetInterface(u8* interfaceNum)
{
	interfaceNum[0] += 1;	// uses 1
	return USB_SendControl(TRANSFER_PGM,&_midiInterface,sizeof(_midiInterface));
}

bool WEAK MIDI_Setup(Setup& setup)
{
    u8 r = setup.bRequest;
    u8 requestType = setup.bmRequestType;
    
    return false;
}

const MIDIEvent MIDI_EVENT_NONE = {0, 0, 0, 0};


void MIDIUSB_::accept(void) 
{
    midi_buffer *buffer = &(this->_rx_buffer);
    int i = (unsigned int)(buffer->head+1) % MIDI_BUFFER_SIZE;
    
    // if we should be storing the received character into the location
    // just before the tail (meaning that the head would advance to the
    // current location of the tail), we're about to overflow the buffer
    // and so we don't write the character or advance the head.

    // while we have room to store a byte
    while (i != buffer->tail) {
        int c = USB_Recv(MIDI_RX);
        if (c == -1)
            break;  // no more data
        buffer->buffer[buffer->head] = c;
        buffer->head = i;

        i = (unsigned int)(buffer->head+1) % MIDI_BUFFER_SIZE;
    }
}

int MIDIUSB_::available(void)
{
    midi_buffer *buffer = &(this->_rx_buffer);
    return ((unsigned int)(MIDI_BUFFER_SIZE + buffer->head - buffer->tail) % MIDI_BUFFER_SIZE) / 4;
}

MIDIEvent MIDIUSB_::peek(void)
{
    if(this->available() < 1) {
        return MIDI_EVENT_NONE;
    } else {
        midi_buffer *buffer = &(this->_rx_buffer);
        MIDIEvent result;
        result.type = buffer->buffer[buffer->tail];
        result.m1 = buffer->buffer[(buffer->tail+1) % MIDI_BUFFER_SIZE];
        result.m2 = buffer->buffer[(buffer->tail+2) % MIDI_BUFFER_SIZE];
        result.m3 = buffer->buffer[(buffer->tail+3) % MIDI_BUFFER_SIZE];

        // This should perhaps be in accept(), but the check is must simpler / faster here.
        // It does require the user sketch to read MIDI events though.
        if(result.type == 0x07 && result.m1 == 0xF0 && result.m3 == 0xF7 && result.m2 == 0x01) {
            watchdogReset();
        }
        return result;
    }
}

MIDIEvent MIDIUSB_::read(void)
{
    MIDIEvent event = this->peek();
    midi_buffer *buffer = &(this->_rx_buffer);
    buffer->tail = (unsigned int)(buffer->tail + 4) % MIDI_BUFFER_SIZE;
    return event;
}

void MIDIUSB_::flush(void)
{
    USB_Flush(MIDI_TX);
}

size_t MIDIUSB_::write(MIDIEvent c)
{
    // TODO: only try to send bytes if the connection is open
    if(USB_Ready(MIDI_TX)) {
        int r = USB_Send(MIDI_TX,&c,4);
        if (r > 0) {
            return r;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

void MIDIUSB_::sendNoteOn(byte pitch, byte velocity, byte channel) {

  MIDIEvent noteOn = {0x09, 0x90 | channel, pitch, velocity};
  write(noteOn);
  USB_Flush(MIDI_TX);
}

void MIDIUSB_::sendNoteOff(byte pitch, byte velocity, byte channel) {

  MIDIEvent noteOff = {0x08, 0x80 | channel, pitch, velocity};
  write(noteOff);
  USB_Flush(MIDI_TX);
}

void MIDIUSB_::sendControlChange(byte control, byte value, byte channel) {
  MIDIEvent event = {0x0B, 0xB0 | channel, control, value};
  write(event);
  USB_Flush(MIDI_TX);
}

void MIDIUSB_::sendPolyPressure(byte control, byte value, byte channel) {
  MIDIEvent event = {0x0A, 0xA0 | channel, control, value};
  write(event);
  USB_Flush(MIDI_TX);
}

void MIDIUSB_::sendProgramChange(byte program, byte channel) {
  MIDIEvent event = {0x0C, 0xC0 | channel, program, 0};
  write(event);
  USB_Flush(MIDI_TX);
}

void MIDIUSB_::sendAfterTouch(byte pressure, byte channel) {
  MIDIEvent event = {0x0D, 0xD0 | channel, pressure, 0};
  write(event);
  USB_Flush(MIDI_TX);
}
void MIDIUSB_::sendPitchBend(byte value, byte channel) { // Not working right now third byte must contain half of value MSB first
  MIDIEvent event = {0x0E, 0xE0 | channel, value, (value >> 7)};
  write(event);
  USB_Flush(MIDI_TX);
}
//Stolen from PJRC.COM
void MIDIUSB_::sendSysEx(uint8_t length, const uint8_t *data) {
  // TODO: MIDI 2.5 lib automatically adds start and stop bytes
  while (length > 3) {
    MIDIEvent event ={0x04, data[0], data[1], data[2]};
    write(event);
    data += 3;
    length -= 3;
  }
  if (length == 3) {
    MIDIEvent event ={0x07, data[0], data[1], data[2]};
    write(event);
  } else if (length == 2) {
    MIDIEvent event ={0x06, data[0], data[1], 0};
    write(event);
  } else if (length == 1) {
    MIDIEvent event ={0x05, data[0], 0, 0};
    write(event);
  }
}

void MIDIUSB_::midi_loop(){
  while(available() > 0) {  // Repeat while notes are available to read.
      MIDIEvent e;
      e = read();
      if(e.type == 0x09){
         if (handleNoteOff) (*handleNoteOff)((e.m1-143), e.m2, e.m3);
      }
      if(e.type == 0x08){ 
         if (handleNoteOn) (*handleNoteOn)((e.m1-127), e.m2, e.m3);
      }
      if(e.type == 0x0B){
         if (handleControlChange) (*handleControlChange)((e.m1-175), e.m2, e.m3);
      }
      write(e);
      flush();
   }

}


// This operator is a convenient way for a sketch to check whether the
// port has actually been configured and opened by the host (as opposed
// to just being connected to the host).  It can be used, for example, in 
// setup() before printing to ensure that an application on the host is
// actually ready to receive and display the data.
MIDIUSB_::operator bool() {
    return USB_Ready(MIDI_TX);
}

MIDIUSB_ usbMIDI;


#endif
#endif
