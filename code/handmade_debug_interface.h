#if !defined(HANDMADE_DEBUG_INTERFACE_H)
#define HANDMADE_DEBUG_INTERFACE_H

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) debug_table *name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

struct debug_id
{
	void *Value[2];
};

#if HANDMADE_INTERNAL
enum debug_type
{
    DebugType_Unknown,

    DebugType_FrameMarker,
	DebugType_BeginBlock,
	DebugType_EndBlock,

    DebugType_OpenDataBlock,
    DebugType_CloseDataBlock,

    DebugType_MarkDebugValue,

    DebugType_b32,
    DebugType_r32,
    DebugType_u32,
    DebugType_s32,
    DebugType_v2,
    DebugType_v3,
    DebugType_v4,
    DebugType_rectangle2,
    DebugType_rectangle3,
	DebugType_bitmap_id,
	DebugType_sound_id,
	DebugType_font_id,

    DebugType_CounterThreadList,
	// DebugVariableType_CounterFunctionList,
};
struct debug_event
{
	u64 Clock;
    char *GUID;
	char *BlockName;
    u16 ThreadID;
    u16 CoreIndex;
	u8 Type;
    union
    {
        debug_id DebugID;
        debug_event *Value_debug_event;

        b32 Value_b32;
		s32 Value_s32;
		u32 Value_u32;
		r32 Value_r32;
		v2 Value_v2;
		v3 Value_v3;
		v4 Value_v4;
        rectangle2 Value_rectangle2;
        rectangle3 Value_rectangle3;
    	bitmap_id Value_bitmap_id;
        sound_id Value_sound_id;
        font_id Value_font_id;
    };
};

struct debug_table
{
	uint32 CurrentEventArrayIndex;
    // TODO(georgy): This could actually be a u32 atomic now, since we
    // only need 1 bit to store which array we're using...
	uint64 volatile EventArrayIndex_EventIndex;
	debug_event Events[2][65536];
};

extern debug_table *GlobalDebugTable;

#define UniqueFileCounterString__(A, B, C) A "(" #B ")." #C
#define UniqueFileCounterString_(A, B, C) UniqueFileCounterString__(A, B, C)
#define UniqueFileCounterString() UniqueFileCounterString_(__FILE__, __LINE__, __COUNTER__)

#define RecordDebugEvent(EventType, Block) \
    uint64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1); \
	uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;									\
	Assert(EventIndex < ArrayCount(GlobalDebugTable->Events[0]));												\
	debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32LL] + EventIndex;	\
    Event->Clock = __rdtsc();										 \
	Event->Type = (uint8)EventType;								\
    Event->CoreIndex = 0;											 \
    Event->ThreadID = (uint16)GetThreadID();                           \
    Event->GUID = UniqueFileCounterString();                                 \
    Event->BlockName = Block;                                                   \

#define FRAME_MARKER(SecondsElapsedInit)  \
    { \
        int Counter = __COUNTER__; \
        RecordDebugEvent(DebugType_FrameMarker, "Frame Marker"); \
        Event->Value_r32 = SecondsElapsedInit; \
    }

#define TIMED_BLOCK__(BlockName, Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, BlockName, ## __VA_ARGS__)
#define TIMED_BLOCK_(BlockName, Number, ...) TIMED_BLOCK__(BlockName, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(BlockName, ...) TIMED_BLOCK_(#BlockName, __LINE__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(__FUNCTION__, __LINE__, ## __VA_ARGS__)

#define BEGIN_BLOCK_(Counter, FileNameInit, LineNumberInit, BlockNameInit)                          \
   {RecordDebugEvent(DebugType_BeginBlock, BlockNameInit);}
#define END_BLOCK_(Counter) \
    { \
    RecordDebugEvent(DebugType_EndBlock, "End Block"); \
    }

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

#else

    #define TIMED_BLOCK(...) 
    #define TIMED_FUNCTION(...) 
    #define BEGIN_BLOCK(...) 
    #define END_BLOCK(...) 
    #define FRAME_MARKER(...)

#endif

// 
// NOTE(georgy): Shared utils
// 
inline uint32
StringLength(char *String)
{
    uint32 Count = 0;
    while(*String++)
    {
        Count++;
    }
    return(Count);
}

#ifdef __cplusplus
}
#endif


#if defined(__cplusplus) && HANDMADE_INTERNAL

inline void
DEBUGValueSetEventData(debug_event *Event, real32 Value)
{
    Event->Type = DebugType_r32;
    Event->Value_r32 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, uint32 Value)
{
    Event->Type = DebugType_u32;
    Event->Value_u32 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, int32 Value)
{
    Event->Type = DebugType_s32;
    Event->Value_s32 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, v2 Value)
{
    Event->Type = DebugType_v2;
    Event->Value_v2 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, v3 Value)
{
    Event->Type = DebugType_v3;
    Event->Value_v3 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, v4 Value)
{
    Event->Type = DebugType_v4;
    Event->Value_v4 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, rectangle2 Value)
{
    Event->Type = DebugType_rectangle2;
    Event->Value_rectangle2 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, rectangle3 Value)
{
    Event->Type = DebugType_rectangle3;
    Event->Value_rectangle3 = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, bitmap_id Value)
{
    Event->Type = DebugType_bitmap_id;
    Event->Value_bitmap_id = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, sound_id Value)
{
    Event->Type = DebugType_sound_id;
    Event->Value_sound_id = Value;
}

inline void
DEBUGValueSetEventData(debug_event *Event, font_id Value)
{
    Event->Type = DebugType_font_id;
    Event->Value_font_id = Value;
}

#define DEBUG_BEGIN_DATA_BLOCK(Name, ID) \
    { \
        RecordDebugEvent(DebugType_OpenDataBlock, #Name); \
        Event->DebugID = ID; \
    }

#define DEBUG_END_DATA_BLOCK() \
    { \
        RecordDebugEvent(DebugType_CloseDataBlock, "End Data Block"); \
    }


#define DEBUG_VALUE(Value) \
    { \
        RecordDebugEvent(DebugType_Unknown, #Value); \
        DEBUGValueSetEventData(Event, Value); \
    }

#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)

inline debug_id DEBUG_POINTER_ID(void *Pointer) 
{ 
    debug_id ID = {Pointer}; 

    return (ID); 
}

#define DEBUG_UI_ENABLED 1

internal void DEBUG_HIT(debug_id ID, real32 ZValue);
internal bool32 DEBUG_HIGHLIGHTED(debug_id ID, v4 *Color);
internal bool32 DEBUG_REQUESTED(debug_id ID);

inline debug_event DEBUGInitializeValue(debug_type Type, debug_event *SubEvent, char *GUID, char *Name)
{
    RecordDebugEvent(DebugType_MarkDebugValue, "");
    Event->GUID = GUID;
    Event->BlockName = Name;
    Event->Value_debug_event = SubEvent;

    SubEvent->Clock = 0;
    SubEvent->GUID = GUID;
    SubEvent->BlockName = Name;
    SubEvent->ThreadID = 0;
    SubEvent->CoreIndex = 0;
	SubEvent->Type = (u8)Type;

    return(*SubEvent);
}

#else

inline debug_id DEBUG_POINTER_ID(void *Pointer) { debug_id NullID = {}; return (NullID); }

#define DEBUG_BEGIN_DATA_BLOCK(...)
#define DEBUG_END_DATA_BLOCK(...)
#define DEBUG_VALUE(...)         
#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)
#define DEBUG_UI_ENABLED 0
#define DEBUG_HIT(...)
#define DEBUG_HIGHLIGHTED(...) 0
#define DEBUG_REQUESTED(...) 0

#define DEBUG_IF__(Path) if(GlobalConstants_##Path)
#define DEBUG_VARIABLE__(type, Path, Variable) type Variable = GlobalConstants_##Path##_##Variable;

#endif

#endif