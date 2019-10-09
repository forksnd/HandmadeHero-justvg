#if !defined(HANDMADE_FILE_FORMATS_H)
#define HANDMADE_FILE_FORMATS_H

enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // NOTE(georgy): Angle in radians off of dued right
	Tag_UnicodeCodepoint,

    Tag_Count
};

enum asset_type_id
{
    Asset_None,

    // 
    // NOTE(georgy): Bitmaps!
    // 

    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,

    Asset_Stone,
    Asset_Grass,

    Asset_Head,
    Asset_Torso,
    Asset_Legs,

	Asset_Font,
    Asset_FontGlyph,

    // 
    // NOTE(georgy): Sounds!
    //

    Asset_Music,
    Asset_Hit, 
    Asset_Jump,
    Asset_Pickup,

    // 
    // 
    // 

    Asset_Count
};

#define HHA_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))

#pragma pack(push, 1)
struct bitmap_id
{
    uint32 Value;
};
struct sound_id
{
    uint32 Value;
};
struct font_id
{
    uint32 Value;
};

struct hha_header
{
#define HHA_MAGIC_VALUE HHA_CODE('h', 'h', 'a', 'f')
	uint32 MagicValue;

#define HHA_VERSION 0
	uint32 Version;

	uint32 TagCount;
	uint32 AssetTypeCount;
	uint32 AssetCount;

	uint64 Tags; // hha_tag[TagCount]
	uint64 AssetTypes; // hha_asset_type[AssetTypeCount]
	uint64 Assets; // hha_asset[AssetCount]
};

struct hha_tag
{
    uint32 ID; 
    real32 Value;
};

struct hha_asset_type
{
	uint32 TypeID;
	uint32 FirstAssetIndex;
	uint32 OnePastLastAssetIndex;
};

enum hha_sound_chain
{
	HHASoundChain_None,
	HHASoundChain_Loop,
	HHASoundChain_Advance,
};
struct hha_bitmap
{
	uint32 Dim[2];
    real32 AlignPercentage[2];
    /* NOTE(georgy): Data is:

        uint32 Pixels[Dim[1]]Dim[[0]]
    */  
};
struct hha_sound
{
    uint32 SampleCount;
	uint32 ChannelCount;
    uint32 Chain; // NOTE(georgy): hha_sound_chain
    /* NOTE(georgy): Data is:

        int16 Channels[ChannelCount][SampleCount]
    */    
};
struct hha_font
{
    uint32 CodePointCount;
    real32 AscenderHeight;
    real32 DescenderHeight;
    real32 ExternalLeading;
    /* NOTE(georgy): Data is:
      
        bitmap_id CodePoints[CodePointCount]
        real32 HorizontalAdvance[CodePointCount*CodePointCount]
    */
};
struct hha_asset
{
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
	union
	{
		hha_bitmap Bitmap;
		hha_sound Sound;	
        hha_font Font;
	};
};

#pragma pack(pop)

#endif