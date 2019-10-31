#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

struct debug_counter_snapshot
{
	uint32 HitCount;
	uint64 CycleCount;
};

#define DEBUG_SNAPSHOT_COUNT 128
struct debug_counter_state
{
	char *FileName;
	char *BlockName;

	uint32 LineNumber;

	debug_counter_snapshot Snapshots[DEBUG_SNAPSHOT_COUNT];
};

struct debug_state
{
	uint32 SnapshotIndex;
	uint32 CounterCount;
	debug_counter_state CounterStates[512];
};

// TODO(georgy): Fix this for looped live code editing
struct render_group;
struct game_assets;
global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, uint32 Width, uint32 Height);

internal void DEBUGOverlay(game_memory *Memory);

#endif