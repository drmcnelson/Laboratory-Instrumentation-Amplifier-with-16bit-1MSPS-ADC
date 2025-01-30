/********************************************************
 * @file MCP33131D_240815.ino
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

/*
  Author:   Mitchell C. Nelson
  Date:     June 1, 2024
  Contact:  drmcnelsonlab@gmail.com

 */

#include "Arduino.h"

#include <SPI.h>

#include <limits.h>

#include <EEPROM.h>

#include "controller_identification.h"

#include "readline.h"

#include "stringlib.h"

#include "controllerpins.h"

#include "MCP33131D.h"

MCP33131D mcp33131d;

#define HASLED

/*
#ifdef IS_TEENSY
#include <ADC.h>
//#include <ADC_util.h>
#include <digitalWriteFast.h>
#endif
*/

//#include "fastisr.h"

#include "eepromlib.h"

#include "mcutemperature.h"


char identifier[128] = {0};

#define BUFFER_LENGTH (4*1024)
mcp33131d_t buffer[BUFFER_LENGTH] = {0};
unsigned int buffer_size = BUFFER_LENGTH *sizeof(mcp33131d_t);
unsigned int buffer_length = BUFFER_LENGTH;
//unsigned int buffer_counter = 0;

bool debug_flag = false;

// ===================================
// CPU Cycles per Usec
#define CYCLES_PER_USEC (F_CPU / 1000000)

// Uncomment for Elapsed time in usecs
elapsedMicros elapsed_usecs;

// ------------------------------------------------------------------
#define thisMANUFACTURER_NAME {'D','R','M','C','N','E','L','S','O','N' }
#define thisMANUFACTURER_NAME_LEN 10

#define thisPRODUCT_NAME {'S','P','_','I','N','A','M','P','1','6','1','M','V','0','1'}
#define thisPRODUCT_NAME_LEN 15

#define thisPRODUCT_SERIAL_NUMBER { 'S','N','0','0','0','0','0','0','0','0','0','1' }
#define thisPRODUCT_SERIAL_NUMBER_LEN 12

extern "C"
{
  struct usb_string_descriptor_struct_manufacturer
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisMANUFACTURER_NAME_LEN];
  };

  usb_string_descriptor_struct_manufacturer usb_string_manufacturer_name = {
    2 + thisMANUFACTURER_NAME_LEN * 2,
    3,
    thisMANUFACTURER_NAME
  };

  // -------------------------------------------------
  
  struct usb_string_descriptor_struct_product
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisPRODUCT_NAME_LEN];
  };

  usb_string_descriptor_struct_product usb_string_product_name = {
    2 + thisPRODUCT_NAME_LEN * 2,
    3,
    thisPRODUCT_NAME
  };

  // -------------------------------------------------
  
  struct usb_string_descriptor_struct_serial_number
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisPRODUCT_SERIAL_NUMBER_LEN];
  };

  usb_string_descriptor_struct_serial_number usb_string_serial_number =
    {
      2 + thisPRODUCT_SERIAL_NUMBER_LEN * 2, 
      3,
      thisPRODUCT_SERIAL_NUMBER
    };
}

// ------------------------------------------------------------------

const char versionstr[] = "INAMP Controller T4 vers 0.1";

const char authorstr[] =  "Patents Pending and (c) 2024 by Mitchell C. Nelson, Ph.D. ";

void blink() {
#ifdef HASLED
  digitalWrite(LED,HIGH);
  delay(500);
  digitalWrite(LED,LOW);
  delay(500);
#else    
  Serial.println( "Error: SPI interface uses the blink ping");
#endif
}


// -----------------------------------------------------------------
#define PRINTBUFFERSIZE 128
char printbuffer[PRINTBUFFERSIZE] = {0};
int serialPrintf(const char *format, ...) {

  int n = 0;

  va_list args;
  va_start(args, format);
  n = vsnprintf(printbuffer, sizeof(printbuffer), format, args);
  va_end(args);

  if ((n > 0)&&(n<(int)sizeof(printbuffer))) {
    Serial.print(printbuffer);
  }
  else {
    Serial.print("Error: serialPrintf conversion error: ");
    Serial.println( format );
  }

  return n;
}


char *parseClock( char *pc, unsigned long *p_usecs, unsigned int *p_knts) {
  char *pc0 = pc;

  if (!(pc=parseUint(pc0,p_knts))) {
    Serial.print("Error: knts not recognized "); Serial.println(pc0);
    return NULL;
  }

  pc0 = pc;
  if (!(pc=parseUlong(pc0,p_usecs))) {
    Serial.print("Error: usecs not recognized "); Serial.println(pc0);
    return NULL;
  }

  if (debug_flag) {
    Serial.print("# Set clock usecs "); Serial.print(*p_usecs);
    Serial.print(" knts "); Serial.println(*p_knts);
  }
  
  return pc;
}

char *parsePinSync(char *pc0, uint8_t *p_pin, uint8_t *p_edge, unsigned int *p_length)
{
  char *pc = pc0;
  
  if (!(pc=parsePinU8(pc,p_pin))) {
    Serial.println(pc0);
    Serial.println("Error: synch pin not recognized");
    return NULL;
  }

  pc = nextWord(pc);
    
  if (!(pc=parseInterruptMode(pc, p_edge))) {
    Serial.println("Error: synch mode not recognized");
    return NULL;
  }

  if ((*p_edge!=RISING)&&(*p_edge!=FALLING)) {
    Serial.println("Error: synch edge not recognized");
    return NULL;
  }

  if (!(pc=parseUint(pc,p_length))) {
    Serial.println("Error: length not recognized");
    return NULL;
  }

  return pc;
}

char *parseTrigger( char *pc, uint8_t *p_pin, uint8_t *p_mode, unsigned int *p_knts ) {
  char *pc0 = pc;
  if (!(pc=parsePinU8(pc,p_pin))) {
    Serial.print("Error: pin not recognized "); Serial.println(pc);
    return NULL;
  }

  pc = nextWord(pc);
  pc0 = pc;
  if (!(pc=parseInterruptMode(pc0,p_mode))) {
    Serial.print("Error: mode not recognized "); Serial.println(pc0);
    return NULL;
  }
  
  pc = nextWord(pc);
  pc0 = pc;
  if (!(pc=parseUint(pc,p_knts))) {
    *p_knts = 1;
    pc = pc0;
    Serial.print("Warning: knts not specified, set to 1, next word "); Serial.println(pc0);
  }
  
  if (debug_flag) {
    Serial.print( "# Set trigger pin"); Serial.print(*p_pin);
    Serial.print( " mode " ); Serial.print( *p_mode );
    Serial.print( " knts " ); Serial.println( *p_knts );
  }
  
  return pc;
}

/* ===================================================================
   Help text
 */
void help() {

  Serial.println("#Microcontroller functions");
  Serial.println("#  version           - report firmware version");
#ifdef IS_TEENSY  
  Serial.println("#  read temperature  - report microcontroller temperature");
#endif
  Serial.println("#  reboot            - reboots the entire board");
  Serial.println("#");
  Serial.println("#Identifier string (63 bytes)");
  Serial.println("#  store identifier <identifier>");
  Serial.println("#  erase identifier");
  Serial.println("#  identifier        - list identifier string");
  Serial.println("#");
  Serial.println("#  ******************************************************************************");
  Serial.println("#MCP33131D (16bit ADC) functions");
  Serial.println("#  configuration     - report configuration, uni/bipolar, 16bits, etc.");
  Serial.println("#  read raw          - read MCP33131D and print as raw value");
  Serial.println("#  read volts        - read MCP33131D and print as voltage");
#ifdef MCP33131D_SIGNED
  Serial.println("#  read              - read MCP33131D and print as signed integer");
#endif
#ifdef MCP33131D_SIGNED
  Serial.println("#  read              - read MCP33131D and print as unsigned integer");
#endif
  Serial.println("#  ------------------------------------------------------------------------------");
  Serial.println("#  clock <knts> <usecs> <action> <format> - clock and report");
  Serial.println("#");
  Serial.println("#    actions:");
  Serial.println("#         average      - report average");
  Serial.println("#         sum          - report sum");
  Serial.println("#         buffer       - read to buffer, report when done");
  Serial.println("#         <none>       - default to read and report single values");
  Serial.println("#");
  Serial.println("#   formats:");
  Serial.println("#        volts         - voltages");
  Serial.println("#        binary        - for buffered reads, 16bit binary transfer");
  Serial.println("#        <default>     - default to integers");
  Serial.println("#");
  Serial.println("#    example:");
  Serial.println("#        clock 10 1000 buffer volts");
  Serial.println("#        clock 10 1000 average volts");
  Serial.println("#");
  Serial.println("#  clock stop        - stop a clocking operatinon");
  Serial.println("#  clock print       - show the clock setup");
  Serial.println("#");
  Serial.println("#  ------------------------------------------------------------------------------");
  Serial.println("#  trigger [keyword] <pin> <mode> [<knt_trig|0>] {action} {format spec}");
  Serial.println("#");
  Serial.println("#     Keywords");
  Serial.println("#       nostart|setup         - see trigger start");
  Serial.println("#       nostart               - see trigger start");
  Serial.println("#       nowait                - see trigger wait ");
  Serial.println("#       timeout <seconds>     - turns on start and wait");
  Serial.println("#       default is start and wait, timeout 0 to wait without timeout");
  Serial.println("#");
  Serial.println("#     pin       - see the pins commands for a list of pins:");
  Serial.println("#     mode      - rising, falling or change");
  Serial.println("#     knt_trig  - number of reads for singles, frames for clocked, 0=forever");
  Serial.println("#");
  Serial.println("#     Actions - single reads");
  Serial.println("#       clock <parameters...>   - as above");
  Serial.println("#       buffer [format spec]    - buffered singlereads");           
  Serial.println("#       average                 - average of single reads");
  Serial.println("#       sum [volts]             - sum of single read");
  Serial.println("#       <none>                  - default to single reads");
  Serial.println("#");
  Serial.println("#       test                    - reports elpased time at each trigger event");
  Serial.println("#");  
  Serial.println("#     Example:");
  Serial.println("#       trig trigger rising 0 clock 100 1000 buffer volts ");  
  Serial.println("#       (on each trigger, clock 100 reads at 1msec intervals, report volts");  
  Serial.println("#");  
  Serial.println("#  trigger stop               - after starting a triggered action, nowait");
  Serial.println("#  trigger start              - after a trigger command with nostart");
  Serial.println("#  trigger wait [timeoutsecs] - after a trigger command with nostart");
  Serial.println("#  trigger print              - display the configured trigger");
  Serial.println("#");  
  Serial.println("#  ------------------------------------------------------------------------------");
  Serial.println("#  Synchronize reads to a pin, buffered");  
  Serial.println("#");  
  Serial.println("#      sync <pin> <rising|falling> <knts> <format specified>");
  Serial.println("#");  
#ifdef IS_TEENSY
  Serial.println("#      see start|stop master clock, below");
#endif
  Serial.println("#");  
  Serial.println("#  ******************************************************************************");
  Serial.println("# Controller Board Pin Functions");
  Serial.println("#");
  Serial.println("#  set pin [n] output|input|pullup|analog|lo|hi|clock <frequency>|noclock");
  Serial.println("#  read pin [n]                     - read pin or all, digital or analog binary");
  Serial.println("#  write pin n high|low|1|0         - caution, sets pin as output");
  Serial.println("#  pulse pin n usecs [high|low|1|0] - caution, sets pin as output");
  Serial.println("#  toggle pin n                     - caution, sets pin as output");
  Serial.println("#");
  Serial.println("#  set pin <n> clock <frequency>    - output a square wave");
  Serial.println("#  set pin <n> noclock              - stop outputing a clock");
  Serial.println("#");
#ifdef IS_TEENSY
  Serial.println("#  start master clock frequency");
  Serial.println("#  stop master clock frequency");
  Serial.println("#");
#endif
  Serial.println("#  ------------------------------------------------------------------------------");
  Serial.println("#Read and average internal analog inputs");
  Serial.println("#  set analog [channel0 channel1 .... ] - select channels for read analog");
  Serial.println("#  read analog [ navgs ]           - reads the selected set");
  Serial.println("#  read analog [ nchannel navgs ]  - read specified channel");
  Serial.println("#");

}


void printVersion() {
  // Patent pending and copyright notice displayed at startup
  Serial.println( versionstr );
  Serial.println( authorstr );
  //Serial.println( salesupportstr );

#ifdef DIAGNOSTICS_CPU 
  Serial.print("F_CPU: "); Serial.print(F_CPU/1e6);  Serial.println(" MHz."); 
  //Serial.print("F_BUS: "); Serial.print(F_BUS/1e6);  Serial.println(" MHz."); 
  Serial.print("ADC_F_BUS: "); Serial.print(ADC_F_BUS/1e6); Serial.println(" MHz.");
#endif

}

/* ===================================================================
   The setup routine runs once when you press reset:
*/

void setup() {

  // LED pin is used by the SPI
#ifdef HASLED
  pinMode(LED, OUTPUT);
  blink();
  delay(2000);
  blink();
#endif
  
  // digital i/o, sync,gate,etc setups
  controller_pins_setup();
  
#ifdef HASLED
  blink();
  delay(1000);
  blink();
  delay(2000);
#endif
  
  // SPI setup for the MCP33131D (ADC)
  mcp33131d.begin();
  
  // Start the serial protocol over USB
  Serial.begin(9600);
  delay(100);

  printVersion();

  // LED pin is used by the SPI
#ifdef HASLED
  blink();
  delay(1000);
  blink();
  delay(1000);
  blink();
  delay(1000);
#endif

}

// the loop routine runs over and over again forever:
void loop() {

  char lowercasebuffer[READLINE_BUFFERSIZE];
  char *pcL = lowercasebuffer;

  char *pc, *pc0, *pc1, *pc2;

  bool recognized = false;

  //  uint8_t pin;
  //  uint8_t mode;
  
  pc0 = readLine();

  // no command, sleep a little bit and then return
  if (!pc0 || !pc0[0] || !stringTrim(pc0,READLINE_BUFFERSIZE)) {
    delay(100);
    return;
  }
    
  stringLower( pc0, pcL, READLINE_BUFFERSIZE );

  if (debug_flag) {
    Serial.print("# rawcommand: ");
    Serial.print("/");
    Serial.print(pc0);
    Serial.println("/");
  }

  /*-----------------------------------------------------------
    Put this at the top, best chance of getting to it if very busy
  */
  if ( startsWith( pcL, "reboot") ) {
    reboot();
  }

  /* --------------------------------------------------------------
   */
  else if ( (pc = startsWith( pcL, "help" )) ) {
    help();
    recognized = true;
  }
     
  /* --------------------------------------------------------------
   */
  else if ( (pc = startsWith( pcL, "version" )) ) {
    printVersion();
    recognized = true;
  }
     
  else if ( (pc = startsWith( pcL, "configuration" )) ) {
    mcp33131d.printConfiguration();
    recognized = true;
  }

  else if ((pc = startsWith( pcL, "store identifier" )) ) {
    if (pc && pc[0] && (pc=nextWord(pc)) && strlen(pc)) {
      storeIdentifier(pc);
      printIdentifier( );
      recognized = true;
    }
  }
  else if ((pc = startsWith( pcL, "erase identifier" )) ) {
    eraseIdentifier( );
    printIdentifier( );
    recognized = true;
  }
  else if ((pc = startsWith( pcL, "identifier" )) ) {
    printIdentifier( );
    recognized = true;
  }
  
  else if ( (pc = startsWith( pcL, "pins" )) ) {
    printPins();
    recognized = true;
  }
  
  else if ((pc = startsWith( pcL, "set debug_flag"))) {
    debug_flag = true;
    recognized = true;
  }
  
  else if ((pc = startsWith( pcL, "clear verbose"))) {
    debug_flag = false;
    recognized = true;
  }
  
  /* -----------------------------------------------------------
     Internal dio pin reporting
  */
  else if ( (pc = startsWith( pcL, "set pin")) ) {

    if ((pc=nextWord(pc)) && setPinStr(pc)) {
      recognized = true;
    }
  }

  else if ( (pc = startsWith( pcL, "read pin")) ) {

    int ipin = 0;
    
    if ((pc=nextWord(pc)) && (pc=parsePin(pc,&ipin)) ) {
      printPin(ipin);
    }

    else {
      printPins();
    }
    recognized = true;

  }

  else if ( (pc = startsWith( pcL, "write pin")) ) {

    if ((pc=nextWord(pc)) && writePinStr(pc)) {
      recognized = true;
    }

  }

  else if ( (pc = startsWith( pcL, "toggle pin")) ) {
    if (togglePinStr(pc)) {
      recognized = true;
    }
  }
  
  else if ( (pc = startsWith( pcL, "pulse pin")) ) {
    if (pulsePinStr(pc)) {
      recognized = true;
    }
  }

#ifdef IS_TEENSY
  
  else if ( (pc = startsWith( pcL, "start master clock")) ) {
    float f;
    if (parseFlt(pc,&f)) {
      Serial.print("# Starting master clock "); Serial.println(f);
      startMasterClock(f);
      recognized = true;
    }
  }
    
  else if ( (pc = startsWith( pcL, "stop master clock")) ) {
    stopMasterClock();
    recognized = true;
  }

#endif
    
  /* -----------------------------------------------------------
     Internal ADC reporting
  */
  else if ( (pc = startsWith( pcL, "set analog channels")) ) {
    unsigned int channels[MAX_ANALOGPINS];
    unsigned int nchannels=0;
    while((nchannels<MAX_ANALOGPINS)&&(pc1 = parseUint(pc, &channels[nchannels]))) {
      nchannels++;
      pc = pc1;
    }
    selectAnalogChannels(channels,nchannels);
    recognized = true;
  }
        
  else if ( (pc = startsWith( pcL, "read analog")) ) {
    unsigned int arg1 = 0;
    unsigned int arg2 = 0;
    double d;
    if (!(pc1 = parseUint( pc, &arg1))) {
      printAnalogSelected(1);
    }      
    else if (!(parseUint( pc1, &arg2 ))) {
      printAnalogSelected(arg1);
    }
    else {
      d = analogReadChannelVoltageAveraged(arg1, arg2);
      Serial.print( "A: " );
      Serial.println(d,6);
    }
    recognized = true;
  }

  /* ------------------------------------------------------------
     Temperature reporting
  */
#ifdef IS_TEENSY
  else if ( (pc = startsWith( pcL, "read temperature")) ) {
    Serial.print( "CHIPTEMPERATURE: " );
    Serial.println( tempmonGetTemp() );
    recognized = true;
  }
#endif

  /* -----------------------------------------------------------
     Instrumentation Amp Input, this has to come after the internal
     functions to avoid conflicts, c.f. read vs read analog (internal)
  */
  else if ((pc = startsWith( pcL, "set verbose"))) {
    mcp33131d.verbose = true;
    recognized = true;
  }
  else if ((pc = startsWith( pcL, "clear verbose"))) {
    mcp33131d.verbose = false;
    recognized = true;
  }
  else if ((pc = startsWith( pcL, "read raw"))) {
    uint16_t uval;

    uval = mcp33131d.read_raw( );

    serialPrintf( "mcp33131d %04x %u\n", uval, uval );

    recognized = true;

  }
  
  else if ((pc = startsWith( pcL, "read volts"))) {
    
    float fval;

    fval = mcp33131d.readVolts( );
    serialPrintf( "mcp33131d read volts => %.6f\n", fval );

    recognized = true;
  }
  
  else if ((pc = startsWith( pcL, "read raw"))) {
    
    uint16_t uval;

    uval = mcp33131d.read_raw( );
    serialPrintf( "mcp33131d read raw => 0x%4x\n", uval );

    recognized = true;
  }

  else if ((pc = startsWith( pcL, "read"))) {
    
    mcp33131d_t val;

    val = mcp33131d.read( );
    Serial.print( "mcp33131d_value: ");Serial.println(val);

    
    recognized = true;
  }

  else if ((pc = startsWith(pcL, "pinsync"))||(pc = startsWith(pcL, "sync"))) {
    unsigned int length = 128;
    uint8_t watchededge = 0;
    uint8_t watchedpin = 0;

    if ((pc=parsePinSync(pc, &watchedpin, &watchededge, &length))) {

      pc = nextWord(pc);

      Serial.print("# watchedpin:");
      Serial.println(watchedpin);
      
      Serial.print("# watchededge:");
      Serial.println(watchededge);
      
      Serial.print("# length:");
      Serial.println(length);
      
      pc = nextWord(pc);
      
      if ( startsWith(pc,"volt")) {
        if (mcp33131d.pinSynchedFrameVolts(watchedpin, watchededge, buffer, length)) {
          recognized = true;
        }
      }        
      else if ( startsWith(pc,"binary")) {
        if (mcp33131d.pinSynchedFrameBinary(watchedpin, watchededge, buffer, length)) {
          recognized = true;
        }
      }
      else {
        if (mcp33131d.pinSynchedFrameFormatted(watchedpin, watchededge, buffer, length)) {
          recognized = true;
        }
      }
    }
  }
  
  else if ((pc = startsWith(pcL, "clock"))) {
    unsigned int knts = 1;
    unsigned long usecs = 10;
    unsigned int length = 128;

    pc = nextWord(pc);

    if (startsWith(pc,"stop")) {
      mcp33131d.stopClock_();
      recognized = true;
    }

    else if (startsWith(pc,"print")) {
      mcp33131d.printClockSetup();
      recognized = true;
    }
    
    else if((pc1=parseClock(pc,&usecs,&knts))) {
      pc = nextWord(pc1);
      if ((pc1=startsWith(pc,"average"))) {
        pc = nextWord(pc1);
        if((pc1=startsWith(pc,"volts"))) {
          mcp33131d.clockedAverageVolts(knts, usecs);
        }
        else {
          mcp33131d.clockedAverage(knts, usecs);
        }
        recognized = true;
      }
      else if ((pc1=startsWith(pc,"sum"))) {
        pc = nextWord(pc1);
        if((pc1=startsWith(pc,"volt"))) {
          mcp33131d.clockedSumVolts(knts, usecs);
        }
        else {
          mcp33131d.clockedSum(knts, usecs);      
        }
        recognized = true;
      }
      else if ((pc1=startsWith(pc,"buffer"))) {
        
        pc = nextWord(pc1);

        if (length > BUFFER_LENGTH) {
          Serial.print("Error: buffer length is too large ");
          Serial.print( knts );
          Serial.print( " max = " );
          Serial.println( BUFFER_LENGTH );
        }
        
        if((pc1=startsWith(pc,"volt"))) {
          mcp33131d.clockBufferVolts(buffer,knts,usecs);
          recognized = true;      
        }
        else if(!pc || !pc[0] || (pc1=startsWith(pc,"binary"))) {
          mcp33131d.clockBufferBinary(buffer,knts,usecs);
          recognized = true;      
        }
        else {
          mcp33131d.clockBufferAscii(buffer,knts,usecs);
          recognized = true;      
        }
      }
      // single reads, report time and result for each, good down to 10usecs
      else if((pc1=startsWith(pc,"volt"))) {
        mcp33131d.clockedSingleVolts(knts, usecs);
        recognized = true;
      }
      else {
        mcp33131d.clockedSingle(knts, usecs);
        recognized = true;
      }
    }
  }
      
  else if ((pc = startsWith(pcL, "trig"))) {
    unsigned int knts = 1;
    uint8_t pin;
    uint8_t mode;

    unsigned long usecs = 1;
    unsigned int length;

    bool start = true;
    bool wait = true;
    float timeout = 0.0;

    pc = nextWord(pc);
    
    if (startsWith(pc, "stop")) {
      Serial.println("stop trigger");
      mcp33131d.stopTrigger_();
      recognized = true;
    }

    else if (startsWith(pc, "start")) {
      Serial.println("start trigger");
      mcp33131d.startTrigger_();
      recognized = true;
    }

    else if ((pc1=startsWith(pc, "wait"))) {
      parseFlt(pc1,&timeout);
      Serial.print("# wait trigger, timeout ");
      Serial.println( timeout );
      mcp33131d.waitTriggered_(timeout);
      recognized = true;
    }

    else if (startsWith(pc, "print")) {
      mcp33131d.printTrigger();
      recognized = true;
    }

    else {

      // Setup only, don't start and therefore, don't wait
      if ((pc1=startsWith(pc, "nostart"))||(pc1=startsWith(pc, "setup"))) {
        Serial.println("# trigger setup, no start");
        start = false;
        wait = false;
        pc = nextWord(pc1);
      }

      // Don't wait, implies start
      if ((pc1=startsWith(pc, "nowait"))) {
        Serial.println("# trigger start without wait");
        start = true;
        wait = false;
        pc = nextWord(pc1);
      }      

      if ((pc1=startsWith(pc, "timeout"))) {
        if ((pc2 = parseFlt(pc1,&timeout))) {
          pc1 = pc2;
        }
        Serial.print("# trigger start wait, timeout ");
        Serial.println( timeout );
        start = true;
        wait = true;
        pc = nextWord(pc1);
      }
      
      // The trigger specification
      if(!(pc1=parseTrigger(pc,&pin,&mode,&knts))) {
        Serial.println("Error: trigger spec not recognized");
      }

      // Do we have a trigger spec
      else if ( !(pc = nextWord(pc1)) || !pc[0] ) {
        Serial.println("Error: trigger spec missing an action specifier");
        Serial.print("# pc1:"); Serial.println(pc1);
        Serial.print("# pc:"); Serial.println(pc);
        
      }

      // Action: test
      else if (startsWith(pc1,"test")) {
        Serial.println("setting trigger test");
        mcp33131d.triggerTest(pin,mode,start,wait,timeout);
        recognized = true;
      }
      
      // Action: clock
      else if ((pc1=startsWith(pc,"clock"))) {
        pc = nextWord(pc1);
        if((pc1=parseClock(pc,&usecs,&length))) {
          pc = nextWord(pc1);

          // clocked average
          if ((pc1=startsWith(pc,"average"))) {
            pc = nextWord(pc1);     
            if((pc1=startsWith(pc,"volts"))) {
              mcp33131d.triggerClockedAverageVolts(length,usecs,pin,mode,knts,start,wait,timeout);
            }
            else {
              mcp33131d.triggerClockedAverage(length,usecs,pin,mode,knts,start,wait,timeout);
            }
            recognized = true;
          }
          
          // clocked sum
          else if ((pc1=startsWith(pc,"sum"))) {
            pc = nextWord(pc1);
            if((pc1=startsWith(pc,"volt"))) {
              mcp33131d.triggerClockedSumVolts(length,usecs,pin,mode,knts,start,wait,timeout);
            }
            else {
              mcp33131d.triggerClockedSum(length,usecs,pin,mode,knts,start,wait,timeout);
            }
            recognized = true;
          }
          
          // clocked buffered reads
          else if ((pc1=startsWith(pc,"buffer"))) {
        
            pc = nextWord(pc1);

            if (length > BUFFER_LENGTH) {
              Serial.print("Error: buffer length is too large ");
              Serial.print( knts );
              Serial.print( " max = " );
              Serial.println( BUFFER_LENGTH );
            }
        
            if((pc1=startsWith(pc,"volt"))) {
              mcp33131d.triggerClockedBufferVolts(buffer, length, usecs, pin, mode, knts, start, wait, timeout);
              recognized = true;          
            }
            else if(!pc || !pc[0] || (pc1=startsWith(pc,"binary"))) {
              mcp33131d.triggerClockedBufferBinary(buffer, length, usecs, pin, mode, knts, start, wait, timeout);
              recognized = true;          
            }
            else {
              mcp33131d.triggerClockedBufferAscii(buffer, length, usecs, pin, mode, knts, start, wait, timeout);
              recognized = true;          
            }
          }

          // clocked single reads, report time and result for each, good down to 10usecs
          else if((pc1=startsWith(pc,"volt"))) {
            mcp33131d.triggerClockedSingleVolts(length,usecs,pin,mode,start,wait,timeout);
            recognized = true;
          }

          else {
            mcp33131d.triggerClockedSingle(length,usecs,pin,mode,start,wait, timeout);
            recognized = true;
          }

        }
      }
      
      else if ((pc1=startsWith(pc,"sync"))||(pc1=startsWith(pc,"pinsync"))) {

        unsigned int length = 128;
        uint8_t watchededge = 0;
        uint8_t watchedpin = 0;

        pc = nextWord(pc1);

        if ((pc=parsePinSync(pc, &watchedpin, &watchededge, &length))) {
        
          if((pc1=startsWith(pc,"volts"))) {
            if (mcp33131d.triggerPinSynchedFrameVolts(watchedpin, watchededge,
                                                      buffer, length, pin, mode, start, wait)) {
              recognized = true;
            }
          }
          else if((pc1=startsWith(pc,"binary"))) {
            if (mcp33131d.triggerPinSynchedFrameBinary(watchedpin, watchededge,
                                                       buffer, length, pin, mode, start, wait)) {
              recognized = true;
            }
          }
          else {
            if (mcp33131d.triggerPinSynchedFrameFormatted(watchedpin, watchededge,
                                                          buffer, length, pin, mode, start, wait)) {
              recognized = true;
            }
          }            
        }        
      }
      
      else if ((pc1=startsWith(pc,"average"))) {
        pc = nextWord(pc1);
        if((pc1=startsWith(pc,"volts"))) {
          mcp33131d.triggeredAverageVolts(pin, mode, knts, start, wait, timeout);
        }
        else {
          mcp33131d.triggeredAverage(pin, mode, knts, start, wait, timeout);      
        }
        recognized = true;
      }
      else if ((pc1=startsWith(pc,"sum"))) {
        pc = nextWord(pc1);
        if((pc1=startsWith(pc,"volts"))) {
          mcp33131d.triggeredSumVolts(pin, mode, knts, start, wait, timeout);
        }
        else {
          mcp33131d.triggeredSum(pin, mode, knts, start, wait, timeout);          
        }
        recognized = true;
      }
      else if ((pc1=startsWith(pc,"buffer"))) {
        pc = nextWord(pc1);
        if((pc1=startsWith(pc,"binary"))) {
          mcp33131d.triggeredBufferBinary(buffer, knts, pin, mode, start, wait, timeout);
        }
        else if((pc1=startsWith(pc,"volts"))) {
          mcp33131d.triggeredBufferVolts(buffer, knts, pin, mode, start, wait, timeout);          
        }
        else {
          mcp33131d.triggeredBufferFormatted(buffer, knts, pin, mode, start, wait, timeout);
        }
        recognized = true;
      }
      else if((pc1=startsWith(pc,"volts"))) {
        Serial.println( "triggered singles volts");
        mcp33131d.triggeredSingleVolts(pin, mode, knts);
        recognized = true;
      }
      else {
        Serial.println( "triggered singles");
        mcp33131d.triggeredSingle(pin, mode, knts);
        recognized = true;
      }
    }
    
  }
      

  /* -----------------------------------------------------------
     Unknown
  */
  if (!recognized) {
    Serial.print( "Error: command not recognized //" );
    Serial.print( (char *) pc0 );
    Serial.println( "//" );
      
  }
  
  // must finish with this if we received a command
  Serial.println( "DONE" );
}


