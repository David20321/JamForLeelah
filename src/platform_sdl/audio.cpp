#include "platform_sdl/audio.h"
#include "platform_sdl/error.h"
#include "internal/memory.h"
#include "internal/common.h"
#include <SDL.h>
#include "stb_vorbis.c"

// TODO: handle audio fade-in/out here?
static void MyAudioCallback (void* userdata, Uint8* stream, int len) {
    AudioContext* audio_context = (AudioContext*)userdata;
    int fill_index = audio_context->buffer_read_byte;
    int fill_amount = min(len, audio_context->buffer_size - audio_context->buffer_read_byte);
    for(int i=0; i<fill_amount; ++i){
        stream[i] = ((Uint8*)audio_context->curr_buffer)[fill_index++];
    }
    for(int i=fill_amount; i<len; ++i){
        stream[i] = 0;
    }
    audio_context->buffer_read_byte += fill_amount;
}

static const float kGameVolume = 0.3f;

void UpdateAudio(AudioContext* audio_context) {
    const SDL_AudioSpec &spec = audio_context->audio_spec;
    SDL_LockAudioDevice(audio_context->device_id);
    int temp_buffer_read_byte = audio_context->buffer_read_byte;
    SDL_UnlockAudioDevice(audio_context->device_id);
    int target_sample_size = (spec.size / spec.samples);
    int buffer_samples = audio_context->buffer_size/target_sample_size;

    static float* temp_buf = (float*)malloc(sizeof(float) * buffer_samples * 2);
    static const int src_sample_size = 2 * sizeof(float);
    SDL_assert(src_sample_size <= target_sample_size);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, AUDIO_F32, 2, 48000, spec.format, spec.channels, spec.freq);
    cvt.len = buffer_samples * src_sample_size;
    cvt.buf = (Uint8*)audio_context->back_buffer;
    float* flt_buf = (float*)cvt.buf;
    for(int i=0, len=buffer_samples*2; i<len; ++i){
        flt_buf[i] = 0.0f;
    }
    for(int i=0; i<audio_context->num_ogg_tracks; ++i){
        OggTrack* ogg_track = audio_context->ogg_tracks[i];
        stb_vorbis_info info = stb_vorbis_get_info(ogg_track->vorbis);
        SDL_assert(info.sample_rate == 48000 && info.channels == 2);
        ogg_track->read_pos += temp_buffer_read_byte / target_sample_size;
        if(ogg_track->read_pos > ogg_track->samples){
            ogg_track->read_pos -= ogg_track->samples;
        }
        if(ogg_track->gain > 0.0f || ogg_track->target_gain > 0.0f){
            int samples_remaining = ogg_track->samples - ogg_track->read_pos;
            int samples_to_read = min(samples_remaining, buffer_samples);
            memcpy(temp_buf, ogg_track->decoded+ogg_track->read_pos*2, sizeof(float) * 2 * samples_to_read);
            if(samples_to_read < buffer_samples){
                memcpy(temp_buf+samples_to_read*2, ogg_track->decoded, sizeof(float) * 2 * (buffer_samples-samples_to_read));
            }
            for(int i=0, len=buffer_samples*2; i<len; ++i){
                ogg_track->gain = MoveTowards(ogg_track->gain, ogg_track->target_gain, ogg_track->transition_speed);
                flt_buf[i] += temp_buf[i] * ogg_track->gain;
            }
        }
    }
    SDL_ConvertAudio(&cvt);
    // fill buffer
    SDL_LockAudioDevice(audio_context->device_id);
    swap(audio_context->curr_buffer, audio_context->back_buffer);
    audio_context->buffer_read_byte -= temp_buffer_read_byte;
    SDL_UnlockAudioDevice(audio_context->device_id);
}

void InitAudio(AudioContext* context, StackAllocator *stack_memory_block) {
    context->num_ogg_tracks = 0;
    context->shutting_down = false;
    SDL_AudioSpec target_audio_spec;
    SDL_zero(target_audio_spec);
    target_audio_spec.freq = 48000;
    target_audio_spec.format = AUDIO_F32;
    target_audio_spec.channels = 2;
    target_audio_spec.samples = 4096;
    target_audio_spec.callback = MyAudioCallback;
    target_audio_spec.userdata = context;

    context->device_id = SDL_OpenAudioDevice(NULL, 0, &target_audio_spec, &context->audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (context->device_id == 0) {
        FormattedError("OpenAudioDevice failed", "Failed to open audio: %s\n", SDL_GetError());
        exit(1);
    }

    const SDL_AudioSpec& spec = context->audio_spec;
    int sample_size = spec.size / spec.samples;
    double buffer_time = (double)spec.samples / (double)spec.channels / (double)spec.freq;
    static const double kMinBufTime = 1.0f/8.0f;
    buffer_time = max(kMinBufTime, buffer_time);
    int buffer_samples = max((int)(buffer_time * spec.freq), spec.samples);
    context->buffer_size = buffer_samples * sample_size;
    context->curr_buffer = stack_memory_block->Alloc(context->buffer_size);
    if(!context->curr_buffer){
        FormattedError("Buffer alloc failed", "Failed to allocate primary audio buffer");
        exit(1);
    }
    context->back_buffer = stack_memory_block->Alloc(context->buffer_size);
    if(!context->back_buffer){
        FormattedError("Buffer alloc failed", "Failed to allocate backup audio buffer");
        exit(1);
    }
    context->buffer_read_byte = 0;
    UpdateAudio(context);
    SDL_PauseAudioDevice(context->device_id, 0);  // start audio playing.
}

void AudioContext::AddOggTrack(OggTrack* ogg_track) {
    ogg_tracks[num_ogg_tracks] = ogg_track;
    ++num_ogg_tracks;
}

void AudioContext::ShutDown() {
    SDL_LockAudioDevice(device_id);
    shutting_down = true;
    SDL_UnlockAudioDevice(device_id);
}
