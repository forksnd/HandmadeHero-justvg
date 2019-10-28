#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

struct debug_record
{
	char *FileName;
	char *FunctionName;

	uint32 LineNumber;
	uint32 Reserved;

	uint64 HitCount_CycleCount;
};

enum debug_event_type
{
	DebugEvent_BeginBlock,
	DebugEvent_EndBlock
};
struct debug_event
{
	uint64 Clock;
	uint16 ThreadIndex;
	uint16 CoreIndex;
	uint16 DebugRecordIndex;
	uint8 Type;
};

debug_record DebugRecords[];

#define MAX_DEBUG_EVENT_COUNT 65536
global_variable uint64 Global_DebugEventArrayIndex_DebugEventIndex;
global_variable debug_event GlobalDebugEventArray[2][MAX_DEBUG_EVENT_COUNT];

inline void
RecordDebugEvent(int RecordIndex, debug_event_type EventType)					
{
	uint64 ArrayIndex_EventIndex = AtomicAddU64(&Global_DebugEventArrayIndex_DebugEventIndex, 1);	
	uint32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;									
	Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);												
	debug_event *Event = GlobalDebugEventArray[ArrayIndex_EventIndex >> 32LL] + EventIndex;	
	Event->Clock = __rdtsc();										 
	Event->ThreadIndex = 0;											 
	Event->CoreIndex = 0;											 
	Event->DebugRecordIndex = (uint16)RecordIndex;					
	Event->Type = (uint8)EventType;								
}

struct timed_block 
{	
	debug_record *Record;
	uint64 StartCycles;
	uint32 HitCount;
	int Counter;

	timed_block(int CounterInit, char *FileName, int LineNumber, char *FunctionName, int HitCountInit = 1)
	{
		Counter = CounterInit;
		HitCount = HitCountInit;
		Record = DebugRecords + Counter;
		Record->FileName=  FileName;
		Record->FunctionName = FunctionName;
		Record->LineNumber = LineNumber;

		StartCycles = __rdtsc();

		// 

		RecordDebugEvent(Counter, DebugEvent_BeginBlock);
	}

	~timed_block()
	{
		uint64 Delta = (__rdtsc() - StartCycles) | ((uint64)HitCount << 32);
		AtomicAddU64(&Record->HitCount_CycleCount, Delta);

		RecordDebugEvent(Counter, DebugEvent_EndBlock);
	}
};

struct debug_counter_snapshot
{
	uint32 HitCount;
	uint64 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 128
struct debug_counter_state
{
	char *FileName;
	char *FunctionName;

	uint32 LineNumber;

	debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state
{
	uint32 SnapshotIndex;
	uint32 CounterCount;
	debug_counter_state CounterStates[512];
	debug_frame_end_info FrameEndInfos[DEBUG_SNAPSHOT_COUNT];
};

// TODO(georgy): Fix this for looped live code editing
struct render_group;
struct game_assets;
global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, uint32 Width, uint32 Height);

internal void DEBUGOverlay(game_memory *Memory);

#endif