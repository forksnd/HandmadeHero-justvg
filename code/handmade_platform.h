#if !defined(HANDMADE_PLATFORM_H)
#define HANDMADE_PLATFORM_H

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

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#define Real32Maximum FLT_MAX

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

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
SafeTruncateToUInt16(uint32 Value)
{
    Assert(Value <= 65535);
    Assert(Value >= 0);
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

#define DEBUG_PLATFROM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFROM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFROM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFROM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFROM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFROM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

extern struct game_memory *DebugGlobalMemory;

#if 0
#if _MSC_VER
#define BEGIN_TIMED_BLOCK_(StartCycleCount) StartCycleCount = __rdtsc();
#define BEGIN_TIMED_BLOCK(ID) uint64 BEGIN_TIMED_BLOCK_(StartCycleCount##ID)
#define END_TIMED_BLOCK_(StartCycleCount, ID) DebugGlobalMemory->Counters[ID].CycleCount += __rdtsc() - StartCycleCount; DebugGlobalMemory->Counters[ID].HitCount++;
#define END_TIMED_BLOCK(ID) END_TIMED_BLOCK_(StartCycleCount##ID, DebugCycleCounter_##ID)
#define END_TIMED_BLOCK_COUNTED(ID, Count) DebugGlobalMemory->Counters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount##ID; DebugGlobalMemory->Counters[DebugCycleCounter_##ID].HitCount += (Count);
#else
#define BEGIN_TIMED_BLOCK(ID)
#define END_TIMED_BLOCK(ID)
#endif
#endif
#endif

#define BITMAP_BYTES_PER_PIXEL 4
struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

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
            //NOTE(george): All buttons must be added above this line
            game_button_state Start;
        };
    };
};

struct game_input
{
    game_button_state MouseButtons[5];
    int32 MouseX, MouseY, MouseZ;

    bool32 ExecutableReloaded;
    real32 dtForFrame;

    game_controller_input Controllers[5];
};

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

#define PLATFORM_ALLOCATE_MEMORY(name) void *name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

struct platform_work_queue; 
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)    
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);
struct platform_api
{
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_next_file *OpenNextFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_file_error *FileError;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

    debug_platform_read_entire_file *DEBUGReadEntireFile;
    debug_platform_free_file_memory *DEBUGFreeFileMemory;
    debug_platform_write_entire_file *DEBUGWriteEntireFile;
};
struct game_memory 
{
    uint64 PermanentStorageSize;
    void *PermanentStorage;

    uint64 TransientStorageSize;
    void *TransientStorage;

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    platform_api PlatformAPI;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
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

#ifdef __cplusplus
}
#endif

#endif