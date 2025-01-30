/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com

 */

#ifndef CONTROLLER_PINS_H
#define CONTROLLER_PINS_H

#include "controller_identification.h"


#ifdef IS_TEENSY
#define MAX_ANALOGPINS 10

#elif defined(IS_ARDUINO_UNO)
#define MAX_ANALOGPINS 6

#else
#error "not recognized as a supported board"

#endif

#ifndef LED
#define LED 13
#endif

#ifndef SYNCPIN
#define SYNCPIN 0
#endif


#ifndef SYNCPIN
#define SYNCPIN 0
#endif

#ifndef BUSYPIN
#define BUSYPIN 1
#endif

#ifndef TRIGGERPIN
#define TRIGGERPIN 2
#endif

#ifndef SPAREPIN
#define SPAREPIN 3
#endif

#ifndef CNVSTPIN
#define CNVSTPIN 10
#endif

#ifndef CSPIN
#define CSPIN 10
#endif

void reboot();

#define MAXPINS 24
#define ANALOGINPUT 0xF
extern uint8_t pin_modes[MAXPINS];
extern uint8_t pin_states[MAXPINS];

extern uint8_t analog_pins[MAX_ANALOGPINS];
extern uint8_t analog_pins_selection[MAX_ANALOGPINS];
extern uint8_t analog_pins_nselected;
extern double analog_pins_voltages[MAX_ANALOGPINS];
extern unsigned int analog_pins_averages;
extern bool internal_adc_error;

#define INTERNAL_ADC_BITS 12
#define INTERNAL_ADC_RESOLUTION (3.3/4096.)

/* =====================================================
   Pins, these need to be constant to take advantage of
   digitalWriteFast etc.
   They are hard coded in the the .cpp file, because
   of the register level macros
*/
// External trigger, sync 
const int syncPin = SYNCPIN;         // On sample or shutter or start frame or start followed by delay - for desktop N2 laser
const int busyPin = BUSYPIN;         // Gate output pin, goes high during busy (or shutter)
const int interruptPin = TRIGGERPIN; // Trigger input pin
const int sparePin =  SPAREPIN;      // Spare pin for digital output

// ccd clock pins
const int fMPin        = 4;     // Master clock out for CCD's
const int fMPinMonitor = 5;     // Clock monitor

// TCD1304 pins
const int ICGPin       = 6;     // Integration clear gate
const int SHPin        = 7;     // Shift gate

// s11639-01 pins
const int STPin   =  6;         // start pin
const int TRGPin  = 7;          // trig to start conversion
const int EOSPin = 8;           // Spare pin to the sensor board

// SPI interface
const int CSPin = CSPIN;
const int SDIPin = 11;
const int SDOPin = 12;
const int CLKPin = 13;

// ADC specific pin
const int CNVSTPin = CNVSTPIN;

/* ===================================================
   States, not constant
 */

// Pin states
extern int intedgemode;
extern unsigned int busyPinState;
extern unsigned int syncPinState;
extern unsigned int sparePinState;

// Pin Toggle States
extern bool sync_toggled;
extern bool busy_toggled;
extern bool spare_toggled;

// Operational States
extern bool is_attached;
extern bool is_busy;

char *parseInterruptMode( char *s, uint8_t *mode );

char *parsePin(char *s, int *ipin);
char *parsePinU8(char *s, uint8_t *pin);

bool setPinStr(char *s);
bool writePinStr(char *s);
bool togglePinStr(char *s);
bool pulsePinStr( char *s );

void setPinMode(uint8_t pin, uint8_t mode);
void writePin(uint8_t pin, uint8_t istate);
void togglePin(uint8_t pin);
uint16_t readPin(uint8_t pin);

inline void attachInterruptPin( void (*isr)(), int edge );
inline void detachInterruptPin();

inline void toggleBusyPin( );
inline void toggleSyncPin( );
inline void toggleSparePin( );

void pulsePin( unsigned int pin, unsigned int usecs );
void pulsePinHigh( unsigned int pin, unsigned int usecs );
void pulsePinLow( unsigned int pin, unsigned int usecs );

#ifdef IS_TEENSY
inline uint16_t fastAnalogReadPin( uint8_t pin );
inline uint16_t fastAnalogReadChannel( uint8_t channel );
#endif

uint16_t analogReadPin(uint8_t pin);
uint16_t analogReadChannel(uint8_t channel);
double analogReadPinVoltage(uint8_t pin);
double analogReadChannelVoltage(uint8_t channel);
double analogReadPinVoltageAveraged(uint8_t pin, int navgs);
double analogReadChannelVoltageAveraged(uint8_t channel, int navgs);

bool selectAnalogChannels( unsigned int *channels, int nchannels );
double *analogReadSelected( int navgs);
void printAnalogSelected( int navgs );

#ifdef IS_TEENSY
void startSquareWave(uint8_t pin, float frequencyHz);
void stopSquareWave(uint8_t pin);

void startMasterClock(float frequencyHz);
void stopMasterClock( );
#endif

inline const char *boolstr(bool val);
inline const char *modestr( uint8_t mode );
inline const char *edgestr(int mode);
void printPinState_(uint8_t pin);
void printTogglePinState_(uint8_t pin, uint8_t extrastate);
void printPin( const uint8_t pin );
void printPins( );

void controller_pins_setup();

#endif
