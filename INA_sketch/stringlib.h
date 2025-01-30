/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com

*/

#ifndef STRINGLIB_H
#define STRINGLIB_H

int stringFormat(char *buffer, const char *format, ...);

unsigned int stringLower( char *s, char *out, int maxlen );

unsigned int stringTrim( char *s, int maxlen );

unsigned int wordLength( char *s );

char *nextWord( char *s );

char *skipSpace(char *s);

unsigned int countWords( char *s );
  
char *startsWith( char *s, const char *key );
  
char *parseInt( char *s, int *ip );
char *parseUint( char *s, unsigned int *u );
char *parseUint32( char *s, uint32_t *u );
  
char *parseLong( char *s, long *ip );
char *parseUlong( char *s, unsigned long *u );

char *parseFlt( char *s, float *p );
  
unsigned int parseUints( char *pc, unsigned int *p, unsigned int nmax );
  
unsigned int parseUint32s( char *pc, uint32_t *p, unsigned int nmax );
  
unsigned int parseFlts( char *pc, float *p, unsigned int nmax );
  
// parse each word as usecs, else as float seconds, convert to usecs
unsigned int parseUsecs( char *pc, uint32_t *p, unsigned int nmax );

#endif
