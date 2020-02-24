#if !defined(HANDMADE_RENDER_GROUP_H)
#define HANDMADE_RENDER_GROUP_H

/* NOTE(george):
    
   1) Everywhere outside the renderer, Y _always_ goes upward, X to the right.

   2) All bitmaps including the render target are assumed to be bottom-up
      (meaning that the first row pointer points to the bottom-most row 
      when viewed on screen).

   3) Unless otherwise specified, all inputs to the renderer are in world
      coordinate ("meters"), NOT pixels. Anything that in pixel values 
      will be explicitly marked as such.

   4) Z is a special coordinate because it is broken up into discrete slices,
      and the renderer actually understands these slices. Z slices are 
      what control the _scaling_ of things, whereas Z offsets inside a slide are
      what control Y offsetting. 

   5) All color values specified to the renderer as V4's are in NON-premultiplied 
      alpha.

*/

struct loaded_bitmap
{
    void *Memory;

    v2 AlignPercentage;
    real32 WidthOverHeight;
    int32 Width;
    int32 Height;
    int32 Pitch;
    u32 Handle;
};

struct environment_map
{
    loaded_bitmap LOD[4];
    real32 Pz;
};

struct render_basis
{
    v3 P;
};

// NOTE(george): render_group_entry is a "compact discriminated union"
enum render_group_entry_type
{
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_saturation,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_coordinate_system,
};

struct render_group_entry_header
{
    render_group_entry_type Type;
};

struct render_entry_clear
{
    v4 Color;
};

struct render_entry_saturation
{
    real32 Level;
};

struct render_entry_coordinate_system
{
    v2 Origin;
    v2 XAxis;
    v2 YAxis;
    v4 Color;
    loaded_bitmap *Texture;
    loaded_bitmap *NormalMap;

    // real32 PixelsToMeters; // NOTE(georgy): Need to store this for lighting

    environment_map *Top;
    environment_map *Middle;
    environment_map *Bottom;
};

struct render_entry_bitmap
{
    loaded_bitmap *Bitmap;

    v2 P;
    v2 Size;
    v4 Color;
};

struct render_entry_rectangle
{
    v2 P;
    v2 Dim;
    v4 Color;    
};

struct object_transform
{
    b32 Upright;
    v3 OffsetP;
    r32 Scale;
};

struct camera_transform
{
    b32 Orthographic;

    // NOTE(georgy): Camera parameters
    r32 MetersToPixels; // NOTE(george): This translates meters _on the monitor_ into pixels _on the monitor_
    v2 ScreenCenter;

    r32 FocalLength;
    r32 DistanceAboveTarget;
};

struct render_group
{
    struct game_assets *Assets;
    r32 GlobalAlpha;

    v2 MonitorHalfDimInMeters;
    camera_transform CameraTransform;

    u32 MissingResourceCount;
    b32 RendersInBackground;

    u32 GenerationID;
    game_render_commands *Commands;
};

struct entity_basis_p_result
{
    v2 P;
    r32 Scale;
    b32 Valid;
    r32 SortKey;
};

struct used_bitmap_dim
{
    entity_basis_p_result Basis;
    v2 Size;
    v2 Align;
    v3 P;
};

struct tile_sort_entry
{
    r32 SortKey;
    u32 PushBufferOffset;
};

inline object_transform
DefaultUprightTransform(void)
{
    object_transform Result = {};

    Result.Upright = true;
    Result.Scale = 1.0f;

    return(Result);
}

inline object_transform
DefaultFlatTransform(void)
{
    object_transform Result = {};

    Result.Scale = 1.0f;

    return(Result);
}

#endif