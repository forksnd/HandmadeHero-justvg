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

debug_record DebugRecords[];

struct timed_block 
{	
	debug_record *Record;
	uint64 StartCycles;
	uint32 HitCount;

	timed_block(int Counter, char *FileName, int LineNumber, char *FunctionName, int HitCountInit = 1)
	{
		HitCount = HitCountInit;
		Record = DebugRecords + Counter;
		Record->FileName=  FileName;
		Record->FunctionName = FunctionName;
		Record->LineNumber = LineNumber;

		StartCycles = __rdtsc();
	}

	~timed_block()
	{
		uint64 Delta = (__rdtsc() - StartCycles) | ((uint64)HitCount << 32);
		AtomicAddU64(&Record->HitCount_CycleCount, Delta);
	}
};

struct debug_counter_snapshot
{
	uint32 HitCount;
	uint32 CycleCount;
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
};

#endif