#include "handmade_debug.h"

// TODO(georgy): Stop using stdio!
#include <stdio.h>

#include "handmade_debug_variables.h"

internal void RestartCollation(debug_state *DebugState, uint32 InvalidEventArrayIndex);

inline debug_state *
DEBUGGetState(game_memory *Memory)
{
    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    Assert(DebugState && DebugState->Initialized);

    return(DebugState);
}

inline debug_state *
DEBUGGetState(void)
{
    debug_state *DebugState = DEBUGGetState(DebugGlobalMemory);

    return(DebugState);
}

internal debug_variable_hierarchy *
AddHierarchy(debug_state *DebugState, debug_variable_reference *Group, v2 AtP)
{   
    debug_variable_hierarchy *Hierarchy = PushStruct(&DebugState->DebugArena, debug_variable_hierarchy);

    Hierarchy->UIP = AtP;
    Hierarchy->Group = Group;
    Hierarchy->Next = DebugState->HierarchySentinel.Next;
    Hierarchy->Prev = &DebugState->HierarchySentinel;

    Hierarchy->Next->Prev = Hierarchy;
    Hierarchy->Prev->Next = Hierarchy;

    return(Hierarchy);
}

internal void 
DEBUGStart(game_assets *Assets, uint32 Width, uint32 Height)
{
    TIMED_FUNCTION();

    debug_state *DebugState = (debug_state *)DebugGlobalMemory->DebugStorage;
    if(DebugState)
    {
        if(!DebugState->Initialized)
        {
            DebugState->HighPriorityQueue = DebugGlobalMemory->HighPriorityQueue;
            DebugState->HierarchySentinel.Next = &DebugState->HierarchySentinel;
            DebugState->HierarchySentinel.Prev = &DebugState->HierarchySentinel;
            DebugState->HierarchySentinel.Group = 0;

            InitializeArena(&DebugState->DebugArena, DebugGlobalMemory->DebugStorageSize - sizeof(debug_state), DebugState + 1);

            debug_variable_definition_context Context = {};
            Context.State = DebugState;
            Context.Arena = &DebugState->DebugArena;
            Context.Group = DEBUGBeginVariableGroup(&Context, "Root");

            DEBUGBeginVariableGroup(&Context, "Debugging");

            DEBUGCreateVariables(&Context);
            DEBUGBeginVariableGroup(&Context, "Profile");
            DEBUGBeginVariableGroup(&Context, "By Thread");
            debug_variable_reference *ThreadList = 
                DEBUGAddVariable(&Context, DebugVariableType_CounterThreadList, "");
            ThreadList->Var->Profile.Dimension = V2(512.0f, 100.0f);
            DEBUGEndVariableGroup(&Context);
            DEBUGBeginVariableGroup(&Context, "By Function");
            debug_variable_reference *FunctionList = 
                DEBUGAddVariable(&Context, DebugVariableType_CounterThreadList, "");
            FunctionList->Var->Profile.Dimension = V2(512.0f, 200.0f);
            DEBUGEndVariableGroup(&Context);
            DEBUGEndVariableGroup(&Context);

            asset_vector MatchVector = {};
            MatchVector.E[Tag_FacingDirection] = 0.0f;
            asset_vector WeightVector = {};
            WeightVector.E[Tag_FacingDirection] = 1.0f;
            bitmap_id ID = GetBestMatchBitmapFrom(Assets, Asset_Head, &MatchVector, &WeightVector);
            DEBUGAddVariable(&Context, "Test Bitmap", ID);

            DEBUGEndVariableGroup(&Context);
            
            DebugState->RootGroup = Context.Group;

            DebugState->RenderGroup = AllocateRenderGroup(Assets, &DebugState->DebugArena, 
                                                          Megabytes(16), false);

            DebugState->Paused = false;
            DebugState->ScopeToRecord = 0;

            DebugState->Initialized = true;

            SubArena(&DebugState->CollateArena, &DebugState->DebugArena, Megabytes(32), 4);
            DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

            RestartCollation(DebugState, 0);

            AddHierarchy(DebugState, DebugState->RootGroup, V2(-0.5f*Width, 0.5f*Height));
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

        DebugState->FontScale = 0.5f;
        Orthographic(DebugState->RenderGroup, Width, Height, 1.0f);
        DebugState->LeftEdge = -0.5f*Width;
        DebugState->RightEdge = 0.5f*Width;

        DebugState->AtY = 0.5f*Height;        
    }
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
                    PushBitmap(RenderGroup, BitmapID, BitmapScale, BitmapOffset, Color);
                }
                else
                {
                    Assert(Op == DEBUGTextOp_SizeText);

                    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
                    if(Bitmap)
                    {
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, Bitmap, BitmapScale, BitmapOffset, 1.0f);
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
DEBUGVariableToText(char *Buffer, char *End, debug_variable *Var, uint32 Flags)
{
    char *At = Buffer;

    if(Flags & DEBUGVarToText_AddDebugUI)
    {
        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                          "#define DEBUGUI_");
    }
    
    if(Flags & DEBUGVarToText_AddName)
    {
        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                          "%s%s ", Var->Name, (Flags & DEBUGVarToText_Colon) ? ":" : "");
    }

    switch(Var->Type)
    {
        case DebugVariableType_Bool32:
        {
            if(Flags & DEBUGVarToText_PrettyBools)
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "%s", Var->Bool32 ? "true" : "false");
            }
            else
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "%d", Var->Bool32);
            }
        } break;

        case DebugVariableType_Int32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "%d", Var->Int32);
        } break;

        case DebugVariableType_UInt32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "%u", Var->UInt32);
        } break;

        case DebugVariableType_Real32:
        {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "%f", Var->Real32);
                if(Flags & DEBUGVarToText_FloatSuffix)
                {
                    *At++ = 'f';
                }
        } break;

        case DebugVariableType_V2:
        {
             At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "V2(%f, %f)", Var->Vector2.x, Var->Vector2.y);
        } break;

        case DebugVariableType_V3:
        {
             At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "V3(%f, %f, %f)", Var->Vector3.x, Var->Vector3.y, Var->Vector3.z);
        } break;

        case DebugVariableType_V4:
        {
             At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                  "V4(%f, %f, %f, %f)", Var->Vector4.x, Var->Vector4.y, Var->Vector4.z, Var->Vector4.w);
        } break;

        case DebugVariableType_Group:
        {
        } break;

        InvalidDefaultCase;
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

internal void
WriteHandmadeConfig(debug_state *DebugState)
{
    char Temp[4096];
    char *At = Temp;
    char *End = Temp + sizeof(Temp);

    int Depth = 0;
    debug_variable_reference *Ref = DebugState->RootGroup->Var->Group.FirstChild;
    while(Ref)
    {
        debug_variable *Var = Ref->Var;
        if (DEBUGShouldBeWritten(Var->Type))
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

            if(Var->Type == DebugVariableType_Group)
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), 
                                "// ");
            }
            At += DEBUGVariableToText(At, End, Var, DEBUGVarToText_AddDebugUI|
                                                    DEBUGVarToText_AddName|
                                                    DEBUGVarToText_FloatSuffix|
                                                    DEBUGVarToText_LineFeedEnd);
        }

        if(Var->Type == DebugVariableType_Group)
        {
            Ref = Var->Group.FirstChild;
            Depth++;
        }
        else
        {
            while(Ref)
            {
                if(Ref->Next)
                {
                    Ref = Ref->Next;
                    break;
                }
                else
                {
                    Ref = Ref->Parent;
                    Depth--;
                }
            }
        }
    }
    
    Platform.DEBUGWriteEntireFile("../code/handmade_config.h", (uint32)(At - Temp), Temp);

    if(!DebugState->Compiling)
    {
        DebugState->Compiling = true;
        DebugState->Compiler = Platform.DEBUGExecuteSystemCommand("..\\code", "c:\\windows\\system32\\cmd.exe", "/C build.bat");
    }
}

internal void 
DrawProfileIn(debug_state *DebugState, rectangle2 ProfileRect, v2 MouseP)
{
    PushRect(DebugState->RenderGroup, ProfileRect, 0.0f, V4(0.0f, 0.0f, 0.0f, 0.25f));

    real32 BarSpacing = 4.0f;
    real32 LaneHeight = 0.0f;
    uint32 LaneCount = DebugState->FrameBarLaneCount;

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
    real32 Scale = ChartWidth*DebugState->FrameBarScale;

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
#if 1
    for(uint32 FrameIndex = 0;
        FrameIndex < MaxFrame;
        FrameIndex++)
    {
        debug_frame *Frame = DebugState->Frames + DebugState->FrameCount - (FrameIndex + 1);
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
                debug_record *Record = Region->Record;
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                            "%s: %I64ucy [%s(%d)]", 
                            Record->BlockName,
                            Region->CycleCount,
                            Record->FileName,
                            Record->LineNumber);
                DEBUGTextOutAt(MouseP + V2(0.0f, 10.0f), TextBuffer);

                // HotRecord = Record;
            }
        }
    }
#if 0
        PushRect(DebugState->RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0.0f), V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
#endif
#endif
}

inline bool32
InteractionsAreEqual(debug_interaction A, debug_interaction B)
{
    bool32 Result = ((A.Type == B.Type) &&
                     (A.Generic == B.Generic));
    return(Result);
}

inline bool32
InteractionIsHot(debug_state *DebugState, debug_interaction B)
{
    bool32 Result = InteractionsAreEqual(DebugState->HotInteraction, B);
    return(Result);
}

internal void
DEBUGDrawMainMenu(debug_state *DebugState, render_group *RenderGroup, v2 MouseP)
{
    for(debug_variable_hierarchy *Hierarchy = DebugState->HierarchySentinel.Next;
        Hierarchy != &DebugState->HierarchySentinel;
        Hierarchy = Hierarchy->Next)
    {
        real32 AtX = Hierarchy->UIP.x;
        real32 AtY = Hierarchy->UIP.y;
        real32 LineAdvance = GetLineAdvanceFor(DebugState->DebugFontInfo)*DebugState->FontScale;

        real32 SpacingY = 4.0f;
        int Depth = 0;
        debug_variable_reference *Ref = Hierarchy->Group->Var->Group.FirstChild;
        while(Ref)
        {
            debug_variable *Var = Ref->Var;

            debug_interaction ItemInteraction = {};
            ItemInteraction.Type = DebugInteraction_AutoModifyVariable;
			ItemInteraction.Var = Var;

            bool32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
            v4 ItemColor = (IsHot) ? V4(1, 1, 0, 1) :  V4(1, 1, 1, 1);

            rectangle2 Bounds = {};
            switch(Var->Type)
            {
                case DebugVariableType_CounterThreadList:
                {
                    v2 MinCorner = V2(AtX + Depth*2.0f*LineAdvance, AtY - Var->Profile.Dimension.y);
                    v2 MaxCorner = V2(MinCorner.x + Var->Profile.Dimension.x, AtY);
                    v2 SizeP = V2(MaxCorner.x, MinCorner.y);
                    Bounds = RectMinMax(MinCorner, MaxCorner);
                    DrawProfileIn(DebugState, Bounds, MouseP);

                    debug_interaction SizeInteraction = {};
                    SizeInteraction.Type = DebugInteraction_Resize;
                    SizeInteraction.P = &Var->Profile.Dimension;
                    rectangle2 SizeBox = RectCenterHalfDim(SizeP, V2(4.0f, 4.0f));
                    PushRect(DebugState->RenderGroup, SizeBox, 0.0f, 
                            InteractionIsHot(DebugState, SizeInteraction) ? 
                            V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
                    
                    if(IsInRectangle(SizeBox, MouseP))
                    {
                        DebugState->NextHotInteraction = SizeInteraction;
                    }
                    else if(IsInRectangle(Bounds, MouseP))
                    {
                        DebugState->NextHotInteraction = ItemInteraction;
                    }   

                    Bounds.Min.y -= SpacingY;
                } break;

                case DebugVariableType_BitmapDisplay:
                {
                    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, Var->BitmapDisplay.ID, RenderGroup->GenerationID);
                    real32 BitmapScale = Var->BitmapDisplay.Dim.y;
                    v2 MinCorner = V2(AtX + Depth*2.0f*LineAdvance, AtY - Var->BitmapDisplay.Dim.y);
                    if(Bitmap)
                    {
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, Bitmap, BitmapScale, V3(MinCorner, 0.0f), 1.0f);
                        Var->BitmapDisplay.Dim.x = Dim.Size.x;
                    }
                    v2 MaxCorner = V2(MinCorner.x + Var->BitmapDisplay.Dim.x, AtY);
                    v2 SizeP = V2(MaxCorner.x, MinCorner.y);
                    Bounds = RectMinMax(MinCorner, MaxCorner);
                    
                    PushRect(DebugState->RenderGroup, Bounds, 0.0f, V4(0, 0, 0, 1));
                    PushBitmap(DebugState->RenderGroup, Var->BitmapDisplay.ID, Var->BitmapDisplay.Dim.y, 
                               V3(MinCorner, 0.0f), V4(1, 1, 1, 1), 0.0f);

                    debug_interaction SizeInteraction = {};
                    SizeInteraction.Type = DebugInteraction_Resize;
                    SizeInteraction.P = &Var->BitmapDisplay.Dim;
                    rectangle2 SizeBox = RectCenterHalfDim(SizeP, V2(4.0f, 4.0f));
                    PushRect(DebugState->RenderGroup, SizeBox, 0.0f, 
                            InteractionIsHot(DebugState, SizeInteraction) ? 
                            V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
                    
                    if(IsInRectangle(SizeBox, MouseP))
                    {
                        DebugState->NextHotInteraction = SizeInteraction;
                    }
                    else if(IsInRectangle(Bounds, MouseP))
                    {
                        debug_interaction TearInteraction = {};
                        TearInteraction.Type = DebugInteraction_TearValue;
                        TearInteraction.Var = Var;
                        DebugState->NextHotInteraction = TearInteraction;
                    }   

                    Bounds.Min.y -= SpacingY;
                } break;

                default:
                {
                    char Text[256];
                    DEBUGVariableToText(Text, Text + sizeof(Text), Var, DEBUGVarToText_AddName|
                                                                        DEBUGVarToText_NullTerminator|
                                                                        DEBUGVarToText_Colon|
                                                                        DEBUGVarToText_PrettyBools);

                    real32 LeftPx = AtX + Depth*2.0f*LineAdvance;
                    real32 TopPy = AtY;
                    rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);

                    Bounds = RectMinMax(V2(LeftPx + TextBounds.Min.x, TopPy - LineAdvance),
                                        V2(LeftPx + TextBounds.Max.x, TopPy));
                    DEBUGTextOutAt(V2(LeftPx, TopPy - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), 
                                   Text, ItemColor);
                                
                    if(IsInRectangle(Bounds, MouseP))
                    {
                        DebugState->NextHotInteraction = ItemInteraction;
                    }                               
                } break;
            }
            
            AtY = GetMinCorner(Bounds).y;

            if((Var->Type == DebugVariableType_Group) && Var->Group.Expanded)
            {
                Ref = Var->Group.FirstChild;
                Depth++;
            }
            else
            {
                while(Ref)
                {
                    if(Ref->Next)
                    {
                        Ref = Ref->Next;
                        break;
                    }
                    else
                    {
                        Ref = Ref->Parent;
                        Depth--;
                    }
                }
            }
        }

        DebugState->AtY = AtY;

        if(1)
        {
            debug_interaction MoveInteraction = {};
            MoveInteraction.Type = DebugInteraction_Move;
            MoveInteraction.P = &Hierarchy->UIP;

            rectangle2 MoveBox = RectCenterHalfDim(Hierarchy->UIP - V2(4.0f, 4.0f), V2(4.0f, 4.0f));
            PushRect(DebugState->RenderGroup, MoveBox, 0.0f, 
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
        char *Text = Var->Name;

        v4 ItemColor = Var->Value ? V4(1, 1, 1, 1) : V4(0.5f, 0.5f, 0.5f, 1);
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
            switch(DebugState->HotInteraction.Var->Type)
            {
                case DebugVariableType_Bool32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_ToggleValue;
                } break;

                case DebugVariableType_Real32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_DragValue;
                } break;

                case DebugVariableType_Group:
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
                debug_variable_reference *RootGroup = DEBUGAddRootGroup(DebugState, "NewUserGroup");
                DEBUGAddVariableReference(DebugState, RootGroup, DebugState->HotInteraction.Var);
                debug_variable_hierarchy *Hierarchy = AddHierarchy(DebugState, RootGroup, V2(0, 0));
                Hierarchy->UIP = MouseP;
                DebugState->HotInteraction.Type = DebugInteraction_Move;
                DebugState->HotInteraction.P = &Hierarchy->UIP;
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
        case DebugInteraction_ToggleValue:
        {
            debug_variable *Var = DebugState->Interaction.Var;
            Assert(Var);
            switch(Var->Type)
            {
                case DebugVariableType_Bool32:
                {
                    Var->Bool32 = !Var->Bool32;
                } break;

                case DebugVariableType_Group:
                {
                    Var->Group.Expanded = !Var->Group.Expanded;
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
        debug_variable *Var = DebugState->Interaction.Var;
        debug_variable_hierarchy *Hierarchy = DebugState->Interaction.Hierarchy;
        v2 *P = DebugState->Interaction.P;

        // NOTE(georgy): Mouse move interaction
        switch(DebugState->Interaction.Type)
        {
	        case DebugInteraction_DragValue:
            {
                switch(Var->Type)
                {
                    case DebugVariableType_Real32:
                    {
                        Var->Real32 += 0.1f*dMouseP.y;
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

internal void
DEBUGEnd(game_input *Input, loaded_bitmap *DrawBuffer)
{
    TIMED_FUNCTION();

    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        render_group *RenderGroup = DebugState->RenderGroup;

        ZeroStruct(DebugState->NextHotInteraction);
        debug_record *HotRecord = 0;

        v2 MouseP = V2((real32)Input->MouseX, (real32)Input->MouseY);

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
            if(DebugState->FrameCount)
            {
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                            "Last frame time: %.02fms\n", 
                            DebugState->Frames[DebugState->FrameCount - 1].WallSecondsElapsed * 1000.0f);
                DEBUGTextLine(TextBuffer);
            }
        }   

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            if(HotRecord)
            {
                DebugState->ScopeToRecord = HotRecord;
            }
            else
            {
                DebugState->ScopeToRecord = 0;
            }
            RefreshCollation(DebugState);
        }

        TiledRenderGroupToOutput(DebugState->HighPriorityQueue, DebugState->RenderGroup, DrawBuffer);
        EndRender(DebugState->RenderGroup);
    }
}

#define DebugRecords_Main_Count __COUNTER__

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
        Result = PushStruct(&DebugState->CollateArena, debug_thread);
        Result->ID = ThreadID;
        Result->LaneIndex = DebugState->FrameBarLaneCount++;
        Result->FirstOpenBlock = 0;
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
    }

    return(Result);
}

internal debug_frame_region *
AddRegion(debug_state *DebugState, debug_frame *CurrentFrame)
{
    Assert(CurrentFrame->RegionCount < MAX_REGIONS_PER_FRAME);
    debug_frame_region *Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;

    return(Result);
}

internal void
RestartCollation(debug_state *DebugState, uint32 InvalidEventArrayIndex)
{
    EndTemporaryMemory(DebugState->CollateTemp);
    DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

    DebugState->FirstThread = 0;
    DebugState->FirstFreeBlock = 0;

    DebugState->Frames = PushArray(&DebugState->CollateArena, MAX_DEBUG_EVENT_ARRAY_COUNT*4, debug_frame);
    DebugState->FrameBarLaneCount = 0;
    DebugState->FrameCount = 0;
    DebugState->FrameBarScale = 1.0f / 86666666.0f;

    DebugState->CollationArrayIndex = InvalidEventArrayIndex + 1;
    DebugState->CollationFrame = 0;
}

inline debug_record *
GetRecordFrom(open_debug_block *Block)
{
    debug_record *Result = Block ? Block->Source : 0;

    return(Result);
}

internal void
CollateDebugRecords(debug_state *DebugState, uint32 InvalidEventArrayIndex)
{
    for(;
        ;
        DebugState->CollationArrayIndex++)
    {
        if(DebugState->CollationArrayIndex == MAX_DEBUG_EVENT_ARRAY_COUNT)
        {
            DebugState->CollationArrayIndex = 0;
        }

        uint32 EventArrayIndex = DebugState->CollationArrayIndex;
        if(EventArrayIndex == InvalidEventArrayIndex)
        {
            break;
        }

        for(uint32 EventIndex = 0;
            EventIndex < GlobalDebugTable->EventCount[EventArrayIndex];
            EventIndex++)
        {
            debug_event *Event = GlobalDebugTable->Events[EventArrayIndex] + EventIndex;
            debug_record *Source = GlobalDebugTable->Records[Event->TranslationUnit] + Event->DebugRecordIndex;

            if(Event->Type == DebugEvent_FrameMarker)
            {
                if(DebugState->CollationFrame)
                {
                    DebugState->CollationFrame->EndClock = Event->Clock;
                    DebugState->CollationFrame->WallSecondsElapsed = Event->SecondsElapsed;
                    DebugState->FrameCount++;
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
                }

                DebugState->CollationFrame = DebugState->Frames + DebugState->FrameCount;
                DebugState->CollationFrame->BeginClock = Event->Clock;
                DebugState->CollationFrame->EndClock = 0;
                DebugState->CollationFrame->RegionCount = 0;
                DebugState->CollationFrame->Regions = PushArray(&DebugState->CollateArena, MAX_REGIONS_PER_FRAME, debug_frame_region);
                DebugState->CollationFrame->WallSecondsElapsed = 0.0f;
            }
            else if(DebugState->CollationFrame)
            {
                uint32 FrameIndex = DebugState->FrameCount;
                debug_thread *Thread = GetDebugThread(DebugState, Event->TC.ThreadID);
                uint64 RelativeClock = Event->Clock - DebugState->CollationFrame->BeginClock;
                if(Event->Type == DebugEvent_BeginBlock)
                {
                    open_debug_block *DebugBlock = DebugState->FirstFreeBlock;
                    if(DebugBlock)
                    {
                        DebugState->FirstFreeBlock = DebugBlock->NextFree;    
                    }
                    else
                    {
                        DebugBlock = PushStruct(&DebugState->CollateArena, open_debug_block);
                    }
                    
                    DebugBlock->StartingFrameIndex = FrameIndex;
                    DebugBlock->Source = Source;
                    DebugBlock->OpeningEvent = Event;
                    DebugBlock->Parent = Thread->FirstOpenBlock;
                    Thread->FirstOpenBlock = DebugBlock;
                    DebugBlock->NextFree = 0;
                }
                else if(Event->Type == DebugEvent_EndBlock)
                {
                    if(Thread->FirstOpenBlock)
                    {
                        open_debug_block *MatchingBlock = Thread->FirstOpenBlock;
                        debug_event *OpeningEvent = MatchingBlock->OpeningEvent;
                        if((OpeningEvent->TC.ThreadID == Event->TC.ThreadID) &&
                           (OpeningEvent->DebugRecordIndex == Event->DebugRecordIndex) &&
                           (OpeningEvent->TranslationUnit == Event->TranslationUnit))
                        {
                            if(MatchingBlock->StartingFrameIndex == FrameIndex)
                            {
                                if(GetRecordFrom(MatchingBlock->Parent) == DebugState->ScopeToRecord)
                                {
                                    real32 MinT = (real32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 MaxT = (real32)(Event->Clock - DebugState->CollationFrame->BeginClock);
                                    real32 ThresholdT = 0.01f;
                                    if((MaxT - MinT) > ThresholdT)
                                    {
                                        debug_frame_region *Region = AddRegion(DebugState, DebugState->CollationFrame);
                                        Region->Record = Source;
                                        Region->CycleCount = Event->Clock - OpeningEvent->Clock;
                                        Region->LaneIndex = (uint16)Thread->LaneIndex;
                                        Region->ColorIndex = (uint16)OpeningEvent->DebugRecordIndex;
                                        Region->MinT = MinT;
                                        Region->MaxT = MaxT;
                                    }
                                }
                            }
                            else
                            {
                                // TODO(georgy): Record all frames in between and begin/end span!
                            }

                            Thread->FirstOpenBlock->NextFree = DebugState->FirstFreeBlock;
                            DebugState->FirstFreeBlock = Thread->FirstOpenBlock;
                            Thread->FirstOpenBlock = MatchingBlock->Parent;
                        }
                        else
                        {
                            // TODO(georgy): Record span that goes to the beginning of the frame series?
                        }
                    }
                }
                else
                {
                    Assert(!"Invalid event type");
                }
            }
        }
    }
}

internal void
RefreshCollation(debug_state *DebugState)
{
    RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
    CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGFrameEnd)
{
    GlobalDebugTable->RecordCount[0] = DebugRecords_Main_Count;

    GlobalDebugTable->CurrentEventArrayIndex++;
    if(GlobalDebugTable->CurrentEventArrayIndex >= ArrayCount(GlobalDebugTable->Events))
    {
        GlobalDebugTable->CurrentEventArrayIndex = 0;
    }
    uint64 ArrayIndex_EventIndex = AtomicExchangeUInt64(&GlobalDebugTable->EventArrayIndex_EventIndex, 
                                                        (uint64)GlobalDebugTable->CurrentEventArrayIndex << 32);


    uint32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    uint32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;
    GlobalDebugTable->EventCount[EventArrayIndex] = EventCount;

    debug_state *DebugState = DEBUGGetState(Memory);
    if(DebugState)
    {
        if(Memory->ExecutableReloaded)
        {
            RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
        }

        if(!DebugState->Paused)
        {
            if(DebugState->FrameCount >= 4*MAX_DEBUG_EVENT_ARRAY_COUNT)
            {
                RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
            }
            CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
        }
    }

    return(GlobalDebugTable);
}