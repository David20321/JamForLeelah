#include "platform_sdl/audio.h"
#include "platform_sdl/error.h"
#include "internal/memory.h"
#include "internal/common.h"
#include <SDL.h>
#ifdef USE_STB_VORBIS
    #include "stb_vorbis.c"
#else
    #include <vorbis/vorbisfile.h>
    #include <memory.h>

size_t OGVmemoryRead(void * buff, size_t b, size_t nelts, void *data);
int OGVmemorySeek(void *data, ogg_int64_t seek, int type);
long OGVmemoryTell(void* data);
int OGVmemoryClose(void *data);

ov_callbacks OV_MEMORY_CALLBACKS = {
    OGVmemoryRead,
    OGVmemorySeek,
    OGVmemoryClose,
    OGVmemoryTell
};

size_t OGVmemoryRead(void * buff, size_t b, size_t nelts, void *data)
{
    tOGVMemoryReader *of = reinterpret_cast<tOGVMemoryReader*>(data);
    size_t len = b * nelts;
    if (of->buff_pos + len > of->buff_size) {
        len = (size_t)(of->buff_size - of->buff_pos);
    }
    if (len)
        memcpy(buff, of->buff + of->buff_pos, len );
    of->buff_pos += len;
    return len;
}

int OGVmemorySeek(void *data, ogg_int64_t seek, int type)
{
    tOGVMemoryReader *of = reinterpret_cast<tOGVMemoryReader*>(data);
    switch (type) {
        case SEEK_CUR:
            of->buff_pos += seek;
            break;
        case SEEK_END:
            of->buff_pos = of->buff_size - seek;
            break;
        case SEEK_SET:
            of->buff_pos = seek;
            break;
        default:
            return -1;
    }
    if ( of->buff_pos < 0) {
        of->buff_pos = 0;
        return -1;
    }
    if ( of->buff_pos > of->buff_size) {
        of->buff_pos = of->buff_size;
        return -1;
    }
    return 0;
}

int OGVmemoryClose(void* data)
{
    tOGVMemoryReader *of = reinterpret_cast<tOGVMemoryReader*>(data);
    delete of;
    return 0;
}

long OGVmemoryTell(void* data) {
    tOGVMemoryReader *of = reinterpret_cast<tOGVMemoryReader*>(data);
    return (long)of->buff_pos;
}

#endif

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

static const float kMasterGain = 1.0f;
static const float kMusicGain = 1.0f;

void UpdateAudio(AudioContext* audio_context, StackAllocator* stack_allocator) {
    // Get playback position in buffer
    SDL_LockAudioDevice(audio_context->device_id);
    int temp_buffer_read_byte = audio_context->buffer_read_byte;
    SDL_UnlockAudioDevice(audio_context->device_id);
    // Create buffer to convert from 2-channel float to whatever our output format is
    static const int src_sample_size = 2 * sizeof(float);
    // Calculate some simple info from audio spec
    const SDL_AudioSpec &spec = audio_context->audio_spec;
    int target_sample_size = (spec.size / spec.samples);
    SDL_assert(src_sample_size <= target_sample_size);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, AUDIO_F32, 2, 48000, spec.format, spec.channels, spec.freq);
    cvt.len = audio_context->buffer_samples * src_sample_size;
    cvt.buf = (Uint8*)audio_context->back_buffer;
    float* flt_buf = (float*)cvt.buf;
    for(int i=0, len=audio_context->buffer_samples*2; i<len; ++i){
        flt_buf[i] = 0.0f;
    }
    if(kMusicGain * kMasterGain > 0.0f){
        // Add each audio track's contribution to output
        for(int i=0; i<audio_context->num_ogg_tracks; ++i){
            OggTrack* ogg_track = audio_context->ogg_tracks[i];
            int channels;
#ifdef USE_STB_VORBIS
            stb_vorbis_info info = stb_vorbis_get_info(ogg_track->vorbis);
            SDL_assert(info.sample_rate == 48000 && info.channels == 2);
            channels = info.channels;
#else
            SDL_assert(false); // Need to implement non-stb-vorbis equivalent
#endif
            int samples_read = temp_buffer_read_byte / target_sample_size;
            int read_offset = samples_read*channels;
            for(int j=0, len=audio_context->buffer_samples-samples_read, index=0; 
                j<len; 
                ++j)
            {
                for(int k=0; k<channels; ++k){
                    ogg_track->decoded[index] = ogg_track->decoded[index+read_offset];
                    ++index;                    
                }
            }
#ifdef USE_STB_VORBIS
            int vorbis_samples_read = 
                stb_vorbis_get_samples_float_interleaved(ogg_track->vorbis, 
                    channels, 
                    &ogg_track->decoded[(audio_context->buffer_samples - samples_read) * channels], 
                    samples_read * channels);
            if(vorbis_samples_read < samples_read){
                stb_vorbis_seek_start(ogg_track->vorbis);
                stb_vorbis_get_samples_float_interleaved(ogg_track->vorbis, 
                    channels, 
                    &ogg_track->decoded[(audio_context->buffer_samples - samples_read + vorbis_samples_read) * channels], 
                    (samples_read - vorbis_samples_read) * channels);
            }
#else
            SDL_assert(false); // Need to implement non-stb-vorbis equivalent
#endif
            ogg_track->read_pos += samples_read;
            if(ogg_track->read_pos > ogg_track->samples){
                ogg_track->read_pos -= ogg_track->samples;
            }
            if(ogg_track->gain > 0.0f || ogg_track->target_gain > 0.0f){
                for(int i=0, len=audio_context->buffer_samples*2; i<len; ++i){
                    ogg_track->gain = MoveTowards(ogg_track->gain, ogg_track->target_gain, ogg_track->transition_speed);
                    flt_buf[i] += ogg_track->decoded[i] * ogg_track->gain * kMusicGain * kMasterGain;
                }
            }
        }
    }
    SDL_ConvertAudio(&cvt);
    // swap audio buffer
    SDL_LockAudioDevice(audio_context->device_id);
    swap(audio_context->curr_buffer, audio_context->back_buffer);
    audio_context->buffer_read_byte -= temp_buffer_read_byte;
    SDL_UnlockAudioDevice(audio_context->device_id);
}

void InitAudio(AudioContext* context, StackAllocator *stack_allocator) {
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
    context->buffer_samples = max((int)(buffer_time * spec.freq), spec.samples);
    context->buffer_size = context->buffer_samples * sample_size;
    context->curr_buffer = stack_allocator->Alloc(context->buffer_size);
    if(!context->curr_buffer){
        FormattedError("Buffer alloc failed", "Failed to allocate primary audio buffer");
        exit(1);
    }
    context->back_buffer = stack_allocator->Alloc(context->buffer_size);
    if(!context->back_buffer){
        FormattedError("Buffer alloc failed", "Failed to allocate backup audio buffer");
        exit(1);
    }
    context->buffer_read_byte = 0;
    UpdateAudio(context, stack_allocator);
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
