#if !defined(HANDMADE_ASSET_H)
#define HANDMADE_ASSET_H

struct loaded_sound
{
    uint32 SampleCount;
    uint32 ChannelCount;
    int16 *Samples[2];
};

struct asset_bitmap_info
{
    char *Filename;
    v2 AlignPercentage;
};

struct asset_sound_info
{
    char *Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};
struct asset_slot
{
    asset_state State;
    union
    {
        loaded_bitmap *Bitmap;
        loaded_sound *Sound;
    };
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
    // platform_file_handle Handle;

    hha_header Header;
    hha_asset_type *AssetTypeArray;

    uint32 TagBase;
};

struct game_assets
{
    struct transient_state *TranState;
    memory_arena Arena; 

    real32 TagRange[Tag_Count];

    uint32 FileCount;
    asset_file *Files;

    uint32 TagCount;
    hha_tag *Tags;

    uint32 AssetCount;
    hha_asset *Assets;
    asset_slot *Slots;

    asset_type AssetTypes[Asset_Count]; 

    uint8 *HHAContents;

    // NOTE(georgy): Structured assets
    // hero_bitmaps HeroBitmaps[4];   

#if 0
    // TODO(georgy): These should go away once we actually load an asset pack file
    uint32 DEBUGUsedAssetCount;   
    uint32 DEBUGUsedTagCount;   
    asset_type *DEBUGAssetType; 
    asset *DEBUGAsset;
#endif
};

inline loaded_bitmap *
GetBitmap(game_assets *Assets, bitmap_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    loaded_bitmap *Result = Assets->Slots[ID.Value].Bitmap;

    return(Result);
}

inline loaded_sound *
GetSound(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    loaded_sound *Result = Assets->Slots[ID.Value].Sound;

    return(Result);
}

inline hha_sound *
GetSoundInfo(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    hha_sound *Result = &Assets->Assets[ID.Value].Sound;

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

#endif