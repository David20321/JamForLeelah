#include "platform_sdl/profiler.h"
#include "platform_sdl/error.h"
#include "internal/common.h"
#include <cstring>

void Profiler::Init() {
    curr_event = -1;
    event_stack_depth = 0;
    num_events = 0;
}

void Profiler::StartEvent(const char* txt) {
    if(num_events < kMaxEvents && event_stack_depth < kMaxEventStackDepth){
        curr_event = num_events;
        event_stack[event_stack_depth] = curr_event;
        Event& event = events[num_events++];
        event.start_time = SDL_GetPerformanceCounter();
        event.label = txt;
        event.depth = event_stack_depth++;
    }
}

void Profiler::EndEvent() {
    if(event_stack_depth > 0 && curr_event != -1){
        Event& event = events[curr_event];
        event.end_time = SDL_GetPerformanceCounter();
        --event_stack_depth;
        if(event_stack_depth > 0){
            curr_event = event_stack[event_stack_depth-1];
        } else {
            curr_event = -1;
        }
    }
}

void Profiler::Export(const char* filename) {
    const int kPerfCountToMicroseconds = (int)(SDL_GetPerformanceFrequency() / 1000000);
    SDL_RWops* file = SDL_RWFromFile(filename, "w");
    if(file){
        static const int kBufSize = 1024;
        char buf[kBufSize];
        for(int i=0; i<num_events; ++i){
            Event& event = events[i];
            int index = 0;
            for(int j=0; j<event.depth; ++j){
                for(int k=0; k<4; ++k){
                    if(index < kBufSize-1){
                        buf[index++] = '-';
                    }
                }
            }
            int microseconds = (int)((event.end_time - event.start_time) / kPerfCountToMicroseconds);
            FormatString(&buf[index], kBufSize-index, "%s: %d us\n", events[i].label, microseconds);
            SDL_RWwrite(file, buf, 1, strlen(buf));
        }
        if(num_events == kMaxEvents){
            FormatString(buf, kBufSize, "--reached kMaxEvents--\n");
            SDL_RWwrite(file, buf, 1, strlen(buf));
        }
        SDL_RWclose(file);
    } else {
        FormattedError("Error", "Could not open %s for writing", filename);
    }
}
