#if !defined(HANDMADE_DEBUG_VARIABLES_H)
#define HANDMADE_DEBUG_VARIABLES_H

struct debug_variable_definition_context
{
    debug_state *State;
    memory_arena *Arena;

    debug_variable *Group;
};

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, debug_variable_type Type, char *Name)
{
    debug_variable *Var = PushStruct(Context->Arena, debug_variable);
    Var->Type = Type;
    Var->Name = (char *)PushCopy(Context->Arena, StringLength(Name) + 1, Name);
    Var->Next = 0;

    debug_variable *Group = Context->Group;
    Var->Parent = Group;
    if(Group)
    {
        if(Group->Group.LastChild)
        {
            Group->Group.LastChild = Group->Group.LastChild->Next = Var;
        }
        else
        {
            Group->Group.FirstChild = Group->Group.LastChild = Var;
        }
    }

    return(Var);
}

internal debug_variable *
DEBUGBeginVariableGroup(debug_variable_definition_context *Context, char *Name)
{
    debug_variable *Group = DEBUGAddVariable(Context, DebugVariableType_Group, Name);
    Group->Group.Expanded = false;
    Group->Group.FirstChild = Group->Group.LastChild = 0;

    Context->Group = Group;

    return(Group);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, bool32 Value)
{
    debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_Bool32, Name);
    Var->Bool32 = Value;

    return(Var);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, real32 Value)
{
    debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_Real32, Name);
    Var->Real32 = Value;

    return(Var);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, v4 Value)
{
    debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_V4, Name);
    Var->Vector4 = Value;

    return(Var);
}

internal void
DEBUGEndVariableGroup(debug_variable_definition_context *Context)
{
    Assert(Context->Group);

    Context->Group = Context->Group->Parent;
}

internal void
DEBUGCreateVariables(debug_state *State)
{
    debug_variable_definition_context Context = {};
    Context.State = State;
    Context.Arena = &State->DebugArena;
    Context.Group = DEBUGBeginVariableGroup(&Context, "Root");

#define DEBUG_VARIABLE_LISTING(Name) DEBUGAddVariable(&Context, #Name, DEBUGUI_##Name);

    DEBUGBeginVariableGroup(&Context, "Group chunks");
    DEBUG_VARIABLE_LISTING(GroundChunkOutlines);
    DEBUG_VARIABLE_LISTING(GroundChunkCheckboards);
    DEBUGEndVariableGroup(&Context);

    DEBUGBeginVariableGroup(&Context, "Renderer");
    {
        DEBUGBeginVariableGroup(&Context, "Camera");
        {
            DEBUG_VARIABLE_LISTING(UseDebugCamera);
            DEBUG_VARIABLE_LISTING(DebugCameraDistance);
            DEBUG_VARIABLE_LISTING(UseRoomBasedCamera);
        }
        DEBUGEndVariableGroup(&Context);
    }
    DEBUGEndVariableGroup(&Context);

    DEBUGBeginVariableGroup(&Context, "Particles");
    DEBUG_VARIABLE_LISTING(ParticleTest);
    DEBUGEndVariableGroup(&Context);

    DEBUG_VARIABLE_LISTING(UseSpaceOutlines);
    DEBUG_VARIABLE_LISTING(FauxV4);

#undef DEBUG_VARIABLE_LISTING

    State->RootGroup = Context.Group;
}

#endif