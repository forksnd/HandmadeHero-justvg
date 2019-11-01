#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

struct debug_counter_snapshot
{
	uint32 HitCount;
	uint64 CycleCount;
};

struct debug_counter_state
{
	char *FileName;
	char *BlockName;

	uint32 LineNumber;
};

struct debug_frame_region
{
	uint32 LaneIndex;
	real32 MinT;
	real32 MaxT;
};


struct debug_frame
{
	uint64 BeginClock;
	uint64 EndClock;
	
	uint32 RegionCount;
	debug_frame_region *Regions;
};

struct debug_state
{
	bool32 Initialized;
	
	// NOTE(georgy): Collation
	memory_arena CollateArena;
	temporary_memory CollateTemp;

	uint32 FrameBarLaneCount;
	uint32 FrameCount;
	real32 FrameBarScale;

	debug_frame *Frames;
};

// TODO(georgy): Fix this for looped live code editing
struct render_group;
struct game_assets;
global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, uint32 Width, uint32 Height);

internal void DEBUGOverlay(game_memory *Memory);

#endif