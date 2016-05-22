#ifndef HANDMADE_H

/* ========================================================================
   File: handmade.h
   Revision: 0.1
   Creator: Jordi Serrano Berbel
   Notice: (C) Copyright 2016 by Jordi Serrano Berbel. All Rights Reserved.
   ======================================================================== */

#include <cstdint>
#define internal static
#define local_persist static
#define global_var static

typedef int8_t int8; //char
typedef int16_t int16; //short
typedef int32_t int32; //int
typedef int64_t int64; //long long

typedef uint8_t uint8; //uchar
typedef uint16_t uint16; //ushort
typedef uint32_t uint32; //uint
typedef uint64_t uint64; //ulong long

typedef float real32;
typedef double real64;

struct game_offscreen_buffer {
	void *memory;
	int width;
	int height;
	int pitch;
};

void GameUpdateAndRender(game_offscreen_buffer &buffer);

#define HANDMADE_H
#endif