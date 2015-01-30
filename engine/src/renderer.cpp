#include <SDL.h>
#include <cstdlib>

static void Update() {
    static int num_audio_devices = 0;
    int new_num_audio_devices = SDL_GetNumAudioDevices(0);
    if(num_audio_devices != new_num_audio_devices){
        for( int i=0; i<new_num_audio_devices; ++i ){
            const char* name = SDL_GetAudioDeviceName(i, 0);
            SDL_Log("Audio device %d: %s\n", i, name);
        }
        num_audio_devices = new_num_audio_devices;
    }
}

static void Draw() {
}

struct AudioCallbackState {
    Uint8* backup_stream_buf;
    Uint8* stream_buf;
    int stream_buf_size;
    int read_cursor;
};

void MyAudioCallback(void *userdata, Uint8 * stream, int len) {
    AudioCallbackState* state = (AudioCallbackState*)userdata;
    int remaining_buf_size = state->stream_buf_size - state->read_cursor;
    int read_bytes = SDL_min(remaining_buf_size, len);
    int missed_bytes = len - read_bytes;
    for(int i=0; i<read_bytes; ++i){
        stream[i] = state->stream_buf[state->read_cursor++];
    }
    for(int i=0; i<missed_bytes; ++i){
        stream[i] = 0;
    }
}

static void InitAudio(SDL_AudioDeviceID *audio_device_id, SDL_AudioSpec *audio_spec, AudioCallbackState* state) {
    SDL_AudioSpec desired_audio_spec;

    SDL_zero(desired_audio_spec);
    desired_audio_spec.freq = 48000;
    desired_audio_spec.format = AUDIO_F32;
    desired_audio_spec.channels = 2;
    desired_audio_spec.samples = 4096;
    desired_audio_spec.callback = MyAudioCallback;  // you wrote this function elsewhere.
    desired_audio_spec.userdata = (void*)state;

    *audio_device_id = SDL_OpenAudioDevice(NULL, 0, &desired_audio_spec, audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (*audio_device_id == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s\n", SDL_GetError());
    } else {
        SDL_Log("Received audio format:\n");
        SDL_Log("Frequency: %d\n", audio_spec->freq);
        SDL_Log("Channels: %d\n", audio_spec->channels);
        SDL_Log("Buffer length: %d ms\n", audio_spec->samples * 1000 / audio_spec->channels / audio_spec->freq);
        bool is_float = SDL_AUDIO_ISFLOAT(audio_spec->format);
        int bit_size = SDL_AUDIO_BITSIZE(audio_spec->format);
        SDL_Log("Format: %s%d\n", is_float?"F":"I", bit_size);
    }
}

int initGame() {
    if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_VIDEO ) ){
		// SDL failed to initialize
		return 1;
	}	
	SDL_Window* window = SDL_CreateWindow("Game Window", SDL_WINDOWPOS_UNDEFINED,
									 	  SDL_WINDOWPOS_UNDEFINED, 640, 480,                    
									 	  SDL_WINDOW_OPENGL);
	if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create window\n");
		return 1;
    }

    SDL_AudioDeviceID audio_device_id;
    SDL_AudioSpec audio_spec;
    AudioCallbackState state;
    InitAudio(&audio_device_id, &audio_spec, &state);    
    state.read_cursor = 0;
    int bytes_per_sample_dst = audio_spec.size / audio_spec.samples;
    int buffer_samples = SDL_max(audio_spec.freq, audio_spec.samples); // Buffer at least a second
    state.stream_buf_size = buffer_samples * bytes_per_sample_dst;
    state.stream_buf = (Uint8*)SDL_malloc(state.stream_buf_size);
    state.backup_stream_buf = (Uint8*)SDL_malloc(state.stream_buf_size);
    SDL_PauseAudioDevice(audio_device_id, 0);  // start audio playing.

    bool game_running = true;
    while (game_running) {
        Uint32 start_time = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch(event.type){
            case SDL_QUIT:
                game_running = false;
                break;
            }
        }
        Update();
        Draw();

        int samples_needed = buffer_samples;
        SDL_AudioCVT cvt;
        SDL_BuildAudioCVT(&cvt, AUDIO_F32, 1, audio_spec.freq, audio_spec.format, audio_spec.channels, audio_spec.freq);
        int bytes_per_sample_src = sizeof(float);
        cvt.len = samples_needed * bytes_per_sample_src;
        cvt.buf = (Uint8 *) SDL_malloc(cvt.len * cvt.len_mult);
        float *src_buf = (float*)cvt.buf;
        static float sin_val_persist = 0.0f;
        static Uint64 total_samples = 0;
        SDL_LockAudioDevice(audio_device_id);
        int old_read_cursor = state.read_cursor;
        SDL_UnlockAudioDevice(audio_device_id);
        int samples_read = old_read_cursor / bytes_per_sample_dst;
        for(int i=0; i<samples_read; ++i){
            float sin_freq = (SDL_sinf((total_samples+i)*0.0001f)+1.0f)*0.01f;
            sin_val_persist += 3.1417f * 2.0f * sin_freq;
            if(sin_val_persist > 3.1417f){
                sin_val_persist -= 3.1417f*2.0f;
            }
        }
        total_samples += samples_read;
        float sin_val = sin_val_persist;
        for(int i=0; i<samples_needed; ++i){
            float sin_freq = (SDL_sinf((total_samples+i)*0.0001f)+1.0f)*0.01f;
            sin_val += 3.1417f * 2.0f * sin_freq;
            if(sin_val > 3.1417f){
                sin_val -= 3.1417f*2.0f;
            }
            src_buf[i] = SDL_sinf(sin_val) * 0.2f;
            src_buf[i] *= SDL_max(0.0f, 1.0f - (i/(float)samples_needed)*2.0f);
        }
        SDL_ConvertAudio(&cvt);
        SDL_memcpy(state.backup_stream_buf, cvt.buf, samples_needed * bytes_per_sample_dst);
        SDL_LockAudioDevice(audio_device_id);
        state.read_cursor -= old_read_cursor;
        Uint8* temp = state.stream_buf;
        state.stream_buf = state.backup_stream_buf;
        state.backup_stream_buf = temp;
        SDL_UnlockAudioDevice(audio_device_id);        
        SDL_free(cvt.buf);    
        SDL_Delay(100);
        Uint32 end_time = SDL_GetTicks();
        SDL_Log("Frame time: %d\n", end_time - start_time);
    }
    
    if(audio_device_id){
        SDL_CloseAudioDevice(audio_device_id);
    }
    SDL_free(state.stream_buf);
    SDL_free(state.backup_stream_buf);

	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();

    // do stuff
    return 0;
}