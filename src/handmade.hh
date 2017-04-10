#pragma once
/* 
 * ========================================================================
 * File: handmade.hh
 * Revision: 1.0
 * Creator: Jordi Serrano Berbel
 * Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
 * =======================================================================
 */

 /*
 * NOTE:
 *
 * HANDMADE_INTERNAL:
 * 0 - Build for public release
 * 1 - Build for developer only
 *
 * HANDMADE_SLOW:
 * 0 - Not slow code allowed! Developer only
 * 1 - Slow code welcome.
 */

#define HANDMADE_WIN32 1
#define HANDMADE_INTERNAL 1
#define HANDMADE_SLOW 1

#ifdef HANDMADE_SLOW
 // TODO: Complete assertion macro - don't worry everyone!
#define Assert(condition) if (!(condition)) { *(int*)0 = 0;}
#else
#define Assert(condition)
#endif

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

#define ArrayCount(arr) ((sizeof (arr)) / (sizeof (arr)[0]))
 // TODO: swap, min, max ... macros???

inline uint32
SafeTruncateUInt64(uint64 value)
{
	// TODO: Defines for maximum values
	Assert(value <= 0xFFFFFFFF); // 0xFFFF ??
	return uint32(value);
}

// NOTE: Services that the platform layer provides to the game
#ifdef HANDMADE_INTERNAL
/*
 * IMPORTANT:
 * These are NOT for doing anything in the shipping game - they are
 * blocking and the write doesn't protect against lost data!
 */
namespace DEBUG
{
	struct read_file_result
	{
		uint32 dataSize;
		void *data;
	};
	read_file_result PlatformReadEntireFile(const char *filename);
	bool32 PlatformWriteEntireFile(const char *filename, uint32 memorySize, void *memory);
	void PlatformFreeFileMemory(void *memory);
}
#endif

/*
 * NOTE: Services that the game provides to the platform layer.
 * (this may expand in the future - sound on separate thread, etc.)
 */

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use

// TODO: In the future, rendering _specifically_ will become a three-tiered abstraction!!!
struct game_offscreen_buffer
{
	// NOTE: Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
	void *memory;
	int32 width;
	int32 height;
	int32 pitch;
};

struct game_sound_output_buffer
{
	int32 samplesPerSecond;
	int32 sampleCount;
	int16 *samples;
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

//struct game_clocks
//{
//	real32 secondsElapsed;
//};

struct game_input
{
	// TODO: Insert clock values here.
	game_controller_input controllers[4];
};

struct game_memory
{
	bool32 isInitialized;

	uint64 permanentStorageSize;
	void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

	uint64 transientStorageSize;
	void *transientStorage; // NOTE: REQUIRED to be cleared to zero at startup
};

void GameUpdateAndRender(game_memory &memory,
						 game_input &input,
						 game_offscreen_buffer &offscreenBuffer,
						 game_sound_output_buffer &soundBuffer);

struct game_state
{
	int32 toneHz;
	int32 greenOffset;
	int32 blueOffset;
};