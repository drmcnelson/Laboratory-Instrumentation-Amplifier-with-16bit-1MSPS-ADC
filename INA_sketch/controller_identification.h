/********************************************************
 * @file controller_identification.h
 * @copyright Copyright (c) 2024 Mitchell C Nelson, PhD
 * @brief identify which controller we are running on
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

#ifndef CONTOLLER_IDENTIFICATION_H
#define CONTOLLER_IDENTIFICATION_H

#if defined(ARDUINO_TEENSY41)||defined(ARDUINO_TEENSY40)

#ifndef IS_TEENSY4
#define IS_TEENSY4
#endif

#ifndef IS_TEENSY
#define IS_TEENSY
#endif

// ---------------------------------------------
#elif defined(ARDUINO_TEENSY36)||defined(ARDUINO_TEENSY35)||defined(ARDUINO_TEENSY32)

#ifndef IS_TEENSY3
#define IS_TEENSY3
#endif

#ifndef IS_TEENSY
#define IS_TEENSY
#endif

// ---------------------------------------------
#elif defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)

#ifndef IS_ARDUINO_UNO_R4
#define IS_ARDUINO_UNO_R4
#endif

#ifndef IS_ARDUINO_UNO
#define IS_ARDUINO_UNO
#endif

#ifndef IS_ARDUINO
#define IS_ARDUINO
#endif

// ---------------------------------------------
#else
#error "not recognized as a supported board"

#endif

#endif
