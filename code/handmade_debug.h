#if !defined(HANDMADE_DEBUG_H)
#define HANDMADE_DEBUG_H

enum debug_variable_type
{
    DebugVariableType_Bool32,
    DebugVariableType_Int32,
    DebugVariableType_UInt32,
    DebugVariableType_Real32,
    DebugVariableType_V2,
    DebugVariableType_V3,
    DebugVariableType_V4,

	DebugVariableType_CounterThreadList,
	// DebugVariableType_CounterFunctionList,

	DebugVariableType_BitmapDisplay,

	DebugVariableType_Group,
};
inline bool32 
DEBUGShouldBeWritten(debug_variable_type Type)
{
	bool32 Result = (Type != DebugVariableType_CounterThreadList) &&
					(Type != DebugVariableType_BitmapDisplay);

	return(Result);
}

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

struct debug_variable;
struct debug_variable_reference
{
	debug_variable *Var;
	debug_variable_reference *Next;
	debug_variable_reference *Parent;
};

struct debug_variable_group
{
	v2 UIP;
	bool32 Expanded;
	debug_variable_reference *FirstChild;
	debug_variable_reference *LastChild;
};

struct debug_variable_hierarchy
{
	v2 UIP;
	debug_variable_reference *Group;

	debug_variable_hierarchy *Next;
	debug_variable_hierarchy *Prev;
};

struct debug_profile_settings
{
	v2 Dimension;
};

struct debug_bitmap_display
{
	bitmap_id ID;
	v2 Dim;
	bool32 Alpha;
};

struct debug_variable
{
    debug_variable_type Type;
    char *Name;

	union
	{
    	bool32 Bool32;
		int32 Int32;
		uint32 UInt32;
		real32 Real32;
		v2 Vector2;
		v3 Vector3;
		v4 Vector4;
		debug_variable_group Group;
		debug_profile_settings Profile;
		debug_bitmap_display BitmapDisplay;
	};
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
	debug_record *Record;
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
	
	uint32 RegionCount;
	debug_frame_region *Regions;
};

struct open_debug_block
{
	uint32 StartingFrameIndex;
	debug_record *Source;
	debug_event *OpeningEvent;
	open_debug_block *Parent;

	open_debug_block *NextFree;
};

struct debug_thread
{
	uint32 ID;
	uint32 LaneIndex;
	open_debug_block *FirstOpenBlock;
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
};
struct debug_interaction
{
	debug_interaction_type Type;
	union
	{
		void *Generic;
		debug_variable *Var;
		debug_variable_hierarchy *Hierarchy;
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

	debug_variable_reference *RootGroup;
	debug_variable_hierarchy HierarchySentinel;

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

	debug_record *ScopeToRecord;

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

internal void DEBUGStart(game_assets *Assets, uint32 Width, uint32 Height);
internal void DEBUGEnd(game_input *Input, loaded_bitmap *DrawBuffer);
internal void RefreshCollation(debug_state *DebugState);

#endif