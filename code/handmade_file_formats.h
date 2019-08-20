#if !defined(HANDMADE_FILE_FORMATS_H)
#define HANDMADE_FILE_FORMATS_H

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
};
struct hha_sound
{
    uint32 SampleCount;
	uint32 ChannelCount;
    uint32 Chain; // NOTE(georgy): hha_sound_chain
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
	};
};

#pragma pack(pop)

#endif