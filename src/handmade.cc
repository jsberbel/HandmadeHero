/*
 * ========================================================================
 * File: handmade.cc
 * Revision: 1.0
 * Creator: Jordi Serrano Berbel
 * Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
 * =======================================================================
 */

#include "handmade.hh"

internal void
GameOutputSound(GameSoundOutputBuffer *soundBuffer, const int toneHz)
{
	local_persist real32 tSine;
	const int16 toneVolume	{ 3000 };
	const int wavePeriod	{ soundBuffer->samplesPerSecond / toneHz };

	int16 *&sampleOut = soundBuffer->samples;
	for (int sampleIndex{ 0 };
		 sampleIndex < soundBuffer->sampleCount;
		 ++sampleIndex)
	{
		const real32 sineValue	{ sinf(tSine) };
		const int16 sampleValue	{ int16(sineValue * toneVolume) };
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;

		tSine += 2.0f*PI32*1.0f / real32(wavePeriod);
	}
}

internal void
RenderWeirdGradient(GameOffscreenBuffer *buffer, const int blueOffset, const int greenOffset)
{
	//const int &width{ buffer.width };
	//const int &height{ buffer.height };

	uint8 *row{ static_cast<uint8*>(buffer->memory) };
	for (int y{ 0 }; y < buffer->height; ++y)
	{
		uint32 *pixel{ reinterpret_cast<uint32*>(row) };
		for (int x{ 0 }; x < buffer->width; ++x)
		{
			// Pixel (32 bits)
			// Memory:		BB GG RR xx
			// Register:	xx RR GG BB
			uint8 blue	{ uint8(x + blueOffset) };
			uint8 green	{ uint8(y + greenOffset) };

			*pixel++ = (green << 8) | blue;
		}
		row += buffer->pitch;
	}
}

internal GameMemory*
GameStartUp(void)
{
	GameMemory *memory = new GameMemory;
	if (memory)
	{
		/*memory->toneHz = 256;
		memory->blueOffset = 0;
		memory->greenOffset = 0;*/
	}
	return memory;
}

internal void
GameShutDown(GameMemory *memory)
{
	delete memory;
}

void GameUpdateAndRender(GameMemory *memory, 
						 GameInput *input,
						 GameOffscreenBuffer *offscreenBuffer,
						 GameSoundOutputBuffer *soundBuffer)
{
	ASSERT(sizeof(GameState) <= memory->permanentStorageSize);

	GameState *gameState = (GameState*)memory->permanentStorage;
	if (!memory->isInitialized)
	{
		const char *filename = __FILE__;

		const Debug::ReadFileResult file = Debug::PlatformReadEntireFile(filename);
		if (file.data)
		{
			Debug::PlatformWriteEntireFile("test.out", file.dataSize, file.data);
			Debug::PlatformFreeFileMemory(file.data);
		}

		gameState->toneHz = 256;
		gameState->blueOffset = 0;
		gameState->greenOffset = 0;
		memory->isInitialized = true;
	}

	for (int controller_index = 0;
		 controller_index < ARRAY_COUNT(input->controllers);
		 ++controller_index)
	{
		GameControllerInput *controller = GetController(input, controller_index);
		if (controller->isAnalog)
		{
			// NOTE: Use analog movement tuning
			gameState->blueOffset += int(4.f*controller->stickAverageX);
			gameState->toneHz = 256 + int(128.f*controller->stickAverageY);
		}
		else
		{
			// NOTE: Use digital movement tuning
			if (controller->actionLeft.endedDown)
			{
				gameState->blueOffset -= 1;
			}

			if (controller->actionRight.endedDown)
			{
				gameState->blueOffset += 1;
			}
		}

		// Input.AButtonEndedDown;
		// Input.AButtonHalfTransitionCount;
		if (controller->actionDown.endedDown)
		{
			gameState->greenOffset += 1;
		}
	}

	// TODO: Allow sample offsets here for more robust platform options
	GameOutputSound(soundBuffer, gameState->toneHz);
	RenderWeirdGradient(offscreenBuffer, gameState->blueOffset, gameState->greenOffset);
}
