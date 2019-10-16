#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

struct debug_record
{
	uint64 CycleCount;

	char *FileName;
	char *FunctionName;

	uint32 LineNumber;
	uint32 HitCount;
};

debug_record DebugRecords[];

struct timed_block 
{	
	debug_record *Record;

	timed_block(int Counter, char *FileName, int LineNumber, char *FunctionName, int HitCount = 1)
	{
		// TODO(georgy): Thread safety
		Record = DebugRecords + Counter;
		Record->FileName=  FileName;
		Record->FunctionName = FunctionName;
		Record->LineNumber = LineNumber;
		Record->CycleCount -= __rdtsc();
		Record->HitCount += HitCount;
	}

	~timed_block()
	{
		Record->CycleCount += __rdtsc();
	}
};

#endif