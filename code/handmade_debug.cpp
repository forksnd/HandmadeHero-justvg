#include "handmade_debug.h"

// TODO(georgy): Stop using stdio!
#include <stdio.h>

// TODO(georgy): Fix this for looped live code editing
global_variable real32 LeftEdge;
global_variable real32 AtY;
global_variable real32 FontScale;
global_variable font_id FontID;

internal void 
DEBUGReset(game_assets *Assets, uint32 Width, uint32 Height)
{
    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_FontType] = (real32)FontType_Debug;
    WeightVector.E[Tag_FontType] = (real32)1.0f;
    FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

    FontScale = 0.5f;
    Orthographic(DEBUGRenderGroup, Width, Height, 1.0f);
    LeftEdge = -0.5f*Width;

    hha_font *Info = GetFontInfo(Assets, FontID);
    AtY = 0.5f*Height - FontScale*GetStartingBaselineY(Info);
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

internal void
DEBUGTextLine(char *String)
{
    if(DEBUGRenderGroup)
    {
        render_group *RenderGroup = DEBUGRenderGroup;

        loaded_font *Font = PushFont(RenderGroup, FontID);
        if(Font)
        {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);
            uint32 PrevCodePoint = 0;
            real32 AtX = LeftEdge;
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

                real32 AdvanceX = FontScale*GetHorizontalAdvanceForPair(Info, Font, PrevCodePoint, CodePoint);
                AtX += AdvanceX;

                if(*At != ' ')
                {
                    bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, CodePoint);
                    hha_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                    PushBitmap(RenderGroup, BitmapID, FontScale*Info->Dim[1], V3(AtX, AtY, 0), V4(1, 1, 1, 1));
                }

                PrevCodePoint = CodePoint;
            }

            AtY -= GetLineAdvanceFor(Info)*FontScale;    
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

internal void
DEBUGOverlay(game_memory *Memory)
{
    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if(DebugState && DEBUGRenderGroup)
    {
        render_group *RenderGroup = DEBUGRenderGroup;

        // TODO(georgy): Layout / cached font info / etc. for real debug display
        loaded_font *Font = PushFont(RenderGroup, FontID);
        if(Font)
        {
            hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);

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

            real32 LaneWidth = 0.0f;
            uint32 LaneCount = DebugState->FrameBarLaneCount;
            real32 BarWidth = LaneWidth*LaneCount;
            real32 BarSpacing = BarWidth + 2.0f;
            real32 ChartLeft = LeftEdge + 10.0f;
            real32 ChartHeight = 150.0f;
            real32 ChartWidth = BarSpacing*(real32)DebugState->FrameCount;
            real32 ChartMinY = AtY - (ChartHeight + 30.0f);
            real32 Scale = DebugState->FrameBarScale;

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
                FrameIndex < DebugState->FrameCount;
                FrameIndex++)
            {
                debug_frame *Frame = DebugState->Frames + FrameIndex;
                real32 StackX = ChartLeft + BarSpacing*(real32)FrameIndex;
                real32 StackY = ChartMinY;
                for(uint32 RegionIndex = 0;
                    RegionIndex < Frame->RegionCount;
                    RegionIndex++)
                {
                    debug_frame_region *Region = Frame->Regions + RegionIndex;

                    v3 Color = Colors[RegionIndex % ArrayCount(Colors)];
                    real32 ThisMinY = StackY + Scale*Region->MinT;
                    real32 ThisMaxY = StackY + Scale*Region->MaxT;
                    PushRect(RenderGroup, V3(StackX + 0.5f*LaneWidth + LaneWidth*Region->LaneIndex, 0.5f*(ThisMinY + ThisMaxY), 0.0f), 
                             V2(LaneWidth, ThisMaxY - ThisMinY), V4(Color, 1.0f));
                }

                PushRect(RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0.0f), V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
            }
#endif
        }
    }
    // DEBUGTextLine("\\5C0F\\8033\\6728\\514E");
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

internal void
CollateDebugRecords(debug_state *DebugState, uint32 InvalidEventArrayIndex)
{
    DebugState->FrameBarLaneCount = 0;
    DebugState->FrameCount = 0;
    DebugState->FrameBarScale = 0.0f;

    debug_frame *CurrentFrame = 0;

    for(uint32 EventArrayIndex = InvalidEventArrayIndex + 1;
        ;
        EventArrayIndex++)
    {
        if(EventArrayIndex == MAX_DEBUG_FRAME_COUNT)
        {
            EventArrayIndex = 0;
        }

        if(EventArrayIndex == InvalidEventArrayIndex)
        {
            break;
        }

        for(uint32 EventIndex = 0;
            EventIndex < MAX_DEBUG_EVENT_COUNT;
            EventIndex++)
        {
            debug_event *Event = GlobalDebugTable->Events[EventArrayIndex] + EventIndex;
            debug_record *Source = GlobalDebugTable->Records[Event->TranslationUnit] + Event->DebugRecordIndex;

            if(Event->Type == DebugEvent_FrameMarker)
            {
                if(CurrentFrame)
                {
                    CurrentFrame->EndClock = Event->Clock;
                }

                CurrentFrame = DebugState->Frames + DebugState->FrameCount++;
                CurrentFrame->BeginClock = Event->Clock;
                CurrentFrame->EndClock = 0;
                CurrentFrame->RegionCount = 0;
            }
            else if(CurrentFrame)
            {
                uint64 RelativeClock = Event->Clock - CurrentFrame->BeginClock;
                uint32 LaneIndex = GetLaneFromThreadIndex(DebugState, Event->ThreadIndex);
                if(Event->Type == DebugEvent_BeginBlock)
                {

                }
                if(Event->Type == DebugEvent_EndBlock)
                {
                    
                }
                else
                {
                    Assert(!"Invalid event type");
                }
            }
        }
    }

#if 0
    debug_counter_state *CounterArray[MAX_DEBUG_TRANSLATION_UNITS];
    debug_counter_state *CurrentCounter = DebugState->CounterStates;
    uint32 TotalRecordCount = 0;
    for(uint32 UnitIndex = 0;
        UnitIndex < MAX_DEBUG_TRANSLATION_UNITS;
        UnitIndex++)
    {
        CounterArray[UnitIndex] = CurrentCounter;
        TotalRecordCount += GlobalDebugTable->RecordCount[UnitIndex];

        CurrentCounter += GlobalDebugTable->RecordCount[UnitIndex];
    }
    DebugState->CounterCount = TotalRecordCount;

    for(uint32 CounterIndex = 0;
        CounterIndex < DebugState->CounterCount;
        ++CounterIndex)
    {
        debug_counter_state *Dest = DebugState->CounterStates + CounterIndex;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = 0;
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = 0; 
    }

    for(uint32 EventIndex = 0;
        EventIndex < EventCount;
        ++EventIndex)
    {
        debug_event *Event = Events + EventIndex;
        debug_counter_state *Dest = CounterArray[Event->TranslationUnit] + Event->DebugRecordIndex;
        debug_record *Source = GlobalDebugTable->Records[Event->TranslationUnit] + Event->DebugRecordIndex;
        Dest->FileName = Source->FileName;
        Dest->BlockName = Source->BlockName;
        Dest->LineNumber = Source->LineNumber;

        if(Event->Type == DebugEvent_BeginBlock)
        {
            Dest->Snapshots[DebugState->SnapshotIndex].HitCount++;
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount -= Event->Clock;
        }
        else if(Event->Type == DebugEvent_EndBlock)
        {
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount += Event->Clock;
        }
    }
#endif
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

    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if(DebugState)
    {
        if(!DebugState->Initialized)
        {
            InitializeArena(&DebugState->CollateArena, Memory->DebugStorageSize - sizeof(debug_state), DebugState + 1);
            DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);
        }

        EndTemporaryMemory(DebugState->CollateTemp);
        DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);

        CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
    }

    return(GlobalDebugTable);
}