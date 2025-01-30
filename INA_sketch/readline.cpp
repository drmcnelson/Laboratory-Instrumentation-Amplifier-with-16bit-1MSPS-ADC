/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com

 */

#include <Arduino.h>

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include "readline.h"

#define READLINE_BUFFERSIZE 256
char readline_buffer[READLINE_BUFFERSIZE] = {0};
int readline_index = 0;
int readline_length = 0;

char *readLine( ) {

  char c;
  
  while ( Serial.available() ) {
    
    c = Serial.read();

    // -1 is error, 0 is improper termination
    if (c <= 0) {

      Serial.print( "Error: readline received " );
      Serial.print( c );
      if (readline_index) {
	Serial.print( ":" );
	Serial.println( readline_buffer );
      }
      Serial.println( "" );
      
      readline_length = 0;
      readline_index = 0;
      return NULL;
    }
    
    // EOL is ctl-character or semi-colon
    // note that doing this here allows zero length lines and eol at end of buffer
    if (iscntrl(c) || c == ';') {

      // terminate the string
      readline_buffer[readline_index] = 0;
      readline_length = readline_index;
      readline_index = 0;

      // return pointer to the buffer
      return &readline_buffer[0];
    }

    // skip leading spaces
    if ( !readline_index && isspace(c) ) {
      Serial.println( "controller readline: start of line with space character, skipping" );
      continue;
    }

    // append the character
    readline_buffer[readline_index++] = c;

    // too long now?
    if ( readline_index >= READLINE_BUFFERSIZE ) {
      Serial.println( (char *)"Error: buffer overflow" );
      readline_length = 0;
      readline_index = 0;
      return NULL;
    }
    
#ifdef DIAGNOSTICS_RCV
    Serial.print( '#' );
    Serial.println( readline_buffer );
#endif
  }

  // Not ready to return the string
  return NULL;
}
