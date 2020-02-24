#if !defined(HANDMADE_ASSET_H)
#define HANDMADE_ASSET_H

struct loaded_sound
{
    int16 *Samples[2];
    uint32 SampleCount;
    uint32 ChannelCount;
};

struct loaded_font
{
    hha_font_glyph *Glyphs;
    real32 *HorizontalAdvance;
    uint32 BitmapIDOffset;
    uint16 *UnicodeMap;
};

// TODO(georgy): Streaming this, by using header pointer as an indicator of unloaded status?
enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
};

struct asset_memory_header
{
    asset_memory_header *Next;
    asset_memory_header *Prev;

    uint32 AssetIndex;
    uint32 TotalSize;
    uint32 GenerationID;
    union
    {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
        loaded_font Font;
    };
};

struct asset
{
    uint32 State;
    asset_memory_header *Header;

    hha_asset HHA;
    int32 FileIndex;
};

struct asset_vector
{
    real32 E[Tag_Count];
};

struct asset_type
{
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct asset_file
{
    platform_file_handle Handle;

    hha_header Header;
    hha_asset_type *AssetTypeArray;

    uint32 TagBase;
    uint32 FontBitmapIDOffset;
};

enum asset_memory_block_flags
{
    AssetMemory_Used = 0x1,
};
struct asset_memory_block
{
    asset_memory_block *Prev;
    asset_memory_block *Next;
    uint64 Flags;
    memory_index Size;
};

struct game_assets
{
    uint32 NextGenerationID;

    struct transient_state *TranState;

    asset_memory_block MemorySentinel;

    asset_memory_header LoadedAssetSentinel;

    real32 TagRange[Tag_Count];

    uint32 FileCount;
    asset_file *Files;

    uint32 TagCount;
    hha_tag *Tags;

    uint32 AssetCount;
    asset *Assets;

    asset_type AssetTypes[Asset_Count];



    uint32 OperationLock;

    uint32 InFlightGenerationCount;
    uint32 InFlightGenerations[16];
};

inline void
BeginAssetLock(game_assets *Assets)
{
    for(;;)
    {
        if(AtomicCompareExchangeUInt32(&Assets->OperationLock, 1, 0) == 0)
        {
            break;
        }
    }
}

inline void
EndAssetLock(game_assets *Assets)
{
    CompletePreviousWritesBeforeFutureWrites;
    Assets->OperationLock = 0;
}

inline void
InsertAssetHeaderAtFront(game_assets *Assets, asset_memory_header *Header)
{
    asset_memory_header *Sentinel = &Assets->LoadedAssetSentinel;
    
    Header->Prev = Sentinel;
    Header->Next = Sentinel->Next;

    Header->Prev->Next = Header;
    Header->Next->Prev = Header;
}

inline void
RemoveAssetsHeaderFromList(asset_memory_header *Header)
{
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;

    Header->Next = Header->Prev = 0;
}

inline asset_memory_header *
GetAsset(game_assets *Assets, uint32 ID, uint32 GenerationID)
{
    Assert(ID <= Assets->AssetCount);
    asset *Asset = Assets->Assets + ID;

    asset_memory_header *Result = 0;
           
    BeginAssetLock(Assets);

    if(Asset->State == AssetState_Loaded)
    {
        Result = Asset->Header;
        RemoveAssetsHeaderFromList(Result);
        InsertAssetHeaderAtFront(Assets, Result);

        if(Asset->Header->GenerationID < GenerationID)
        {
            Asset->Header->GenerationID = GenerationID;
        }

        CompletePreviousWritesBeforeFutureWrites;
    }

    EndAssetLock(Assets);
    
    return(Result);
}

inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID, uint32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_bitmap *Result = Header ? &Header->Bitmap : 0;
    
    return(Result);
}

inline hha_bitmap *
GetBitmapInfo(game_assets *Assets, bitmap_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    hha_bitmap *Result = &Assets->Assets[ID.Value].HHA.Bitmap;

    return(Result);
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID, uint32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_sound *Result = Header ? &Header->Sound : 0;
    
    return(Result);
}

inline hha_sound *
GetSoundInfo(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound *Result = &Assets->Assets[ID.Value].HHA.Sound;

    return(Result);
}

inline loaded_font *
GetFont(game_assets *Assets, font_id ID, uint32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_font *Result = Header ? &Header->Font : 0;

    return(Result);
}

inline hha_font *
GetFontInfo(game_assets *Assets, font_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    hha_font *Result = &Assets->Assets[ID.Value].HHA.Font;

    return(Result);
}

inline bool32
IsValid(bitmap_id ID)
{
    bool32 Result = (ID.Value != 0);

    return(Result);
}

inline bool32
IsValid(sound_id ID)
{
    bool32 Result = (ID.Value != 0);

    return(Result);
}   

internal void LoadBitmap(game_assets *Assets, bitmap_id ID, bool32 Immediate);
inline void PrefetchBitmap(game_assets *Assets, bitmap_id ID) {LoadBitmap(Assets, ID, false);}
internal void LoadSound(game_assets *Assets, sound_id ID);
inline void PrefetchSound(game_assets *Assets, sound_id ID) {LoadSound(Assets, ID);}
internal void LoadFont(game_assets *Assets, font_id ID, bool32 Immediate);
inline void PrefetchFont(game_assets *Assets, font_id ID) {LoadFont(Assets, ID, false);}

inline sound_id 
GetNextSoundInChain(game_assets *Assets, sound_id ID)
{
    sound_id Result = {};

    hha_sound *Info = GetSoundInfo(Assets, ID);
    switch(Info->Chain)
    {
        case HHASoundChain_None:
        {
            // NOTE(georgy): Nothing to do.
        } break;

        case HHASoundChain_Loop:
        {
            Result = ID;
        } break;

        case HHASoundChain_Advance:
        {
            Result.Value = ID.Value + 1;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    return(Result);
}

inline u32 
BeginGeneration(game_assets *Assets)
{
    BeginAssetLock(Assets);

    Assert(Assets->InFlightGenerationCount < ArrayCount(Assets->InFlightGenerations));
    u32 Result = Assets->NextGenerationID++;
    Assets->InFlightGenerations[Assets->InFlightGenerationCount++] = Result;

    EndAssetLock(Assets);

    return(Result);
}

inline void 
EndGeneration(game_assets *Assets, u32 GenerationID)
{
    BeginAssetLock(Assets);

    for(u32 Index = 0;
        Index < Assets->InFlightGenerationCount;
        Index++)
    {
        if(Assets->InFlightGenerations[Index] == GenerationID)
        {
            Assets->InFlightGenerations[Index] = 
                Assets->InFlightGenerations[--Assets->InFlightGenerationCount];
            break;
        }
    }

    EndAssetLock(Assets);
}

#endif