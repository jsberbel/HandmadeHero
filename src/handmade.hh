#pragma once

/* ========================================================================
   File: handmade.h
   Revision: 1.0
   Creator: Jordi Serrano Berbel
   Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
   ======================================================================== */

#include <cstdint>

#define internal		static
#define local_persist	static
#define global_var		static

#define PI32 3.141592265359f

typedef int8_t	 int8;	 // char
typedef int16_t	 int16;	 // short
typedef int32_t	 int32;	 // int
typedef int64_t	 int64;	 // long long

typedef uint8_t	 uint8;	 // uchar
typedef uint16_t uint16; // ushort
typedef uint32_t uint32; // uint
typedef uint64_t uint64; // ulong long

typedef bool	 bool32;
typedef float	 real32; // float
typedef double	 real64; // double

struct game_sound_output_buffer
{
	int32 samplesPerSecond;
	int32 sampleCount;
	int16 *samples;
};

struct game_offscreen_buffer // Pixels are always 32-bit wide, Memory Order BB GG RR XX
{
	void *memory;
	int32 width;
	int32 height;
	int32 pitch;
};

struct game_button_state
{
	int32 halfTransitionCount;
	bool32 endedDown;
};

struct game_controller_input
{
	bool32 isAnalog;

	real32 startX;
	real32 startY;

	real32 minX;
	real32 minY;

	real32 maxX;
	real32 maxY;

	real32 endX;
	real32 endY;

	union
	{
		game_button_state buttons[6];
		struct
		{
			game_button_state up;
			game_button_state down;
			game_button_state left;
			game_button_state right;
			game_button_state leftShoulder;
			game_button_state rightShoulder;
		};
	};
	
};

struct game_clocks
{
	real32 secondsElapsed;
};

struct game_input
{
	game_controller_input controllers[4];
};

struct game_memory
{
	bool32 isInitialized;
	uint64 permanentStorageSize;
	uint64 transientStorageSize;
	void *permanentStorage;
	void *transientStorage;
};

void GameUpdateAndRender(game_memory &memory,
						 game_input &input,
						 game_offscreen_buffer &offscreenBuffer,
						 game_sound_output_buffer &soundBuffer);

struct game_state
{
	int32 toneHz;
	int32 blueOffset;
	int32 greenOffset;
};