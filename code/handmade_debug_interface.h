#if !defined(HANDMADE_DEBUG_INTERFACE_H)
#define HANDMADE_DEBUG_INTERFACE_H

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) void name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
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

    DebugType_ThreadIntervalGraph,
    DebugType_FrameBarGraph,
    DebugType_LastFrameInfo,
    DebugType_DebugMemoryInfo,
    DebugType_FrameSlider,
    DebugType_TopClocksList,
};
struct debug_event
{
	u64 Clock;
    char *GUID;
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
    debug_event EditEvent;
    u32 RecordIncrement;

	uint32 CurrentEventArrayIndex;
    // TODO(georgy): This could actually be a u32 atomic now, since we
    // only need 1 bit to store which array we're using...
	uint64 volatile EventArrayIndex_EventIndex;
	debug_event Events[2][65536];
};

extern debug_table *GlobalDebugTable;

#define UniqueFileCounterString__(A, B, C, D) A "|" #B "|" #C "|" D
#define UniqueFileCounterString_(A, B, C, D) UniqueFileCounterString__(A, B, C, D)
#define DEBUG_NAME(Name) UniqueFileCounterString_(__FILE__, __LINE__, __COUNTER__, Name)

#define DEBUGSetEventRecording(Enabled) (GlobalDebugTable->RecordIncrement = (Enabled) ? 1 : 0)

#define RecordDebugEvent(EventType, GUIDInit) \
    uint64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, GlobalDebugTable->RecordIncrement); \
	uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;									\
	Assert(EventIndex < ArrayCount(GlobalDebugTable->Events[0]));												\
	debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32LL] + EventIndex;	\
    Event->Clock = __rdtsc();										 \
	Event->Type = (uint8)EventType;								\
    Event->CoreIndex = 0;											 \
    Event->ThreadID = (uint16)GetThreadID();                           \
    Event->GUID = GUIDInit;                                 

#define FRAME_MARKER(SecondsElapsedInit)  \
    { \
        RecordDebugEvent(DebugType_FrameMarker, DEBUG_NAME("Frame Marker")); \
        Event->Value_r32 = SecondsElapsedInit; \
    }

#define TIMED_BLOCK__(GUID, Number, ...) timed_block TimedBlock_##Number(GUID, ## __VA_ARGS__)
#define TIMED_BLOCK_(GUID, Number, ...) TIMED_BLOCK__(GUID, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(Name, ...) TIMED_BLOCK_(DEBUG_NAME(Name), __COUNTER__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(DEBUG_NAME(__FUNCTION__), __COUNTER__, ## __VA_ARGS__)

#define BEGIN_BLOCK_(GUID) {RecordDebugEvent(DebugType_BeginBlock, GUID);}
#define END_BLOCK_(GUID) {RecordDebugEvent(DebugType_EndBlock, GUID);}

#define BEGIN_BLOCK(Name) BEGIN_BLOCK_(DEBUG_NAME(Name))
#define END_BLOCK() END_BLOCK_("END_BLOCK_")

struct timed_block 
{	
	timed_block(char *GUID, int HitCountInit = 1)
	{
        BEGIN_BLOCK_(GUID);
        // TODO(georgy): Record the hit count value here?
	}

	~timed_block()
	{
        END_BLOCK();
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

#define DEBUGValueSetEventData_(type) \
inline void \
DEBUGValueSetEventData(debug_event *Event, type Ignored, void *Value) \
{ \
    Event->Type = DebugType_##type; \
    if(GlobalDebugTable->EditEvent.GUID == Event->GUID) \
    { \
        *(type *)Value = GlobalDebugTable->EditEvent.Value_##type; \
    } \
    Event->Value_##type = *(type *)Value; \
}

DEBUGValueSetEventData_(r32)
DEBUGValueSetEventData_(u32)
DEBUGValueSetEventData_(s32)
DEBUGValueSetEventData_(v2)
DEBUGValueSetEventData_(v3)
DEBUGValueSetEventData_(v4)
DEBUGValueSetEventData_(rectangle2)
DEBUGValueSetEventData_(rectangle3)
DEBUGValueSetEventData_(bitmap_id)
DEBUGValueSetEventData_(sound_id)
DEBUGValueSetEventData_(font_id)

struct debug_data_block
{
    debug_data_block(char *Name)
    {
        RecordDebugEvent(DebugType_OpenDataBlock, Name);
        // Event->DebugID = ID; 
    }

    ~debug_data_block(void)
    {
        RecordDebugEvent(DebugType_CloseDataBlock, DEBUG_NAME("End Data Block"));
    }
};

#define DEBUG_DATA_BLOCK(Name) debug_data_block DataBlock__(DEBUG_NAME(Name))

#define DEBUG_VALUE(Value) \
    { \
        RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value)); \
        DEBUGValueSetEventData(Event, Value, (void *)&Value); \
    }

#define DEBUG_B32(Value) \
    { \
        RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value)); \
        DEBUGValueSetEventData(Event, (s32)0, (void *)&Value); \
        Event->Type = DebugType_b32; \
    }

#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)

#define DEBUG_UI_ELEMENT(Type, Name) \
    { \
        RecordDebugEvent(Type, DEBUG_NAME(#Name)); \
    }

inline debug_id DEBUG_POINTER_ID(void *Pointer) 
{ 
    debug_id ID = {Pointer}; 

    return (ID); 
}

#define DEBUG_UI_ENABLED 1

internal void DEBUG_HIT(debug_id ID, real32 ZValue);
internal bool32 DEBUG_HIGHLIGHTED(debug_id ID, v4 *Color);
internal bool32 DEBUG_REQUESTED(debug_id ID);

#else

inline debug_id DEBUG_POINTER_ID(void *Pointer) { debug_id NullID = {}; return (NullID); }

#define DEBUG_DATA_BLOCK(...)
#define DEBUG_VALUE(...)         
#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)
#define DEBUG_UI_ENABLED 0
#define DEBUG_HIT(...)
#define DEBUG_HIGHLIGHTED(...) 0
#define DEBUG_REQUESTED(...) 0

#endif

#endif