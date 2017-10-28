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

#pragma once

#define HANDMADE_WIN32 1
#define HANDMADE_INTERNAL 1
#define HANDMADE_SLOW 1

#ifdef HANDMADE_SLOW
 // TODO: Complete assertion macro - don't worry everyone!
#define ASSERT(condition) if (!(condition)) { *(int*)0 = 0;}
#else
#define ASSERT(condition)
#endif

#define KYLOBYTES(value) ((value)*1024LL)
#define MEGABYTES(value) (KYLOBYTES(value)*1024LL)
#define GIGABYTES(value) (MEGABYTES(value)*1024LL)
#define TERABYTES(value) (GIGABYTES(value)*1024LL)

#define ARRAY_COUNT(arr) ((sizeof (arr)) / (sizeof (arr)[0]))
 // TODO: swap, min, max ... macros???

inline uint32
SafeTruncateUInt64(uint64 value)
{
	// TODO: Defines for maximum values
	ASSERT(value <= 0xFFFFFFFF);
	return uint32(value);
}

// NOTE: Services that the platform layer provides to the game
#ifdef HANDMADE_INTERNAL
/*
 * IMPORTANT:
 * These are NOT for doing anything in the shipping game - they are
 * blocking and the write doesn't protect against lost data!
 */
namespace Debug
{
	struct ReadFileResult
	{
		uint32 dataSize;
		void *data;
	};
	ReadFileResult PlatformReadEntireFile(const char *filename);
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
struct GameOffscreenBuffer
{
	// NOTE: Pixels are always 32-bits wide, Memory Order BB GG RR XX
	void *memory;
	int32 width;
	int32 height;
	int32 pitch;
};

struct GameSoundOutputBuffer
{
	int32 samplesPerSecond;
	int32 sampleCount;
	int16 *samples;
};

struct GameButtonState
{
	int32 halfTransitionCount;
	bool32 endedDown;
};

struct GameControllerInput
{
	bool32 isConnected;
	bool32 isAnalog;
	real32 stickAverageX;
	real32 stickAverageY;

	union
	{
		GameButtonState buttons[12];
		struct
		{
			GameButtonState moveUp;
			GameButtonState moveDown;
			GameButtonState moveLeft;
			GameButtonState moveRight;

			GameButtonState actionUp;
			GameButtonState actionDown;
			GameButtonState actionLeft;
			GameButtonState actionRight;

			GameButtonState leftShoulder;
			GameButtonState rightShoulder;

			GameButtonState back;
			GameButtonState start;
		};
	};
	
};

//struct game_clocks
//{
//	real32 secondsElapsed;
//};

struct GameInput
{
	// TODO: Insert clock values here.
	GameControllerInput controllers[5];
};
inline GameControllerInput *GetController(GameInput *input, int controllerIndex)
{
	ASSERT(controllerIndex < ARRAY_COUNT(input->controllers));
	return &input->controllers[controllerIndex];
}

struct GameMemory
{
	bool32 isInitialized;

	uint64 permanentStorageSize;
	void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

	uint64 transientStorageSize;
	void *transientStorage; // NOTE: REQUIRED to be cleared to zero at startup
};

void GameUpdateAndRender(GameMemory *memory,
						 GameInput *input,
						 GameOffscreenBuffer *offscreenBuffer,
						 GameSoundOutputBuffer *soundBuffer);

struct GameState
{
	int32 toneHz;
	int32 greenOffset;
	int32 blueOffset;
};