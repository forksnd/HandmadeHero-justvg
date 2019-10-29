#include "handmade_debug.h"

// TODO(georgy): Stop using stdio!
#include <stdio.h>

// TODO(georgy): Fix this for looped live code editing
global_variable real32 LeftEdge;
global_variable real32 AtY;
global_variable real32 FontScale;
global_variable font_id FontID;
#if 0
internal void
UpdateDebugRecords(debug_state *DebugState, uint32 CounterCount)
{
    for(uint32 CounterIndex = 0;
        CounterIndex < CounterCount;
        ++CounterIndex)
    {
        debug_record *Source = GlobalDebugTable.Records + CounterIndex;
        debug_counter_state *Dest = DebugState->CounterStates + DebugState->CounterCount++;

        uint64 HitCount_CycleCount = AtomicExchangeUInt64(&Source->HitCount_CycleCount, 0);
        Dest->FileName = Source->FileName;
        Dest->FunctionName = Source->FunctionName;
        Dest->LineNumber = Source->LineNumber;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (uint32)(HitCount_CycleCount >> 32);
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (uint32)(HitCount_CycleCount & 0xFFFFFFFF); 
    }
}
#endif
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

                if(Counter->FunctionName)
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
                                Counter->FunctionName,
                                Counter->LineNumber,
                                (uint32)CycleCount.Avg,
                                (uint32)HitCount.Avg, 
                                (uint32)CycleOverHit.Avg);
                    DEBUGTextLine(TextBuffer);
#endif
                }
            }

            real32 BarWidth = 8.0f;
            real32 BarSpacing = 10.0f;
            real32 ChartLeft = LeftEdge + 10.0f;
            real32 ChartHeight = 150.0f;
            real32 ChartWidth = BarSpacing*(real32)DEBUG_SNAPSHOT_COUNT;
            real32 ChartMinY = AtY - (ChartHeight + 30.0f);
            real32 Scale = 1.0f / 0.03333f;

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

            for(uint32 SnapshotIndex = 0;
                SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
                SnapshotIndex++)
            {
                debug_frame_end_info *Info = DebugState->FrameEndInfos + SnapshotIndex;
                real32 StackY = ChartMinY;
                real32 PrevTimestampSeconds = 0.0f;
                for(uint32 TimestampIndex = 0;
                    TimestampIndex < Info->TimestampCount;
                    TimestampIndex++)
                {
                    debug_frame_timestamp *Timestamp = Info->Timestamps + TimestampIndex;
                    real32 ThisSecondsElapsed = Timestamp->Seconds - PrevTimestampSeconds;
                    PrevTimestampSeconds = Timestamp->Seconds;

                    v3 Color = Colors[TimestampIndex];
                    real32 ThisProportion = Scale*ThisSecondsElapsed;
                    real32 ThisHeight = ChartHeight*ThisProportion;
                    PushRect(RenderGroup, V3(ChartLeft + BarSpacing*(real32)SnapshotIndex + 0.5f*BarWidth, StackY + 0.5f*ThisHeight, 0.0f), 
                             V2(BarWidth, ThisHeight), V4(Color, 1.0f));
                    StackY += ThisHeight;
                }

                PushRect(RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0.0f), V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
            }
        }
    }
    // DEBUGTextLine("\\5C0F\\8033\\6728\\514E");
}

#define DebugRecords_Main_Count __COUNTER__

debug_table GlobalDebugTable;

internal void
CollateDebugRecords(debug_state *DebugState, uint32 EventCount, debug_event *Events)
{
#define DebugRecords_Platform_Count 0
    DebugState->CounterCount = DebugRecords_Main_Count + DebugRecords_Platform_Count;
    for(uint32 CounterIndex = 0;
        CounterIndex < DebugState->CounterCount;
        ++CounterIndex)
    {
        debug_counter_state *Dest = DebugState->CounterStates + CounterIndex;
        Dest->Snapshots[DebugState->SnapshotIndex].HitCount = 0;
        Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = 0; 
    }

    debug_counter_state *CounterArray[2] = 
    {
        DebugState->CounterStates,
        DebugState->CounterStates + DebugRecords_Main_Count
    };
    for(uint32 EventIndex = 0;
        EventIndex < EventCount;
        ++EventIndex)
    {
        debug_event *Event = Events + EventIndex;
        debug_counter_state *Dest = CounterArray[Event->TranslationUnit] + Event->DebugRecordIndex;
        debug_record *Source = GlobalDebugTable.Records[Event->TranslationUnit] + Event->DebugRecordIndex;
        Dest->FileName = Source->FileName;
        Dest->FunctionName = Source->FunctionName;
        Dest->LineNumber = Source->LineNumber;

        if(Event->Type == DebugEvent_BeginBlock)
        {
            Dest->Snapshots[DebugState->SnapshotIndex].HitCount++;
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount -= Event->Clock;
        }
        else
        {
            Assert(Event->Type == DebugEvent_EndBlock);
            Dest->Snapshots[DebugState->SnapshotIndex].CycleCount += Event->Clock;
        }
    }
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{
    GlobalDebugTable.CurrentEventArrayIndex = !GlobalDebugTable.CurrentEventArrayIndex;
    uint64 ArrayIndex_EventIndex = AtomicExchangeUInt64(&GlobalDebugTable.EventArrayIndex_EventIndex, 
                                                        (uint64)GlobalDebugTable.CurrentEventArrayIndex << 32);

    uint32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    uint32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if(DebugState)
    {
        DebugState->CounterCount = 0;
        CollateDebugRecords(DebugState, EventCount, GlobalDebugTable.Events[EventArrayIndex]);

        DebugState->FrameEndInfos[DebugState->SnapshotIndex] = *Info;

        DebugState->SnapshotIndex++;
        if(DebugState->SnapshotIndex >= DEBUG_SNAPSHOT_COUNT)
        {
            DebugState->SnapshotIndex = 0;
        }
    }
}