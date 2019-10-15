#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

#define TIMED_BLOCK(ID) timed_block TimedBlock##ID(DebugCycleCounter_##ID)

struct timed_block 
{	
	uint64 StartCycleCount;
	uint32 ID;

	timed_block(uint32 IDInit)
	{
		ID = IDInit;
		BEGIN_TIMED_BLOCK_(StartCycleCount);
	}

	~timed_block()
	{
		END_TIMED_BLOCK_(StartCycleCount, ID);
	}
};

#endif