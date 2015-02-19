#include "SDL.h"
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "platform_sdl/audio.h"
#include "platform_sdl/error.h"
#include "platform_sdl/file_io.h"
#include "platform_sdl/graphics.h"
#include "platform_sdl/profiler.h"
#include "internal/common.h"
#include "internal/memory.h"
#include "game/game_state.h"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <new>

static void RunGame(Profiler* profiler, FileLoadThreadData* file_load_thread_data, 
                    StackAllocator* stack_allocator, GraphicsContext* graphics_context,
                    AudioContext* audio_context) 
{
    /*while(true){
        UpdateAudio(audio_context);
    }*/
    GameState* game_state;
    game_state = new((GameState*)stack_allocator->Alloc(sizeof(GameState))) GameState();
    if(!game_state){
        FormattedError("Error", "Could not alloc memory for game state");
        exit(1);
    }
    game_state->Init(audio_context, profiler, file_load_thread_data, stack_allocator);
    int last_ticks = SDL_GetTicks();
    bool game_running = true;
    while(game_running){
        profiler->StartEvent("Game loop");
        SDL_Event event;
        glm::vec2 mouse_rel;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_QUIT:
                game_running = false;
                break;
            case SDL_MOUSEMOTION:
                mouse_rel[0] += event.motion.xrel;
                mouse_rel[1] += event.motion.yrel;
                break;
            }
        }
        profiler->StartEvent("Update");
        float time_scale = 1.0f;// 0.1f;
        int ticks = SDL_GetTicks();
        game_state->Update(mouse_rel, (ticks - last_ticks) / 1000.0f * time_scale);
        last_ticks = ticks;
        profiler->EndEvent();
        profiler->StartEvent("Draw");
        game_state->Draw(graphics_context, SDL_GetTicks());
        profiler->EndEvent();
        profiler->StartEvent("Audio");
        UpdateAudio(audio_context);
        profiler->EndEvent();
        profiler->StartEvent("Swap");
        SDL_GL_SwapWindow(graphics_context->window);
        profiler->EndEvent();
        profiler->EndEvent();
    }
}

int main(int argc, char* argv[]) {
    Profiler profiler;
    profiler.Init();

    profiler.StartEvent("Allocate game memory block");
        static const int kGameMemSize = 1024*1024*64;
        StackAllocator stack_allocator;
        stack_allocator.Init(malloc(kGameMemSize), kGameMemSize);
        if(!stack_allocator.mem){
            FormattedError("Malloc failed", "Could not allocate enough memory");
            exit(1);
        }
    profiler.EndEvent();

    profiler.StartEvent("Initializing SDL");
        if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
            FormattedError("SDL_Init failed", "Could not initialize SDL: %s", SDL_GetError());
            return 1;
        }
        srand((unsigned)SDL_GetPerformanceCounter());
        char* write_dir = SDL_GetPrefPath("Wolfire", "UnderGlass");
    profiler.EndEvent();

    profiler.StartEvent("Checking for assets folder");
    {
        struct stat st;
        if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
            char *basePath = SDL_GetBasePath();
            ChangeWorkingDirectory(basePath);
            SDL_free(basePath);
            if(stat(ASSET_PATH "under_glass_game_assets_folder.txt", &st) == -1){
                FormattedError("Assets?", "Could not find assets directory, possibly running from inside archive");
                exit(1);
            }
        }
    }
    profiler.EndEvent();

    profiler.StartEvent("Set up file loader");
        FileLoadThreadData file_load_thread_data;
        file_load_thread_data.memory_len = 0;
        file_load_thread_data.memory = stack_allocator.Alloc(FileLoadThreadData::kMaxFileLoadSize);
        if(!file_load_thread_data.memory){
            FormattedError("Alloc failed", "Could not allocate memory for FileLoadData");
            return 1;
        }
        file_load_thread_data.wants_to_quit = false;
        file_load_thread_data.mutex = SDL_CreateMutex();
        if (!file_load_thread_data.mutex) {
            FormattedError("SDL_CreateMutex failed", "Could not create file load mutex: %s", SDL_GetError());
            return 1;
        }
        SDL_Thread* file_thread = SDL_CreateThread(FileLoadAsync, "FileLoaderThread", &file_load_thread_data);
        if(!file_thread){
            FormattedError("SDL_CreateThread failed", "Could not create file loader thread: %s", SDL_GetError());
            return 1;
        }
    profiler.EndEvent();

    profiler.StartEvent("Set up graphics context");
        GraphicsContext graphics_context;
        InitGraphicsContext(&graphics_context);
    profiler.EndEvent();

    AudioContext audio_context;
    InitAudio(&audio_context, &stack_allocator);

    RunGame(&profiler, &file_load_thread_data, &stack_allocator, 
            &graphics_context, &audio_context);

    {
        static const int kMaxPathSize = 4096;
        char path[kMaxPathSize];
        FormatString(path, kMaxPathSize, "%sprofile_data.txt", write_dir);
        profiler.Export(path);
    }

    // Wait for the audio to fade out
    // TODO: handle this better -- e.g. force audio fade immediately
    SDL_Delay(200);
    // We can probably just skip most of this if we want to quit faster
    SDL_CloseAudioDevice(audio_context.device_id);
    SDL_GL_DeleteContext(graphics_context.gl_context);  
    SDL_DestroyWindow(graphics_context.window);
    // Cleanly shut down file load thread 
    if (SDL_LockMutex(file_load_thread_data.mutex) == 0) {
        file_load_thread_data.wants_to_quit = true;
        SDL_UnlockMutex(file_load_thread_data.mutex);
        SDL_WaitThread(file_thread, NULL);
    } else {
        FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
        exit(1);
    }
    SDL_free(write_dir);
    SDL_Quit();
    free(stack_allocator.mem);
    return 0;
}