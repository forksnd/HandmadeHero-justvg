#include "handmade_debug.h"

// TODO(georgy): Stop using stdio!
#include <stdio.h>

#include "handmade_debug.h"

inline bool32
DebugIDsAreEqual(debug_id A, debug_id B)
{
    bool32 Result = (A.Value[0] == B.Value[0]) &&
                    (A.Value[1] == B.Value[1]);

    return(Result);
}

inline debug_id
DebugIDFromLink(debug_tree *Tree, debug_variable_link *Link)
{
    debug_id Result = {};

    Result.Value[0] = Tree;
    Result.Value[1] = Link;

    return(Result);
}

inline debug_id
DebugIDFromGUID(debug_tree *Tree, char *GUID)
{
    debug_id Result = {};

    Result.Value[0] = Tree;
    Result.Value[1] = GUID;

    return(Result);
}

inline debug_state *
DEBUGGetState(game_memory *Memory)
{
    debug_state *DebugState = 0;
    if(Memory)
    {
        DebugState = (debug_state *)Memory->DebugStorage;
        if(!DebugState->Initialized)
        {
            DebugState = 0;
        }
    }

    return(DebugState);
}

inline debug_state *
DEBUGGetState(void)
{
    debug_state *DebugState = DEBUGGetState(DebugGlobalMemory);

    return(DebugState);
}

internal debug_tree *
AddTree(debug_state *DebugState, debug_variable_group *Group, v2 AtP)
{   
    debug_tree *Tree = PushStruct(&DebugState->DebugArena, debug_tree);

    Tree->UIP = AtP;
    Tree->Group = Group;

    DLIST_INSERT(&DebugState->TreeSentinel, Tree);

    return(Tree);
}

inline bool32
IsHex(char Char)
{
    bool32 Result = (((Char >= '0') && (Char <= '9')) ||
                    ((Char >= 'A') && (Char <= 'F')));
    return(Result);
}

inline uint32
GetHex(char Char)
{
    uint32 Result = 0;

    if((Char >= '0') && (Char <= '9'))
    {
        Result = Char - '0';
    }
    else if((Char >= 'A') && (Char <= 'F'))
    {
        Result = 0xA + (Char - 'A');
    }

    return(Result);
}

internal rectangle2
DEBUGTextOp(debug_state *DebugState, debug_text_op Op, v2 P, char *String, v4 Color = V4(1, 1, 1, 1))
{
    rectangle2 Result = InvertedInfinityRectangle2();
    if(DebugState && DebugState->DebugFont)
    {
        render_group *RenderGroup = DebugState->RenderGroup;
        loaded_font *Font = DebugState->DebugFont; 
        hha_font *FontInfo = DebugState->DebugFontInfo;

        uint32 PrevCodePoint = 0;
        real32 AtX = P.x;
        real32 AtY = P.y;
        for(char *At = String;
            *At;
            At++)
        {
            uint32 CodePoint = *At;
            if((At[0] == '\\') &&
            (IsHex(At[1])) && 
            (IsHex(At[2])) &&
            (IsHex(At[3])) &&
            (IsHex(At[4])))
            {
                CodePoint = ((GetHex(At[1]) << 12) |
                            (GetHex(At[2]) << 8) |
                            (GetHex(At[3]) << 4) |
                            (GetHex(At[4]) << 0));
                            
                At += 4;
            }

            real32 AdvanceX = DebugState->FontScale*GetHorizontalAdvanceForPair(FontInfo, Font, PrevCodePoint, CodePoint);
            AtX += AdvanceX;

            if(*At != ' ')
            {
                bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, FontInfo, Font, CodePoint);
                hha_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                real32 BitmapScale = DebugState->FontScale*Info->Dim[1];
                v3 BitmapOffset = V3(AtX, AtY, 0);
                if(Op == DEBUGTextOp_DrawText)
                {
                    PushBitmap(RenderGroup, DefaultFlatTransform(), BitmapID, BitmapScale, BitmapOffset, Color);
                }
                else
                {
                    Assert(Op == DEBUGTextOp_SizeText);

                    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
                    if(Bitmap)
                    {
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, DefaultFlatTransform(), Bitmap, BitmapScale, BitmapOffset, 1.0f);
                        rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
                        Result = Union2(Result, GlyphDim);
                    }
                }
            }

            PrevCodePoint = CodePoint;
        }
    }

    return(Result);
}

internal void
DEBUGTextOutAt(v2 P, char *String, v4 Color = V4(1, 1, 1, 1))
{
    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        DEBUGTextOp(DebugState, DEBUGTextOp_DrawText, P, String, Color); 
    }
}

internal rectangle2
DEBUGGetTextSize(debug_state *DebugState, char *String)
{
    rectangle2 Result = DEBUGTextOp(DebugState, DEBUGTextOp_SizeText, V2(0, 0), String);
    
    return(Result);
}

internal void
DEBUGTextLine(char *String)
{
    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        render_group *RenderGroup = DebugState->RenderGroup;

        loaded_font *Font = PushFont(RenderGroup, DebugState->FontID);
        if(Font)
        {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, DebugState->FontID);
        
            DEBUGTextOutAt(V2(DebugState->LeftEdge, 
                              DebugState->AtY - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), String);

            DebugState->AtY -= GetLineAdvanceFor(Info)*DebugState->FontScale;    
        }
    }
}

struct debug_statistic
{
    real64 Min;
    real64 Max;
    real64 Avg;
    uint32 Count;
};
inline void
BeginDebugStatistic(debug_statistic *Stat)
{
    Stat->Count = 0;
    Stat->Min = Real32Maximum;
    Stat->Max = -Real32Maximum;
    Stat->Avg = 0.0;
}

inline void
AccumulateDebugStatistic(debug_statistic *Stat, real64 Value)
{
    Stat->Count++;

    if(Stat->Min > Value)
    {
        Stat->Min = Value;
    }

    if(Stat->Max < Value)
    {
        Stat->Max = Value;
    }

    Stat->Avg += Value;
}

inline void
EndDebugStatistic(debug_statistic *Stat)
{
    if(Stat->Count != 0)
    {
        Stat->Avg /= (real64)Stat->Count;
    }
    else
    {
        Stat->Min = Stat->Max = 0.0;
    }
}

internal memory_index 
DEBUGEventToText(char *Buffer, char *End, debug_event *Event, uint32 Flags)
{
    char *At = Buffer;
    char *Name = Event->BlockName;

    if(Flags & DEBUGVarToText_AddDebugUI)
    {
        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                          "#define DEBUGUI_");
    }
    
    if(Flags & DEBUGVarToText_AddName)
    {
        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                          "%s%s ", Name, (Flags & DEBUGVarToText_Colon) ? ":" : "");
    }

    switch(Event->Type)
    {
        case DebugType_b32:
        {
            if(Flags & DEBUGVarToText_PrettyBools)
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%s", Event->Value_b32 ? "true" : "false");
            }
            else
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%d", Event->Value_b32);
            }
        } break;

        case DebugType_s32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%d", Event->Value_s32);
        } break;

        case DebugType_u32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%u", Event->Value_u32);
        } break;

        case DebugType_r32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%f", Event->Value_r32);
                if(Flags & DEBUGVarToText_FloatSuffix)
                {
                    *At++ = 'f';
                }
        } break;

        case DebugType_v2:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "V2(%f, %f)", Event->Value_v2.x, Event->Value_v2.y);
        } break;

        case DebugType_v3:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "V3(%f, %f, %f)", Event->Value_v3.x, Event->Value_v3.y, Event->Value_v3.z);
        } break;

        case DebugType_v4:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "V4(%f, %f, %f, %f)", Event->Value_v4.x, Event->Value_v4.y, Event->Value_v4.z, Event->Value_v4.w);
        } break;

        case DebugType_rectangle2:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "Rect2(%f, %f -> %f, %f)", 
                                Event->Value_rectangle2.Min.x,
                                Event->Value_rectangle2.Min.y, 
                                Event->Value_rectangle2.Max.x,
                                Event->Value_rectangle2.Max.y);
        } break;

        case DebugType_rectangle3:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "Rect3(%f, %f, %f -> %f, %f, %f)", 
                                Event->Value_rectangle3.Min.x,
                                Event->Value_rectangle3.Min.y, 
                                Event->Value_rectangle3.Min.z, 
                                Event->Value_rectangle3.Max.x,
                                Event->Value_rectangle3.Max.y,
                                Event->Value_rectangle3.Max.z);
        } break;

        case DebugType_OpenDataBlock:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "%s", Event->BlockName);
        } break;

        case DebugType_CounterThreadList:
        case DebugType_bitmap_id:
        {
        } break;

        default:
        {
            At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "UNHANDLED: %s", Event->BlockName);
        } break;
    }

    if(Flags & DEBUGVarToText_LineFeedEnd)
    {
        *At++ = '\n';
    }

    if(Flags & DEBUGVarToText_NullTerminator)
    {
        *At++ = 0;
    }

    return(At - Buffer);
}

struct debug_variable_iterator
{
    debug_variable_link *Link;
    debug_variable_link *Sentinel;
};

internal void
WriteHandmadeConfig(debug_state *DebugState)
{
#if 0
    char Temp[4096];
    char *At = Temp;
    char *End = Temp + sizeof(Temp);

    u32 Depth = 0;
    debug_variable_iterator Stack[DEBUG_MAX_VARIABLE_STACK_DEPTH];

    Stack[Depth].Link = DebugState->RootGroup->VarGroup.Next;
    Stack[Depth].Sentinel = &DebugState->RootGroup->VarGroup;
    Depth++;
    while(Depth > 0)
    {
        debug_variable_iterator *Iter = Stack + (Depth - 1);
        if(Iter->Link == Iter->Sentinel)
        {
            Depth--;
        }
        else
        {
            debug_variable *Var = Iter->Link->Var;
            Iter->Link = Iter->Link->Next;

            if (DEBUGShouldBeWritten(Event->Type))
            {
                for(int Index = 0;
                    Index < Depth;
                    Index++)
                {
                    *At++ = ' ';
                    *At++ = ' ';
                    *At++ = ' ';
                    *At++ = ' ';
                }

                if(Event->Type == DebugType_OpenDataBlock)
                {
                    At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                    "// ");
                }
                At += DEBUGEventToText(At, End, Var, DEBUGVarToText_AddDebugUI|
                                                     DEBUGVarToText_AddName|
                                                     DEBUGVarToText_FloatSuffix|
                                                     DEBUGVarToText_LineFeedEnd);
            }

            if(Event->Type == DebugType_OpenDataBlock)
            {
                Iter = Stack + Depth;
                Iter->Link = Event->VarGroup.Next;
                Iter->Sentinel = &Event->VarGroup;
                Depth++;
            }
        }
    }
    
    Platform.DEBUGWriteEntireFile("../code/handmade_config.h", (uint32)(At - Temp), Temp);

    if(!DebugState->Compiling)
    {
        DebugState->Compiling = true;
        DebugState->Compiler = Platform.DEBUGExecuteSystemCommand("..\\code", "c:\\windows\\system32\\cmd.exe", "/C build.bat");
    }
#endif
}

internal void 
DrawProfileIn(debug_state *DebugState, rectangle2 ProfileRect, v2 MouseP)
{
    PushRect(DebugState->RenderGroup, DefaultFlatTransform(), ProfileRect, 0.0f, V4(0.0f, 0.0f, 0.0f, 0.25f));

    r32 BarSpacing = 4.0f;
    r32 LaneHeight = 0.0f;
    u32 LaneCount = DebugState->FrameBarLaneCount;
    r32 FrameBarScale = FLT_MAX;
    for(debug_frame *Frame = DebugState->OldestFrame;
        Frame;
        Frame = Frame->Next)
    {
        if(FrameBarScale > Frame->FrameBarScale)
        {
            FrameBarScale = Frame->FrameBarScale;
        }
    }

    uint32 MaxFrame = DebugState->FrameCount;
    if(MaxFrame > 10)
    {
        MaxFrame = 10;
    }

    if((MaxFrame > 0) && (LaneCount > 0))
    {
        LaneHeight = ((GetDim(ProfileRect).y / (real32)MaxFrame) - BarSpacing) / (real32)LaneCount;
    }

    real32 BarHeight = LaneHeight*LaneCount;
    real32 BarsPlusSpacing = BarHeight + BarSpacing;
    real32 ChartLeft = ProfileRect.Min.x;
    real32 ChartHeight = BarsPlusSpacing*(real32)MaxFrame;
    real32 ChartWidth = GetDim(ProfileRect).x;
    real32 ChartTop = ProfileRect.Max.y;
    real32 Scale = ChartWidth*FrameBarScale;

    v3 Colors[] = 
    {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 0},
        {0, 1, 1},
        {1, 0, 1},
        {1, 0.5f, 0},
        {1, 0, 0.5f},
        {0.5f, 1, 0},
        {0, 1, 0.5f},
        {0.5f, 0, 1},
        {0, 0.5f, 1},
    };

#if 0
    u32 FrameIndex = 0;
    for(debug_frame *Frame = DebugState->OldestFrame;
        Frame;
        Frame = Frame->Next, FrameIndex++)
    {
        real32 StackX = ChartLeft;
        real32 StackY = ChartTop - BarsPlusSpacing*(real32)FrameIndex;
        for(uint32 RegionIndex = 0;
            RegionIndex < Frame->RegionCount;
            RegionIndex++)
        {
            debug_frame_region *Region = Frame->Regions + RegionIndex;

            v3 Color = Colors[Region->ColorIndex % ArrayCount(Colors)];
            real32 ThisMinX = StackX + Scale*Region->MinT;
            real32 ThisMaxX = StackX + Scale*Region->MaxT;

            rectangle2 RegionRect = RectMinMax(V2(ThisMinX, StackY - LaneHeight*(Region->LaneIndex + 1)), 
                                               V2(ThisMaxX, StackY - LaneHeight*Region->LaneIndex));

            PushRect(DebugState->RenderGroup, RegionRect, 0.0f, V4(Color, 1.0f));

            if(IsInRectangle(RegionRect, MouseP))
            {
                debug_event *Record = Region->Event;
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                            "%s: %I64ucy [%s(%d)]", 
                            Record->BlockName,
                            Region->CycleCount,
                            Record->FileName,
                            Record->LineNumber);
                DEBUGTextOutAt(MouseP + V2(0.0f, 10.0f), TextBuffer);
            }
        }
    }
#endif
#if 0
        PushRect(DebugState->RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0.0f), V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
#endif
}

inline bool32
InteractionsAreEqual(debug_interaction A, debug_interaction B)
{
    bool32 Result = (DebugIDsAreEqual(A.ID, B.ID) &&
                     (A.Type == B.Type) &&
                     (A.Generic == B.Generic));
    return(Result);
}

inline bool32
InteractionIsHot(debug_state *DebugState, debug_interaction B)
{
    bool32 Result = InteractionsAreEqual(DebugState->HotInteraction, B);
    return(Result);
}

struct layout
{
    debug_state *DebugState;
    v2 MouseP;
    v2 At;
    u32 Depth;
    real32 LineAdvance;
    real32 SpacingY;
};

struct layout_element
{
    // NOTE(georgy): Storage
    layout *Layout;
    v2 *Dim;
    v2 *Size;
    debug_interaction Interaction;

    // NOTE(georgy): Out
    rectangle2 Bounds;
};

inline layout_element 
BeginElementRectangle(layout *Layout, v2 *Dim)
{
    layout_element Element = {};

    Element.Layout = Layout;
    Element.Dim = Dim;

    return(Element);
}

inline void    
MakeElementSizeable(layout_element *Element)
{
    Element->Size = Element->Dim;
}

inline void
DefaultInteraction(layout_element *Element, debug_interaction Interaction)
{
    Element->Interaction = Interaction;
}

inline void
EndElement(layout_element *Element)
{
    object_transform NoTransform = DefaultFlatTransform();

    layout *Layout = Element->Layout;
    debug_state *DebugState = Layout->DebugState;

    real32 SizeHandlePixels = 4.0f;

    v2 Frame = {};
    if(Element->Size)
    {
        Frame = V2(SizeHandlePixels, SizeHandlePixels);
    }
    v2 TotalDim = *Element->Dim + 2.0f*Frame;

    v2 TotalMinCorner = V2(Layout->At.x + Layout->Depth*2.0f*Layout->LineAdvance, 
                           Layout->At.y - TotalDim.y);
    v2 TotalMaxCorner = TotalMinCorner + TotalDim;

    v2 InteriorMinCorner = TotalMinCorner + Frame;
    v2 InteriorMaxCorner = InteriorMinCorner + *Element->Dim;

    rectangle2 TotalBounds = RectMinMax(TotalMinCorner, TotalMaxCorner);
    Element->Bounds = RectMinMax(InteriorMinCorner, InteriorMaxCorner);

    if(Element->Interaction.Type && IsInRectangle(Element->Bounds, Layout->MouseP))
    {
        DebugState->NextHotInteraction = Element->Interaction;
    }

    if(Element->Size)
    {
        PushRect(DebugState->RenderGroup, NoTransform, RectMinMax(V2(TotalMinCorner.x, InteriorMinCorner.y), 
                                                                  V2(InteriorMinCorner.x, InteriorMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMaxCorner.x, InteriorMinCorner.y), 
                                                                  V2(TotalMaxCorner.x, InteriorMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMinCorner.x, InteriorMaxCorner.y), 
                                                                  V2(InteriorMaxCorner.x, TotalMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMinCorner.x, TotalMinCorner.y), 
                                                                  V2(InteriorMaxCorner.x, InteriorMinCorner.y)), 
                0.0f, V4(0, 0, 0, 1));

        debug_interaction SizeInteraction = {};
        SizeInteraction.Type = DebugInteraction_Resize;
        SizeInteraction.P = Element->Size;

        rectangle2 SizeBox = RectMinMax(V2(InteriorMaxCorner.x, TotalMinCorner.y),
                                        V2(TotalMaxCorner.x, InteriorMinCorner.y));
        PushRect(DebugState->RenderGroup, NoTransform, SizeBox, 0.0f, 
                InteractionIsHot(DebugState, SizeInteraction) ? 
                V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
        
        if(IsInRectangle(SizeBox, Layout->MouseP))
        {
            DebugState->NextHotInteraction = SizeInteraction;
        }
    }

    real32 SpacingY = Layout->SpacingY;
    if(0)
    {
        SpacingY = 0.0f;
    }
    Layout->At.y = GetMinCorner(TotalBounds).y - SpacingY;
}

internal debug_view *
GetOrCreateDebugViewFor(debug_state *DebugState, debug_id ID)
{
    // TODO(georgy): Better hash function
    uint32 HashIndex = (((uint32)ID.Value[0] >> 2) + ((uint32)ID.Value[1] >> 2)) % 
                                            ArrayCount(DebugState->ViewHash);
    debug_view **HashSlot = DebugState->ViewHash + HashIndex;

    debug_view *Result = 0;
    for(debug_view *Search = *HashSlot;
        Search;
        Search = Search->NextInHash)
    {
        if(DebugIDsAreEqual(Search->ID, ID))
        {
            Result = Search;
        }
    }

    if(!Result)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_view);
        Result->ID = ID;
        Result->Type = DebugViewType_Unknown;
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }

    return(Result);
}

inline debug_interaction
EventInteraction(debug_state *DebugState, debug_id DebugID, debug_interaction_type Type, debug_event *Event)
{
    debug_interaction Result = {};
    Result.ID = DebugID;
    Result.Type = Type;
    Result.Event = Event;

    return(Result);
}

inline debug_interaction
DebugIDInteraction(debug_interaction_type Type, debug_id ID)
{
    debug_interaction Result = {};
    Result.ID = ID;
    Result.Type = Type;

    return(Result);
}

inline bool32
IsSelected(debug_state *DebugState, debug_id ID)
{
    bool32 Result = false;

    for(uint32 Index = 0;
        Index < DebugState->SelectedIDCount;
        Index++)
    {
        if(DebugIDsAreEqual(ID, DebugState->SelectedID[Index]))
        {
            Result = true;
            break;
        }
    }
    
    return(Result);
}

inline void
ClearSelection(debug_state *DebugState)
{
    DebugState->SelectedIDCount = 0;
}

internal void
AddToSelection(debug_state *DebugState, debug_id ID)
{
    if((DebugState->SelectedIDCount < ArrayCount(DebugState->SelectedID)) && 
       !IsSelected(DebugState, ID))
    {
        DebugState->SelectedID[DebugState->SelectedIDCount++] = ID;
    }
}

internal void 
DEBUG_HIT(debug_id ID, real32 ZValue)
{
    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        DebugState->NextHotInteraction = DebugIDInteraction(DebugInteraction_Select, ID);
    }
}

internal bool32 
DEBUG_HIGHLIGHTED(debug_id ID, v4 *Color)
{
    bool32 Result = false;

    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        if(IsSelected(DebugState, ID))
        {
            *Color = V4(0, 1, 1, 1);
            Result = true;
        } 
        
        if(DebugIDsAreEqual(ID, DebugState->HotInteraction.ID))
        {
            *Color = V4(1, 1, 0, 1);
            Result = true;
        }
    }

    return(Result);
}

internal bool32 
DEBUG_REQUESTED(debug_id ID)
{
    bool32 Result = false;

    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        Result = IsSelected(DebugState, ID) || 
                 DebugIDsAreEqual(ID, DebugState->HotInteraction.ID);
    }

    return(Result);
}

internal void
DEBUGDrawEvent(layout *Layout, debug_stored_event *StoredEvent, debug_id DebugID)
{
    object_transform NoTransform = DefaultFlatTransform();

    debug_state *DebugState = Layout->DebugState;
    render_group *RenderGroup = DebugState->RenderGroup;

    if(StoredEvent)
    {
        debug_event *Event = &StoredEvent->Event;   
        debug_interaction ItemInteraction = 
                EventInteraction(DebugState, DebugID, DebugInteraction_AutoModifyVariable, Event);

        bool32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
        v4 ItemColor = (IsHot) ? V4(1, 1, 0, 1) :  V4(1, 1, 1, 1);

        debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugID);
        switch(Event->Type)
        {
            case DebugType_bitmap_id:
            {
                loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, Event->Value_bitmap_id, RenderGroup->GenerationID);
                real32 BitmapScale = View->InlineBlock.Dim.y;
                if(Bitmap)
                {
                    used_bitmap_dim Dim = GetBitmapDim(RenderGroup, NoTransform, Bitmap, BitmapScale, V3(0.0f, 0.0f, 0.0f), 1.0f);
                    View->InlineBlock.Dim.x = Dim.Size.x;
                }

                debug_interaction TearInteraction = 
                        EventInteraction(DebugState, DebugID, DebugInteraction_TearValue, Event);
                
                layout_element Element = BeginElementRectangle(Layout, &View->InlineBlock.Dim);
                MakeElementSizeable(&Element);
                DefaultInteraction(&Element, TearInteraction);
                EndElement(&Element);

                PushRect(DebugState->RenderGroup, NoTransform, Element.Bounds, 0.0f, V4(0, 0, 0, 1));
                PushBitmap(DebugState->RenderGroup, NoTransform, Event->Value_bitmap_id, View->InlineBlock.Dim.y, 
                        V3(GetMinCorner(Element.Bounds), 0.0f), V4(1, 1, 1, 1), 0.0f);
            } break;

            default:
            {
                char Text[256];
                DEBUGEventToText(Text, Text + sizeof(Text), Event, DEBUGVarToText_AddName|
                                                                DEBUGVarToText_NullTerminator|
                                                                DEBUGVarToText_Colon|
                                                                DEBUGVarToText_PrettyBools);

                rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);
                v2 Dim = {GetDim(TextBounds).x, Layout->LineAdvance};

                layout_element Element = BeginElementRectangle(Layout, &Dim);
                DefaultInteraction(&Element, ItemInteraction);
                EndElement(&Element);

                DEBUGTextOutAt(V2(GetMinCorner(Element.Bounds).x, 
                                GetMaxCorner(Element.Bounds).y - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), 
                                Text, ItemColor);
            } break;
        }
    }
}

internal void
DEBUGDrawElement(layout *Layout, debug_tree *Tree, debug_element *Element, debug_id DebugID)
{
    debug_state *DebugState = Layout->DebugState;
    debug_stored_event *OldestEvent = Element->OldestEvent;
    if(OldestEvent)
    {
        debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugID);

        switch(OldestEvent->Event.Type)
        {
            case DebugType_CounterThreadList:
            {
                layout_element Element = BeginElementRectangle(Layout, &View->InlineBlock.Dim);
                MakeElementSizeable(&Element);
                // DefaultInteraction(&Element, ItemInteraction);
                EndElement(&Element);

                DrawProfileIn(DebugState, Element.Bounds, Layout->MouseP);
            } break;
            
            case DebugType_OpenDataBlock:
            {
                debug_stored_event *LastOpenBlock = OldestEvent;
                for(debug_stored_event *Event = OldestEvent;
                    Event;
                    Event = Event->Next)
                {
                    if(Event->Event.Type == DebugType_OpenDataBlock)
                    {
                        LastOpenBlock = Event;
                    }
                }

                for(debug_stored_event *Event = LastOpenBlock;
                    Event;
                    Event = Event->Next)
                {
                    debug_id NewID = DebugIDFromGUID(Tree, Event->Event.GUID);
                    DEBUGDrawEvent(Layout, Event, NewID);    
                }
            } break;

            default:
            {
                debug_stored_event *Event = Element->MostRecentEvent;
                DEBUGDrawEvent(Layout, Event, DebugID);
            } break;
        }
    }
}

internal void
DEBUGDrawMainMenu(debug_state *DebugState, render_group *RenderGroup, v2 MouseP)
{
    object_transform NoTransform = DefaultFlatTransform();

    for(debug_tree *Tree = DebugState->TreeSentinel.Next;
        Tree != &DebugState->TreeSentinel;
        Tree = Tree->Next)
    {
        layout Layout = {};
        Layout.DebugState = DebugState;
        Layout.MouseP = MouseP;
        Layout.At = Tree->UIP;
        Layout.Depth = 0;
        Layout.LineAdvance = GetLineAdvanceFor(DebugState->DebugFontInfo)*DebugState->FontScale;
        Layout.SpacingY = 4.0f;

        u32 Depth = 0;
        debug_variable_iterator Stack[DEBUG_MAX_VARIABLE_STACK_DEPTH];

        debug_variable_group *Group = Tree->Group;
        if(Group)
        {
            Stack[Depth].Link = Group->Sentinel.Next;
            Stack[Depth].Sentinel = &Group->Sentinel;
            Depth++;
            while(Depth > 0)
            {
                debug_variable_iterator *Iter = Stack + (Depth - 1);
                if(Iter->Link == Iter->Sentinel)
                {
                    Depth--;
                }
                else
                {
                    Layout.Depth = Depth;

                    debug_variable_link *Link = Iter->Link;
                    Iter->Link = Iter->Link->Next;

                    if(Link->Children) 
                    {
                        debug_id ID = DebugIDFromLink(Tree, Link);
                        debug_view *View = GetOrCreateDebugViewFor(DebugState, ID);
                        debug_interaction ItemInteraction = DebugIDInteraction(DebugInteraction_ToggleExpansion, ID);

                        char Text[256];
                        Assert((Link->Children->NameLength + 1) < ArrayCount(Text));
                        Copy(Link->Children->NameLength, Link->Children->Name, Text);
                        Text[Link->Children->NameLength] = 0;

                        rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);
                        v2 Dim = {GetDim(TextBounds).x, Layout.LineAdvance};

                        layout_element Element = BeginElementRectangle(&Layout, &Dim);
                        DefaultInteraction(&Element, ItemInteraction);
                        EndElement(&Element);

                        bool32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
                        v4 ItemColor = (IsHot) ? V4(1, 1, 0, 1) :  V4(1, 1, 1, 1);

                        DEBUGTextOutAt(V2(GetMinCorner(Element.Bounds).x, 
                                        GetMaxCorner(Element.Bounds).y - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), 
                                        Text, ItemColor);

                        if(View->Collapsible.ExpandedAlways)
                        {
                            Iter = Stack + Depth;
                            Iter->Link = Link->Children->Sentinel.Next;
                            Iter->Sentinel = &Link->Children->Sentinel;
                            Depth++;
                        }
                    }
                    else
                    {
                        debug_id DebugID = DebugIDFromLink(Tree, Link);
                        DEBUGDrawElement(&Layout, Tree, Link->Element, DebugID);
                    }
                }
            }
        }

        DebugState->AtY = Layout.At.y;

        if(1)
        {
            debug_interaction MoveInteraction = {};
            MoveInteraction.Type = DebugInteraction_Move;
            MoveInteraction.P = &Tree->UIP;

            rectangle2 MoveBox = RectCenterHalfDim(Tree->UIP - V2(4.0f, 4.0f), V2(4.0f, 4.0f));
            PushRect(DebugState->RenderGroup, NoTransform, MoveBox, 0.0f, 
                     InteractionIsHot(DebugState, MoveInteraction) ?
                     V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
            
            if(IsInRectangle(MoveBox, MouseP))
            {
                DebugState->NextHotInteraction = MoveInteraction;
            }
        }
    }

#if 0
    uint32 NewHotMenuIndex = ArrayCount(DebugVariableList);
    real32 BestDistanceSq = Real32Maximum;

    real32 MenuRadius = 150.0f;
    real32 AngleStep = 2.0f*Pi32 / (real32)ArrayCount(DebugVariableList);
    for(uint32 MenuItemIndex = 0;
        MenuItemIndex < ArrayCount(DebugVariableList);
        MenuItemIndex++)
    {
        debug_variable *Var = DebugVariableList + MenuItemIndex;
        char *Text = Event->Name;

        v4 ItemColor = Event->Value ? V4(1, 1, 1, 1) : V4(0.5f, 0.5f, 0.5f, 1);
        if(MenuItemIndex == DebugState->HotMenuIndex)
        {
            ItemColor = V4(1, 1, 0, 1);
        }
        real32 Angle = (real32)MenuItemIndex*AngleStep;
        v2 TextP = DebugState->MenuP + MenuRadius*Arm2(Angle);

        real32 ThisDistanceSq = LengthSq(TextP - MouseP);
        if(BestDistanceSq > ThisDistanceSq)
        {
            NewHotMenuIndex = MenuItemIndex;
            BestDistanceSq = ThisDistanceSq;
        }

        rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);
        DEBUGTextOutAt(TextP - 0.5f*GetDim(TextBounds), Text, ItemColor);
    }

    if(LengthSq(MouseP - DebugState->MenuP) > Square(MenuRadius))
    {
        DebugState->HotMenuIndex = NewHotMenuIndex;
    }
    else
    {
        DebugState->HotMenuIndex = ArrayCount(DebugVariableList);
    }
#endif
}

internal void
DEBUGBeginInteract(debug_state *DebugState, game_input *Input, v2 MouseP, bool32 AltUI)
{
    if(DebugState->HotInteraction.Type)
    {
        if(DebugState->HotInteraction.Type == DebugInteraction_AutoModifyVariable)
        {
            switch(DebugState->HotInteraction.Event->Type)
            {
                case DebugType_b32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_ToggleValue;
                } break;

                case DebugType_r32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_DragValue;
                } break;

                case DebugType_OpenDataBlock:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_ToggleValue;
                } break;
            }

            if(AltUI)
            {
                DebugState->HotInteraction.Type = DebugInteraction_TearValue;
            }
        }

        switch(DebugState->HotInteraction.Type)
        {
            case DebugInteraction_TearValue:
            {
#if 0
                debug_variable *RootGroup = DEBUGAddRootGroup(DebugState, "NewUserGroup");
                DEBUGAddVariableToGroup(DebugState, RootGroup, DebugState->HotInteraction.Var);
                debug_tree *Tree = AddTree(DebugState, RootGroup, V2(0, 0));
                Tree->UIP = MouseP;
                DebugState->HotInteraction.Type = DebugInteraction_Move;
                DebugState->HotInteraction.P = &Tree->UIP;
#endif
            } break;

            case DebugInteraction_Select:
            {
                if(!Input->ShiftDown)
                {
                    ClearSelection(DebugState);
                }
                AddToSelection(DebugState, DebugState->HotInteraction.ID);
            } break;
        }

        DebugState->Interaction = DebugState->HotInteraction;
    }
    else
    {
        DebugState->Interaction.Type = DebugInteraction_NOP;
    }
}

internal void
DEBUGEndInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
    switch(DebugState->Interaction.Type)
    {
        case DebugInteraction_ToggleExpansion:
        {
            debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugState->Interaction.ID);
            View->Collapsible.ExpandedAlways = !View->Collapsible.ExpandedAlways;
        } break;

        case DebugInteraction_ToggleValue:
        {
            debug_event *Event = DebugState->Interaction.Event;
            Assert(Event);
            switch(Event->Type)
            {
                case DebugType_b32:
                {
                    Event->Value_b32 = !Event->Value_b32;
                } break;
            }
        } break;
    }
    
    WriteHandmadeConfig(DebugState);

    DebugState->Interaction.Type = DebugInteraction_None;
    DebugState->Interaction.Generic = 0;
}

internal void
DEBUGInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
    v2 dMouseP = MouseP - DebugState->LastMouseP;

/*
    if(Input->MouseButtons[PlatformMouseButton_Right].EndedDown)
    {
        if(Input->MouseButtons[PlatformMouseButton_Right].HalfTransitionCount)
        {
            DebugState->MenuP = MouseP;
        }
        DrawDebugMainMenu(DebugState, RenderGroup, MouseP);
    }
    else if(Input->MouseButtons[PlatformMouseButton_Right].HalfTransitionCount)
*/
    if(DebugState->Interaction.Type)
    {
        debug_event *Event = DebugState->Interaction.Event;
        debug_tree *Tree = DebugState->Interaction.Tree;
        v2 *P = DebugState->Interaction.P;

        // NOTE(georgy): Mouse move interaction
        switch(DebugState->Interaction.Type)
        {
	        case DebugInteraction_DragValue:
            {
                switch(Event->Type)
                {
                    case DebugType_r32:
                    {
                        Event->Value_r32 += 0.1f*dMouseP.y;
                    } break;
                }
            } break;

            case DebugInteraction_Resize:
            {
                *P += V2(dMouseP.x, -dMouseP.y);
                P->x = Maximum(P->x, 10.0f);
                P->y = Maximum(P->y, 10.0f);
            } break;

            case DebugInteraction_Move:
            {
                *P += V2(dMouseP.x, dMouseP.y);
            } break;
        }

        bool32 AltUI = Input->MouseButtons[PlatformMouseButton_Right].EndedDown;

        // NOTE(georgy): Click interaction
        for(uint32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            TransitionIndex++)
        {
            DEBUGEndInteract(DebugState, Input, MouseP);
            DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
        }

        if(!Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            DEBUGEndInteract(DebugState, Input, MouseP);
        }
    }
    else
    {
        DebugState->HotInteraction = DebugState->NextHotInteraction;

        bool32 AltUI = Input->MouseButtons[PlatformMouseButton_Right].EndedDown;
        for(uint32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            TransitionIndex++)
        {
            DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
            DEBUGEndInteract(DebugState, Input, MouseP);
        }

        if(Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
        }
    }

    DebugState->LastMouseP = MouseP;
}

global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;

inline uint32
GetLaneFromThreadIndex(debug_state *DebugState, uint32 ThreadIndex)
{
    uint32 Result = 0;

    // TODO(georgy): Implement thread ID lookup

    return(Result);
}

internal debug_thread *
GetDebugThread(debug_state *DebugState, uint32 ThreadID)
{
    debug_thread *Result = 0;
    for(debug_thread *Thread = DebugState->FirstThread;
        Thread;
        Thread = Thread->Next)
    {
        if(Thread->ID == ThreadID)
        {
            Result = Thread;
            break;
        }
    }

    if(!Result)
    {
        FREELIST_ALLOCATE(Result, DebugState->FirstFreeThread, PushStruct(&DebugState->DebugArena, debug_thread));

        Result->ID = ThreadID;
        Result->LaneIndex = DebugState->FrameBarLaneCount++;
        Result->FirstOpenCodeBlock = 0;
        Result->FirstOpenDataBlock = 0;
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
    }

    return(Result);
}

#if 0
internal debug_frame_region *
AddRegion(debug_state *DebugState, debug_frame *CurrentFrame)
{
    Assert(CurrentFrame->RegionCount < MAX_REGIONS_PER_FRAME);
    debug_frame_region *Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;

    return(Result);
}
#endif

inline open_debug_block *
AllocateOpenDebugBlock(debug_state *DebugState, debug_element *Element,
                       uint32 FrameIndex, debug_event *Event, 
                       open_debug_block **FirstOpenBlock)
{
    open_debug_block *Result;
    FREELIST_ALLOCATE(Result, DebugState->FirstFreeBlock, PushStruct(&DebugState->DebugArena, open_debug_block));
    
    Result->StartingFrameIndex = FrameIndex;
    Result->OpeningEvent = Event;
    Result->Element = Element;
    Result->NextFree = 0;

    Result->Parent = *FirstOpenBlock;
    *FirstOpenBlock = Result;

    return(Result);
}

inline void
DeallocateOpenDebugBlock(debug_state *DebugState, open_debug_block **FirstOpenBlock)
{
    open_debug_block *FreeBlock = *FirstOpenBlock;
    *FirstOpenBlock = FreeBlock->Parent;

    FreeBlock->NextFree = DebugState->FirstFreeBlock;
    DebugState->FirstFreeBlock = FreeBlock;
}

inline bool32
EventsMatch(debug_event *A, debug_event *B)
{
    // TODO(georgy): Have counters for blocks?
    bool32 Result = (A->ThreadID == B->ThreadID);

    return(Result);
}

internal debug_variable_link *
AddElementToGroup(debug_state *DebugState, debug_variable_group *Parent, debug_element *Element)
{
    debug_variable_link *Link = PushStruct(&DebugState->DebugArena, debug_variable_link);

    DLIST_INSERT(&Parent->Sentinel, Link);
    Link->Children = 0;
    Link->Element = Element;

    return(Link);
}

internal void
FreeVariableGroup(debug_state *DebugState, debug_variable_group *Group)
{
    Assert(!"Not implemented!");
}

internal debug_variable_link *
AddGroupToGroup(debug_state *DebugState, debug_variable_group *Parent, debug_variable_group *Group)
{
    debug_variable_link *Link = PushStruct(&DebugState->DebugArena, debug_variable_link);
    
    DLIST_INSERT(&Parent->Sentinel, Link);
    Link->Children = Group;
    Link->Element = 0;

    return(Link);
}

internal debug_variable_group *
CreateVariableGroup(debug_state *DebugState, u32 NameLength, char *Name)
{
    debug_variable_group *Group = PushStruct(&DebugState->DebugArena, debug_variable_group);
    DLIST_INIT(&Group->Sentinel);

    Group->NameLength = NameLength;
    Group->Name = Name;

    return(Group);
}

internal debug_variable_group *
GetOrCreateGroupWithName(debug_state *DebugState, debug_variable_group *Parent, u32 NameLength, char *Name)
{
    debug_variable_group *Result = 0;
    for(debug_variable_link *Link = Parent->Sentinel.Next;
        Link != &Parent->Sentinel;
        Link = Link->Next)
    {
        if(Link->Children && StringsAreEqual(Link->Children->NameLength, Link->Children->Name,
                                             NameLength, Name))
        {
            Result = Link->Children;
            break;
        }
    }

    if(!Result)
    {
        Result = CreateVariableGroup(DebugState, NameLength, Name);
        AddGroupToGroup(DebugState, Parent, Result);
    }

    return(Result);
}

internal debug_variable_group *
GetGroupForHierarchicalName(debug_state *DebugState, debug_variable_group *Parent, char *Name)
{
    debug_variable_group *Result = Parent;

    char *FirstUnderscore = 0;
    for(char *Scan = Name;
        *Scan;
        Scan++)
    {
        if(*Scan == '_')
        {
            FirstUnderscore = Scan;
            break;
        }
    }

    if(FirstUnderscore)
    {
        debug_variable_group *SubGroup = GetOrCreateGroupWithName(DebugState, Parent, (u32)(FirstUnderscore - Name), Name);
        Result = GetGroupForHierarchicalName(DebugState, SubGroup, FirstUnderscore + 1);
    }

    return(Result);
}

internal void
FreeFrame(debug_state *DebugState, debug_frame *Frame)
{
    for(u32 ElementHashIndex = 0;
        ElementHashIndex < ArrayCount(DebugState->ElementHash);
        ElementHashIndex++)
    {
        for(debug_element *Element = DebugState->ElementHash[ElementHashIndex];
            Element;
            Element = Element->NextInHash)
        {
            while(Element->OldestEvent && (Element->OldestEvent->FrameIndex <= Frame->FrameIndex))
            {
                debug_stored_event *FreeEvent = Element->OldestEvent;
                Element->OldestEvent = Element->OldestEvent->Next;
                if(Element->MostRecentEvent == FreeEvent)
                {
                    Assert(FreeEvent->Next == 0);
                    Element->MostRecentEvent = 0;
                }

                FREELIST_DEALLOCATE(FreeEvent, DebugState->FirstFreeStoredEvent);
            }
        }
    }

    FREELIST_DEALLOCATE(Frame, DebugState->FirstFreeFrame);
}

internal void
FreeOldestFrame(debug_state *DebugState)
{
    if(DebugState->OldestFrame)
    {
        debug_frame *Frame = DebugState->OldestFrame;
        DebugState->OldestFrame = DebugState->OldestFrame->Next;
        if(DebugState->MostRecentFrame == Frame)
        {
            Assert(Frame->Next == 0);
            DebugState->MostRecentFrame = 0;
        }

        FreeFrame(DebugState, Frame);
    }
}

internal debug_frame *
NewFrame(debug_state *DebugState, u64 BeginClock)
{
    debug_frame *Result = 0;
    while(!Result)
    {
        Result = DebugState->FirstFreeFrame;
        if(Result)
        {
            DebugState->FirstFreeFrame = Result->NextFree;
        }
        else
        {
            if(ArenaHasRoomFor(&DebugState->PerFrameArena, sizeof(debug_frame)))
            {
                Result = PushStruct(&DebugState->PerFrameArena, debug_frame);
            }
            else
            {
                Assert(DebugState->OldestFrame);
                FreeOldestFrame(DebugState);
            }
        }
    } 

    ZeroStruct(*Result);
    Result->FrameIndex = DebugState->TotalFrameCount++;
    Result->FrameBarScale = 1.0f;
    
    Result->BeginClock = BeginClock;

    return(Result);
}

internal debug_stored_event *
StoreEvent(debug_state *DebugState, debug_element *Element, debug_event *Event)
{
    debug_stored_event *Result = 0;
    while(!Result)
    {
        Result = DebugState->FirstFreeStoredEvent;
        if(Result)
        {
            DebugState->FirstFreeStoredEvent = Result->NextFree;
        }
        else
        {
            if(ArenaHasRoomFor(&DebugState->PerFrameArena, sizeof(debug_stored_event)))
            {
                Result = PushStruct(&DebugState->PerFrameArena, debug_stored_event);
            }
            else
            {
                Assert(DebugState->OldestFrame);
                FreeOldestFrame(DebugState);
            }
        }
    } 

    Result->Next = 0;
    Result->FrameIndex = DebugState->CollationFrame->FrameIndex;
    Result->Event = *Event;

    if(Element->MostRecentEvent)
    {
        Element->MostRecentEvent = Element->MostRecentEvent->Next = Result;
    }
    else
    {
        Element->OldestEvent = Element->MostRecentEvent = Result;
    }

    return(Result);
}

internal debug_element *
GetElementFromEvent(debug_state *DebugState, debug_event *Event)
{
    Assert(Event->GUID);

    u32 HashValue = (u32)((memory_index)Event->GUID >> 2);
    // TODO(georgy): Verify this turns into an and (not a mod)
    u32 Index = (HashValue % ArrayCount(DebugState->ElementHash));

    debug_element *Result = 0;

    for(debug_element *Chain = DebugState->ElementHash[Index];
        Chain;
        Chain = Chain->NextInHash)
    {
        if(Chain->GUID == Event->GUID)
        {
            Result = Chain;
            break;
        }
    }

    if(!Result)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_element);

        Result->GUID = Event->GUID;
        Result->NextInHash = DebugState->ElementHash[Index];
        DebugState->ElementHash[Index] = Result;

        Result->OldestEvent = Result->MostRecentEvent = 0;

        debug_variable_group *ParentGroup = GetGroupForHierarchicalName(DebugState, DebugState->RootGroup, Event->BlockName);
        AddElementToGroup(DebugState, ParentGroup, Result);
    }

    return(Result);
}

internal void
CollateDebugRecords(debug_state *DebugState, u32 EventCount, debug_event *EventArray)
{
    for(uint32 EventIndex = 0;
        EventIndex < EventCount;
        EventIndex++)
    {
        debug_event *Event = EventArray + EventIndex;
        debug_element *Element = GetElementFromEvent(DebugState, Event);

        if(!DebugState->CollationFrame)
        {
            DebugState->CollationFrame = NewFrame(DebugState, Event->Clock);
        }

        if(Event->Type == DebugType_MarkDebugValue)
        {
            StoreEvent(DebugState, Element, Event);
        }
        else if(Event->Type == DebugType_FrameMarker)
        {
            Assert(DebugState->CollationFrame);

            DebugState->CollationFrame->EndClock = Event->Clock;
            DebugState->CollationFrame->WallSecondsElapsed = Event->Value_r32;
            
#if 0
            real32 ClockRange = (real32)(DebugState->CollationFrame->EndClock - DebugState->CollationFrame->BeginClock);
            if(ClockRange > 1.0f)
            {
                real32 FrameBarScale = 1.0f / ClockRange;
                if(DebugState->FrameBarScale > FrameBarScale)
                {
                    DebugState->FrameBarScale = FrameBarScale;
                }
            }
#endif

            if(DebugState->Paused)
            {
                FreeFrame(DebugState, DebugState->CollationFrame);
            }
            else
            {
                if(DebugState->MostRecentFrame)
                {
                    DebugState->MostRecentFrame = DebugState->MostRecentFrame->Next = DebugState->CollationFrame;
                }
                else
                {
                    DebugState->MostRecentFrame = DebugState->OldestFrame = DebugState->CollationFrame;
                }
                DebugState->FrameCount++;
            }

            DebugState->CollationFrame = NewFrame(DebugState, Event->Clock);
        }
        else
        {
            Assert(DebugState->CollationFrame);
            
            uint32 FrameIndex = DebugState->FrameCount;
            debug_thread *Thread = GetDebugThread(DebugState, Event->ThreadID);
            uint64 RelativeClock = Event->Clock - DebugState->CollationFrame->BeginClock;

            switch(Event->Type)
            {
                case DebugType_BeginBlock:
                {
                    open_debug_block *DebugBlock = AllocateOpenDebugBlock(DebugState, Element, FrameIndex, Event, 
                                                                            &Thread->FirstOpenCodeBlock);
                } break;

                case DebugType_EndBlock:
                {
                    if(Thread->FirstOpenCodeBlock)
                    {
                        open_debug_block *MatchingBlock = Thread->FirstOpenCodeBlock;
                        debug_event *OpeningEvent = MatchingBlock->OpeningEvent;
                        if(EventsMatch(OpeningEvent, Event))
                        {
                            if(MatchingBlock->StartingFrameIndex == FrameIndex)
                            {
                                char *MatchName = 
                                    MatchingBlock->Parent ? MatchingBlock->Parent->OpeningEvent->BlockName : 0;
                                if(MatchName == DebugState->ScopeToRecord)
                                {
#if 0
                                    real32 MinT = (real32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 MaxT = (real32)(Event->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 ThresholdT = 0.01f;
                                    if((MaxT - MinT) > ThresholdT)
                                    {
                                        debug_frame_region *Region = AddRegion(DebugState, DebugState->CollationFrame);
                                        Region->Event = OpeningEvent;
                                        Region->CycleCount = Event->Clock - OpeningEvent->Clock;
                                        Region->LaneIndex = (uint16)Thread->LaneIndex;
                                        Region->MinT = MinT;
                                        Region->MaxT = MaxT;
                                        Region->ColorIndex = (uint16)OpeningEvent->BlockName;
                                    }
#endif
                                }   
                            }
                            else
                            {
                                // TODO(georgy): Record all frames in between and begin/end span!
                            }

                            DeallocateOpenDebugBlock(DebugState, &Thread->FirstOpenCodeBlock);
                        }
                        else
                        {
                            // TODO(georgy): Record span that goes to the beginning of the frame series?
                        }
                    }
                } break;

                case DebugType_OpenDataBlock:
                {
                    open_debug_block *DebugBlock = AllocateOpenDebugBlock(DebugState, Element, FrameIndex, Event,
                                                                          &Thread->FirstOpenDataBlock);
                    StoreEvent(DebugState, Element, Event);
                } break;                

                case DebugType_CloseDataBlock:
                {
                    if(Thread->FirstOpenDataBlock)
                    {
                        StoreEvent(DebugState, Thread->FirstOpenDataBlock->Element, Event);

                        open_debug_block *MatchingBlock = Thread->FirstOpenDataBlock;
                        debug_event *OpeningEvent = MatchingBlock->OpeningEvent;
                        if(EventsMatch(OpeningEvent, Event))
                        {
                            DeallocateOpenDebugBlock(DebugState, &Thread->FirstOpenDataBlock);
                        }
                    }
                } break;                

                default:
                {
                    debug_element *StorageElement = Element;
                    if(Thread->FirstOpenDataBlock)
                    {
                        StorageElement = Thread->FirstOpenDataBlock->Element;
                    }
                    StoreEvent(DebugState, StorageElement, Event);
                } break;
            }
        }
    }
}

internal void 
DEBUGStart(debug_state *DebugState, game_assets *Assets, uint32 Width, uint32 Height)
{
    TIMED_FUNCTION();

    if(DebugState)
    {
        if(!DebugState->Initialized)
        {
            DebugState->FrameBarLaneCount = 0;
            DebugState->FirstThread = 0;
            DebugState->FirstFreeThread = 0;
            DebugState->FirstFreeBlock = 0;

            DebugState->OldestFrame = DebugState->MostRecentFrame = DebugState->FirstFreeFrame = 0;
            DebugState->CollationFrame = 0;

            DebugState->HighPriorityQueue = DebugGlobalMemory->HighPriorityQueue;
            DebugState->TreeSentinel.Next = &DebugState->TreeSentinel;
            DebugState->TreeSentinel.Prev = &DebugState->TreeSentinel;
            DebugState->TreeSentinel.Group = 0;

            memory_index TotalMemorySize = DebugGlobalMemory->DebugStorageSize - sizeof(debug_state);
            InitializeArena(&DebugState->DebugArena, TotalMemorySize, DebugState + 1);
#if 1
            SubArena(&DebugState->PerFrameArena, &DebugState->DebugArena, (TotalMemorySize / 2));
#else
            // NOTE(georgy): This is the stress-testing case to make sure the memory
            // recycling works.
            SubArena(&DebugState->PerFrameArena, &DebugState->DebugArena, 128*1024);
#endif

            DebugState->RootGroup = CreateVariableGroup(DebugState, 4, "Root");

#if 0
            debug_variable_definition_context Context = {};
            Context.State = DebugState;
            Context.Arena = &DebugState->DebugArena;
            Context.GroupStack[0] = 0;

            DebugState->RootGroup = DEBUGBeginVariableGroup(&Context, "Root");
            DEBUGBeginVariableGroup(&Context, "Debugging");

            DEBUGCreateVariables(&Context);
            DEBUGBeginVariableGroup(&Context, "Profile");
            DEBUGBeginVariableGroup(&Context, "By Thread");
            DEBUGAddVariable(&Context, DebugType_CounterThreadList, "");
            DEBUGEndVariableGroup(&Context);
            DEBUGBeginVariableGroup(&Context, "By Function");
            DEBUGAddVariable(&Context, DebugType_CounterThreadList, "");
            DEBUGEndVariableGroup(&Context);
            DEBUGEndVariableGroup(&Context);

            asset_vector MatchVector = {};
            MatchVector.E[Tag_FacingDirection] = 0.0f;
            asset_vector WeightVector = {};
            WeightVector.E[Tag_FacingDirection] = 1.0f;
            bitmap_id ID = GetBestMatchBitmapFrom(Assets, Asset_Head, &MatchVector, &WeightVector);
            DEBUGAddVariable(&Context, "Test Bitmap", ID);

            DEBUGEndVariableGroup(&Context);
            DEBUGEndVariableGroup(&Context);
            Assert(Context.GroupDepth == 0);
#endif

            DebugState->RenderGroup = AllocateRenderGroup(Assets, &DebugState->DebugArena, 
                                                          Megabytes(16), false);

            DebugState->Paused = false;
            DebugState->ScopeToRecord = 0;

            DebugState->Initialized = true;

            AddTree(DebugState, DebugState->RootGroup, V2(-0.5f*Width, 0.5f*Height));
        }

        BeginRender(DebugState->RenderGroup);

        DebugState->GlobalWidth = (real32)Width;
        DebugState->GlobalHeight = (real32)Height;

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_FontType] = (real32)FontType_Debug;
        WeightVector.E[Tag_FontType] = (real32)1.0f;
        DebugState->FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

        DebugState->DebugFont = PushFont(DebugState->RenderGroup, DebugState->FontID);
        DebugState->DebugFontInfo = GetFontInfo(DebugState->RenderGroup->Assets, DebugState->FontID);

        DebugState->FontScale = 0.45f;
        Orthographic(DebugState->RenderGroup, Width, Height, 1.0f);
        DebugState->LeftEdge = -0.5f*Width;
        DebugState->RightEdge = 0.5f*Width;

        DebugState->AtY = 0.5f*Height;        
    }
}

internal void
DEBUGDumpStruct(uint32 MemberCount, member_definition *MemberDefs, void *StructPtr, uint32 IndentLevel = 0)
{
    for(uint32 MemberIndex = 0;
        MemberIndex < MemberCount;
        MemberIndex++)
    {
        char TextBufferBase[256];
        char *TextBuffer = TextBufferBase;
        for(uint32 Indent = 0;
            Indent < IndentLevel;
            Indent++)
        {
            *TextBuffer++ = ' ';
            *TextBuffer++ = ' ';
            *TextBuffer++ = ' ';
            *TextBuffer++ = ' ';
        }
        TextBuffer[0] = 0;
        size_t TextBufferLeft = (TextBufferBase + sizeof(TextBufferBase)) - TextBuffer;

        member_definition *Member = MemberDefs + MemberIndex;

        void *MemberPtr = (uint8 *)StructPtr + Member->Offset;
        if(Member->Flags & MetaMemberFlag_IsPointer)
        {
            MemberPtr = *(void **)MemberPtr;
        }
        if(MemberPtr)
        {
            switch(Member->Type)
            {
                case MetaType_uint32: 
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: %u", Member->Name, *(uint32 *)MemberPtr);
                } break;

                case MetaType_bool32: 
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: %u", Member->Name, *(bool32 *)MemberPtr);
                } break;

                case MetaType_int32: 
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: %d", Member->Name, *(int32 *)MemberPtr);
                } break;

                case MetaType_real32: 
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: %f", Member->Name, *(real32 *)MemberPtr);
                } break;

                case MetaType_v2:
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: {%f,%f}", 
                                Member->Name, 
                                ((v2 *)MemberPtr)->x, ((v2 *)MemberPtr)->y);
                } break;

                case MetaType_v3:
                {
                    _snprintf_s(TextBuffer, TextBufferLeft, TextBufferLeft, "%s: {%f,%f,%f}", 
                                Member->Name, 
                                ((v3 *)MemberPtr)->x, ((v3 *)MemberPtr)->y, ((v3 *)MemberPtr)->z);
                } break;

                META_HANDLE_TYPE_DUMP(MemberPtr, IndentLevel + 1);
            }

            if(TextBuffer[0])
            {
                DEBUGTextLine(TextBufferBase);
            }
        }
    }
}

internal void
DEBUGEnd(debug_state *DebugState, game_input *Input, loaded_bitmap *DrawBuffer)
{
    TIMED_FUNCTION();

    if(DebugState)
    {
        render_group *RenderGroup = DebugState->RenderGroup;

        debug_event *HotEvent = 0;

        v2 MouseP = Unproject(DebugState->RenderGroup, DefaultFlatTransform(), V2((real32)Input->MouseX, (real32)Input->MouseY)).xy;

        DEBUGDrawMainMenu(DebugState, RenderGroup, MouseP);
        DEBUGInteract(DebugState, Input, MouseP);

        if(DebugState->Compiling)
        {
            debug_process_state State = Platform.DEBUGGetProcessState(DebugState->Compiler);
            if(State.IsRunning)
            {
                DEBUGTextLine("COMPILING");
            }
            else
            {
                DebugState->Compiling = false;
            }
        }

        loaded_font *Font = DebugState->DebugFont;
        hha_font *Info = DebugState->DebugFontInfo;
        if(Font)
        {
#if 0
            for(uint32 CounterIndex = 0;
                CounterIndex < DebugState->CounterCount;
                ++CounterIndex)
            {
                debug_counter_state *Counter = DebugState->CounterStates + CounterIndex;

                debug_statistic HitCount, CycleCount, CycleOverHit;
                BeginDebugStatistic(&HitCount);
                BeginDebugStatistic(&CycleCount);
                BeginDebugStatistic(&CycleOverHit);
                
                for(uint32 SnapshotIndex = 0;
                    SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
                    SnapshotIndex++)
                {
                    AccumulateDebugStatistic(&HitCount, Counter->Snapshots[SnapshotIndex].HitCount);
                    AccumulateDebugStatistic(&CycleCount, (uint32)Counter->Snapshots[SnapshotIndex].CycleCount);

                    real64 HOC = 0.0;
                    if(Counter->Snapshots[SnapshotIndex].HitCount)
                    {
                        HOC = (real64)Counter->Snapshots[SnapshotIndex].CycleCount / 
                            (real64)Counter->Snapshots[SnapshotIndex].HitCount;
                            
                    }
                    AccumulateDebugStatistic(&CycleOverHit, HOC);
                }
                EndDebugStatistic(&HitCount);
                EndDebugStatistic(&CycleCount);
                EndDebugStatistic(&CycleOverHit);

                if(Counter->BlockName)
                {
                    if(CycleCount.Max > 0.0f)
                    {
                        real32 BarWidth = 4.0f;
                        real32 ChartLeft = 150.0f;
                        real32 ChartMinY = AtY;
                        real32 ChartHeight = Info->AscenderHeight*FontScale;
                        real32 Scale = 1.0f / (real32)CycleCount.Max;
                        for(uint32 SnapshotIndex = 0;
                            SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
                            SnapshotIndex++)
                        {
                            real32 ThisProportion = Scale*(real32)Counter->Snapshots[SnapshotIndex].CycleCount;
                            real32 ThisHeight = ChartHeight*ThisProportion;
                            PushRect(RenderGroup, V3(ChartLeft + BarWidth*(real32)SnapshotIndex + 0.5f*BarWidth, ChartMinY + 0.5f*ThisHeight, 0.0f), 
                                     V2(BarWidth, ThisHeight), V4(ThisProportion, 1.0f, 0.0f, 1.0f));
                        }
                    }

#if 1
                    char TextBuffer[256];
                    _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                                "%s(%d): %ucy %uh %ucy/h\n", 
                                Counter->BlockName,
                                Counter->LineNumber,
                                (uint32)CycleCount.Avg,
                                (uint32)HitCount.Avg, 
                                (uint32)CycleOverHit.Avg);
                    DEBUGTextLine(TextBuffer);
#endif
                }
            }
#endif
            if(DebugState->MostRecentFrame)
            {
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                            "Last frame time: %.02fms", 
                            DebugState->MostRecentFrame->WallSecondsElapsed * 1000.0f);
                DEBUGTextLine(TextBuffer);

                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                            "Per-frame arena space remaining: %ukb", 
                            (u32)(GetArenaSizeRemaining(&DebugState->PerFrameArena, AlignNoClear(1)) / 1024));
                DEBUGTextLine(TextBuffer);
            }
        }   

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            if(HotEvent)
            {
                DebugState->ScopeToRecord = HotEvent->BlockName;
            }
            else
            {
                DebugState->ScopeToRecord = 0;
            }
        }

        RenderToOutput(DebugState->HighPriorityQueue, DebugState->RenderGroup, DrawBuffer,
                       &DebugState->DebugArena);
        EndRender(DebugState->RenderGroup);

        // NOTE(georgy): Clear the UI state for the next frame
        ZeroStruct(DebugState->NextHotInteraction);
    }
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGFrameEnd)
{
    GlobalDebugTable->CurrentEventArrayIndex = !GlobalDebugTable->CurrentEventArrayIndex;
    uint64 ArrayIndex_EventIndex = AtomicExchangeUInt64(&GlobalDebugTable->EventArrayIndex_EventIndex, 
                                                        (uint64)GlobalDebugTable->CurrentEventArrayIndex << 32);

    uint32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    Assert(EventArrayIndex <= 1);
    uint32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if(DebugState)
    {
        game_assets *Assets = DEBUGGetGameAssets(Memory);

        DEBUGStart(DebugState, Assets, Buffer->Width, Buffer->Height);
        CollateDebugRecords(DebugState, EventCount, GlobalDebugTable->Events[EventArrayIndex]);

        loaded_bitmap DrawBuffer = {};
        DrawBuffer.Width = Buffer->Width;
        DrawBuffer.Height = Buffer->Height;
        DrawBuffer.Pitch = Buffer->Pitch;
        DrawBuffer.Memory = Buffer->Memory;
        DEBUGEnd(DebugState, Input, &DrawBuffer);
    }

    return(GlobalDebugTable);
}