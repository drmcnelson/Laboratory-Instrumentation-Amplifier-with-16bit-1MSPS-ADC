/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com

*/

#include "Arduino.h"

#include <limits.h>

#include <string.h>

#include "controllerpins.h"

#include "stringlib.h"


//#include <digitalWriteFast.h>
//#include <elapsedMillis.h>  // uncomment this if not Teensy

/* Note to self, use this for portability to Arduinos
   
   attachInterrupt( digitalPinToInterrupt( pin_number ), pin_ISR, CHANGE);

*/


// =============================================
#if defined(IS_TEENSY)

void reboot() {
  _reboot_Teensyduino_();
}

#include <digitalWriteFast.h>
#define DIGITALREAD(a) digitalReadFast(a)
#define DIGITALWRITE(a,b) digitalWriteFast(a,b)
#define DIGITALTOGGLE(a) digitalToggleFast(a)

#include <ADC.h>
#include <ADC_util.h>

ADC *adc = new ADC();

inline uint16_t fastAnalogReadPin( uint8_t pin )
{
  adc->adc0->startReadFast(pin);
  pin_modes[pin] = ANALOGINPUT; // use the conversion to record the mode
  while ( adc->adc0->isConverting() );
  return adc->adc0->readSingle();
}

inline uint16_t fastAnalogReadChannel( uint8_t channel )
{
  if (channel<MAX_ANALOGPINS) {
    return fastAnalogReadPin(analog_pins[channel]);
  }
  return 0xFFFF;
}

inline void fastAnalogReadSetup()
{
  uint16_t u;
  
  // Setup the onboard ADCs
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3); 
  adc->adc0->setAveraging(1);                 // set number of averages
  adc->adc0->setResolution(INTERNAL_ADC_BITS);               // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); 
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); 
  adc->adc0->wait_for_cal();  
  adc->adc0->singleMode();              // need this for the fast read

  // Setup the analog pin modes
  for (int n=0; n<MAX_ANALOGPINS; n++) {
    //pin_states[analog_pins[n]] = ANALOGINPUT;
    u = fastAnalogReadChannel(n);
    analog_pins_voltages[n] = u*INTERNAL_ADC_RESOLUTION;
  }

}

uint8_t analog_pins[MAX_ANALOGPINS] = { A0, A1, A2, A3, A4, A5, A6, A7, A8, A9 };
uint8_t analog_pins_selection[MAX_ANALOGPINS] = { A0, A1, A2, A3, A4, A5, A6, A7, A8, A9 };
uint8_t analog_pins_nselected = MAX_ANALOGPINS;
double analog_pins_voltages[MAX_ANALOGPINS] = { 0. };
unsigned int analog_pins_averages;
bool internal_adc_error = false;

// =============================================
#elif defined(IS_ARDUINO_UNO)

void(*_rebootFunc)(void)=0;

void reboot()
{
  _rebootFunc();
}

#define DIGITALREAD(a) digitalRead(a)
#define DIGITALWRITE(a,b) digitalWrite(a,b)
#define DIGITALTOGGLE(a) digitalWrite(a,!digitalRead(a))

uint8_t analog_pins[MAX_ANALOGPINS] = { A0, A1, A2, A3, A4, A5};
uint8_t analog_pins_selection[MAX_ANALOGPINS] = { A0, A1, A2, A3, A4, A5 };
uint8_t analog_pins_nselected = MAX_ANALOGPINS;
double analog_pins_voltages[MAX_ANALOGPINS] = { 0. };
unsigned int analog_pins_averages;
bool internal_adc_error = false;

#endif

// ==================================================================
uint8_t pin_modes[MAXPINS] = { 0xFF };
uint8_t pin_states[MAXPINS] = { 0xFF };

#define ISANALOGPIN(a) (((a) >= analog_pins[0]) && ((a) <= analog_pins[MAX_ANALOGPINS-1]))

// Pin states
int intedgemode = RISING;        // Default trigger mode
unsigned int busyPinState = LOW;
unsigned int syncPinState = HIGH;
unsigned int sparePinState = LOW;

// running toggled states
bool sync_toggled = false;
bool busy_toggled = false;
bool spare_toggled = false;

bool is_attached = false;
bool is_busy = false;

// -----------------------------------------------

inline void pinDelay( unsigned int usecs ) {
  if (usecs > 16000) {
    delay((float)usecs/1000.);
  }
  else if (usecs > 0) {
    delayMicroseconds(usecs);
  }  
}

char *parseHighLow( char *s, uint8_t *state )
{
  char *s1 = NULL;
  if ( (s1=startsWith(s, "lo")) || (s1=startsWith(s, "0"))) {
    *state = LOW;
  }
  else if ( (s1=startsWith(s, "hi")) ||  (s1=startsWith(s, "1"))) {
    *state = HIGH;
  }
  return s1;
}

char *parseInterruptMode( char *s, uint8_t *mode )
{
  char *ps;

  if (s && s[0]) {
    s = skipSpace(s);
    if (s && s[0]) {  
      if ( (ps=startsWith(s,"rising")) ) {
	*mode = RISING;
	return ps;
      }
      else if ((ps=startsWith(s,"falling"))) {
	*mode = FALLING;
	return ps;
      }
      else if ((ps=startsWith(s,"change"))) {
	*mode = CHANGE;
	return ps;
      }
    }
  }
  return NULL;
}

char *parsePin( char *s, int *ipin )
{
  unsigned int u;
  char *ps;

  if (s && s[0]) {
    s = skipSpace(s);
    if (s && s[0]) {  
      if ( (ps=startsWith(s,"busy")) ) {
	*ipin = busyPin;
	return ps;
      }
      else if ((ps=startsWith(s,"sync"))) {
	*ipin = syncPin;
	return ps;
      }
      else if ( (ps=startsWith(s,"spare"))) {
	*ipin = sparePin;
	return ps;
      }
      else if ( (ps=startsWith(s,"trig"))) {
	*ipin = interruptPin;
	return ps;
      }
      else if ((ps=parseUint(s,&u))&&(u<MAXPINS)) {
	*ipin = u;
	return ps;
      }
    }
  }
  return NULL;
}

char *parsePinU8(char *s, uint8_t *pin)
{
  int ipin = *pin;
  s = parsePin(s,&ipin);
  if (s) {
    *pin = ipin;
  }
  return s;
}

bool setPinStr( char *s )
{
  char *s1 = s;
  int ipin = 0;
  uint8_t state = LOW;
  float f = 0.;
  
  if ( (s=parsePin(s,&ipin)) ) {
    
    s = skipSpace(s);
    
    if (s[0]) {
      
      if ((s1 = startsWith(s, "out"))) {
	setPinMode( ipin, OUTPUT);	
      }
#ifdef IS_TEENSY
      else if ( (s1 = startsWith(s, "clock") ) && parseFlt(s1,&f)) {
        startSquareWave(ipin,f);
      }
      else if ( (s1 = startsWith(s, "noclock"))) {
        stopSquareWave(ipin);
      }
#endif
      else if ( (s1 = startsWith(s, "in") ) ) {
	setPinMode( ipin, INPUT);
      }
      else if ( (s1 = startsWith(s, "pullup")) ) {
	setPinMode( ipin, INPUT_PULLUP);
      }
      else if ( (s1 = startsWith(s, "analog")) ) {
	setPinMode( ipin, ANALOGINPUT);
      }
      else if ((s1=parseHighLow(s, &state))) {
	writePin(ipin,state);
      }
      else {
	return false;
      }
    }
    printPin(ipin);
    return true;
  }
  return false;
}

bool writePinStr( char *s )
{
  char *s1 = s;
  int ipin = 0;
  uint8_t state = LOW;

  if ( (s=parsePin(s,&ipin)) ) {
    s = skipSpace(s);
    if (s[0]) {
      if ((s1=parseHighLow(s, &state))) {
	writePin(ipin,state);
      }
      else {
	return false;
      }
    }
    printPin(ipin);
    return true;
  }
  return false;
}

bool pulsePinStr( char *s )
{
  char *s1 = s;
  int ipin = 0;
  unsigned int usecs = 0;

  if ((s=parsePin(s,&ipin))&&(s=parseUint(s,&usecs))) {
    if (s && s[0]) s = skipSpace(s);
    if (!s[0]) {
      pulsePin( ipin, usecs );
    }
    else if ( (s1 = startsWith(s, "lo")) || (s1 = startsWith(s, "0"))) {
      pulsePinLow( ipin, usecs );
    }
    else if ( (s1 = startsWith(s, "hi")) ||  (s1 = startsWith(s, "1"))) {
      pulsePinHigh( ipin, usecs );
    }
    else {
      return false;
    }
    printPin(ipin);
    return true;
  }
  return false;
}

bool togglePinStr( char *s )
{
  int ipin = 0;

  if ( (s=parsePin(s,&ipin)) ) {
    togglePin(ipin);
    printPin(ipin);
    return true;
  }
  return false;
}

// -----------------------------------------------		 
void setPinMode(uint8_t pin, uint8_t mode)
{
  if (pin < MAXPINS) {
    if (mode != ANALOGINPUT) {
      pinMode( pin, mode );
    }
    else {
      analogReadPin(pin);
    }
    pin_modes[pin] = mode;
  }  
}

void writePin(uint8_t pin, uint8_t istate)
{
  if (pin < MAXPINS) {
    DIGITALWRITE( pin, istate );
    pin_modes[pin] = OUTPUT;
    pin_states[pin] = istate;
  }  
}

void togglePin(uint8_t pin)
{
  if (pin < MAXPINS) {
    if (pin_modes[pin] == OUTPUT) {
      DIGITALTOGGLE( pin );
      pin_states[pin] = !pin_states[pin];
    }
    else {
      writePin(pin,HIGH);
    }
  }  
}

uint16_t readPin(uint8_t pin)
{
  if (pin < MAXPINS) {
    if (pin_modes[pin] == ANALOGINPUT) {
      return analogReadPin(pin);
    }
    else if (pin_modes[pin] == OUTPUT) {
      return pin_states[pin];
    }
    else {
      pin_states[pin] = DIGITALREAD(pin);
      return pin_states[pin];
    }
  }
  return 0xFFFF;
}

// -----------------------------------------------
inline void attachInterruptPin( void (*isr)(), int edge )
{
  is_attached = true;
  intedgemode = edge;
  switch (edge)
    {
    case RISING:
      attachInterrupt(digitalPinToInterrupt(interruptPin), isr, RISING);
      break;
    case FALLING:
      attachInterrupt(digitalPinToInterrupt(interruptPin), isr, FALLING);
      break;
    case CHANGE:
      attachInterrupt(digitalPinToInterrupt(interruptPin), isr, CHANGE);
      break;
    default:
      Serial.println("Error: interrupt edge not supported");
    }
}

inline void detachInterruptPin()
{
  is_attached = false;
  detachInterrupt(digitalPinToInterrupt(interruptPin));
}

// -----------------------------------------------
// Toggle specific pins
inline void toggleBusyPin( )
{
  DIGITALTOGGLE(busyPin);
  busy_toggled = !busy_toggled;
}

inline void toggleSyncPin( )
{
  DIGITALTOGGLE(syncPin);
  sync_toggled = !sync_toggled;
}

inline void toggleSparePin( )
{
  DIGITALTOGGLE(sparePin);
  spare_toggled = !spare_toggled;
}

// -----------------------------------------------
// Pulse pin agnostic to initial state
void pulsePin( unsigned int pin, unsigned int usecs )
{
  if (usecs <= 16000) {
    DIGITALTOGGLE(pin);
    delayMicroseconds(usecs);
    DIGITALTOGGLE(pin);
  }
  else {
    float msecs = (float) usecs/1000.;
    DIGITALTOGGLE(pin);
    delay(msecs);
    DIGITALTOGGLE(pin);    
  }
}

// -----------------------------------------------
// Pulse pin to specific state
void pulsePinHigh( unsigned int pin, unsigned int usecs )
{
  if (usecs <= 16000) {
    DIGITALWRITE(pin,HIGH);
    delayMicroseconds(usecs);
    DIGITALWRITE(pin,LOW);
  }
  else {
    float msecs = (float) usecs/1000.;
    DIGITALWRITE(pin,HIGH);
    delay(msecs);
    DIGITALWRITE(pin,LOW);
  }
  pin_states[pin] = LOW;
  pin_modes[pin] = OUTPUT;
}

void pulsePinLow( unsigned int pin, unsigned int usecs )
{
  if (usecs <= 16000) {
    DIGITALWRITE(pin,LOW);
    delayMicroseconds(usecs);
    DIGITALWRITE(pin,HIGH);
  }
  else {
    float msecs = (float) usecs/1000.;
    DIGITALWRITE(pin,LOW);
    delay(msecs);
    DIGITALWRITE(pin,HIGH);
  }
  pin_states[pin] = HIGH;
  pin_modes[pin] = OUTPUT;
}

// -----------------------------------------------

uint16_t analogReadPin(uint8_t pin)
{
  if (ISANALOGPIN(pin))
    {
      pin_modes[pin] = ANALOGINPUT;
      return analogReadPin(pin);
    }
  return 0xFFFF;
}

uint16_t analogReadChannel(uint8_t channel)
{
  if (channel<MAX_ANALOGPINS) {
    return analogReadPin(analog_pins[channel]);
  }
  return 0xFFFF;
}

// ----------------------------------------------- 
double analogReadPinVoltage(uint8_t pin)
{
  int ival;
  ival = analogReadPin( pin );
  return ival * INTERNAL_ADC_RESOLUTION;
}

double analogReadChannelVoltage(uint8_t channel)
{
  if (channel<MAX_ANALOGPINS) {
    return analogReadPinVoltage(analog_pins[channel]);
  }
  return 0.;
}

// -----------------------------------------------		 
double analogReadPinVoltageAveraged(uint8_t pin, int navgs)
{
  unsigned long long ull = 0;
  int n = 0;
  if (navgs <1) navgs = 1;
  while( n < navgs ) {
    ull += analogReadPin( pin );
    n++;
  }
  return ull * INTERNAL_ADC_RESOLUTION/navgs;
}

double analogReadChannelVoltageAveraged(uint8_t channel, int navgs)
{
  if (channel<MAX_ANALOGPINS) {
    return analogReadPinVoltageAveraged(analog_pins[channel],navgs);
  }
  return 0.;
}

// -----------------------------------------------
bool selectAnalogChannels( unsigned int *channels, int nchannels )
{
  if (!channels||!nchannels) {
    Serial.print("SelectedAnalogChannels: ");
    for(int m=0; m<analog_pins_nselected; m++) {
      Serial.print(" ");
      Serial.print( analog_pins_selection[m]);
    }
    Serial.println("");
    return (analog_pins_nselected>0);
  }
  
  if (nchannels >= MAX_ANALOGPINS) {
    Serial.println( "Error: channel list is too long");
    return false;
  }
    
  for (int i=0; i<nchannels; i++) {
    if (channels[i] >= MAX_ANALOGPINS) {
      Serial.println( "Error: channel selection out of range");
      return false;
    }
  }
  
  for (int i=0; i<nchannels; i++) {
    analog_pins_selection[i] = channels[i];
    analogReadPin(channels[i]);
  }
  analog_pins_nselected = nchannels;

  return true;
}

double *analogReadSelected( int navgs) {
  unsigned long long ull[MAX_ANALOGPINS] = {0};
  int n;
  int m;
  for(n = 0; n<navgs; n++) {
    for (m=0;m<analog_pins_nselected;m++) {
      ull[m] += analogReadPin(analog_pins_selection[m]);
    }
  }
  for (m=0;m<analog_pins_nselected;m++) {
    analog_pins_voltages[m] = ull[m] * INTERNAL_ADC_RESOLUTION/navgs;
  }
  return analog_pins_voltages;
}

void printAnalogSelected( int navgs ) {
  analogReadSelected(navgs);
  Serial.print( "A" );
  for( int m=0; m<analog_pins_nselected; m++) {
    Serial.print( " " );
    Serial.print( analog_pins_voltages[m], 6 );
  }
  Serial.println("");
}

// -----------------------------------------------
// PWM clock - generate square wave on specified pin

#ifdef IS_TEENSY

void stopSquareWave(uint8_t pin)
{
  Serial.print("stopSquareWave ");
  Serial.println( pin );
  
  pinMode(pin, OUTPUT);   
  DIGITALWRITE(pin, LOW);
}

void startSquareWave(uint8_t pin, float frequencyHz)
{
  Serial.print("startSquareWave "); Serial.print( pin );
  Serial.print( " f "); Serial.println(frequencyHz);
  
  // this stops it if already reunning
  stopSquareWave(pin);

  // this sets it up
  Serial.println("startSquareWave writing resolution");  
  analogWriteResolution(4);          // pwm range 4 bits, i.e. 2^4

  Serial.println("startSquareWave writing frequency");  
  analogWriteFrequency(pin, frequencyHz);

  // this starts it
  Serial.println("startSquareWave writing dutycycle");  
  analogWrite(pin,8);              // dutycycle 50% for 2^4

  Serial.println("startSquareWave returning");  
  
}

// -----------------------------------------------

void startMasterClock(float frequencyHz)
{
  stopSquareWave(fMPin);
  
  pinMode(fMPinMonitor, INPUT);
  
  startSquareWave(fMPin, frequencyHz);

}

void stopMasterClock( )
{
  stopSquareWave(fMPin);
}

#endif

// -----------------------------------------------

inline const char *boolstr(bool val)
{
  return val?"true":"false";
}

inline const char *modestr( uint8_t mode )
{
  switch (mode)
    {
    case INPUT:
      return "input";
      break;
    case INPUT_PULLUP:
      return "input_pullup";
      break;
    case OUTPUT:
      return "output";
      break;
    case ANALOGINPUT:
      return "analog";
      break;
    default:
      return "notset";
    }
  return "notset";
}

inline const char *edgestr(int mode)
{
  switch (mode)
    {
    case RISING:
      return "rising";
      break;
    case FALLING:
      return "falling";
      break;
    case CHANGE:
      return "change";
      break;
    default:
      return "notrecognized";
    }
  return "notrecognized";
}

void printPinState_(uint8_t pin)
{
  uint16_t istate;
  uint8_t mode;

  if (pin < MAXPINS) {
    mode = pin_modes[pin];
    istate = readPin(pin);
    
    Serial.print( " " );
    Serial.print( pin );
    Serial.print( " " );
    Serial.print( modestr(mode) );

    Serial.print( ": " );
    if (mode==ANALOGINPUT) {
      Serial.print( istate );
      Serial.print( " " );
      Serial.print( istate*INTERNAL_ADC_RESOLUTION, 6 );      
    }
    else {
      Serial.print( boolstr(pin_states[pin]) );
    }
  }
}

void printTogglePinState_(uint8_t pin, uint8_t extrastate)
{
  printPinState_(pin);
  Serial.print( " toggled: " );
  Serial.print( boolstr(extrastate) );
}

void printPin( const uint8_t pin )
{
  if (pin < MAXPINS) {
    switch(pin)
      {
      case syncPin:
	Serial.print( "sync    " );
	printTogglePinState_(pin,sync_toggled);
	Serial.println("");
	break;
      case busyPin:
	Serial.print( "busy    " );
	printTogglePinState_(pin,busy_toggled);
	Serial.println("");
	break;
      case sparePin:
	Serial.print( "spare   " );
	printTogglePinState_(pin,spare_toggled);
	Serial.println("");
	break;
      case interruptPin:
	Serial.print( "trigger " );
	printPinState_(pin);
	Serial.print(" ");
	Serial.print(edgestr(intedgemode));
	Serial.println("");
	break;
      default:
	Serial.print( "pin" );
	printPinState_(pin);
	Serial.println("");
	break;
      }
  }
}

void printPins( )
{
  uint8_t pin;
  for (pin=0;pin<=MAXPINS;pin++) {
    printPin(pin);
  }
}

/* ===================================================================
   The setup routine should be run from setup() in the master sketch, after you setup Serial.
*/

void controller_pins_setup()
{
  Serial.println( "controller pins Setup" );

#ifdef IS_TEENSY
  Serial.println( "controller pins fastAnalogSetup" );
  fastAnalogReadSetup();
  Serial.println( "controller pins fastAnalogSetup done" );
#endif

  // Setup the external synch pins
  setPinMode(interruptPin, INPUT);
  writePin(busyPin, busyPinState);
  writePin(syncPin, syncPinState);
  writePin(sparePin, sparePinState);

  Serial.println( "controller pins Setup done" );
  
}


