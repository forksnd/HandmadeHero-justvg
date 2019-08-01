#if !defined(HANDMADE_ASSET_H)
#define HANDMADE_ASSET_H

struct bitmap_id
{
    uint32 Value;
};
struct sound_id
{
    uint32 Value;
};

struct loaded_sound
{
    uint32 SampleCount;
    uint32 ChannelCount;
    int16 *Samples[2];
};

enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // NOTE(georgy): Angle in raians off of due right

    Tag_Count
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

struct asset_tag
{
    uint32 ID; // NOTE(georgy): Tag ID
    real32 Value;
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

struct asset
{
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;

    union
    {
        asset_bitmap_info Bitmap;
        asset_sound_info Sound;
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

struct game_assets
{
    struct transient_state *TranState;
    memory_arena Arena; 

    real32 TagRange[Tag_Count];

    uint32 TagCount;
    asset_tag *Tags;

    uint32 AssetCount;
    asset *Assets;
    asset_slot *Slots;

    asset_type AssetTypes[Asset_Count]; 

    // NOTE(georgy): Structured assets
    // hero_bitmaps HeroBitmaps[4];   

    // TODO(georgy): These should go away once we actually load an asset pack file
    uint32 DEBUGUsedBitmapCount;
    uint32 DEBUGUsedSoundCount;
    uint32 DEBUGUsedAssetCount;   
    uint32 DEBUGUsedTagCount;   
    asset_type *DEBUGAssetType; 
    asset *DEBUGAsset;
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

inline asset_sound_info *
GetSoundInfo(game_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    asset_sound_info *Result = &Assets->Assets[ID.Value].Sound;

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