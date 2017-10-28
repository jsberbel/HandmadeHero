/*
* ========================================================================
* File: win32_handmade.cc
* Revision: 1.0
* Creator: Jordi Serrano Berbel
* Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
* =======================================================================
*/

#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_var static

#define PI32 3.141592265359f

typedef int8_t	 int8;	 // char
typedef int16_t	 int16;	 // short
typedef int32_t	 int32;	 // int
typedef int64_t	 int64;	 // long long

typedef uint8_t	 uint8;	 // uchar
typedef uint16_t uint16; // ushort
typedef uint32_t uint32; // uint
typedef uint64_t uint64; // ulong long

typedef int32	 bool32;
typedef float	 real32; // float
typedef double	 real64; // double

#include "handmade.hh"
#include "handmade.cc"

#include <Windows.h>
#include <stdio.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>

//#pragma comment(lib, "dsound.lib")
//#pragma comment(lib, "dxguid.lib")
//#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#include "win32_handmade.hh"

global_var bool32 globalRunning; //temporary global
global_var Win32::OffscreenBuffer globalBackBuffer;
global_var LPDIRECTSOUNDBUFFER globalSecondaryBuffer;

// ----------------------------------------------------------------------------------------------------	//
// DYNAMIC LINKING OF WINDOWS GAMEPAD CONTROLLERS														//
// ----------------------------------------------------------------------------------------------------	//
// Allows us to avoid the .lib because of platform lacks, limited to Win8, WinVista & DirectX			//
// ----------------------------------------------------------------------------------------------------	//
DWORD WINAPI XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_var auto XInputGetState_ = &XInputGetStateStub;
#define XInputGetState XInputGetState_

DWORD WINAPI XInputSetStateStub(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_var auto XInputSetState_ = &XInputSetStateStub;
#define XInputSetState XInputSetState_
// ----------------------------------------------------------------------------------------------------	//

internal Debug::ReadFileResult
Debug::PlatformReadEntireFile(const char *filename)
{
	ReadFileResult result {};

	HANDLE fileHandle = CreateFileA(filename,
								    GENERIC_READ, // desired access: read, write, both...
								    FILE_SHARE_READ, // share mode
								    nullptr,
								    OPEN_EXISTING, // opens file if exists
								    0, nullptr);
	
	if (fileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx(fileHandle, &fileSize))
		{
			uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
			result.data = VirtualAlloc(nullptr, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (result.data)
			{
				DWORD bytesRead;
				if (ReadFile(fileHandle, result.data, fileSize32, &bytesRead, nullptr) &&
					fileSize32 == bytesRead)
				{
					result.dataSize = fileSize32;
				}
				else
				{
					PlatformFreeFileMemory(result.data);
					result.data = nullptr;
				}
			}
			else
			{
				
			}
		}
		else
		{
			
		}

		CloseHandle(fileHandle);
	}
	else
	{
	}
	
	return result;
}

internal bool32
Debug::PlatformWriteEntireFile(const char *filename, uint32 memorySize, void *memory)
{
	bool32 result = false;

	HANDLE fileHandle = CreateFileA(filename,
								    GENERIC_WRITE, // desired access: read, write, both...
								    0, nullptr,
								    CREATE_ALWAYS, // opens file if exists
								    0, nullptr);

	if (fileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, nullptr))
		{
			result = bytesWritten == memorySize;
		}
		else
		{
		}

		CloseHandle(fileHandle);
	}
	else
	{
	}

	return result;
}

internal void 
Debug::PlatformFreeFileMemory(void *memory)
{
	if (memory)
		VirtualFree(memory, 0, MEM_RELEASE);
}

internal void
Win32LoadXInput(void)
{
	HMODULE xInputLibrary { 
		LoadLibraryA("xinput1_4.dll") // only works on windows 8
	};
	if (!xInputLibrary) 
		xInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	if (!xInputLibrary) 
		xInputLibrary = LoadLibraryA("xinput1_3.dll"); // works on any other windows version

	if (xInputLibrary)
	{
		XInputGetState = reinterpret_cast<decltype(XInputGetState)>(GetProcAddress(xInputLibrary, "XInputGetState"));
		if (!XInputGetState)
			XInputGetState = XInputGetStateStub;
		XInputSetState = reinterpret_cast<decltype(XInputSetState)>(GetProcAddress(xInputLibrary, "XInputSetState"));
		if (!XInputSetState)
			XInputSetState = XInputSetStateStub;
	}
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI direct_sound_create (LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32InitDSound(const HWND &window, const int32 samplesPerSecond, const int32 bufferSize)
{
	const HMODULE dSoundLibrary { LoadLibraryA("dsound.dll") };

	if (dSoundLibrary)
	{
		direct_sound_create* directSoundCreate { 
			reinterpret_cast<direct_sound_create*>(GetProcAddress(dSoundLibrary, "DirectSoundCreate")) 
		};
		
		LPDIRECTSOUND directSound;
		if (directSoundCreate && SUCCEEDED(directSoundCreate(nullptr, &directSound, nullptr)))
		{
			WAVEFORMATEX waveFormat {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = samplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels*waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign*waveFormat.nSamplesPerSec;
			waveFormat.cbSize = 0;

			if (SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC bufferDescription {};
				bufferDescription.dwSize = sizeof bufferDescription;
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				//PRIMARY BUFFER: handle to sound card
				LPDIRECTSOUNDBUFFER primaryBuffer;
				if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, nullptr)))
				{
					HRESULT error = primaryBuffer->SetFormat(&waveFormat);
					if (SUCCEEDED(error))
					{
						OutputDebugStringA("Primary buffer format was set.\n");
					}
				}
			}

			DSBUFFERDESC bufferDescription {};
			bufferDescription.dwSize = sizeof(bufferDescription);
			bufferDescription.dwFlags = 0;
			bufferDescription.dwBufferBytes = bufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;

			HRESULT error = directSound->CreateSoundBuffer(&bufferDescription, &globalSecondaryBuffer, nullptr);
			if (SUCCEEDED(error))
			{
				OutputDebugStringA("Secondary buffer created successfully.\n");
			}
		} else {
			
		}
	}
}

internal void
Win32ClearSoundBuffer(const Win32::SoundOutput &soundOutput)
{
	VOID *region1, *region2;
	DWORD region1Size, region2Size;
	if (SUCCEEDED(globalSecondaryBuffer->Lock(0, soundOutput.secondaryBufferSize, 
											  &region1, &region1Size, 
											  &region2, &region2Size, 
											  0)))
	{
		uint8 *destSample{ static_cast<uint8*>(region1) };
		memset(destSample, 0, region1Size);
		destSample = static_cast<uint8*>(region2);
		memset(destSample, 0, region2Size);
		globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal void
Win32FillSoundBuffer(Win32::SoundOutput *soundOutput, DWORD byteToLock, DWORD bytesToWrite,
				GameSoundOutputBuffer *sourceBuffer)
{
	VOID *region1, *region2;
	DWORD region1Size, region2Size;
	if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, 
											  &region1, &region1Size, 
											  &region2, &region2Size, 
											  0)))
	{
		DWORD region1SampleCount { region1Size / soundOutput->bytesPerSample };
		int16 *dstSample{ static_cast<int16*>(region1) };
		int16 *srcSample { sourceBuffer->samples };
		for (DWORD sampleIndex { 0 }; 
			 sampleIndex < region1SampleCount; 
			 ++sampleIndex)
		{
			*dstSample++ = *srcSample++;
			*dstSample++ = *srcSample++;
			++soundOutput->runningSampleIndex;
		}
		DWORD region2SampleCount { region2Size / soundOutput->bytesPerSample };
		dstSample = static_cast<int16*>(region2);
		for (DWORD sampleIndex { 0 }; 
			 sampleIndex < region2SampleCount;
			 ++sampleIndex)
		{
			*dstSample++ = *srcSample++;
			*dstSample++ = *srcSample++;
			++soundOutput->runningSampleIndex;
		}
		globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal void
Win32ProcessKeyboardMessage(GameButtonState *newState, bool32 isDown)
{
	ASSERT(newState->endedDown != isDown);
	newState->endedDown = isDown;
	++newState->halfTransitionCount;
}

internal void
Win32ProcessXInputDigitalButton(const DWORD xInputButtonState,
						   GameButtonState *oldState, const DWORD buttonBit,
						   GameButtonState *newState)
{
	newState->endedDown = (xInputButtonState & buttonBit) == buttonBit;
	newState->halfTransitionCount = oldState->endedDown == newState->endedDown ? 1 : 0;
}

internal Win32::WindowDimensions&
Win32GetWindowDimensions(const HWND &window)
{
	static Win32::WindowDimensions result {};
	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;
	return result;
}

internal void
Win32ResizeDIBSection(Win32::OffscreenBuffer *buffer, const int width, const int height)
{
	if (buffer->memory)
		VirtualFree(buffer->memory, 0, MEM_RELEASE);

	buffer->width = width;
	buffer->height = height;

	BITMAPINFOHEADER &bitmapInfoHeader { buffer->info.bmiHeader };
	bitmapInfoHeader.biSize = sizeof bitmapInfoHeader;
	bitmapInfoHeader.biWidth = width;
	bitmapInfoHeader.biHeight = -height;
	bitmapInfoHeader.biPlanes = 1;
	bitmapInfoHeader.biBitCount = 32;
	bitmapInfoHeader.biCompression = BI_RGB;

	const int bytesPerPixel { 4 };
	const SIZE_T bitmapMemorySize { SIZE_T((width*height)*bytesPerPixel) };
	buffer->memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	buffer->pitch = width*bytesPerPixel; //how many bytes to move from one row to the next row
}

internal void
Win32DisplayBufferInWindow(const Win32::OffscreenBuffer &buffer, const HDC &deviceContext, const int windowWidth, const int windowHeight)
{
	StretchDIBits(deviceContext, 
				  0, 0, windowWidth, windowHeight,
				  0, 0, buffer.width, buffer.height,
				  buffer.memory, 
				  &buffer.info, 
				  DIB_RGB_COLORS, SRCCOPY);
}

internal real32
Win32ProcessXInputStickValue(const SHORT value, const SHORT deadZoneTreshold)
{
	real32 result = 0;
	if (value < -deadZoneTreshold)
	{
		result = (real32)(value + deadZoneTreshold) / (32768.f - deadZoneTreshold);
	}
	else if (value > deadZoneTreshold)
	{
		result = (real32)(value + deadZoneTreshold) / (32767.f - deadZoneTreshold);
	}
	return result;
}

internal void
Win32ProcessPendingMessages(GameControllerInput *keyboardController)
{
	MSG msg;
	while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		switch (msg.message)
		{
			case WM_QUIT:
				globalRunning = false;
				break;
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP: {
				const uint32 vKCode	{ uint32(msg.wParam) }; //virtual-key code of non-system key
				const bool wasDown { (msg.lParam & (1 << 30)) != 0 };
				const bool isDown { (msg.lParam & (1 << 31)) == 0 };
				if (wasDown != isDown) {
					switch (vKCode) {
						case 'W':
							Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
							break;
						case 'A':
							Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
							break;
						case 'S':
							Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
							break;
						case 'D':
							Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
							break;
						case 'Q':
							Win32ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
							break;
						case 'E':
							Win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
							break;
						case VK_UP:
							Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
							break;
						case VK_DOWN:
							Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
							break;
						case VK_RIGHT:
							Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
							break;
						case VK_LEFT:
							Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
							break;
						case VK_SPACE:
							Win32ProcessKeyboardMessage(&keyboardController->start, isDown);
							break;
						case VK_ESCAPE:
							Win32ProcessKeyboardMessage(&keyboardController->back, isDown);
							break;
						case VK_F4:
							if (msg.lParam & (1 << 29)) // ALT + F4
								globalRunning = false;
							break;
						default:;
					}
				}
			} break;
			default: {
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			}
		}
	}
}

internal LRESULT CALLBACK
MainWindowCallback(const HWND window, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	LRESULT result { 0 };

	switch (msg) 
	{
		case WM_SIZE: {

		} break;
		case WM_CLOSE: {
			globalRunning = false;
		} break;
		case WM_ACTIVATEAPP: {

		} break;
		case WM_DESTROY: {
			globalRunning = false;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
			ASSERT(!"Keyboard input came in through a non-dispatch message!");
			break;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext { BeginPaint(window, &paint) };
			Win32::WindowDimensions dimensions = Win32GetWindowDimensions(window);
			Win32DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);
			EndPaint(window, &paint);
		} break;
		default: {
			result = DefWindowProc(window, msg, wParam, lParam);
		} break;
	}

	return result;
}

int CALLBACK
WinMain(__in HINSTANCE hInstance,
		__in_opt HINSTANCE hPrevInstance, 
		__in_opt LPSTR lpCmdLine,
		__in int nShowCmd)
{
	LARGE_INTEGER perfCounterFrequencyResult;
	QueryPerformanceFrequency(&perfCounterFrequencyResult);
	const int64 perfCounterFrequency = perfCounterFrequencyResult.QuadPart;

	Win32LoadXInput();

	WNDCLASS windowClass {};

	Win32ResizeDIBSection(&globalBackBuffer, 1280, 720);

	windowClass.lpfnWndProc = MainWindowCallback; //process messages sent to window
	windowClass.hInstance = hInstance; //actual process of the app
	windowClass.hbrBackground = HBRUSH(COLOR_BACKGROUND); // A handle to the class background brush
	//windowClass.hIcon
	windowClass.lpszClassName = L"HandmadeHeroWindowClass";
	windowClass.style = CS_HREDRAW | CS_VREDRAW; //repaint the whole window when resized

	if (RegisterClass(&windowClass))
	{
		HWND window { 
			CreateWindow(windowClass.lpszClassName, L"Handmade Hero",
						 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
						 CW_USEDEFAULT, CW_USEDEFAULT,
						 CW_USEDEFAULT, CW_USEDEFAULT,
						 nullptr, nullptr, hInstance, nullptr) 
		};
		if (window)
		{
			HDC deviceContext { GetDC(window) };
			int xOffset { 0 }, yOffset { 0 };

			Win32::SoundOutput soundOutput {};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.bytesPerSample = sizeof(int16) * 2;
			soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;
			Win32InitDSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
			Win32ClearSoundBuffer(soundOutput);
			globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

		#if 1
			int16 *samples = static_cast<int16*>(VirtualAlloc(nullptr, soundOutput.secondaryBufferSize, 
															  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
		#else
			int16 *samples = static_cast<int16 *>(_alloca(soundOutput.secondaryBufferSize));
		#endif

		#ifdef HANDMADE_INTERNAL
			LPVOID baseAddress = reinterpret_cast<LPVOID>(TERABYTES(2));
		#else
			LPVOID baseAddress = nullptr;
		#endif

			GameMemory gameMemory {};
			gameMemory.permanentStorageSize = MEGABYTES(64);
			gameMemory.transientStorageSize = GIGABYTES(4); // why not 4 ??

			uint64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;

			gameMemory.permanentStorage = static_cast<int16*>(VirtualAlloc(baseAddress, size_t(totalSize),
																		   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
			
			gameMemory.transientStorage = static_cast<uint8 *>(gameMemory.permanentStorage) + gameMemory.permanentStorageSize;
			
			if (samples && gameMemory.permanentStorage && gameMemory.transientStorage)
			{
				GameInput input[2] {};
				GameInput *newInput = &input[0];
				GameInput *oldInput = &input[1];

				LARGE_INTEGER beginCounter;
				QueryPerformanceCounter(&beginCounter);
				uint64 beginCycleCount { __rdtsc() };

				globalRunning = true;
				while (globalRunning)
				{
					GameControllerInput *oldKeyboardController = GetController(oldInput, 0);
					GameControllerInput *newKeyboardController = GetController(newInput, 0);
					*newKeyboardController = {};
					newKeyboardController->isConnected = true;
					for (int btnIndex = 0;
						 btnIndex < ARRAY_COUNT(oldKeyboardController->buttons);
						 ++btnIndex)
					{
						newKeyboardController->buttons[btnIndex].endedDown = 
							oldKeyboardController->buttons[btnIndex].endedDown;
					}
					Win32ProcessPendingMessages(newKeyboardController);

					DWORD maxControllerCount = 1 + XUSER_MAX_COUNT;
					if (maxControllerCount > ARRAY_COUNT(newInput->controllers) - 1)
					{
						maxControllerCount = ARRAY_COUNT(newInput->controllers) - 1;
					}

					for (DWORD controllerIndex { 0 }; //loop through all possible input controllers
						 controllerIndex < maxControllerCount;
						 ++controllerIndex)
					{
						DWORD ourControllerIndex = controllerIndex + 1;
						GameControllerInput *oldController = GetController(oldInput, ourControllerIndex);
						GameControllerInput *newController = GetController(newInput, ourControllerIndex);

						XINPUT_STATE controllerState;
						if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
						{
							//controller plugged in
							newController->isConnected = true;

							XINPUT_GAMEPAD *pad { &controllerState.Gamepad };

							newController->isAnalog = true;
							newController->stickAverageX = Win32ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							newController->stickAverageY = Win32ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if (newController->stickAverageX != 0.f ||
								newController->stickAverageY != 0.f)
							{
								newController->isAnalog = true;
							}

							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								newController->stickAverageY = 1.f;
								newController->isAnalog = false;
							}
							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								newController->stickAverageY = -1.f;
								newController->isAnalog = false;
							}
							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								newController->stickAverageX = 1.f;
								newController->isAnalog = false;
							}
							if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								newController->stickAverageX = -1.f;
								newController->isAnalog = false;
							}

							constexpr real32 treshold = 0.5f;
							Win32ProcessXInputDigitalButton((newController->stickAverageX < -treshold) ? 1 : 0,
															&oldController->moveLeft, 1,
															&newController->moveLeft);
							Win32ProcessXInputDigitalButton((newController->stickAverageX > treshold) ? 1 : 0,
															&oldController->moveRight, 1,
															&newController->moveRight);
							Win32ProcessXInputDigitalButton((newController->stickAverageY < -treshold) ? 1 : 0,
															&oldController->moveDown, 1,
															&newController->moveDown);
							Win32ProcessXInputDigitalButton((newController->stickAverageY > treshold) ? 1 : 0,
															&oldController->moveUp, 1,
															&newController->moveUp);

							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->actionDown, XINPUT_GAMEPAD_A,
															&newController->actionDown);
							Win32ProcessXInputDigitalButton(pad->wButtons,
													   &oldController->actionRight, XINPUT_GAMEPAD_B,
													   &newController->actionRight);
							Win32ProcessXInputDigitalButton(pad->wButtons,
													   &oldController->actionLeft, XINPUT_GAMEPAD_X,
													   &newController->actionLeft);
							Win32ProcessXInputDigitalButton(pad->wButtons,
													   &oldController->actionUp, XINPUT_GAMEPAD_Y,
													   &newController->actionUp);
							Win32ProcessXInputDigitalButton(pad->wButtons,
													   &oldController->leftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
													   &newController->leftShoulder);
							Win32ProcessXInputDigitalButton(pad->wButtons,
													   &oldController->rightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
													   &newController->rightShoulder);

							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->start, XINPUT_GAMEPAD_START,
															&newController->start);
							Win32ProcessXInputDigitalButton(pad->wButtons,
															&oldController->back, XINPUT_GAMEPAD_BACK,
															&newController->back);
						}
						else
						{
							//controller not available
							newController->isConnected = false;
						}
					}

					//XINPUT_VIBRATION vibration {};
					//vibration.wLeftMotorSpeed = 60000;
					//vibration.wRightMotorSpeed = 60000;
					//XInputSetState(0, &vibration);

					DWORD byteToLock = 0;
					DWORD targetCursor = 0;
					DWORD bytesToWrite = 0;
					DWORD playCursor = 0;
					DWORD writeCursor = 0;
					bool soundIsValid = false;
					if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
					{
						byteToLock = (soundOutput.runningSampleIndex*soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
						targetCursor =
							((playCursor +
							(soundOutput.latencySampleCount*soundOutput.bytesPerSample)) %
							 soundOutput.secondaryBufferSize);
						if (byteToLock > targetCursor)
						{
							//bytesToWrite = ((playCursor - byteToLock) + soundOutput.secondaryBufferSize) % soundOutput.secondaryBufferSize;
							bytesToWrite = (soundOutput.secondaryBufferSize - byteToLock);
							bytesToWrite += targetCursor;
						}
						else
						{
							bytesToWrite = targetCursor - byteToLock;
						}
						soundIsValid = true;
					}

					GameSoundOutputBuffer soundBuffer {};
					soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
					soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
					soundBuffer.samples = samples;

					GameOffscreenBuffer offscreenBuffer {};
					offscreenBuffer.memory = globalBackBuffer.memory;
					offscreenBuffer.width = globalBackBuffer.width;
					offscreenBuffer.height = globalBackBuffer.height;
					offscreenBuffer.pitch = globalBackBuffer.pitch;
					GameUpdateAndRender(&gameMemory, newInput, &offscreenBuffer, &soundBuffer);

					if (soundIsValid)
						Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);

					Win32::WindowDimensions dimensions = Win32GetWindowDimensions(window);
					Win32DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);

					uint64 endCycleCount { __rdtsc() };

					LARGE_INTEGER endCounter;
					QueryPerformanceCounter(&endCounter);

					uint64 cyclesElapsed { endCycleCount - beginCycleCount };
					int64 counterElapsed { endCounter.QuadPart - beginCounter.QuadPart }; // us
					real64 msPerFrame { (real64(counterElapsed)*1000.0f) / real64(perfCounterFrequency) };
					real64 fps { real64(perfCounterFrequency) / real64(counterElapsed) };
					real64 mcpf { real64(cyclesElapsed) / (1000.0f * 1000.0f) }; // mega cycles per frame

				#if 0
					char buffer[256];
					sprintf_s(buffer, "%fms/f, %ff/s, %fmc/f\n", msPerFrame, fps, mcpf);
					OutputDebugStringA(buffer);
				#endif

					beginCounter = endCounter;
					beginCycleCount = endCycleCount;

					GameInput *temp = newInput;
					newInput = oldInput;
					oldInput = temp;
					//Swap();
				}
			}
		}
	}

	return EXIT_SUCCESS;
}