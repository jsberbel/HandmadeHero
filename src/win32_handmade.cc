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
global_var Win32::offscreen_buffer globalBackBuffer;
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

internal DEBUG::read_file_result
DEBUG::PlatformReadEntireFile(const char *filename)
{
	read_file_result result {};

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
DEBUG::PlatformWriteEntireFile(const char *filename, uint32 memorySize, void *memory)
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
DEBUG::PlatformFreeFileMemory(void *memory)
{
	if (memory)
		VirtualFree(memory, 0, MEM_RELEASE);
}

internal void
Win32::LoadXInput(void)
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
Win32::InitDSound(const HWND &window, int32 samplesPerSecond, int32 bufferSize)
{
	HMODULE dSoundLibrary { LoadLibraryA("dsound.dll") };

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
Win32::ClearSoundBuffer(Win32::sound_output &soundOutput)
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
Win32::FillSoundBuffer(Win32::sound_output &soundOutput, DWORD byteToLock, DWORD bytesToWrite,
					   game_sound_output_buffer &sourceBuffer)
{
	VOID *region1, *region2;
	DWORD region1Size, region2Size;
	if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, 
											  &region1, &region1Size, 
											  &region2, &region2Size, 
											  0)))
	{
		DWORD region1SampleCount { region1Size / soundOutput.bytesPerSample };
		int16 *dstSample{ static_cast<int16*>(region1) };
		int16 *srcSample { sourceBuffer.samples };
		for (DWORD sampleIndex { 0 }; 
			 sampleIndex < region1SampleCount; 
			 ++sampleIndex)
		{
			*dstSample++ = *srcSample++;
			*dstSample++ = *srcSample++;
			++soundOutput.runningSampleIndex;
		}
		DWORD region2SampleCount { region2Size / soundOutput.bytesPerSample };
		dstSample = static_cast<int16*>(region2);
		for (DWORD sampleIndex { 0 }; 
			 sampleIndex < region2SampleCount;
			 ++sampleIndex)
		{
			*dstSample++ = *srcSample++;
			*dstSample++ = *srcSample++;
			++soundOutput.runningSampleIndex;
		}
		globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal void
Win32::ProcessKeyboardMessage(game_button_state &newState, bool32 isDown)
{
	newState.endedDown = isDown;
	++newState.halfTransitionCount;
}

internal void
Win32::ProcessXInputDigitalButton(DWORD xInputButtonState,
								  game_button_state &oldState, DWORD buttonBit,
								  game_button_state &newState)
{
	newState.endedDown = (xInputButtonState & buttonBit) == buttonBit;
	newState.halfTransitionCount = oldState.endedDown == newState.endedDown ? 1 : 0;
}

internal Win32::window_dimensions&
Win32::GetWindowDimensions(const HWND &window)
{
	static Win32::window_dimensions result {};
	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;
	return result;
}

internal void
Win32::ResizeDIBSection(Win32::offscreen_buffer &buffer, int width, int height)
{
	if (buffer.memory)
		VirtualFree(buffer.memory, 0, MEM_RELEASE);

	buffer.width = width;
	buffer.height = height;

	BITMAPINFOHEADER &bitmapInfoHeader { buffer.info.bmiHeader };
	bitmapInfoHeader.biSize = sizeof bitmapInfoHeader;
	bitmapInfoHeader.biWidth = width;
	bitmapInfoHeader.biHeight = -height;
	bitmapInfoHeader.biPlanes = 1;
	bitmapInfoHeader.biBitCount = 32;
	bitmapInfoHeader.biCompression = BI_RGB;

	const int bytesPerPixel { 4 };
	const SIZE_T bitmapMemorySize { SIZE_T((width*height)*bytesPerPixel) };
	buffer.memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	buffer.pitch = width*bytesPerPixel; //how many bytes to move from one row to the next row
}

internal void
Win32::DisplayBufferInWindow(Win32::offscreen_buffer &buffer, const HDC &deviceContext, int windowWidth, int windowHeight)
{
	StretchDIBits(deviceContext, 
				  0, 0, windowWidth, windowHeight,
				  0, 0, buffer.width, buffer.height,
				  buffer.memory, 
				  &buffer.info, 
				  DIB_RGB_COLORS, SRCCOPY);
}

internal void
ProcessPendingMessages(game_controller_input &keyboardController)
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
				uint32	vKCode { uint32(msg.wParam) }; //virtual-key code of nonsystem key
				bool	wasDown { (msg.lParam & (1 << 30)) != 0 };
				bool	isDown { (msg.lParam & (1 << 31)) == 0 };
				if (wasDown != isDown) {
					switch (vKCode) {
						case 'W':
							break;
						case 'A':
							break;
						case 'S':
							break;
						case 'D':
							break;
						case 'Q':
							Win32::ProcessKeyboardMessage(keyboardController.leftShoulder, isDown);
							break;
						case 'E':
							Win32::ProcessKeyboardMessage(keyboardController.rightShoulder, isDown);
							break;
						case VK_UP:
							Win32::ProcessKeyboardMessage(keyboardController.up, isDown);
							break;
						case VK_DOWN:
							Win32::ProcessKeyboardMessage(keyboardController.down, isDown);
							break;
						case VK_RIGHT:
							Win32::ProcessKeyboardMessage(keyboardController.right, isDown);
							break;
						case VK_LEFT:
							Win32::ProcessKeyboardMessage(keyboardController.left, isDown);
							break;
						case VK_SPACE:
							break;
						case VK_ESCAPE:
							globalRunning = false;
							break;
							//case VK_F4:
							//	if (msg.lParam & (1 << 29)) globalRunning = false; // ALT + F4
							//	break;
						default:;
					}
				}
			} break;
			default:
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
				break;
		}
	}
}

internal LRESULT CALLBACK
Win32::MainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
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
			Assert(!"Keyboard input came in through a non-dispatch message!");
			break;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext { BeginPaint(window, &paint) };
			Win32::window_dimensions dimensions = Win32::GetWindowDimensions(window);
			Win32::DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);
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
	int64 perfCounterFrequency = perfCounterFrequencyResult.QuadPart;

	Win32::LoadXInput();

	WNDCLASS windowClass {};

	Win32::ResizeDIBSection(globalBackBuffer, 1280, 720);

	windowClass.lpfnWndProc = Win32::MainWindowCallback; //process messages sent to window
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

			Win32::sound_output soundOutput {};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.bytesPerSample = sizeof(int16) * 2;
			soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;
			Win32::InitDSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
			Win32::ClearSoundBuffer(soundOutput);
			globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

		#if 1
			int16 *samples = static_cast<int16*>(VirtualAlloc(nullptr, soundOutput.secondaryBufferSize, 
															  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
		#else
			int16 *samples = static_cast<int16 *>(_alloca(soundOutput.secondaryBufferSize));
		#endif

		#ifdef HANDMADE_INTERNAL
			LPVOID baseAddress = reinterpret_cast<LPVOID>(Terabytes(2));
		#else
			LPVOID baseAddress = nullptr;
		#endif

			game_memory gameMemory {};
			gameMemory.permanentStorageSize = Megabytes(64);
			gameMemory.transientStorageSize = Gigabytes(4); // why not 4 ??

			uint64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;

			gameMemory.permanentStorage = static_cast<int16*>(VirtualAlloc(baseAddress, size_t(totalSize),
																		   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
			
			gameMemory.transientStorage = static_cast<uint8 *>(gameMemory.permanentStorage) + gameMemory.permanentStorageSize;
			
			if (samples && gameMemory.permanentStorage && gameMemory.transientStorage)
			{
				game_input input[2] {};
				game_input &newInput = input[0];
				game_input &oldInput = input[1];

				LARGE_INTEGER beginCounter;
				QueryPerformanceCounter(&beginCounter);
				uint64 beginCycleCount { __rdtsc() };

				globalRunning = true;
				while (globalRunning)
				{
					game_controller_input &keyboardController = newInput.controllers[0];
					game_controller_input zeroController {};
					keyboardController = zeroController;

					Win32::ProcessPendingMessages(keyboardController);

					
					DWORD maxControllerCount = XUSER_MAX_COUNT;
					if (maxControllerCount > ArrayCount(newInput.controllers))
					{
						maxControllerCount = ArrayCount(newInput.controllers);
					}

					for (DWORD controllerIndex { 0 }; //loop through all possible input controllers
						 controllerIndex < XUSER_MAX_COUNT;
						 ++controllerIndex)
					{
						game_controller_input *oldController = &oldInput.controllers[controllerIndex];
						game_controller_input *newController = &newInput.controllers[controllerIndex];

						XINPUT_STATE controllerState;
						if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
						{
							//controller plugged in
							XINPUT_GAMEPAD *pad { &controllerState.Gamepad };

							auto up { pad->wButtons & XINPUT_GAMEPAD_DPAD_UP };
							auto down { pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN };
							auto left { pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT };
							auto right { pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT };

							newController->isAnalog = true;
							newController->startX = oldController->endX;
							newController->startY = oldController->endY;

							real32 x = (real32)pad->sThumbLX / (pad->sThumbLX < 0) ? 32768.f : 32767.f;
							newController->minX = newController->maxX = newController->endX = x;

							real32 y = (real32)pad->sThumbLY / (pad->sThumbLY < 0) ? 32768.f : 32767.f;
							newController->minY = newController->maxY = newController->endY = y;

							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->down, XINPUT_GAMEPAD_A,
															newController->down);
							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->right, XINPUT_GAMEPAD_B,
															newController->right);
							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->left, XINPUT_GAMEPAD_X,
															newController->left);
							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->up, XINPUT_GAMEPAD_Y,
															newController->up);
							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->leftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
															newController->leftShoulder);
							Win32::ProcessXInputDigitalButton(pad->wButtons,
															oldController->rightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
															newController->rightShoulder);

							auto start { pad->wButtons & XINPUT_GAMEPAD_START };
							auto back { pad->wButtons & XINPUT_GAMEPAD_BACK };
							auto leftShoulder { pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER };
							auto rightShoulder { pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER };
							auto aButton { pad->wButtons & XINPUT_GAMEPAD_A };
							auto bButton { pad->wButtons & XINPUT_GAMEPAD_B };
							auto xButton { pad->wButtons & XINPUT_GAMEPAD_X };
							auto yButton { pad->wButtons & XINPUT_GAMEPAD_Y };


						}
						else
						{
							//controller not available
						}
					}

					/*XINPUT_VIBRATION vibration {};
					vibration.wLeftMotorSpeed = 60000;
					vibration.wRightMotorSpeed = 60000;
					XInputSetState(0, &vibration);*/

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

					game_sound_output_buffer soundBuffer {};
					soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
					soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
					soundBuffer.samples = samples;

					game_offscreen_buffer offscreenBuffer {};
					offscreenBuffer.memory = globalBackBuffer.memory;
					offscreenBuffer.width = globalBackBuffer.width;
					offscreenBuffer.height = globalBackBuffer.height;
					offscreenBuffer.pitch = globalBackBuffer.pitch;
					GameUpdateAndRender(gameMemory, newInput, offscreenBuffer, soundBuffer);

					if (soundIsValid)
						Win32::FillSoundBuffer(soundOutput, byteToLock, bytesToWrite, soundBuffer);

					Win32::window_dimensions dimensions = Win32::GetWindowDimensions(window);
					Win32::DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);

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

					game_input *temp = &newInput;
					newInput = oldInput;
					oldInput = *temp;
					//Swap();
				}
			}
		}
	}

	return EXIT_SUCCESS;
}