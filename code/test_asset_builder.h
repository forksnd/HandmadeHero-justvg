#if !defined(TEST_ASSET_BUILDER_H)
#define TEST_ASSET_BUILDER_H

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "handmade_platform.h"
#include "handmade_file_formats.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

enum asset_type
{
    AssetType_Sound,
    AssetType_Bitmap,
    AssetType_Font,
    AssetType_FontGlyph,
};

struct loaded_font;
struct asset_source_font
{
    loaded_font *Font;
};

struct asset_source_font_glyph
{
    loaded_font *Font;
    uint32 CodePoint;
};

struct asset_source_sound
{
    char *Filename;
    uint32 FirstSampleIndex;
};

struct asset_source_bitmap
{
    char *Filename;
};

struct asset_source
{
    asset_type Type;
    union 
    {
        asset_source_bitmap Bitmap;
        asset_source_font Font;
        asset_source_font_glyph Glyph;
        asset_source_sound Sound;
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
    asset_source AssetsSource[VERY_LARGE_NUMBER];
    hha_asset Assets[VERY_LARGE_NUMBER];

    hha_asset_type *DEBUGAssetType; 
    uint32 AssetIndex;
};

#endif