/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com

*/


#ifndef READLINE_H
#define READLINE_H

#define READLINE_BUFFERSIZE 256
extern char readline_buffer[READLINE_BUFFERSIZE];
extern int readline_index;
extern int readline_length;

char *readLine( );

#endif
