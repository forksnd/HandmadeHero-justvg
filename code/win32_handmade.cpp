    #include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef float real32;
typedef double real64;

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

// TODO(george): Implement sine ourselves
#include <math.h>
#include "handmade.cpp"

#include <Windows.h>
#include "win32_handmade.h"

#include <Xinput.h>
#include <dsound.h>

#include <stdio.h>

// TODO(george): It is global for bow
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;

#define D_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef D_SOUND_CREATE(d_sound_create);

// NOTE(george): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(george): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void 
Win32LoadInput(void)
{
    // TODO(george): Test this on Windows8
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

	if (!XInputLibrary)
	{
		// TODO(george): Diagnostic		
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}

    if (!XInputLibrary)
    {
        // TODO(george): Diagnostic		
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState){XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState){XInputSetState = XInputSetStateStub;}
        
        // TODO(george): Diagnostic
    }
    else
    {
        // TODO(george): Diagnostic
    }
    
} 

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadzoneThreshold)
{
    real32 Result = 0;

    if (Value < -DeadzoneThreshold)
    {
        Result = ((real32)(Value + DeadzoneThreshold) / (32768.0f - DeadzoneThreshold));
    }
    else if (Value > DeadzoneThreshold)
    {
        Result = ((real32)(Value - DeadzoneThreshold) / (32767.0f - DeadzoneThreshold));
    }

    return(Result);
}                         

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if ((ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0)) && (FileSize32 == BytesRead))
                {
                    // NOTE(george): File read succesfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    // TODO(george): Logging
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents= 0;
                }
            }
            else
            {
                // TODO(george): Logging
            }
        }
        else
        {
            // TODO(george): Logging
        }

        CloseHandle(FileHandle);
    }
    else
    {
        // TODO(george): Logging
    }

    return(Result);
}

internal void 
DEBUGPlatformFreeFileMemory(void *Memory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

internal bool32 
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFile(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(george): File written succesfully
            Result = (MemorySize == BytesWritten);
        }
        else
        {
            // TODO(george): Logging
        }

        CloseHandle(FileHandle);   
    }
    else
    {
        // TODO(george): Logging
    }

    return(Result);
}


internal void 
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE(george): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        // NOTE(george): Get a DirectSound object! - cooperative
        d_sound_create *DirectSoundCreate = (d_sound_create*) GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {sizeof(DSBUFFERDESC)};
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                // NOTE(george): "Create" a primary buffer
                // TODO(george): DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // NOTE(george): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(george): Diagnostic
                    }
                }
                else
                {
                    // TODO(george): Diagnostic
                }
            }
            else
            {
                // TODO(george): Diagnostic				
            }
            
            // NOTE(george): Create a secondary buffer	
            DSBUFFERDESC BufferDescription = {sizeof(DSBUFFERDESC)};
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created\n");
            }
        }
        else
        {
            // TODO(george): Diagnostic
        }
    }
    else
    {
        // TODO(george): Diagnostic
    }
}

internal win32_window_dimension GetWindowDimenstion(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return (Result);
}

internal void 
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    int BytesPerPixel = 4;
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = Width * Height * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    // TODO(george): Probably clear this to black
}

internal void 
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}


internal void
Win32ClearBuffer(win32_sound_output *SoundOutput) 
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        uint8 *DestByte = (uint8 *)Region1;
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
        {
            *DestByte++ = 0;
        }
                        
        DestByte = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
        {
            *DestByte++ = 0;
        }
                        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}


internal void 
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *) Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;            

            SoundOutput->RunningSampleIndex++;
        }
                        
        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16 *) Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;            

            SoundOutput->RunningSampleIndex++;
        }
                        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}
 
internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    NewState->HalfTransitionCount++;
}


internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state OldState, DWORD ButtonBit, game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit); // == ButtonBit ??
    NewState->HalfTransitionCount = (OldState.EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) 
    {
        switch(Message.message) 
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32) Message.wParam;
                bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if (WasDown != IsDown)
                { 
                    if (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                    
                    bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                    if ((VKCode == VK_F4) && AltKeyWasDown)
                    {
                        GlobalRunning = false;
                    }
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch (Message)
    {
        case WM_SIZE:
        {
        } break;
        
        case WM_DESTROY:
        {
            // TODO(george): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            // TODO(george): Handle this with a message to the user?
            GlobalRunning = false;
        } break;
        
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            
            win32_window_dimension Dimension = GetWindowDimenstion(Window);
            
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return(Result);
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCounterFrequency;
    return(Result);
}

int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    Win32LoadInput();
    
    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //	WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    // TODO(george): How do we reliably query on this on Windows?
    int MonitorRefreshHz = 60;
    int GameUpdateHz = MonitorRefreshHz / 2;
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;
    
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        
        if (Window)
        {
            // NOTE(george): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone
            HDC DeviceContext = GetDC(Window);
            
            // NOTE(george): DirestSound output test
            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);

            int16* Samples = (int16 *)VirtualAlloc(0, 48000*2*sizeof(int16), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = 0;
#else 
            LPVOID BaseAddress = Terabytes(2);
#endif

            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(4);

            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.TransientStorageSize); 

            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();

                uint64 LastCycleCount = __rdtsc();
                GlobalRunning = true;
                while (GlobalRunning)
                {   
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ButtonIndex++)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                                                OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages(NewKeyboardController);
                    
                    // TODO(george): Need to not poll disconnected controllers to avoid
                    // xinput frame rate hit on older libraries...
                    // TODO(george): Should we poll this more frequently?
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
                    }
                    for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++)
                    {
                        DWORD OurControllerIndex = ControllerIndex + 1;
                        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(george): This controller is plugged in
                            NewController->IsConnected = true;

                            // TODO(george): See if ControllerState.dwPacketNumber increments too rapidly
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            NewController->StickAverageX =  Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if(NewController->StickAverageX || NewController->StickAverageY)
                            {
                                NewController->IsAnalog = true;
                            }

                            real32 Threshold = 0.5f;
                            Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, OldController->MoveLeft, 1, &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, OldController->MoveRight, 1, &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, OldController->MoveDown, 1, &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, OldController->MoveUp, 1, &NewController->MoveLeft);

                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                NewController->StickAverageY = 1.0f;
                                NewController->IsAnalog = false;
                            }

                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                NewController->StickAverageY = -1.0f;                                
                                NewController->IsAnalog = false;
                            }

                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                NewController->StickAverageX = -1.0f;
                                NewController->IsAnalog = false;
                            }

                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                NewController->StickAverageX = 1.0f;                                
                                NewController->IsAnalog = false;
                            }

                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
                        }
                        else
                        {
                            // NOTE(george): This controller is not available
                            NewController->IsConnected = false;
                        }
                    }
                    
                    DWORD ByteToLock = 0;
                    DWORD BytesToWrite = 0;
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    bool32 SoundIsValid = false;
                    // TODO(george): Tighten up sound logic so that we know where we should be
                    // writing to and can anticipate the time spent in the game update.
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {
                        ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

                        DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize;

                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                            BytesToWrite += TargetCursor;
                        }
                        else 
                        {
                            BytesToWrite = TargetCursor - ByteToLock;   
                        }

                        SoundIsValid = true;
                    }
                    
                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.Samples = Samples;
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;

                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackbuffer.Memory;
                    Buffer.Height = GlobalBackbuffer.Height;
                    Buffer.Width = GlobalBackbuffer.Width;
                    Buffer.Pitch = GlobalBackbuffer.Pitch;
                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

                    if (SoundIsValid) 
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }

                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                    
                    real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                    }

                    win32_window_dimension Dimension = GetWindowDimenstion(Window);
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);

#if 0
                    real64 MSPerFrame = (real64) ((1000.0f*(real64)CounterElapsed) / (real64)GlobalPerfCounterFrequency);
                    real64 FPS = (real64) ((real64)GlobalPerfCounterFrequency / (real64)CounterElapsed);
                    real64 MCPF = (real64) ((real64)CyclesElapsed / (1000.0f * 1000.0f));

                    char Buffer[256];
                    sprintf(Buffer, "%.2fms/f,  %.2ff/s %.2fmc/f\n", MSPerFrame, FPS, MCPF);
                    OutputDebugStringA(Buffer); 
#endif
                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    // TODO(george): Should I clean this?

                    LARGE_INTEGER EndCounter = Win32GetWallClock();  
					real64 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                    char FPSBuffer[256];
                    _snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/f\n", MSPerFrame);
                    OutputDebugString(FPSBuffer);
                    LastCounter = EndCounter;

                    uint64 EndCycleCount = __rdtsc();
                    int64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    LastCycleCount = EndCycleCount;
                }   
            }
            else
            {
                // TODO(george): Logging
            }
        }
        else
        {
            // TODO(george): Logging
        }
    }
    else
    {
        // TODO(george): Logging
    }
    
    return (0);
}