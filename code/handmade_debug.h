#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

#define DEBUG_MAX_VARIABLE_STACK_DEPTH 64

enum debug_variable_to_text_flag
{
	DEBUGVarToText_AddDebugUI = 0x01,
	DEBUGVarToText_AddName = 0x02,
	DEBUGVarToText_FloatSuffix = 0x04,
	DEBUGVarToText_LineFeedEnd = 0x08,
	DEBUGVarToText_NullTerminator = 0x10,
	DEBUGVarToText_Colon = 0x20,
	DEBUGVarToText_PrettyBools = 0x40,
};

struct debug_view_inline_block
{
	v2 Dim;
};

struct debug_view_collapsible
{
	bool32 ExpandedAlways;
	bool32 ExpandedAltView;
};

enum debug_view_type
{
	DebugViewType_Unknown,

	DebugViewType_Basic,
	DebugViewType_InlineBlock,
	DebugViewType_Collapsible,
};

struct debug_view
{
	debug_id ID;
	debug_view *NextInHash;

    debug_view_type Type;
	union
	{
		debug_view_inline_block InlineBlock;
		debug_view_collapsible Collapsible;
	};
};

struct debug_variable_group;
struct debug_variable_link
{
	debug_variable_link *Next;
	debug_variable_link *Prev;
	debug_variable_group *Children;
	debug_event *Event;
};

struct debug_variable_group
{
	debug_variable_link Sentinel;
};

struct debug_tree
{
	v2 UIP;
	debug_variable_group *Group;

	debug_tree *Next;
	debug_tree *Prev;
};

struct debug_variable_array
{
	uint32 Count;
	debug_event *Vars;
};

struct render_group;
struct game_assets;
struct loaded_bitmap;
struct loaded_font;
struct hha_font;

enum debug_text_op
{
	DEBUGTextOp_DrawText,
	DEBUGTextOp_SizeText
};

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
	debug_event *Event;
	uint64 CycleCount;
	uint16 LaneIndex;
	uint16 ColorIndex;
	real32 MinT;
	real32 MaxT;
};

#define MAX_REGIONS_PER_FRAME 5000
struct debug_frame
{
	uint64 BeginClock;
	uint64 EndClock;
	real32 WallSecondsElapsed;

	debug_variable_group *RootGroup;
	
	uint32 RegionCount;
	debug_frame_region *Regions;
};

struct open_debug_block
{
	uint32 StartingFrameIndex;
	debug_event *OpeningEvent;
	open_debug_block *Parent;

	// NOTE(georgy): Only for data blocks?
	debug_variable_group *Group;

	open_debug_block *NextFree;
};

struct debug_thread
{
	uint32 ID;
	uint32 LaneIndex;
	open_debug_block *FirstOpenCodeBlock;
	open_debug_block *FirstOpenDataBlock;
	debug_thread *Next;
};

enum debug_interaction_type
{
	DebugInteraction_None,

	DebugInteraction_NOP,
	DebugInteraction_AutoModifyVariable,

	DebugInteraction_ToggleValue,
	DebugInteraction_DragValue,
	DebugInteraction_TearValue,

	DebugInteraction_Resize,
	DebugInteraction_Move,

	DebugInteraction_Select,
};
struct debug_interaction
{
	debug_id ID;
	debug_interaction_type Type;
	
	union
	{
		void *Generic;
		debug_event *Event;
		debug_tree *Tree;
		v2 *P;
	};
};

struct debug_state
{
	bool32 Initialized;

	platform_work_queue *HighPriorityQueue;

	memory_arena DebugArena;

	render_group *RenderGroup;
	loaded_font *DebugFont;
	hha_font *DebugFontInfo;
	
	bool32 Compiling;
	debug_executing_process Compiler;
	
	v2 MenuP;
	bool32 MenuActive;

	uint32 SelectedIDCount;
	debug_id SelectedID[64];

	debug_variable_group *ValuesGroup;

	debug_variable_group *RootGroup;
	debug_view *ViewHash[4096];
	debug_tree TreeSentinel;

	v2 LastMouseP;
	debug_interaction Interaction;
	debug_interaction HotInteraction;
	debug_interaction NextHotInteraction;

	real32 LeftEdge;
	real32 RightEdge;
	real32 AtY;
	real32 FontScale;
	font_id FontID;
	real32 GlobalWidth;
	real32 GlobalHeight;

	char *ScopeToRecord;

	// NOTE(georgy): Collation
	memory_arena CollateArena;
	temporary_memory CollateTemp;

	uint32 CollationArrayIndex;
    debug_frame *CollationFrame;
	uint32 FrameBarLaneCount;
	uint32 FrameCount;
	real32 FrameBarScale;
	bool32 Paused;

	debug_frame *Frames;
    debug_thread *FirstThread;
	open_debug_block *FirstFreeBlock;
};

#endif