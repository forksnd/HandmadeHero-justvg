#if !defined(HANDMADE_ASSET_TYPE_ID_H)
#define HANDMADE_ASSET_TYPE_ID_H

enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection, // NOTE(georgy): Angle in raians off of due right

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

#endif