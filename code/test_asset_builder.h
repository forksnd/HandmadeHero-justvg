#if !defined(TEST_ASSET_BUILDER_H)
#define TEST_ASSET_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"
#include "handmade_file_formats.h"

struct bitmap_id
{
    uint32 Value;
};
struct sound_id
{
    uint32 Value;
};

struct asset_bitmap_info
{
    char *Filename;
    real32 AlignPercentage[2];
};

struct asset_sound_info
{
    char *Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    sound_id NextIDToPlay;
};

struct asset
{
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
    union
    {
        asset_bitmap_info Bitmap;
        asset_sound_info Sound;
    };
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
    uint32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

	uint32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count]; 

    uint32 AssetCount;
    asset Assets[VERY_LARGE_NUMBER];

    hha_asset_type *DEBUGAssetType; 
    asset *DEBUGAsset;
};

#endif