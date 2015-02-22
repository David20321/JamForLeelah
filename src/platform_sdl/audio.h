#pragma once
#ifndef PLATFORM_SDL_AUDIO_HPP
#define PLATFORM_SDL_AUDIO_HPP

#include <SDL.h>
#ifdef USE_STB_VORBIS
    #define STB_VORBIS_HEADER_ONLY
    #include "stb_vorbis.c"
    #undef STB_VORBIS_HEADER_ONLY
#else
    #include <vorbis/vorbisfile.h>

extern ov_callbacks OV_MEMORY_CALLBACKS;

struct tOGVMemoryReader {
    char* buff;
    ogg_int64_t buff_size;
    ogg_int64_t buff_pos;

    tOGVMemoryReader(const void* aBuff, size_t aBuffSize) : buff((char*)aBuff), buff_size(aBuffSize), buff_pos(0) {}
};
#endif

class StackAllocator;

struct OggTrack {
    void* mem;
    int mem_len;
#ifdef USE_STB_VORBIS
    stb_vorbis_alloc vorbis_alloc;
    stb_vorbis* vorbis;
#else
    OggVorbis_File vorbis;
#endif
    float* decoded;
    int samples;
    int read_pos;
    float target_gain;
    float gain;
    float transition_speed; // gain change per sample
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