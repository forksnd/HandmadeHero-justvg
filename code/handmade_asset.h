#if !defined(HANDMADE_ASSET_H)
#define HANDMADE_ASSET_H

struct loaded_sound
{
    // TODO(georgy): This could be shrunk to 12 bytes if the loaded_bitmap
    // ever got down that small
    int16 *Samples[2];
    uint32 SampleCount;
    uint32 ChannelCount;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
    AssetState_StateMask = 0xFF,

    AssetState_Sound = 0x1000,
    AssetState_Bitmap = 0x2000,
    AssetState_TypeMask = 0xF000,
};
struct asset_slot
{
    uint32 State;
    union
    {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
    };
};

struct asset
{
    hha_asset HHA;
    uint32 FileIndex;
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

struct asset_memory_header
{
    asset_memory_header *Next;
    asset_memory_header *Prev;
    uint32 SlotIndex;
    uint32 Reserved;
};

struct asset_file
{
    platform_file_handle *Handle;

    hha_header Header;
    hha_asset_type *AssetTypeArray;

    uint32 TagBase;
};

struct game_assets
{
    struct transient_state *TranState;
    memory_arena Arena; 

    uint64 TargetMemoryUsed;
    uint64 TotalMemoryUsed;
    asset_memory_header LoadedAssetSentinel;

    real32 TagRange[Tag_Count];

    uint32 FileCount;
    asset_file *Files;

    uint32 TagCount;
    hha_tag *Tags;

    uint32 AssetCount;
    asset *Assets;
    asset_slot *Slots;

    asset_type AssetTypes[Asset_Count]; 

#if 0
    uint8 *HHAContents;

    // NOTE(georgy): Structured assets
    // hero_bitmaps HeroBitmaps[4];   

    // TODO(georgy): These should go away once we actually load an asset pack file
    uint32 DEBUGUsedAssetCount;   
    uint32 DEBUGUsedTagCount;   
    asset_type *DEBUGAssetType; 
    asset *DEBUGAsset;
#endif
};

inline uint32
GetType(asset_slot *Slot)
{
    uint32 Result = Slot->State & AssetState_TypeMask;
    return(Result);
}

inline uint32
GetState(asset_slot *Slot)
{
    uint32 Result = Slot->State & AssetState_StateMask;
    return(Result);
}

inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    asset_slot *Slot = Assets->Slots + ID.Value;

    loaded_bitmap *Result = 0;
    if(GetState(Slot) >= AssetState_Loaded)
    {
        CompletePreviousReadsBeforeFutureReads;
        Result = &Slot->Bitmap;
    }

    return(Result);
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    asset_slot *Slot = Assets->Slots + ID.Value;

    loaded_sound *Result = 0;
    if(GetState(Slot) >= AssetState_Loaded)
    {
        CompletePreviousReadsBeforeFutureReads;
        Result = &Slot->Sound;
    }

    return(Result);
}

inline hha_sound *
GetSoundInfo(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound *Result = &Assets->Assets[ID.Value].HHA.Sound;

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

internal void LoadBitmap(game_assets *Assets, bitmap_id ID);
inline void PrefetchBitmap(game_assets *Assets, bitmap_id ID) {LoadBitmap(Assets, ID);}
internal void LoadSound(game_assets *Assets, sound_id ID);
inline void PrefetchSound(game_assets *Assets, sound_id ID) {LoadSound(Assets, ID);}

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

#endif