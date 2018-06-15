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

// TODO(george): This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

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
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit); // == ButtonBit ??
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
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
            uint32 VKCode = (uint32) WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 31)) == 0);
            
            if (WasDown != IsDown)
            { 
                if (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_DOWN)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if (IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if (WasDown)
                    {
                        OutputDebugStringA("WasDown ");
                    }
                    OutputDebugStringA("\n");
                }
                else if (VKCode == VK_SPACE)
                {
                }
                
                bool32 AltKeyWasDown = (LParam & (1 << 29));
                if ((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            }
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return (Result);
}

int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    int64 PerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    Win32LoadInput();
    
    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //	WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    
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

                LARGE_INTEGER LastCounter;
                QueryPerformanceCounter(&LastCounter);
                uint64 LastCycleCount = __rdtsc();

                GlobalRunning = true;
                while (GlobalRunning)
                {
                    MSG Message;
                    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                    {
                        if (Message.message == WM_QUIT)
                        {
                            GlobalRunning = false;
                        }
                        
                        TranslateMessage(&Message);
                        DispatchMessageA(&Message);
                    }
                    
                    // TODO(george): Should we poll this more frequently?
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > ArrayCount(NewInput->Controllers))
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers);
                    }
                    for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++)
                    {
                        game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
                        game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(george): This controller is plugged in
                            // TODO(george): See if ControllerState.dwPacketNumber increments too rapidly
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                            
                            // TODO(george): DPad
                            bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                            NewController->IsAnalog = true;
                            NewController->StartX = OldController->EndX;
                            NewController->StartY = OldController->EndY;

                            // TODO(george): Deadzone processing 
                            real32 X;
                            if (Pad->sThumbLX < 0)
                            {
                                X = (real32)Pad->sThumbLX / 32768.0f;
                            }
                            else
                            {
                                X = (real32)Pad->sThumbLX / 32767.0f;
                            }
                            NewController->MinX = NewController->MaxX = NewController->EndX = X;

                            real32 Y;
                            if (Pad->sThumbLY < 0)
                            {
                                Y = (real32)Pad->sThumbLY / 32768.0f;
                            }
                            else
                            {
                                Y = (real32)Pad->sThumbLY / 32767.0f;
                            }
                            NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

                            //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                            //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        }
                        else
                        {
                            // NOTE(george): This controller is not available
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
                    
                    win32_window_dimension Dimension = GetWindowDimenstion(Window);
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
                    
                    uint64 EndCycleCount = __rdtsc();

                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);

                    uint64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                    int64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    real64 MSPerFrame = (real64) ((1000.0f*(real64)CounterElapsed) / (real64)PerfCounterFrequency);
                    real64 FPS = (real64) ((real64)PerfCounterFrequency / (real64)CounterElapsed);
                    real64 MCPF = (real64) ((real64)CyclesElapsed / (1000.0f * 1000.0f));

                /*  char Buffer[256];
                    sprintf(Buffer, "%.2fms/f,  %.2ff/s %.2fmc/f\n", MSPerFrame, FPS, MCPF);
                    OutputDebugStringA(Buffer); 
                */

                    LastCounter = EndCounter;
                    LastCycleCount = EndCycleCount;

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = NewInput;
                    // TODO(george): Should I clean this?
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