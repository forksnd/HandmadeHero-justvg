#if !defined(HANDMADE_PLATFORM_H)
#define HANDMADE_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif


// 
// NOTE(george): Compilers
// 
#if !defined(COMPILER_MSVC)
    #define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
    #define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
    #if _MSC_VER
        #undef  COMPILER_MSVC
        #define COMPILER_MSVC 1
    #else
        #undef COMPILER_LLVM
        // TODO(george): More compilers!!!
        #define COMPILER_LLVM 1
    #endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif

// 
// NOTE(george): Types
// 
    #include <stdint.h>
    
    typedef uint8_t uint8;
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint64_t uint64;

    typedef int8_t int8;
    typedef int16_t int16;
    typedef int32_t int32;
    typedef int64_t int64;
    typedef int32_t bool32;

    typedef size_t memory_index;

    typedef float real32;
    typedef double real64;

    struct thread_context
    {
        int Placeholder;
    }

    /*
        NOTE(george): Services that the platform layer provides to the game
    */
    ;

    #if HANDMADE_INTERNAL

    /* IMPORTANT(george):

        These are NOT for doing anything in the shipping game - they are
        blocking and the write doesn't protect against lost data!
    */

    struct debug_read_file_result
    {
        uint32 ContentsSize;
        void *Contents;
    };

    #define DEBUG_PLATFROM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *Thread, char *Filename)
    typedef DEBUG_PLATFROM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

    #define DEBUG_PLATFROM_FREE_FILE_MEMORY(name) void name(thread_context *Thread, void *Memory)
    typedef DEBUG_PLATFROM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

    #define DEBUG_PLATFROM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory)
    typedef DEBUG_PLATFROM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

    #endif

    struct game_offscreen_buffer
    {
        void *Memory;
        int Width;
        int Height;
        int Pitch;
        int BytesPerPixel;    
    };

    struct game_sound_output_buffer
    {
        int16 *Samples;
        int SamplesPerSecond;
        int SampleCount;
    };

    struct game_button_state
    {
        int HalfTransitionCount;
        bool32 EndedDown;
    };

    struct game_controller_input
    {
        bool32 IsConnected;

        bool32 IsAnalog;
        real32 StickAverageX;
        real32 StickAverageY;

        union 
        {
            game_button_state Buttons[12];
            struct 
            {
                game_button_state MoveUp;
                game_button_state MoveDown;
                game_button_state MoveLeft;
                game_button_state MoveRight;

                game_button_state ActionUp;
                game_button_state ActionDown;
                game_button_state ActionLeft;
                game_button_state ActionRight;

                game_button_state LeftShoulder;
                game_button_state RightShoulder;

                game_button_state Back;
                //NOTE(george): All buttons must be added above this line
                game_button_state Start;
            };
        };
    };

    struct game_input
    {
        game_button_state MouseButtons[5];
        int32 MouseX, MouseY, MouseZ;

        real32 dtForFrame;

        game_controller_input Controllers[5];
    };

    struct game_memory 
    {
        bool32 IsInitialized;

        uint64 PermanentStorageSize;
        void *PermanentStorage;
        uint64 TransientStorageSize;
        void *TransientStorage;

        debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
        debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
        debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
    };

    #define GAME_UPDATE_AND_RENDER(name) void name(thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
    typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

    // NOTE(george): At the moment, this has to be a very fast function, it cannot be
    // more than a millisecond or so.
    // TODO(george): Reduce the pressure on this function's performance by measuring it
    // or asking about it, etc.
    #define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer)
    typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#endif