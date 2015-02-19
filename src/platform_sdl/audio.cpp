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
    /*
    int src_sample_size = sizeof(float);
    SDL_assert(src_sample_size <= target_sample_size);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, AUDIO_F32, 1, 48000, spec.format, spec.channels, spec.freq);
    cvt.len = buffer_samples * src_sample_size;
    cvt.buf = (Uint8*)audio_context->back_buffer;
    float* sound_buf = (float*)cvt.buf;
    static int persist_index = 0;
    persist_index += temp_buffer_read_byte / target_sample_size;
    for(int i=0; i<buffer_samples; ++i){
        sound_buf[i] = SDL_sinf((float)(i+persist_index)*0.01f)*kGameVolume;
        if(i+persist_index < 1000){
            sound_buf[i] *= (i+persist_index)/1000.0f;
        }
        if(i > buffer_samples - 1000){
            sound_buf[i] *= (buffer_samples - i)/1000.0f;
        }
    }
    SDL_ConvertAudio(&cvt);*/
    for(int i=0; i<audio_context->num_ogg_tracks; ++i){
        OggTrack* ogg_track = audio_context->ogg_tracks[i];
        stb_vorbis_info info = stb_vorbis_get_info(ogg_track->vorbis);
        int src_sample_size = info.channels * sizeof(float);
        SDL_assert(src_sample_size <= target_sample_size);
        SDL_AudioCVT cvt;
        SDL_BuildAudioCVT(&cvt, AUDIO_F32, info.channels, info.sample_rate, spec.format, spec.channels, spec.freq);
        cvt.len = buffer_samples * src_sample_size;
        cvt.buf = (Uint8*)audio_context->back_buffer;
        ogg_track->read_pos += temp_buffer_read_byte / target_sample_size;
        if(ogg_track->read_pos > ogg_track->samples){
            ogg_track->read_pos -= ogg_track->samples;
        }
        stb_vorbis_seek(ogg_track->vorbis, ogg_track->read_pos);
        int samples_received = stb_vorbis_get_samples_float_interleaved(
            ogg_track->vorbis,
            info.channels,
            (float*)cvt.buf,
            buffer_samples * info.channels);
        if(samples_received < buffer_samples){
            stb_vorbis_seek(ogg_track->vorbis, 0);
            stb_vorbis_get_samples_float_interleaved(
                ogg_track->vorbis,
                info.channels,
                (float*)cvt.buf+(samples_received*info.channels),
                (buffer_samples-samples_received) * info.channels);
        }
        SDL_ConvertAudio(&cvt);
    }
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
