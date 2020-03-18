
inline render_group
BeginRenderGroup(game_assets *Assets, game_render_commands *Commands,
                 u32 GenerationID, bool32 RendersInBackground)
{
	render_group Result = {};

    Result.Assets = Assets;
    Result.RendersInBackground = RendersInBackground;
    Result.GlobalAlpha = 1.0f;
    Result.GenerationID = GenerationID;
    Result.Commands = Commands;

	return(Result);
}

inline void
EndRenderGroup(render_group *RenderGroup)
{
    // TODO(georgy):
    // RenderGroup->Commands->MissingResourceCount += RenderGroup->MissingResourceCount;
}

inline void
Perspective(render_group *RenderGroup, uint32 PixelWidth, uint32 PixelHeight, 
            real32 MetersToPixels, real32 FocalLength, real32 DistanceAboveTarget)
{
    real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
    RenderGroup->MonitorHalfDimInMeters = {0.5f*PixelWidth*PixelsToMeters,
                                           0.5f*PixelHeight*PixelsToMeters};

    RenderGroup->CameraTransform.MetersToPixels = MetersToPixels;
    RenderGroup->CameraTransform.FocalLength = FocalLength; // NOTE(george): Meters the person is sitting from their monitor
    RenderGroup->CameraTransform.DistanceAboveTarget = DistanceAboveTarget;
    RenderGroup->CameraTransform.ScreenCenter = V2(0.5f*PixelWidth, 0.5f*PixelHeight);
    RenderGroup->CameraTransform.Orthographic = false;
}

inline void
Orthographic(render_group *RenderGroup, uint32 PixelWidth, uint32 PixelHeight, real32 MetersToPixels)
{
    real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
    RenderGroup->MonitorHalfDimInMeters = {0.5f*PixelWidth*PixelsToMeters, 0.5f*PixelHeight*PixelsToMeters};

    RenderGroup->CameraTransform.MetersToPixels = MetersToPixels;
    RenderGroup->CameraTransform.FocalLength = 1.0f; // NOTE(george): Meters the person is sitting from their monitor
    RenderGroup->CameraTransform.DistanceAboveTarget = 1.0f;
    RenderGroup->CameraTransform.ScreenCenter = V2(0.5f*PixelWidth, 0.5f*PixelHeight);
    RenderGroup->CameraTransform.Orthographic = true;
}

internal entity_basis_p_result
GetRenderEntityBasisP(camera_transform CameraTransform,
                      object_transform ObjectTransform,
                      v3 OriginalP)
{
    entity_basis_p_result Result = {};

    v3 P = V3(OriginalP.xy, 0.0f) + ObjectTransform.OffsetP;

    if(CameraTransform.Orthographic)
    {
        Result.P = CameraTransform.ScreenCenter + CameraTransform.MetersToPixels*P.xy; 
        Result.Scale = CameraTransform.MetersToPixels;
        Result.Valid = true;
    }
    else
    {
        real32 OffsetZ = 0.0f;

        real32 DistanceAboveTarget = CameraTransform.DistanceAboveTarget;

        if(Global_Renderer_Camera_UseDebug)        
        {
            DistanceAboveTarget += Global_Renderer_Camera_DebugDistance;
        }

        real32 DistanceToPZ = DistanceAboveTarget - P.z;
        real32 NearClipPlane = 0.2f;

        v3 RawXY = V3(P.xy, 1.0f);

        if(DistanceToPZ > NearClipPlane)
        {
            v3 ProjectedXY = (1.0f / DistanceToPZ) * CameraTransform.FocalLength * RawXY;
            Result.Scale = CameraTransform.MetersToPixels*ProjectedXY.z;
            Result.P = CameraTransform.ScreenCenter + CameraTransform.MetersToPixels*ProjectedXY.xy + V2(0.0f, Result.Scale*OffsetZ);  
            Result.Valid = true;
        }
    }

    Result.SortKey = ObjectTransform.SortBias + 4096.0f*(2.0f*P.z + 1.0f*(r32)ObjectTransform.Upright) - P.y;

    return(Result);
}

#define PushRenderElement(Group, type, SortKey) (type *)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type, SortKey)
inline void *
PushRenderElement_(render_group *Group, uint32 Size, render_group_entry_type Type, r32 SortKey)
{
    game_render_commands *Commands = Group->Commands;

    void *Result = 0;

    Size += sizeof(render_group_entry_header);

    if((Commands->PushBufferSize + Size) < (Commands->SortEntryAt - sizeof(tile_sort_entry)))
    {
        render_group_entry_header *Header = (render_group_entry_header *)(Commands->PushBufferBase + Commands->PushBufferSize);
        Header->Type = Type;
        Result = (uint8 *)Header + sizeof(*Header);

        Commands->SortEntryAt -= sizeof(tile_sort_entry);
        tile_sort_entry *Entry = (tile_sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);
        Entry->SortKey = SortKey;
        Entry->PushBufferOffset = Commands->PushBufferSize;

        Commands->PushBufferSize += Size;
        Commands->PushBufferElementCount++;
    }
    else
    {
        InvalidCodePath;
    }

    return(Result);
}

inline used_bitmap_dim 
GetBitmapDim(render_group *Group, object_transform ObjectTransform,
             loaded_bitmap *Bitmap, real32 Height, v3 Offset, real32 CAlign)
{
    used_bitmap_dim Dim;

    Dim.Size = V2(Height*Bitmap->WidthOverHeight, Height);
    Dim.Align = CAlign*Hadamard(Bitmap->AlignPercentage, Dim.Size);
    Dim.P = Offset - V3(Dim.Align, 0.0f);
    Dim.Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, Dim.P);

    return(Dim);
}

inline void
PushBitmap(render_group *Group, object_transform ObjectTransform, 
           loaded_bitmap *Bitmap, r32 Height, v3 Offset, 
           v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
    used_bitmap_dim Dim = GetBitmapDim(Group, ObjectTransform, Bitmap, Height, Offset, CAlign);
    if(Dim.Basis.Valid)
    {
        render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap, Dim.Basis.SortKey);
        if(Entry)
        {
            Entry->Bitmap = Bitmap;
            Entry->P = Dim.Basis.P;
            Entry->Color = Group->GlobalAlpha*Color;    
            Entry->Size = Dim.Basis.Scale*Dim.Size;
        }
    }
}

inline void
PushBitmap(render_group *Group, object_transform ObjectTransform, 
           bitmap_id ID, real32 Height, v3 Offset, 
           v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
    loaded_bitmap *Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);
    if(Group->RendersInBackground && !Bitmap)
    {
        LoadBitmap(Group->Assets, ID, true);
        Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);
    }

    if(Bitmap)
    {
        PushBitmap(Group, ObjectTransform, Bitmap, Height, Offset, Color, CAlign);
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadBitmap(Group->Assets, ID, false);
        Group->MissingResourceCount++;
    }
}

inline loaded_font *
PushFont(render_group *Group, font_id ID)
{
    loaded_font *Font = GetFont(Group->Assets, ID, Group->GenerationID);
   
    if(Font)
    {
        // NOTE(georgy): Nothing to do
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadFont(Group->Assets, ID, false);
        Group->MissingResourceCount++;
    }

    return(Font);
}

inline void
PushRect(render_group *Group, object_transform ObjectTransform, 
         v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
    v3 P = Offset - V3(0.5f*Dim, 0.0f);

    entity_basis_p_result Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P);
    if(Basis.Valid)
    {
        render_entry_rectangle *Rect = PushRenderElement(Group, render_entry_rectangle, Basis.SortKey);
        if(Rect)
        {
            Rect->P = Basis.P;
            Rect->Color = Color; 
            Rect->Dim = Basis.Scale*Dim;
        }  
    }    
}

inline void
PushRect(render_group *Group, object_transform ObjectTransform, 
         rectangle2 Rectangle, real32 Z, v4 Color = V4(1, 1, 1, 1))
{
    PushRect(Group, ObjectTransform, V3(GetCenter(Rectangle), Z), GetDim(Rectangle), Color);
}


inline void
PushRectOutline(render_group *Group, object_transform ObjectTransform, 
                v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1), real32 Thickness = 0.1f)
{
    // NOTE(george): Top and bottom
    PushRect(Group, ObjectTransform, Offset - V3(0, Dim.y/2, 0), V2(Dim.x, Thickness), Color);    
    PushRect(Group, ObjectTransform, Offset + V3(0, Dim.y/2, 0), V2(Dim.x, Thickness), Color);    

    // NOTE(george): Left and right
    PushRect(Group, ObjectTransform, Offset - V3(Dim.x/2, 0, 0), V2(Thickness, Dim.y), Color);    
    PushRect(Group, ObjectTransform, Offset + V3(Dim.x/2, 0, 0), V2(Thickness, Dim.y), Color);    
}

inline void
Clear(render_group *RenderGroup, v4 Color)
{
    render_entry_clear *Entry = PushRenderElement(RenderGroup, render_entry_clear, Real32Minimum);
    if(Entry)
    {
        Entry->Color = Color;
    }
}

inline void
CoordinateSystem(render_group *Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, 
                 loaded_bitmap *Texture, loaded_bitmap *NormalMap,
                 environment_map *Top, environment_map *Middle, environment_map *Bottom)
{
#if 0
    entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenDim);

    if(Basis.Valid)
    {
        render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_coordinate_system);
        if(Entry)
        {
            Entry->Origin = Origin;
            Entry->XAxis = XAxis;
            Entry->YAxis = YAxis;
            Entry->Color = Color;
            Entry->Texture = Texture;
            Entry->NormalMap = NormalMap;
            Entry->Top = Top;
            Entry->Middle = Middle;
            Entry->Bottom = Bottom;
        }
    }
#endif
}

inline v3
Unproject(render_group *Group, object_transform ObjectTransform, v2 PixelsXY)
{
    camera_transform Transform = Group->CameraTransform;

    v2 UnprojectedXY;
    if(Transform.Orthographic)
    {
        UnprojectedXY = (1.0f / Transform.MetersToPixels) * (PixelsXY - Transform.ScreenCenter);
    }
    else
    {
        v2 A = (PixelsXY - Transform.ScreenCenter) * (1.0f / Transform.MetersToPixels);
        UnprojectedXY = A * ((Transform.DistanceAboveTarget - ObjectTransform.OffsetP.z) / Transform.FocalLength);
    }
 
    v3 Result = V3(UnprojectedXY, ObjectTransform.OffsetP.z);
    Result -= ObjectTransform.OffsetP;

    return(Result);
}

inline v2
UnprojectOld(render_group *Group, v2 ProjectedXY, real32 AtDistanceFromCamera)
{
    v2 WorldXY = (AtDistanceFromCamera / Group->CameraTransform.FocalLength)*ProjectedXY;
    return(WorldXY);
}

inline rectangle2
GetCameraRectangleAtDistance(render_group *Group, real32 DistanceFromCamera)
{
    v2 RawXY = UnprojectOld(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);

    rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);

    return(Result);
}

inline rectangle2
GetCameraRectangleAtTarget(render_group *Group)
{
    rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->CameraTransform.DistanceAboveTarget);
    return(Result);
}

inline bool32 
AllResourcesPresent(render_group *Group)
{
    bool32 Result = (Group->MissingResourceCount == 0);

    return(Result);
} 