#pragma once
#ifndef PLATFORM_SDL_AUDIO_HPP
#define PLATFORM_SDL_AUDIO_HPP

#include <SDL.h>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

class StackAllocator;

struct OggTrack {
    void* mem;
    int mem_len;
    stb_vorbis_alloc vorbis_alloc;
    stb_vorbis* vorbis;
    int samples;
    int read_pos;
};

struct AudioContext {
    SDL_AudioDeviceID device_id;
    SDL_AudioSpec audio_spec;
    bool shutting_down;
    void* curr_buffer;
    void* back_buffer;
    int buffer_read_byte;
    int buffer_size;
    static const int kMaxOggTracks = 10;
    int num_ogg_tracks;
    OggTrack* ogg_tracks[kMaxOggTracks];
    void AddOggTrack(OggTrack* vorbis);
    void ShutDown();
};

void UpdateAudio(AudioContext* audio_context);
void InitAudio(AudioContext* context, StackAllocator *stack_memory_block);

#endif