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
	DEBUGVarToText_ShowEntireGUID = 0x80,
	DEBUGVarToText_AddValue = 0x100,
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

struct debug_profile_node
{
	struct debug_element *Element;
	struct debug_stored_event *FirstChild;
	struct debug_stored_event *NextSameParent;
	u32 ParentRelativeClock;
	u32 Duration;
	u32 AggregateCount;
	u16 ThreadOrdinal;
	u16 CoreIndex;
};

struct debug_stored_event
{
	union
	{
		debug_stored_event *Next;
		debug_stored_event *NextFree;
	};

	u32 FrameIndex;

	union
	{
		debug_profile_node ProfileNode;
		debug_event Event;
	};
};

struct debug_string
{
	u32 Length;
	char *Value;
};

struct debug_element
{
	char *OriginalGUID; // NOTE(georgy): Can never be printed! Might point into unloaded DLL.
	char *GUID;
	u32 FileNameCount;
	u32 LineNumber;
	u32 NameStartsAt;

	b32 ValueWasEdited;

	debug_element *NextInHash;

	debug_stored_event *OldestEvent;
	debug_stored_event *MostRecentEvent;
};
inline char *GetName(debug_element *Element) {char *Result = Element->GUID + Element->NameStartsAt; return(Result);}
inline debug_string GetFileName(debug_element *Element) {debug_string Result = {Element->FileNameCount, Element->GUID}; return(Result);}

struct debug_variable_group;
struct debug_variable_link
{
	debug_variable_link *Next;
	debug_variable_link *Prev;
	debug_variable_group *Children;
	debug_element *Element;
};

struct debug_variable_group
{
	char *Name;
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
	// IMPORTANT(georgy): This actually gets freed as a set in FreeFrame! 

	union
	{
		debug_frame *Next;
		debug_frame *NextFree;
	};

	uint64 BeginClock;
	uint64 EndClock;
	real32 WallSecondsElapsed;

	real32 FrameBarScale;

	u32 FrameIndex;

	u32 StoredEventCount;
	u32 ProfileBlockCount;
	u32 DataBlockCount;

	debug_stored_event *RootProfileNode;
};

struct open_debug_block
{
	union
	{
		open_debug_block *Parent;
		open_debug_block *NextFree;
	};

	uint32 StartingFrameIndex;
	debug_element *Element;
	u64 BeginClock;
	debug_stored_event *Node;

	// NOTE(georgy): Only for data blocks?
	debug_variable_group *Group;
};

struct debug_thread
{
	union
	{
		debug_thread *Next;
		debug_thread *NextFree;
	};

	uint32 ID;
	uint32 LaneIndex;
	open_debug_block *FirstOpenCodeBlock;
	open_debug_block *FirstOpenDataBlock;
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

	DebugInteraction_ToggleExpansion,
};
struct debug_interaction
{
	debug_id ID;
	debug_interaction_type Type;
	
	union
	{
		void *Generic;
		debug_element *Element;
		debug_tree *Tree;
		debug_variable_link *Link;
		v2 *P;
	};
};

struct debug_state
{
	b32 Initialized;

	memory_arena DebugArena;
	memory_arena PerFrameArena;

	render_group RenderGroup;
	loaded_font *DebugFont;
	hha_font *DebugFontInfo;

	object_transform TextTransform;
    object_transform ShadowTransform;
    object_transform BackingTransform;
	
	v2 MenuP;
	b32 MenuActive;

	u32 SelectedIDCount;
	debug_id SelectedID[64];

	debug_element *ElementHash[1024];
	debug_view *ViewHash[4096];
	debug_variable_group *RootGroup;
	debug_variable_group *ProfileGroup;
	debug_tree TreeSentinel;

	v2 LastMouseP;
	b32 AltUI;
	debug_interaction Interaction;
	debug_interaction HotInteraction;
	debug_interaction NextHotInteraction;
	b32 Paused;

	r32 LeftEdge;
	r32 RightEdge;
	r32 AtY;
	r32 FontScale;
	font_id FontID;
	r32 GlobalWidth;
	r32 GlobalHeight;

	char *ScopeToRecord;

	u32 TotalFrameCount;
	u32 FrameCount;
	debug_frame *OldestFrame;
	debug_frame *MostRecentFrame;

    debug_frame *CollationFrame;

	u32 FrameBarLaneCount;
    debug_thread *FirstThread;
	debug_thread *FirstFreeThread;
	open_debug_block *FirstFreeBlock;

	// NOTE(georgy): Per-frame storage management
	debug_stored_event *FirstFreeStoredEvent;
	debug_frame *FirstFreeFrame;
};

internal debug_variable_group *CreateVariableGroup(debug_state *DebugState, u32 NameLength, char *Name);
internal debug_variable_group *CloneVariableGroup(debug_state *DebugState, debug_variable_link *Source);

#endif