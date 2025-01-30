/********************************************************
 * @file MCP33131D.h
 * @copyright Copyright (c) 2024 Mitchell C Nelson, PhD
 * @brief A header-only library for the MCP33131D ADC, for Teensy and Arduino
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 *    list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 * 3, Neither the name of the copyright holder nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED  WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 *********************************************************/

#ifndef MCP33131D_H
#define MCP33131D_H

#include <Arduino.h>
#include <SPI.h>

#include "controller_identification.h"

// ==============================
// CPU clock cycles per usec, secs per cycle
#define MCP33131D_CYCLES_PER_USEC (F_CPU / 1000000)
#define MCP33131D_SECS_PER_CYCLE (1./F_CPU)

// nanoseconds to cycles
#define MCP33131D_NANOSECS_TO_CYCLES(n) (((n)*MCP33131D_CYCLES_PER_USEC)/1000)

// ==============================
// CNVSTPIN, can be selected externally
#ifndef MCP33131D_CNVSTPIN
#ifndef CNVSTPIN
#error need to define either MCP33131D_CNVSTPIN or CNVSTPIN before #include MCP33131D.h, consider including controllerpins.h
#endif
#define MCP33131D_CNVSTPIN CNVSTPIN
#endif

#ifndef SYNCPIN
#error need to define SYNCPIN, consider including controllerpins.h
#endif

#ifndef BUSYPIN
#error need to define BUSYPIN, consider including controllerpins.h
#endif

// ==========================================================
#if defined(IS_TEENSY4)

#include <digitalWriteFast.h>
#define MCP33131D_DIGITALREAD(a) digitalReadFast(a)
#define MCP33131D_DIGITALWRITE(a,b) digitalWriteFast(a,b)
#define MCP33131D_DIGITALTOGGLE(a) digitalToggleFast(a)

// ----------------------
#ifdef USETIMERONE
#include <TimerOne.h>

#else
#ifndef USEINTERVALTIMER
#define USEINTERVALTIMER
#endif
static IntervalTimer mcp33131d_timer;

#endif

// ----------------------
#define MCP33131D_SPI_SPEED 30000000
SPISettings spi_settings(MCP33131D_SPI_SPEED, MSBFIRST, SPI_MODE0);   // 30 MHz, reads

#ifndef MCP33131D_TEENSY_FASTSPI
#define MCP33131D_TEENSY_FASTSPI
#endif

#include "imxrt.h"
#include "pins_arduino.h"

static IMXRT_LPSPI_t *lpspi = &IMXRT_LPSPI4_S;

static uint16_t saved_framesz;

static inline uint16_t get_framesz(  IMXRT_LPSPI_t *port ) {
  return (port->TCR & 0x00000fff) + 1;
}

static inline void set_framesz( IMXRT_LPSPI_t *port, uint16_t nbits ) {
  port->TCR = (port->TCR & 0xfffff000) | LPSPI_TCR_FRAMESZ(nbits-1); 
}

static inline uint16_t transfer16( IMXRT_LPSPI_t *port, uint16_t data ) {
  port->TDR = data;                         // output 16 bit data
  while (port->RSR & LPSPI_RSR_RXEMPTY) {}  // wait while RSR fifo is empty
  return port->RDR;                         // return data read
}

#endif

// ==========================================================
#if defined(IS_ARDUINO_UNO_R4)

#define MCP33131D_DIGITALREAD(a) digitalRead(a)
#define MCP33131D_DIGITALWRITE(a,b) digitalWrite(a,b)
#define MCP33131D_DIGITALTOGGLE(a) digitalWrite(a,!digitalRead(a))

#include <elapsedMillis.h>

#define ARM_DWT_CYCCNT DWT->CYCCNT

void setupCYCCNT() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  ITM->LAR = 0xc5acce55;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


#ifdef USEINTERVALTIMER
#error INTERVALTIMER not available for UNO R4
#endif

#ifndef USETIMERONE
#define  USETIMERONE
#endif
#include <TimerOne.h>

//#define MCP33131D_SPI_SPEED 12000000
#define MCP33131D_SPI_SPEED 24000000
SPISettings spi_settings(MCP33131D_SPI_SPEED, MSBFIRST, SPI_MODE0);   // 30 MHz, reads

//#define MCP33131D_UNOR4_SLOWSPI
#define MCP33131D_UNOR4_FASTSPI

#endif

// ==========================================
// Define signed or unsigned, not both
#if defined(MCP33131D_SIGNED) && defined(MCP33131D_UNSIGNED)
#error need to define either MCP33131D_SIGNED or MCP33131D_UNSIGNED, not both
#endif

// Default to signed
#if !defined(MCP33131D_SIGNED) && !defined(MCP33131D_UNSIGNED)
#define MCP33131D_SIGNED
#endif

// ==========================================
// Scaling
#ifndef MCP33131D_VFS
#define MCP33131D_VFS 8.0
#endif

// Binaryscale
#if defined(MCP33131D_SIGNED)
#define MCP33131D_BFS 32768
#elif defined(MCP33131D_UNSIGNED)
#define MCP33131D_BFS 65536
#endif

// Precision, volts per LSB
#ifndef MCP33131D_LSB
#define MCP33131D_LSB ((float)MCP33131D_VFS/MCP33131D_BFS)
#endif

// binary to volts
#define MCP33131D_VOLTS(a) ((mcp33131d_t)a*MCP33131D_LSB)

// Data represenation (type)
#if defined(MCP33131D_SIGNED)
typedef int16_t mcp33131d_t;
#elif defined(MCP33131D_UNSIGNED)
typedef uint16_t mcp33131d_t;
#endif

#define MCP33131D_FORMAT_BINARY 0
#define MCP33131D_FORMAT_ASCII 1
#define MCP33131D_FORMAT_VOLTS 2

// ==========================================
// MCP33131D timing

#define MCP33131D_MIN_CLOCK_USECS 5

#define MCP33131D_ACQUIRE_NANOSECONDS 300
#define MCP33131D_WAIT_ACQUIRE MCP33131D_NANOSECS_TO_CYCLES(MCP33131D_ACQUIRE_NANOSECONDS)

#if defined(IS_ARDUINO_UNO_R4)
#define MCP33131D_WAIT_CONVERSION MCP33131D_NANOSECS_TO_CYCLES(50)

#elif defined(MCP33131D_TEENSY_FASTSPI)
#define MCP33131D_WAIT_CONVERSION MCP33131D_NANOSECS_TO_CYCLES(650)

#elif defined(IS_TEENSY4)
#define MCP33131D_WAIT_CONVERSION MCP33131D_NANOSECS_TO_CYCLES(580)

#else
#error "controller board type not defined, UNOR4 or Teensy"

#endif


// ==================================================================
static void printCounter( const char *s, unsigned int counter, unsigned int knts, bool eol=true )
{
  Serial.print(s);
  Serial.print(" "); Serial.print(counter);
  Serial.print( " / " ); Serial.print( knts );
  if(eol) Serial.println("");
}

bool pointermatch( void (*f)(), void (*g)() )

{
  return (f == g) ? true : false;
}

uint64_t cycles64( )
{
  static uint32_t oldCycles = ARM_DWT_CYCCNT;
  static uint32_t highDWORD = 0;

  uint32_t newCycles = ARM_DWT_CYCCNT;

  if (newCycles < oldCycles) {
    ++highDWORD;
  }
  oldCycles = newCycles;
  
  return (((uint64_t)highDWORD << 32) | newCycles);
}

/******************************************************************
 * @brief The MCP33131D class
 *
 */

class MCP33131D
{
  
public:  
  uint32_t elapsed_cycles_holder = ARM_DWT_CYCCNT;

  uint16_t format = 0;

  //  inline static uint32_t cyccnt_holder;
  inline static uint64_t cyccnt_holder64;

  inline static bool verbose;

  inline static bool errorflag;
  
  inline static mcp33131d_t *buffer;
  inline static mcp33131d_t *buffer_next;
  inline static unsigned int buffer_length;
  inline static unsigned int buffer_counter;
  inline static void (*buffer_callback)();

  inline static uint8_t watched_pin;
  inline static uint8_t watched_edge;
  
  inline static unsigned int frame_counter;
  inline static unsigned int frame_knts;

  inline static mcp33131d_t single_val;
  inline static unsigned int single_knts;
  inline static unsigned int single_counter;
  inline static void (*single_callback)();
  inline static unsigned int single_cyccnt;
  
  inline static mcp33131d_t sum_val;
  inline static unsigned int sum_counter;
  inline static unsigned int sum_knts;
  inline static void (*sum_callback)();

  inline static unsigned int clock_counter;
  inline static unsigned int clock_knts;
  //inline static unsigned int clock_usecs;
  inline static unsigned long clock_usecs;
  inline static bool clock_active;
  inline static void (*clock_isr)();
  //inline static uint32_t clock_cyccnt;
  //inline static uint32_t clock_cyccnt_holder;
  inline static uint64_t clock_cyccnt_holder64;

  inline static unsigned int interrupt_counter;
  inline static unsigned int interrupt_knts;
  inline static uint8_t interrupt_pin;
  inline static uint8_t interrupt_mode;
  inline static bool interrupt_active;
  inline static void (*interrupt_isr)();
  //inline static uint32_t interrupt_cyccnt;
  //inline static uint32_t interrupt_cyccnt_holder;
  
  inline static uint64_t interrupt_cyccnt_holder64;
  
  // -------------------------------------
    
  MCP33131D()
  {
  }
    
  ~MCP33131D( )
  {
  }

  void begin()
  {
#ifdef IS_ARDUINO_UNO_R4
    setupCYCCNT();
#endif
    
    // SPI setup for the MCP33131D (ADC)
    pinMode(MCP33131D_CNVSTPIN, OUTPUT);
    MCP33131D_DIGITALWRITE(MCP33131D_CNVSTPIN, LOW);
  
    SPI.begin();
    SPI.beginTransaction(spi_settings);

#ifdef MCP33131D_TEENSY_FASTSPI
    saved_framesz = get_framesz( lpspi );
    set_framesz( lpspi, 16 );
#endif
    
#ifdef MCP33131D_UNOR4_FASTSPI
    SPI.transfer16_setup();
#endif
    

#ifdef USETIMERONE
    Timer1.initialize(100000);
    delay(1000);
    Timer1.stop();
#endif

  }

  void end()
  {
#ifdef MCP33131D_TEENSY_FASTSPI
    set_framesz( lpspi, saved_framesz );
#endif

#ifdef MCP33131D_UNOR4_FASTSPI
    SPI.transfer16_cleanup();
#endif
    
    SPI.endTransaction();
    SPI.end();
  }

  bool error()
  {
    bool saveflag = errorflag;
    errorflag = false;
    return saveflag;
  }

  void printConfiguration( )
  {
    Serial.print("Configuration: MCP33131D (16 bit differential ADC)");
#ifdef MCP33131D_SIGNED
    Serial.print(" read_as signed");
#endif
#ifdef MCP33131D_UNSIGNED
    Serial.print(" read_as unsigned");
#endif
    Serial.print(" bits 16");
    Serial.print(" vfs "); Serial.print(MCP33131D_VFS,6);
    Serial.print(" volts_per_lsb "); Serial.print(MCP33131D_LSB,6);
    Serial.println("");
  }
  
  /* ========================================
     Basic read function
  */
  static mcp33131d_t read( )
  {
    uint16_t u16;

    uint32_t raw_cycles_holder = ARM_DWT_CYCCNT;

    // Start the ADC conversion, data is ready 730nsec later.
    MCP33131D_DIGITALWRITE( MCP33131D_CNVSTPIN, HIGH );

    // Wait for the conversion time
    while((ARM_DWT_CYCCNT-raw_cycles_holder) < MCP33131D_WAIT_CONVERSION);
    
    // Enable readout
    MCP33131D_DIGITALWRITE( MCP33131D_CNVSTPIN, LOW );
    
#if defined(MCP33131D_TEENSY_FASTSPI)
    u16 = transfer16(lpspi,0xFFFF);

#elif defined(MCP33131D_UNOR4_SLOWSPI)
    u16 = SPI.transfer16_asbytes(0xFFFF);

#elif defined(MCP33131D_UNOR4_FASTSPI)
    u16 = SPI.transfer16_transfer(0xFFFF);    

#else
    u16 = SPI.transfer16(0xFFFF);

#endif

#ifdef MCP33131D_UNSIGNED
    // Generally, pretty fast compared to the adc acquire time
    u16 ^= (uint16_t) 0x8000;
#endif

    return (mcp33131d_t) u16;
  }

  // ----------------------------------------
  static double readVolts() {
    mcp33131d_t a = read();
    return a*MCP33131D_LSB;
  }

  // ----------------------------------------
  // This one is for diagnostics
  
  static uint16_t read_raw( )
  {
    uint16_t u16;

    uint32_t raw_cycles_holder = ARM_DWT_CYCCNT;

    // Start the ADC conversion, data is ready 730nsec later.
    MCP33131D_DIGITALWRITE( MCP33131D_CNVSTPIN, HIGH );

    // Wait for the conversion time
    while((ARM_DWT_CYCCNT-raw_cycles_holder) < MCP33131D_WAIT_CONVERSION);
    
    // Enable readout
    MCP33131D_DIGITALWRITE( MCP33131D_CNVSTPIN, LOW );


#if defined(MCP33131D_TEENSY_FASTSPI)
    u16 = transfer16(lpspi,0xFFFF);

#elif defined(MCP33131D_UNOR4_SLOWSPI)
    u16 = SPI.transfer16_asbytes(0xFFFF);

#elif defined(MCP33131D_UNOR4_FASTSPI)
    u16 = SPI.transfer16_transfer(0xFFFF);    

#else
    u16 = SPI.transfer16(0xFFFF);

#endif
    
    return u16;
  }

  // -----------------------------------------------------------
  // Wait for the duration of the acquisition window then read
  static inline void waitAcquire( )
  {
    uint32_t raw_cycles_holder = ARM_DWT_CYCCNT;
    while ((ARM_DWT_CYCCNT - raw_cycles_holder) < MCP33131D_WAIT_ACQUIRE);
    
    return;
  }
  
  inline uint16_t acquire_raw( ) {
    waitAcquire();
    return read_raw();
  }

  inline mcp33131d_t acquire( ) {
    waitAcquire();
    return read();
  }

  inline double acquireVolts() {
    waitAcquire();
    return readVolts();
  }
  
  /* ========================================
     Buffered reads, can be clocked or triggerd or wait on pin transition
     Note the buffer stores the raw read result
  */
  bool setupBuffer_(mcp33131d_t *ptr, unsigned int length, void (*callback)())
  {
    if (!ptr || !length) {
      Serial.print("Error: setupBuffer_");
      Serial.print(" buffer "); Serial.print( (unsigned int)ptr, HEX);
      Serial.print( " length " ); Serial.print( length );
      Serial.print( " callback " ); Serial.println( (unsigned int)callback, HEX );
      return false;
    }
    buffer = ptr;
    buffer_next = buffer;
    buffer_length = length;
    buffer_counter = 0;
    buffer_callback = callback;

    printBufferSetup();
    
    return true;
  }

  // Simple buffer wait, for triggered and pin synched read by read.
  bool waitBuffer_(float timeout_milliseconds=2000, float timestep_milliseconds=20.)
  {
    while((buffer_counter<buffer_length) && (timeout_milliseconds>0.)) {
      delay(timestep_milliseconds);
      timeout_milliseconds -= timestep_milliseconds;
    }

    return buffer_length - buffer_counter;
  }
  
  void printBufferSetup()
  {
    Serial.print( "Buffer: addr " ); Serial.print( (unsigned int)buffer, HEX);
    Serial.print( " counter " ); Serial.print( buffer_counter );
    Serial.print( " length " ); Serial.print( buffer_length );
    Serial.print( " callback " );
    if (pointermatch(buffer_callback,sendFormatted)) {
      Serial.print("sendFormatted");
    } else if (pointermatch(buffer_callback,sendFormattedVolts)) {
      Serial.print("sendFormattedVolts");
    } else if (pointermatch(buffer_callback,sendBinary)) {
      Serial.print("sendBinary");
    } else {
      Serial.print( (unsigned int)buffer_callback, HEX );
    }
    Serial.println("");    
  }  
  
  /* ----------------------------------------------
     Send the buffered data
  */
  static void sendFrameCounter()
  {
    Serial.print("FRAME_COUNTER ");Serial.print(frame_counter);
    Serial.print( " of "); Serial.print(frame_knts);
    if (frame_knts && frame_counter >= frame_knts) {
      Serial.print(" COMPLETE ");
    }
    Serial.println("");
  }
  
  static void sendBinary( )
  {
    sendFrameCounter();
    
    if(buffer&&buffer_length&&buffer_counter) {
      unsigned int nbytes = sizeof(mcp33131d_t)*buffer_length;
      Serial.print( "BINARY16 " ); Serial.println( buffer_counter );
      Serial.write( (byte *) buffer, nbytes );
      Serial.println( "END BINARY16" );      
    }
    
  }

  // -----------------------------------------------
  static void sendFormatted( )
  {
    sendFrameCounter();
    
    if(buffer&&buffer_length&&buffer_counter) {
      mcp33131d_t *ptr = (mcp33131d_t *)buffer;
      Serial.print( "DATA_INTS " ); Serial.println( buffer_counter );
      for (unsigned int n = 0; n < buffer_counter; n++ ) {
	Serial.println( (mcp33131d_t) ptr[n] );
      }
      Serial.println( "END DATA" );
    }
  }

  static void sendFormattedVolts( )
  {
    sendFrameCounter();

    if(buffer&&buffer_length&&buffer_counter) {
      mcp33131d_t *ptr = (mcp33131d_t *)buffer;
      float f;
      Serial.print( "DATA_VOLTS " ); Serial.println( buffer_counter );
      for (unsigned int n = 0; n < buffer_counter; n++ ) {
	f = ptr[n]*(MCP33131D_LSB);
	Serial.println( f, 6 );
      }
      Serial.println( "END DATA" );
    }
  }

  // ------------------------------------------------------
  // abstraction for send buffer, we'll probably ditch this
  inline void sendBuffer( )
  {
    switch( format )
      {
      case MCP33131D_FORMAT_BINARY:
	sendBinary();
	break;
      case MCP33131D_FORMAT_ASCII:
	sendFormatted();
	break;
      case MCP33131D_FORMAT_VOLTS:
	sendFormattedVolts();
	break;
      }
  }
  
  inline bool setFormat( const char *s )
  {
    if (!strncmp( "binary", s, 6)) {
      format = MCP33131D_FORMAT_BINARY;
    }
    else if (!strncmp( "ascii", s, 3)) {
      format = MCP33131D_FORMAT_ASCII;
    }    
    else if (!strncmp( "volt", s, 4)) {
      format = MCP33131D_FORMAT_VOLTS;
    }
    else {
      Serial.println( "Error: format specified not recognized" );
      return false;
    }
    return true;
  }

  /* ==================================================================
     Basic trigger
  */
  static void triggerTestISR_()
  {
    /*
    uint32_t interrupt_cyccnt = ARM_DWT_CYCCNT;
    double t = (interrupt_cyccnt-clock_cyccnt_holder)*MCP33131D_SECS_PER_CYCLE;
    */
    double t = (cycles64()-clock_cyccnt_holder64)*MCP33131D_SECS_PER_CYCLE;
    Serial.print( "# triggerTestISR_: ");
    Serial.println( t, 8 );
  }
  
  bool setupTrigger_( uint8_t pin, uint8_t mode,void (*isr)())
  {
    if (mode != RISING && mode != FALLING && mode != CHANGE) {
      Serial.println("Error: trigger mode not recognized");
      return false;
    }
    if (!isr) {
      Serial.println("Error: interrupt isr is null");
      return false;
    }

    switch (mode)
      {
      case RISING:
        interrupt_mode = RISING;
        break;
      case FALLING:
        interrupt_mode = FALLING;
        break;
      case CHANGE:
        interrupt_mode = CHANGE;
        break;
      default:
        Serial.println("Error: interrupt mode not supported");
        return false;
      }
        
    interrupt_pin = pin;
    interrupt_isr = isr;

    printTrigger();
    
    return true;
  }

  static void startTrigger_()
  {
    Serial.println("# startTrigger_");
    interrupt_active = true;
    
    //interrupt_cyccnt_holder = ARM_DWT_CYCCNT;
    interrupt_cyccnt_holder64 = cycles64();

    switch(interrupt_mode)
      {
      case RISING:
        attachInterrupt(digitalPinToInterrupt(interrupt_pin),interrupt_isr,RISING);
        break;
      case FALLING:
        attachInterrupt(digitalPinToInterrupt(interrupt_pin),interrupt_isr,FALLING);
        break;
      case CHANGE:
        attachInterrupt(digitalPinToInterrupt(interrupt_pin),interrupt_isr,CHANGE);
        break;
      }       
  }
  
  static void stopTrigger_()
  {
    if (interrupt_active) {
      detachInterrupt(digitalPinToInterrupt(interrupt_pin));
      interrupt_active = false;
      Serial.println("# stopTrigger_ stopped");
    }
  }

  bool waitTriggered_(unsigned int *counter, unsigned int *knts,
		      float timeoutsecs=100.0, float stepsecs=0.01)
  {
    float msecs = timeoutsecs * 1000;
    float msecstep = stepsecs * 1000;
    
    while ((*counter<*knts)) {
      if (Serial.available()) {
	stopTrigger_();
	Serial.println("Warning: waitTriggered_ ended on serial input.");
	break;
      }
      if (timeoutsecs) {
	if(msecs <= 0) {
	  stopTrigger_();
	  Serial.println( "Error: waitTriggerd timed out");
	  break;
	}
	delay(msecstep);
	msecs -= msecstep;
      }
    }

    return (*counter<*knts)?false:true;
  }
  
  bool waitTriggered_(float timeoutsecs=100.0, int stepsecs=0.01)
  {
    int msecs = timeoutsecs * 1000;
    int msecstep = stepsecs * 1000;
    
    while (interrupt_active) {
      if (Serial.available()) {
	stopTrigger_();
	Serial.println("Warning: waitTriggered_ ended on serial input.");
	return false;
      }
      if (timeoutsecs) {
	if(msecs <= 0) {
	  stopTrigger_();
	  Serial.println( "Error: waitTriggerd timed out");
	  return false;
	}
	delay(msecstep);
	msecs -= msecstep;
      }
    }

    return true;
  }
  
  bool printTrigger()
  {
    Serial.print("Trigger: ISR ");

    if (pointermatch(interrupt_isr,startClockedBuffer)) {      
      Serial.print("startClockedBuffer");
    } else if (pointermatch(interrupt_isr,startClockedSingle_)) {        
      Serial.print("startClockedSingle_");
    } else if (pointermatch(interrupt_isr,startClockedSum_)) {
      Serial.print("startClockedSum_");
    } else if (pointermatch(interrupt_isr, triggeredBufferISR_)) {
      Serial.print("triggeredBufferISR_");
    } else if (pointermatch(interrupt_isr,triggeredSingleISR_)) {
      Serial.print("triggeredSingleISR_");
    } else if (pointermatch(interrupt_isr,triggeredSumISR_)) {
      Serial.print("triggeredSumISR_");
    } else if (pointermatch(interrupt_isr,triggerTestISR_)) {
      Serial.print("triggerTestISR_");
    } else if (pointermatch(interrupt_isr,pinSynchedFrame_Rising_)) {
      Serial.print("pinSynchedFrame_Rising_");
    } else if (pointermatch(interrupt_isr,pinSynchedFrame_Falling_)) {
      Serial.print("pinSynchedFrame_Falling_");
    }
    else {
      Serial.print( "0x" );
      Serial.print((unsigned int)interrupt_isr, HEX);
    }
                
    Serial.print( " pin " ); Serial.print( interrupt_pin );
    
    switch(interrupt_mode)
      {
      case RISING:
	Serial.print(" rising");
	break;
      case FALLING:
	Serial.print(" falling");
	break;
      case CHANGE:
	Serial.print(" change");
	break;
      default:
	Serial.print(" mode not recognized");
      }

    if (interrupt_active) {
      Serial.println(" active");
    }
    else {
      Serial.println(" not-active");
    }

    return interrupt_active;
  }
    
  bool triggerTest(uint8_t pin, uint8_t mode, bool start=true, bool wait=true, float timeout=0)
  {
    Serial.println("Setup: triggerTest");
    
    if (!setupTrigger_(pin, mode, triggerTestISR_)) {
      return false;
    }

    if (start) {
      startTrigger_();
      if (wait) {
	return waitTriggered_(timeout);
      }
    }
    return true;
  }
  
  // ------------------------------------------------------
  // Read, save raw binary to buffer
  static void triggeredBufferISR_()
  {
    if (buffer_counter < buffer_length) {
      *buffer_next++ = read();
      buffer_counter++;
      if (buffer_counter >= buffer_length) {
	stopTrigger_();
	// The call back will normally be one of the sendbuffer routines
	if (buffer_callback) {
	  (*buffer_callback)();
	}
      }
    }
  }

  bool setupTriggeredBuffer_(mcp33131d_t *ptr, unsigned int length, void (*callback)(), uint8_t pin, uint8_t mode)
  {    
    if (!setupBuffer_(ptr, length, callback)) {
      return false;
    }

    if (!setupTrigger_(pin, mode, triggeredBufferISR_)) {
      return false;
    }
    
    return true;    
  }

  bool triggeredBuffer_(mcp33131d_t *ptr, unsigned int length, void (*callback)(), uint8_t pin, uint8_t mode,
		       bool start=true, bool wait=false, float timeout=0)
  {
    Serial.println("Setup: triggeredBuffer");
    
    if (!setupTriggeredBuffer_(ptr, length, callback, pin, mode)) {
      return false;
    }
    
    if (start) {
      startTrigger_();

      if (wait) {
	return waitTriggered_(&buffer_counter,&buffer_length,timeout);
      }
    }
    
    return true;
  }

  bool triggeredBufferBinary(mcp33131d_t *ptr, unsigned int length, uint8_t pin, uint8_t mode,
			     bool start=true, bool wait=false, float timeout=0)
  {
    return triggeredBuffer_(ptr, length, sendBinary, pin, mode, start, wait, timeout);
  }

  bool triggeredBufferFormatted(mcp33131d_t *ptr, unsigned int length, uint8_t pin, uint8_t mode,
				bool start=true, bool wait=false, float timeout=0)
  {
    return triggeredBuffer_(ptr, length, sendFormatted, pin, mode, start, wait, timeout);
  }
  
  bool triggeredBufferVolts(mcp33131d_t *ptr, unsigned int length, uint8_t pin, uint8_t mode,
		       bool start=true, bool wait=false, float timeout=0)
  {
    return triggeredBuffer_(ptr, length, sendFormattedVolts, pin, mode, start, wait, timeout);
  }
  
  /* ------------------------------------------
     Basic Clock
  */
  bool setupClock_(unsigned long usecs,void (*isr)())
  {
    if (usecs<MCP33131D_MIN_CLOCK_USECS) {
      Serial.print( "Error: setupClock_ too fast " ); Serial.println( usecs );
      return false;
    }
    if (!isr) {
      Serial.println("Error: clocked isr is null");
      return false;
    }
    clock_usecs = usecs;
    clock_isr = isr;

#ifdef USETIMERONE
    stopClock_();
    Timer1.initialize(clock_usecs);
    Timer1.stop();
    Timer1.attachInterrupt(clock_isr);
#endif

    printClockSetup();
    
    return true;
  }
  
  static void startClock_()
  {
    clock_active = true;
    //clock_cyccnt_holder = ARM_DWT_CYCCNT;

    clock_cyccnt_holder64 = cycles64();

#ifdef USEINTERVALTIMER
    mcp33131d_timer.begin(clock_isr,clock_usecs);
#elif defined(USETIMERONE)
    Timer1.start();
#else
#error no timer defined
#endif

#ifdef SYNCPIN
    MCP33131D_DIGITALTOGGLE(SYNCPIN);
#endif
    
#ifdef BUSYPIN
    MCP33131D_DIGITALTOGGLE(BUSYPIN);
#endif
    
#ifdef SYNCPIN
    if(clock_usecs>6) {
      while((cycles64()-clock_cyccnt_holder64) < 5*MCP33131D_CYCLES_PER_USEC);
    }
    else {
      while((cycles64()-clock_cyccnt_holder64) < (MCP33131D_MIN_CLOCK_USECS-1)*MCP33131D_CYCLES_PER_USEC);
    }
    MCP33131D_DIGITALTOGGLE(SYNCPIN);
#endif

  }
  
  static void stopClock_()
  {
    if (clock_active) {
      clock_active = false;
#ifdef USEINTERVALTIMER
      mcp33131d_timer.end();
#elif defined(USETIMERONE)
      Timer1.stop();
      Timer1.detachInterrupt();
#else
#error no timer defined
#endif

#ifdef BUSYPIN
      MCP33131D_DIGITALTOGGLE(BUSYPIN);
#endif      
    }
  }

  
  bool waitClocked_(unsigned int *counter, unsigned int *knts,
		    unsigned long sleepusecs=100000)
  {
    unsigned long usecs = (*knts-*counter) * clock_usecs;
    unsigned int countdown = 3 + (unsigned int)(usecs/sleepusecs);

    if (verbose) {
      printCounter("# waitClocked_",*counter,*knts);
      Serial.print( " wait usecs " ); Serial.println( usecs );
      Serial.print( " clock usecs " ); Serial.println( clock_usecs );
      Serial.print( " countdown " ); Serial.println( countdown );
    }
        
    while( *knts>*counter ) {
      
      if (Serial.available()) {
	printCounter("# waitClocked_ interruped by serial input",*counter,*knts);
	break;
      }
      
      if (!clock_active) {
	if (*knts>*counter) {
	  printCounter("Error: clock stopped",*counter,*knts);
	}
	else if (verbose) {
	  printCounter("# waitClocked_ found clock stopped",*counter,*knts);
	}
	break;
      }
      
      if (!countdown) {
	if (*knts>*counter) {
	  printCounter("Error: wait countdown exhausted",*counter,*knts);
	}
	else if (verbose) {
	  printCounter("# waitClocked_ countdown used up",*counter,*knts);
	}
	break;
      }
      
    
      usecs = ((*knts)-(*counter)) * clock_usecs;
      if (usecs > sleepusecs)
	{
	  if (sleepusecs<16000) {
	    delayMicroseconds(sleepusecs);
	  }
	  else {
	    delay(sleepusecs/1000);
	  }
	}
      else if (usecs>16000)
	{
	  delay(usecs/1000);
	}
      else
	{
	  delayMicroseconds(usecs+100);
	}
      countdown--;


    }
    
    if (verbose) {
      printCounter("waitClocked_ after loop",*counter,*knts);
    }

    if (*counter < *knts) {
      stopClock_();
      printCounter("Error: waitClocked_ with counts remaining ",*counter,*knts);
    }
    
    return (*knts>*counter) ? false : true;
  }

  void printClockSetup()
  {
    Serial.print("Clock: ISR ");
    
    if (clock_isr) {

      if (pointermatch(clock_isr,clockedBufferISR_)) {
        Serial.print("clockedBufferISR_");
      } else if (pointermatch(clock_isr,clockedSingleISR_)) {
        Serial.print("clockedSingleISR_");
      } else if (pointermatch(clock_isr,clockedSumISR_)) {
        Serial.print("clockedSumISR_");
      } else {
        Serial.print( " 0x" );
        Serial.print((unsigned int)clock_isr, HEX);
      }
    }

    Serial.print(" usecs "); Serial.println( clock_usecs);    

    /*
    if (pointermatch(clock_isr,clockedBufferISR_)) {
      printBufferSetup();
    }
    */
  }

  // -----------------------------------------------------------------
  static void clockedBufferISR_()
  {
    if (buffer_counter < buffer_length) {

      *buffer_next++ = read();
      buffer_counter++;

      // We completed this buffer(frame)
      if (buffer_counter == buffer_length) {

	// Stop the clock
	stopClock_();
	
	// Increment the frame counter
	frame_counter++;

	// We've completed the triggered frame set
	if (frame_knts && (frame_counter >= frame_knts)) {
	  stopTrigger_();
	}

	// The call back will normally be one of the sendbuffer routines
	if (buffer_callback) {
	  (*buffer_callback)();
	}
      }
    }
  }
  
  bool setupClockedBuffer_(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, void (*callback)() )
  {
    if (!setupBuffer_(ptr,length,callback)) {
      return false;
    }
    if (!setupClock_(usecs,clockedBufferISR_)) {
      return false;
    }

    return true;
  }

  // This might be called from an interrupt, so reset pointer and counter
  static void startClockedBuffer( )
  {
    if (clock_active) {
      Serial.println("Error: startClockedBuffer while clock is active");
    }
    else {      
      buffer_next = buffer;
      buffer_counter = 0;
      startClock_();
    }
  }

  bool clockBuffer(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, void (*callback)(),
		   bool start=true, bool wait=true)
  {
    if (clock_active) {
      Serial.println("Error: starClockedBufferRead_ clock still active");
      return false;
    }
    
    Serial.println("Setup: ClockedBuffer");
    
    if (!setupClockedBuffer_(ptr, length, usecs, callback)) {
      Serial.println("Error: failure in setupClockedBuffer_");
      return false;
    }

    frame_counter = 0;
    frame_knts = 1;

    if (start) {
      startClockedBuffer();

      if (wait) {
	return waitClocked_(&buffer_counter, &buffer_length);
      }
    }
    
    return true;
  }
  
  bool clockBufferAscii(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, bool start=true, bool wait=true)
  {
    return clockBuffer(ptr,length,usecs,sendFormatted,start,wait);
  }
  
  bool clockBufferBinary(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, bool start=true, bool wait=true)
  {
    return clockBuffer(ptr,length,usecs,sendBinary,start,wait);
  }

  bool clockBufferVolts(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, bool start=true, bool wait=true)
  {
    return clockBuffer(ptr,length,usecs,sendFormattedVolts,start,wait);
  }

  // --------------------------------------------------------------------------------------
    bool triggerClockedBuffer_(mcp33131d_t *ptr, unsigned int length, unsigned long usecs, void (*callback)(),
			       uint8_t pin, uint8_t mode, unsigned int knts,
			       bool start=true, bool wait=false, float timeout=0)
  {
  
    if (clock_active) {
      Serial.println("Error: starClockedBufferRead_ clock still active");
      return false;
    }
    
    Serial.println("Setup: triggeredClockedBuffer");
    
    if (!setupClockedBuffer_(ptr, length, usecs, callback)) {
      Serial.print("Error: triggerClockedBuffer failed setupClockedBuffer "); Serial.print(length);      
      Serial.print(" usecs"); Serial.println(usecs);
      return false;
    }

    if (!setupTrigger_(pin, mode, startClockedBuffer)) {
      Serial.print("Error: triggerClockedBuffer+ failed setupTrigger "); Serial.print(pin);      
      Serial.print(" mode "); Serial.println(mode);
      
      return false;
    }

    frame_counter = 0;
    frame_knts = knts;

    if (start) {
      
      startTrigger_();

      if (knts and wait) {
	return waitTriggered_(&frame_counter,&frame_knts,timeout);
      }
    }
    
    return true;    
  }

  bool triggerClockedBufferAscii(mcp33131d_t *ptr, unsigned int length, 
				 unsigned long usecs, uint8_t pin, uint8_t mode, unsigned int knt,
				 bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedBuffer_(ptr, length, usecs, sendFormatted, pin, mode, knt, start, wait, timeout);
  }

  bool triggerClockedBufferBinary(mcp33131d_t *ptr, unsigned int length, 
				  unsigned long usecs, uint8_t pin, uint8_t mode, unsigned int knt,
				  bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedBuffer_(ptr, length, usecs, sendBinary, pin, mode, knt, start, wait, timeout);
  }

  bool triggerClockedBufferVolts(mcp33131d_t *ptr, unsigned int length, 
				 unsigned long usecs, uint8_t pin, uint8_t mode, unsigned int knt,
				 bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedBuffer_(ptr, length, usecs, sendFormattedVolts, pin, mode, knt, start, wait, timeout);
  }

  /* ------------------------------------------
     Pin Synched Buffered Read
  */
  bool setupPinSynchedBuffer_(mcp33131d_t *ptr, unsigned int length, void (*callback)(),
                              uint8_t watchedpin, uint8_t watchededge)
  {
    if (!setupBuffer_(ptr,length,callback)) {
      return false;
    }
    watched_pin = watchedpin;
    watched_edge = watchededge;  // only needed for the printout

    printPinSyncSetup();
    
    return true;
  }

  void printPinSyncSetup()
  {
    Serial.print("PinSync: "); Serial.print( watched_pin);

    if (watched_edge == RISING) {
      Serial.print(" RISING ");
    }
    else if (watched_edge == FALLING) {
      Serial.print(" FALLING ");
    }
    else {
      Serial.print(" unknown ");
    }

    Serial.println("");
    
  }

  // --------------------------------------------------------------
  // One read per function call
  static void pinSynched_BufferedRead_Rising_()
  { 
    if (buffer_counter < buffer_length) {

      while (MCP33131D_DIGITALREAD(watched_pin)) {}   // wait while high
      while (!MCP33131D_DIGITALREAD(watched_pin)) {}  // wait while low
            
      *buffer_next++ = read();
      buffer_counter++;

      if ((buffer_counter >= buffer_length) && (buffer_callback)) {
	  (*buffer_callback)();
      }
      
    }
  }

  static void pinSynched_BufferedRead_Falling_()
  { 
    if (buffer_counter < buffer_length) {

      while (!MCP33131D_DIGITALREAD(watched_pin)) {}  // wait while low
      while (MCP33131D_DIGITALREAD(watched_pin)) {}   // wait while high
            
      *buffer_next++ = read();
      buffer_counter++;

      if ((buffer_counter >= buffer_length)&& (buffer_callback)) {
	  (*buffer_callback)();
      }
      
    }
  }

  // --------------------------------------------------------------
  // Read one entire frame per call
  static void pinSynchedFrame_Rising_()
  {
    
    while (buffer_counter < buffer_length) {

      while (MCP33131D_DIGITALREAD(watched_pin)) {}   // wait while high
      while (!MCP33131D_DIGITALREAD(watched_pin)) {}  // wait while low
            
      *buffer_next++ = read();
      buffer_counter++;
    }

    if (buffer_callback) {
      (*buffer_callback)();
    }
  }


  static void pinSynchedFrame_Falling_()
  { 
    while (buffer_counter < buffer_length) {

      while (!MCP33131D_DIGITALREAD(watched_pin)) {}  // wait while low
      while (MCP33131D_DIGITALREAD(watched_pin)) {}   // wait while high
            
      *buffer_next++ = read();
      buffer_counter++;
    }

    if (buffer_callback) {
      (*buffer_callback)();
    } 
  }

  // -------------------------------------------------------------
  bool pinSynchedFrame(uint8_t watchedpin, uint8_t watchededge, mcp33131d_t *ptr, unsigned int length,  void (*callback)())
  {
    Serial.println("Setup: pinSynchedFrame");
    
    if (!setupPinSynchedBuffer_(ptr, length, callback, watchedpin, watchededge)) {
      Serial.println("Error: pinSynchedFrame, setupPinSynchedBuffer_");
      return false;
    }

    if (watchededge==RISING) {
      pinSynchedFrame_Rising_();
      return true;
    }
    else if (watchededge==FALLING) {
      pinSynchedFrame_Falling_();
      return true;
    }

    Serial.println("Error: pinSynchedFrame, edge not recognized");
    return false;
  }
      
  bool pinSynchedFrameVolts(uint8_t watchedpin, uint8_t watchededge,
                                  mcp33131d_t *ptr, unsigned int length)
  {
    return pinSynchedFrame(watchedpin, watchededge,ptr,length,sendFormattedVolts);
  }
  
  bool pinSynchedFrameFormatted(uint8_t watchedpin, uint8_t watchededge,
                               mcp33131d_t *ptr, unsigned int length)
  {
    return pinSynchedFrame(watchedpin, watchededge,ptr,length,sendFormatted);
  }
  
  bool pinSynchedFrameBinary(uint8_t watchedpin, uint8_t watchededge,
                            mcp33131d_t *ptr, unsigned int length)
  {
    return pinSynchedFrame(watchedpin, watchededge,ptr,length,sendBinary);
  }

  // ---------------------------------------------------------------------------
  
  bool triggerPinSynchedFrame_(uint8_t watchedpin, uint8_t watchededge,
                              mcp33131d_t *ptr, unsigned int length,  void (*callback)(),
                              uint8_t pin, uint8_t mode, bool start=true, bool wait=false )
  {
    Serial.println("Setup: triggerPinSynchedFrame");
    
    if (!setupPinSynchedBuffer_(ptr, length, callback, watchedpin, watchededge)) {
      return false;
    }
    
    if (watchededge == RISING) {
      if (!setupTrigger_(pin,mode,pinSynchedFrame_Rising_)) {
        return false;
      }      
    }
    else if (watchededge == FALLING) {
      if (!setupTrigger_(pin,mode,pinSynchedFrame_Falling_)) {
        return false;
      }      
    }
    else {
      Serial.println("Error: watchededge not recognized");
      return false;
    }

    if (start) {
      
      startTrigger_();
    
      if (wait) {
	return waitTriggered_(&buffer_counter, &buffer_length);
      }
    }
    
    return true;
  }

  bool triggerPinSynchedFrameFormatted(uint8_t watchedpin, uint8_t watchededge,
                                    mcp33131d_t *ptr, unsigned int length,
                                    uint8_t pin, uint8_t mode, bool start=true, bool wait=false )
  {
    return triggerPinSynchedFrame_(watchedpin,watchededge, ptr, length, sendFormatted, pin, mode, start, wait);
  }

  bool triggerPinSynchedFrameBinary(uint8_t watchedpin, uint8_t watchededge,
                                    mcp33131d_t *ptr, unsigned int length,
                                    uint8_t pin, uint8_t mode, bool start=true, bool wait=false )
  {
    return triggerPinSynchedFrame_(watchedpin,watchededge, ptr, length, sendBinary, pin, mode, start, wait);
  }
  
  bool triggerPinSynchedFrameVolts(uint8_t watchedpin, uint8_t watchededge,
                                    mcp33131d_t *ptr, unsigned int length,
                                    uint8_t pin, uint8_t mode, bool start=true, bool wait=false )
  {
    return triggerPinSynchedFrame_(watchedpin,watchededge, ptr, length, sendFormattedVolts, pin, mode, start, wait);
  }
  
  /* ===================================================================
   * Single Reads
  */
  bool setupSingles_(unsigned int knts, void (*callback)())
  {
    single_knts = knts;
    single_counter = 0;
    single_val = 0;
    single_callback = callback;
    single_cyccnt = 0;

    printSinglesSetup();
    
    return true;
  }

  void printSinglesSetup()
  {
    Serial.print( "Singles: "); Serial.print(single_knts);
    Serial.print( " callback " );
    if (pointermatch(single_callback,printSingle)) {
      Serial.print("printSingle");
    }
    else if (pointermatch(single_callback,printSingleVolts)) {
      Serial.print("printSingleVolts");
    }
    else {
      Serial.print( "0x"); Serial.print((unsigned int)single_callback,HEX);
    }
    Serial.println("");
  }
  
  static void printSingle()
  {
    //double t = (single_cyccnt-clock_cyccnt_holder)*MCP33131D_SECS_PER_CYCLE;
    double t = (cycles64() - clock_cyccnt_holder64)*MCP33131D_SECS_PER_CYCLE;
    
    if (single_counter == 1) {
      Serial.println("START SINGLE INTS");
    }
    
    if (verbose) {
      printCounter("Singles: counter", single_counter, single_knts, false );
    }
    Serial.print("adc: ");Serial.print(t,8);
    Serial.print(" ");Serial.println(single_val);

    if (single_counter == single_knts) {
      Serial.println("END SINGLE");
    }
  }
  
  static void printSingleVolts()
  {
    //double t = (single_cyccnt-clock_cyccnt_holder)*MCP33131D_SECS_PER_CYCLE;
    double t = (cycles64() - clock_cyccnt_holder64)*MCP33131D_SECS_PER_CYCLE;
    
    if (single_counter==1) {
      Serial.println("START SINGLE VOLTS");
    }
    if (verbose) {
      printCounter("Single volts: counter", single_counter, single_knts, false );
    }
    Serial.print("ADC: ");Serial.print(t,8);
    Serial.print(" ");Serial.println(single_val*MCP33131D_LSB, 6 );
    if (single_counter == single_knts) {
      Serial.println("END SINGLE");
    }
  }

  // -------------------------------------------
  static void triggeredSingleISR_()
  {
    if (!single_knts||(single_counter < single_knts)) {
      single_val = read();
      single_cyccnt = ARM_DWT_CYCCNT;
      single_counter++;
      
      if (single_knts && (single_counter >= single_knts)) {
	stopTrigger_();
      }

      if (single_callback) {
	(*single_callback)();
      }
    }
  }

  bool triggeredSingle_(uint8_t pin, uint8_t mode, void (*callback)(), unsigned int knts, bool start=true, bool wait=false)
  {

    Serial.println("Setup: triggeredSingle");
    
    if (!setupSingles_(knts,callback)) {
      return false;
    }
    if (!setupTrigger_(pin,mode,triggeredSingleISR_)) {
      return false;
    }

    single_callback = callback;

    if (start) {
      
      startTrigger_();
    
      if (wait && knts) {
	return waitTriggered_(&single_counter, &single_knts);
      }
    }
    
    return true;
  }

  mcp33131d_t triggeredSingle(uint8_t pin, uint8_t mode, unsigned int knts=1, bool start=true, bool wait=true)
  {
    if (!triggeredSingle_(pin, mode, printSingle, knts, start, wait)) {
      return 0;
    }
    return single_val;
  }    

  double triggeredSingleVolts(uint8_t pin, uint8_t mode, unsigned int knts=1, bool start=true, bool wait=true) {
    if (!triggeredSingle_(pin, mode, printSingleVolts, knts, start, wait)) {
      return 0;
    }
    return single_val*MCP33131D_LSB;
  }

  // ============================================
  // Clocked single
  static void clockedSingleISR_()
  {
    //printCounter( "clockedSumISR_ ", sum_counter, sum_knts);
    if (single_counter < single_knts) {
      single_val = read();
      single_cyccnt = ARM_DWT_CYCCNT;
      single_counter++;
      
      if (single_counter >= single_knts) {
        stopClock_();
      }

      if (single_callback) {
	(*single_callback)();
      }
    }
  }

  bool setupClockedSingle_(unsigned int knts, unsigned long usecs, void (*callback)())
  {
    errorflag = false;
    
    if (!setupSingles_(knts,callback)) {
      errorflag = true;
      return false;
    }
    if (!setupClock_(usecs,clockedSingleISR_)) {
      errorflag = true;
      return false;
    }

    return true;    
  }

  // this can be called from a trigger
  static void startClockedSingle_()
  {
    single_counter = 0;
    startClock_();
  }

  bool clockedSingle_(unsigned int knts, unsigned long usecs, void (*callback)(), bool wait=false)
  {
    Serial.println("Setup: ClockedSingle");
    
    if (!setupClockedSingle_(knts, usecs, callback)) {
      return false;
    }

    startClockedSingle_();

    if (wait) {
      if (waitClocked_(&single_counter, &single_knts)) {
	errorflag = true;
	return false;
      }
    }

    return true;
  }

  mcp33131d_t clockedSingle(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true)
  {
    if (!clockedSingle_(knts,usecs,printSingle,wait)) {
      return 0;
    }
    return single_val;
  }

  double clockedSingleVolts(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true)
  {
    
    if (!clockedSingle_(knts,usecs,printSingleVolts,wait)) {
      return 0;
    }
    return single_val*MCP33131D_LSB;
  }

  // -------------------------------------
  bool triggerClockedSingle_(unsigned int length,unsigned long usecs, void (*callback)(), 
			     uint8_t pin, uint8_t mode, unsigned int knts,
			     bool start=true, bool wait=false, float timeout=0)
  {
    Serial.println("Setup: triggerClockedSingle");
    
    if (!setupClockedSingle_(length, usecs, callback)) {
      return false;
    }

    if (!setupTrigger_(pin, mode, startClockedSingle_)) {
      return false;
    }

    frame_counter = 0;
    frame_knts = knts;
    
    if (start) {
      Serial.println("# triggerClockedSingle_ calling startTrigger_");
      startTrigger_();

      if (wait) {
	return waitTriggered_(&frame_counter, &frame_knts, timeout);
      }
    }
    return true;    
  }
      
  bool triggerClockedSingle(unsigned int knts,unsigned long usecs, uint8_t pin, uint8_t mode, bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSingle_(knts,usecs,printSingle,pin,mode,0,start,wait, timeout);
  }

  bool triggerClockedSingleVolts(unsigned int knts,unsigned long usecs, uint8_t pin, uint8_t mode, bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSingle_(knts,usecs,printSingleVolts,pin,mode,0,start,wait,timeout);
  }
    
  /* ===================================================================
   * Summing Reads
  */
  bool setupSum_(unsigned int knts, void (*callback)())
  {
    if (!knts) {
      Serial.println("Error: setupSum_ 0 knts");
      return false;
    }
    sum_knts = knts;
    sum_counter = 0;
    sum_val = 0;
    sum_callback = callback;

    printSumSetup();
    
    return true;
  }

  void printSumSetup()
  {
    Serial.print("Sum: "); Serial.print(sum_knts);
    Serial.print( " callback " );
    if (pointermatch(sum_callback,printSum)) {
      Serial.print("printSum");
    }
    else if (pointermatch(sum_callback,printSumVolts)) {
      Serial.print("printSumVolts");
    }
    else if (pointermatch(sum_callback,printAverage)) {
      Serial.print("printAverage");
    }
    else if (pointermatch(sum_callback,printAverageVolts)) {
      Serial.print("printAverageVolts");
    }
    Serial.println("");
  }
  
  static void printSum()
  {
    printCounter("Res: Sum, counter", sum_counter, sum_knts, false );
    Serial.print( " => "); Serial.println( sum_val );    
  }

  static void printAverage()
  {
    float avg = sum_counter? (float) sum_val/sum_counter : 0.0;
    printCounter("Res: Average, counter", sum_counter, sum_knts, false );
    Serial.print( " => "); Serial.println( avg );
  }

  static void printSumVolts() {
    float volts = sum_val*MCP33131D_LSB;
    printCounter("Res: Sum Volts, counter", sum_counter, sum_knts, false );
    Serial.print( " => "); Serial.println( volts, 6 );
  }
  static void printAverageVolts() {
    float volts = sum_val*MCP33131D_LSB;
    float avg = sum_counter? volts/sum_counter : 0.0;
    printCounter("Res: Average Volts, counter", sum_counter, sum_knts, false );
    Serial.print( " => "); Serial.println( avg, 6 );
  }

  // -------------------------------------------
  static void triggeredSumISR_() {
    if (sum_counter < sum_knts) {
      sum_val += read();
      sum_counter++;
      if (sum_counter >= sum_knts) {
	if (interrupt_active) {
	  stopTrigger_();
	}
	if (sum_callback) {
	  (*sum_callback)();
	}
      }
    }
  }
  
  bool triggeredSum_(uint8_t pin, uint8_t mode, void (*callback)(), unsigned int knts, bool start=true, bool wait=true, float timeout=0) {

    Serial.println("Setup: triggeredSum");
    
    if (!setupSum_(knts,callback)) {
      return false;
    }
    if (!setupTrigger_(pin,mode,triggeredSumISR_)) {
      return false;
    }

    if (start) {
      startTrigger_();
    
      if (wait) {
	return waitTriggered_(&sum_counter, &sum_knts, timeout);
      }
    }
    
    return true;
  }

  bool triggeredSum(uint8_t pin, uint8_t mode, unsigned int knts, bool start=true, bool wait=true, float timeout=0) {
    return triggeredSum_(pin, mode, printSum, knts, start, wait, timeout);
  }    

  bool triggeredSumVolts(uint8_t pin, uint8_t mode, unsigned int knts=1, bool start=true, bool wait=true, float timeout=0) {
    return triggeredSum_(pin, mode, printSumVolts, knts, start, wait, timeout);
  }

  bool triggeredAverage(uint8_t pin, uint8_t mode, unsigned int knts=1, bool start=true, bool wait=true, float timeout=0) {
    return triggeredSum_(pin, mode, printAverage, knts, start, wait, timeout);
  }    

  bool triggeredAverageVolts(uint8_t pin, uint8_t mode, unsigned int knts=1, bool start=true, bool wait=true, float timeout=0) {
    return triggeredSum_(pin, mode, printAverageVolts, knts, start, wait, timeout);
  }
  
  // ============================================
  // Clocked sum
  static void clockedSumISR_()
  {
    //printCounter( "clockedSumISR_ ", sum_counter, sum_knts);
    if (sum_counter < sum_knts) {
      sum_val += read();
      sum_counter++;
      if (sum_counter >= sum_knts) {
	if (clock_active) {
          stopClock_();
	  if (sum_callback) {
	    (*sum_callback)();
	  }
	}
      }
    }
  }

  bool setupClockedSum_(unsigned int knts, unsigned long usecs, void (*callback)())
  {
    errorflag = false;
    
    if (!setupSum_(knts,callback)) {
      errorflag = true;
      return false;
    }
    if (!setupClock_(usecs,clockedSumISR_)) {
      errorflag = true;
      return false;
    }

    return true;
  }

  // can be called from interrupt
  static void startClockedSum_()
  {
    sum_counter = 0;
    startClock_();
  }
  
  bool clockedSum_(unsigned int knts, unsigned long usecs, void (*callback)(), bool wait=false)
  {
    Serial.println("Setup: clockedSum");
    
    if (!setupClockedSum_(knts, usecs, callback)) {
      return false;
    }

    startClockedSum_();
  
    if (wait) {
      if (waitClocked_(&sum_counter, &sum_knts)) {
	errorflag = true;
	return false;
      }
    }

    return true;
  }

  mcp33131d_t clockedSum(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true)
  {
    if (!clockedSum_(knts,usecs,printSum,wait)) {
      return 0;
    }
    return sum_val;
  }

  double clockedSumVolts(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true) {
    if (!clockedSum_(knts,usecs,printSumVolts,wait)) {
      return 0;
    }
    return sum_val*MCP33131D_LSB;
  }

  double clockedAverage(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true) {
    if (!clockedSum_(knts,usecs,printAverage,wait)) {
      return 0;
    }
    return sum_counter ? (float) sum_val/sum_counter:0.;
  }

  double clockedAverageVolts(unsigned int knts=1000, unsigned long usecs=1000, bool wait=true) {
    if (!clockedSum_(knts,usecs,printAverageVolts,wait)) {
      return 0;
    }
    return sum_counter ? (sum_val * MCP33131D_LSB)/sum_counter:0.;
  }

  // -------------------------------------------------------
  bool triggerClockedSum_(unsigned int length, unsigned long usecs, void (*callback)(),
			  uint8_t pin, uint8_t mode, unsigned int knts, 
			  bool start=true, bool wait=false, float timeout=0)
  {
    Serial.println("Setup: triggerClockedSum");
    
    if (!setupClockedSum_(length, usecs, callback)) {
      return false;
    }

    if (!setupTrigger_(pin,mode,startClockedSum_)) {
      return false;
    }

    if (start) {
      startTrigger_();

      if (wait) {
	return waitTriggered_(&sum_counter,&sum_knts,timeout);
      }
    }

    return true;
  }

  bool triggerClockedSum(unsigned int length, unsigned long usecs,
			 uint8_t pin, uint8_t mode, unsigned int knts,
			 bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSum_(length, usecs, printSum, pin, mode, knts, start, wait, timeout);
  }
  
  bool triggerClockedSumVolts(unsigned int length, unsigned long usecs,
			      uint8_t pin, uint8_t mode, unsigned int knts,
			      bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSum_(length, usecs, printSumVolts, pin, mode, knts, start, wait, timeout);
  }
  
  bool triggerClockedAverage(unsigned int length, unsigned long usecs,
			      uint8_t pin, uint8_t mode, unsigned int knts,
			      bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSum_(length, usecs, printAverage,pin, mode, knts, start, wait, timeout);
  }
  
  bool triggerClockedAverageVolts(unsigned int length, unsigned long usecs,
			      uint8_t pin, uint8_t mode, unsigned int knts,
			      bool start=true, bool wait=false, float timeout=0)
  {
    return triggerClockedSum_(length, usecs, printAverageVolts, pin, mode, knts, start, wait, timeout);
  }
  
};

#endif
