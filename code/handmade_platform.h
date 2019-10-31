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

    uint64 DebugStorageSize;
    void *DebugStorage;

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

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = _InterlockedCompareExchange((long *)Value, New, Expected);

    return(Result);
}
inline uint64 AtomicExchangeUInt64(uint64 volatile *Value, uint64 New)
{
    uint64 Result = _InterlockedExchange64((__int64 *)Value, New);

    return(Result);
}
inline uint64 AtomicAddU64(uint64 volatile *Value, uint64 Addend)
{
    // NOTE(georgy): Returns the original value _prior_ to adding
    uint64 Result = _InterlockedExchangeAdd64((__int64 *)Value, Addend);

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

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) debug_table *name(game_memory *Memory)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

inline game_controller_input *GetController(game_input *Input, unsigned int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));

    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

struct debug_record
{
	char *FileName;
	char *BlockName;

	uint32 LineNumber;
	uint32 Reserved;

	uint64 HitCount_CycleCount;
};

enum debug_event_type
{
    DebugEvent_FrameMarker,
	DebugEvent_BeginBlock,
	DebugEvent_EndBlock,
};
struct debug_event
{
	uint64 Clock;
	uint16 ThreadIndex;
	uint16 CoreIndex;
	uint16 DebugRecordIndex;
    uint8 TranslationUnit;
	uint8 Type;
};

#define MAX_DEBUG_TRANSLATION_UNITS 2
#define MAX_DEBUG_EVENT_COUNT 65536
#define MAX_DEBUG_RECORD_COUNT 65536
struct debug_table
{
	uint32 CurrentEventArrayIndex;
	uint64 volatile EventArrayIndex_EventIndex;
	debug_event Events[64][MAX_DEBUG_EVENT_COUNT];

	uint32 RecordCount[MAX_DEBUG_TRANSLATION_UNITS];
	debug_record Records[MAX_DEBUG_TRANSLATION_UNITS][MAX_DEBUG_RECORD_COUNT];
};

extern debug_table *GlobalDebugTable;

inline void
RecordDebugEvent(int RecordIndex, debug_event_type EventType)					
{
    uint64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1);	
	uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;									
	Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);												
	debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32LL] + EventIndex;	
	Event->Clock = __rdtsc();										 
	Event->ThreadIndex = (uint16)GetThreadID();											 
	Event->CoreIndex = 0;											 
	Event->DebugRecordIndex = (uint16)RecordIndex;					
    Event->TranslationUnit = TRANSLATION_UNIT_INDEX;
	Event->Type = (uint8)EventType;								
}

#define FRAME_MARKER()  \
    { \
        int Counter = __COUNTER__; \
        RecordDebugEvent(Counter, DebugEvent_FrameMarker); \
        debug_record *Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter;  \
        Record->FileName=  __FILE__;                                                        \
        Record->BlockName = "Frame Marker";                                                   \
        Record->LineNumber = __LINE__;                                                    \
        RecordDebugEvent(Counter, DebugEvent_BeginBlock);                               \
}

#define TIMED_BLOCK__(BlockName, Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, BlockName, ## __VA_ARGS__)
#define TIMED_BLOCK_(BlockName, Number, ...) TIMED_BLOCK__(BlockName, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(BlockName, ...) TIMED_BLOCK_(#BlockName, __LINE__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(__FUNCTION__, __LINE__, ## __VA_ARGS__)

#define BEGIN_BLOCK_(Counter, FileNameInit, LineNumberInit, BlockNameInit)                          \
   {debug_record *Record = GlobalDebugTable->Records[TRANSLATION_UNIT_INDEX] + Counter;  \
    Record->FileName=  FileNameInit;                                                        \
    Record->BlockName = BlockNameInit;                                                   \
    Record->LineNumber = LineNumberInit;                                                    \
    RecordDebugEvent(Counter, DebugEvent_BeginBlock);}
#define END_BLOCK_(Counter) \
    RecordDebugEvent(Counter, DebugEvent_EndBlock);

#define BEGIN_BLOCK(Name) \
    int Counter_##Name = __COUNTER__; \
    BEGIN_BLOCK_(Counter_##Name, __FILE__, __LINE__, #Name);

#define END_BLOCK(Name) \
    END_BLOCK_(Counter_##Name);

struct timed_block 
{	
	int Counter;

	timed_block(int CounterInit, char *FileName, int LineNumber, char *BlockName, int HitCountInit = 1)
	{
        // TODO(georgy): Record the hit count value here?
        Counter = CounterInit;
        BEGIN_BLOCK_(Counter, FileName, LineNumber, BlockName);
	}

	~timed_block()
	{
        END_BLOCK_(Counter);
	}
};

#ifdef __cplusplus
}
#endif

#endif