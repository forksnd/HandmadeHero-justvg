#if !defined(HANDMADE_DEBUG_VARIABLES_H)
#define HANDMADE_DEBUG_VARIABLES_H

struct debug_variable_definition_context
{
    debug_state *State;
    memory_arena *Arena;

    debug_variable_reference *Group;
};

internal debug_variable *
DEBUGAddUnreferencedVariable(debug_state *State, debug_variable_type Type, char *Name)
{
    debug_variable *Var = PushStruct(&State->DebugArena, debug_variable);
    Var->Type = Type;
    Var->Name = (char *)PushCopy(&State->DebugArena, StringLength(Name) + 1, Name);

    return(Var);
}

internal debug_variable_reference *
DEBUGAddVariableReference(debug_state *State, debug_variable_reference *GroupRef, debug_variable *Var)
{
    debug_variable_reference *Ref = PushStruct(&State->DebugArena, debug_variable_reference);
    Ref->Var = Var;
    Ref->Next = 0;

    Ref->Parent = GroupRef;
    debug_variable *Group = Ref->Parent ? Ref->Parent->Var : 0;
    if(Group)
    {
        if(Group->Group.LastChild)
        {
            Group->Group.LastChild = Group->Group.LastChild->Next = Ref;
        }
        else
        {
            Group->Group.FirstChild = Group->Group.LastChild = Ref;
        }
    }

    return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariableReference(debug_variable_definition_context *Context, debug_variable *Var)
{
    debug_variable_reference *Ref = DEBUGAddVariableReference(Context->State, Context->Group, Var);
    return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, debug_variable_type Type, char *Name)
{
    debug_variable *Var = DEBUGAddUnreferencedVariable(Context->State, Type, Name);
    debug_variable_reference *Ref = DEBUGAddVariableReference(Context, Var);

    return(Ref);
}

internal debug_variable *
DEBUGAddRootGroupInternal(debug_state *State, char *Name)
{
    debug_variable *Group = DEBUGAddUnreferencedVariable(State, DebugVariableType_Group, Name);
    Group->Group.Expanded = true;
    Group->Group.FirstChild = Group->Group.LastChild = 0;

    return(Group);
}

internal debug_variable_reference *
DEBUGAddRootGroup(debug_state *State, char *Name)
{
    debug_variable_reference *GroupRef = DEBUGAddVariableReference(State, 0, DEBUGAddRootGroupInternal(State, Name));

    return(GroupRef);
}

internal debug_variable_reference *
DEBUGBeginVariableGroup(debug_variable_definition_context *Context, char *Name)
{
    debug_variable_reference *Group = DEBUGAddVariableReference(Context, 
                                                                DEBUGAddRootGroupInternal(Context->State, Name));
    Group->Var->Group.Expanded = false;
    Group->Var->Group.FirstChild = Group->Var->Group.LastChild = 0;

    Context->Group = Group;

    return(Group);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, bool32 Value)
{
    debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_Bool32, Name);
    Ref->Var->Bool32 = Value;

    return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, real32 Value)
{
    debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_Real32, Name);
    Ref->Var->Real32 = Value;

    return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, v4 Value)
{
    debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_V4, Name);
    Ref->Var->Vector4 = Value;

    return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, bitmap_id Value)
{
    debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_BitmapDisplay, Name);
    Ref->Var->BitmapDisplay.ID = Value;
    Ref->Var->BitmapDisplay.Dim = V2(25.0f, 25.0f);
    Ref->Var->BitmapDisplay.Alpha = true;

    return(Ref);
}

internal void
DEBUGEndVariableGroup(debug_variable_definition_context *Context)
{
    Assert(Context->Group);

    Context->Group = Context->Group->Parent;
}

internal void
DEBUGCreateVariables(debug_variable_definition_context *Context)
{
    debug_variable_reference *UseDebugCamRef = 0;

#define DEBUG_VARIABLE_LISTING(Name) DEBUGAddVariable(Context, #Name, DEBUGUI_##Name);

    DEBUGBeginVariableGroup(Context, "Group chunks");
    DEBUG_VARIABLE_LISTING(GroundChunkOutlines);
    DEBUG_VARIABLE_LISTING(GroundChunkCheckboards);
    DEBUGEndVariableGroup(Context);

    DEBUGBeginVariableGroup(Context, "Renderer");
    {
        DEBUGBeginVariableGroup(Context, "Camera");
        {
            UseDebugCamRef = DEBUG_VARIABLE_LISTING(UseDebugCamera);
            DEBUG_VARIABLE_LISTING(DebugCameraDistance);
            DEBUG_VARIABLE_LISTING(UseRoomBasedCamera);
        }
        DEBUGEndVariableGroup(Context);
    }
    DEBUGEndVariableGroup(Context);

    DEBUGBeginVariableGroup(Context, "Particles");
    DEBUG_VARIABLE_LISTING(ParticleTest);
    DEBUGEndVariableGroup(Context);

    DEBUG_VARIABLE_LISTING(UseSpaceOutlines);
    DEBUG_VARIABLE_LISTING(FauxV4);
    
#undef DEBUG_VARIABLE_LISTING
}

#endif