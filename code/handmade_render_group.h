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

struct render_transform
{
    bool32 Orthographic;

    // NOTE(georgy): Camera parameters
    real32 MetersToPixels; // NOTE(george): This translates meters _on the monitor_ into pixels _on the monitor_
    v2 ScreenCenter;

    // NOTE(george): Camera parameters
    real32 FocalLength;
    real32 DistanceAboveTarget;

    v3 OffsetP;
    real32 Scale;
};

struct render_group
{
    struct game_assets *Assets;

    real32 GlobalAlpha;

    v2 MonitorHalfDimInMeters;
    render_transform Transform;

    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 *PushBufferBase;

    uint32 MissingResourceCount;

    bool32 AssetsShouldBeLocked;
};

#endif