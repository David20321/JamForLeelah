#pragma once
#ifndef PROFILER_HPP
#define PROFILER_HPP

#include <SDL.h>

class Profiler {
public:
    void Init();
    void StartEvent(const char* txt);
    void EndEvent();
    void Export( const char* filename );
private:
    struct Event {
        const char* label;
        int depth;
        Uint64 start_time;
        Uint64 end_time;
    };
    static const int kMaxEvents = 1024;
    Event events[kMaxEvents];
    int num_events;
    static const int kMaxEventStackDepth = 32;
    int event_stack[kMaxEventStackDepth];
    int event_stack_depth;
    int curr_event;
};


#endif