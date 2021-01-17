#if !defined(HANDMADE_PLATFORM_H)
#define HANDMADE_PLATFORM_H

// TODO(georgy): Have the meta-parser ignore its own #define
#define introspect(IGNORED)

#ifdef __cplusplus
extern "C" {
#endif

// 
// NOTE(george): Compilers
// 
#if !defined(COMPILER_MSVC)
    #define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
    #define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
    #if _MSC_VER
        #undef  COMPILER_MSVC
        #define COMPILER_MSVC 1
    #else
        #undef COMPILER_LLVM
        // TODO(george): More compilers!!!
        #define COMPILER_LLVM 1
    #endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SSE/NEON optimizations are not available for this compiler yet
#endif

// 
// NOTE(george): Types
// 
#include <stdint.h>
#include <stddef.h>
#include <float.h>
#include <climits>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;
typedef int8 s8;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;
typedef real32 r32;
typedef real64 r64;

typedef uintptr umm;

#pragma pack(push, 1)
struct bitmap_id
{
    uint32 Value;
};
struct sound_id
{
    uint32 Value;
};
struct font_id
{
    uint32 Value;
};
#pragma pack(pop)

union v2
{	
	struct 
	{
		real32 x, y;
	};
	struct 
	{
		real32 u, v;
	};
	real32 E[2];
};

union v3
{	
	struct 
	{
		real32 x, y, z;
	};
	struct 
	{
		real32 u, v, w;
	};
	struct
	{
		real32 r, g, b;
	};
	struct
	{
		v2 xy;
		real32 Ignored_0;
	};
	struct
	{
		real32 Ignored_1;
		v2 yz;
	};
	struct
	{
		v2 uv;
		real32 Ignored_2;
	};
	struct
	{
		real32 Ignored_3;
		v2 vw;
	};
	real32 E[3];
};

union v4
{	
	struct 
	{
		real32 x, y, z, w;
	};
	struct
	{
		union
		{
			v3 xyz;
			struct 
			{
				real32 x, y, z;
			};
		};

		real32 w;
	};
	struct
	{
		union
		{
			v3 rgb;
			struct 
			{
				real32 r, g, b;
			};
		};

		real32 a;
	};
	struct
	{
		v2 xy;
		real32 Ignored_0;
		real32 Ignored_1;
	};
	struct
	{
		real32 Ignored_1;		
		v2 yz;
		real32 Ignored_2;
	};
	struct
	{
		real32 Ignored_3;		
		real32 Ignored_4;
		v2 zw;
	};
	real32 E[4];
};

introspect(category:"math") struct rectangle2
{
	v2 Min;
	v2 Max;
};

introspect(category:"math") struct rectangle3
{
	v3 Min;
	v3 Max;
};

#define Real32Maximum FLT_MAX
#define Real32Minimum -FLT_MAX

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f
#define Tau32 (2.0f*3.14159265359f)

#if HANDMADE_SLOW
#define Assert(Expresion) if (!(Expresion)) { *(int *) 0 = 0; }
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase default: {InvalidCodePath} break;

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignemnt) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32) Value;
    return (Result);
}

inline uint16
SafeTruncateToU16(uint32 Value)
{
    Assert(Value <= 0xFFFF);
    uint16 Result = (uint16) Value;
    return (Result);
}

inline int16
SafeTruncateToInt16(int32 Value)
{
    Assert(Value < 32767);
    Assert(Value >= -32768);
    int16 Result = (int16) Value;
    return (Result);
}

/*
    NOTE(george): Services that the platform layer provides to the game
*/
;

#if HANDMADE_INTERNAL

/* IMPORTANT(george):

    These are NOT for doing anything in the shipping game - they are
    blocking and the write doesn't protect against lost data!
*/

struct debug_read_file_result
{
    uint32 ContentsSize;
    void *Contents;
};

struct debug_executing_process
{
    uint64 OSHandle;
};

struct debug_process_state
{
    bool32 StartedSuccessfully;
    bool32 IsRunning;
    int32 ReturnCode;
};

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#define DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(name) debug_executing_process name(char *Path, char *Command, char *CommandLine)
typedef DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(debug_platform_execute_system_command);

#define DEBUG_PLATFORM_GET_PROCESS_STATE(name) debug_process_state name(debug_executing_process Process)
typedef DEBUG_PLATFORM_GET_PROCESS_STATE(debug_platform_get_process_state);

extern struct game_memory *DebugGlobalMemory;

#endif

#define BITMAP_BYTES_PER_PIXEL 4
struct game_offscreen_buffer
{
    void *Memory;
    u32 Width;
    u32 Height;
    u32 Pitch;
};

struct game_render_commands
{
    u32 Width;
    u32 Height;
    
    u32 MaxPushBufferSize;
    u32 PushBufferSize;
    u8 *PushBufferBase;

    u32 PushBufferElementCount;
    u32 SortEntryAt;

    u32 ClipRectCount;
    struct render_entry_cliprect *ClipRects;

    render_entry_cliprect *FirstRect;
    render_entry_cliprect *LastRect;
};
#define RenderCommandStruct(MaxPushBufferSize, PushBuffer, Width, Height) \
    {Width, Height, MaxPushBufferSize, 0, (u8 *)PushBuffer, 0, MaxPushBufferSize};

struct game_sound_output_buffer
{
    // NOTE(georgy): Samples must be padded to a multiple of 4 samples!
    int16 *Samples;
    int SamplesPerSecond;
    int SampleCount;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsConnected;

    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union 
    {
        game_button_state Buttons[12];
        struct 
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Back;
            // NOTE(georgy): All buttons must be added above this line
            game_button_state Start;
        };
    };
};

enum game_input_mouse_button
{
    PlatformMouseButton_Left,
    PlatformMouseButton_Middle,
    PlatformMouseButton_Right,
    PlatformMouseButton_Extended0,
    PlatformMouseButton_Extended1,

    PlatformMouseButton_Count
};

struct game_input
{
    real32 dtForFrame;

    game_controller_input Controllers[5];

    // NOTE(georgy): Signals back to the platform layer 
    b32 QuitRequested;

    // NOTE(georgy): For debugging only
    game_button_state MouseButtons[PlatformMouseButton_Count];
    real32 MouseX, MouseY, MouseZ;
    bool32 ShiftDown, AltDown, ControlDown;
};

inline bool32
WasPressed(game_button_state State)
{
    bool32 Result = ((State.HalfTransitionCount > 1) || 
                    ((State.HalfTransitionCount == 1) && (State.EndedDown)));

    return(Result);
}

inline bool32
IsDown(game_button_state State)
{
    bool32 Result = State.EndedDown;
    return(Result);
}

struct platform_file_handle
{
    bool32 NoErrors;
    void *Platform;
};
struct platform_file_group
{
    uint32 FileCount;
    void *Platform;
};

enum platform_file_type
{
    PlatformFileType_AssetFile,
    PlatformFileType_SavedGameFile,

    PlatformFileType_Count,
};

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_NEXT_FILE(name) platform_file_handle name(platform_file_group *FileGroup)
typedef PLATFORM_OPEN_NEXT_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, uint64 Offset, uint64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors) 

#define PLATFORM_ALLOCATE_TEXTURE(name) void *name(u32 Width, u32 Height, void *Memory)
typedef PLATFORM_ALLOCATE_TEXTURE(platform_allocate_texture);

#define PLATFORM_DEALLOCATE_TEXTURE(name) void name(void *Texture)
typedef PLATFORM_DEALLOCATE_TEXTURE(platform_deallocate_texture);

#define PLATFORM_ALLOCATE_MEMORY(name) void *name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

struct platform_work_queue; 
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)    
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);

typedef void platform_opengl_render(struct render_group *RenderGroup, struct loaded_bitmap *OutputTarget);

struct platform_api
{
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_allocate_texture *AllocateTexture;
    platform_deallocate_texture *DeallocateTexture;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_next_file *OpenNextFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_file_error *FileError;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

#if HANDMADE_INTERNAL
    debug_platform_read_entire_file *DEBUGReadEntireFile;
    debug_platform_free_file_memory *DEBUGFreeFileMemory;
    debug_platform_write_entire_file *DEBUGWriteEntireFile;
    debug_platform_execute_system_command *DEBUGExecuteSystemCommand;
    debug_platform_get_process_state *DEBUGGetProcessState;
#endif
};
struct game_memory 
{
    uint64 PermanentStorageSize;
    void *PermanentStorage;

    uint64 TransientStorageSize;
    void *TransientStorage;

    uint64 DebugStorageSize;
    void *DebugStorage;

#if HANDMADE_INTERNAL
    struct debug_table *DebugTable;
#endif

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    bool32 ExecutableReloaded;
    platform_api PlatformAPI;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE(george): At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO(george): Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

inline game_controller_input *GetController(game_input *Input, unsigned int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));

    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);

    return(Result);
}
inline uint64 AtomicExchangeUInt64(uint64 volatile *Value, uint64 New)
{
    uint64 Result = _InterlockedExchange64((__int64 volatile *)Value, New);

    return(Result);
}
inline uint64 AtomicAddU64(uint64 volatile *Value, uint64 Addend)
{
    // NOTE(georgy): Returns the original value _prior_ to adding
    uint64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);

    return(Result);
}
inline uint32
GetThreadID(void)
{
    uint8 *ThreadLocalStorage = (uint8 *)__readgsqword(0x30);
    uint32 ThreadID = *(uint32 *)(ThreadLocalStorage + 0x48);
    
    return(ThreadID);
}
#else
// TODO(georgy): Need GCC/LLVM equivalents!
#endif

#include "handmade_debug_interface.h"

#endif