#include <SDL.h>
#include <cstdlib>

SDL_AudioDeviceID g_audio_device_id;
SDL_AudioSpec g_audio_spec;

static void Update() {
    static int num_audio_devices = 0;
    int new_num_audio_devices = SDL_GetNumAudioDevices(0);
    if(num_audio_devices != new_num_audio_devices){
        for( int i=0; i<new_num_audio_devices; ++i ){
            SDL_Log("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
        }
        num_audio_devices = new_num_audio_devices;
    }
}

static void Draw() {
}


void MyAudioCallback(void *userdata, Uint8 * stream, int len) {
    int bytes_per_sample_dst = g_audio_spec.size / g_audio_spec.samples;
    int samples_needed = len / bytes_per_sample_dst;
    int bytes_per_sample_src = sizeof(float);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, AUDIO_F32, 1, g_audio_spec.freq, g_audio_spec.format, g_audio_spec.channels, g_audio_spec.freq);
    cvt.len = samples_needed * bytes_per_sample_src;
    cvt.buf = (Uint8 *) SDL_malloc(cvt.len * cvt.len_mult);
    float *src_buf = (float*)cvt.buf;
    static float sin_val = 0.0f;
    static Uint64 total_samples = 0;
    for(int i=0; i<samples_needed; ++i){
        ++total_samples;
        float sin_freq = (SDL_sinf(total_samples*0.0001f)+1.0f)*0.01f;
        sin_val += 3.1417f * 2.0f * sin_freq;
        if(sin_val > 3.1417f){
            sin_val -= 3.1417f*2.0f;
        }
        src_buf[i] = SDL_sinf(sin_val) * 0.2f;
    }
    SDL_ConvertAudio(&cvt);
    SDL_memcpy(stream, cvt.buf, len);
    SDL_free(cvt.buf);    
}
/*
#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x)      (!SDL_AUDIO_ISSIGNED(x))*/

static void InitAudio() {
    SDL_AudioSpec desired_audio_spec;

    SDL_zero(desired_audio_spec);
    desired_audio_spec.freq = 48000;
    desired_audio_spec.format = AUDIO_F32;
    desired_audio_spec.channels = 2;
    desired_audio_spec.samples = 4096;
    desired_audio_spec.callback = MyAudioCallback;  // you wrote this function elsewhere.

    g_audio_device_id = SDL_OpenAudioDevice(NULL, 0, &desired_audio_spec, &g_audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (g_audio_device_id == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s\n", SDL_GetError());
    } else {
        SDL_Log("Received audio format:\n");
        SDL_Log("Frequency: %d\n", g_audio_spec.freq);
        SDL_Log("Channels: %d\n", g_audio_spec.channels);
        SDL_Log("Buffer length: %d ms\n", g_audio_spec.samples * 1000 / g_audio_spec.channels / g_audio_spec.freq);
        bool is_float = SDL_AUDIO_ISFLOAT(g_audio_spec.format);
        int bit_size = SDL_AUDIO_BITSIZE(g_audio_spec.format);
        SDL_Log("Format: %s%d\n", is_float?"F":"I", bit_size);
        SDL_PauseAudioDevice(g_audio_device_id, 0);  // start audio playing.
    }
}

int initGame() {
    if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_VIDEO ) ){
		// SDL failed to initialize
		return 1;
	}	
	SDL_Window* window = SDL_CreateWindow("Game Window",       
									 	  SDL_WINDOWPOS_UNDEFINED,
									 	  SDL_WINDOWPOS_UNDEFINED,
									 	  640,                    
									 	  480,                    
									 	  SDL_WINDOW_OPENGL);
    
	if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not create window\n");
		return 1;
    }

    InitAudio();    

    bool game_running = true;
    while (game_running) {
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
    }
    
    if(g_audio_device_id){
        SDL_CloseAudioDevice(g_audio_device_id);
    }

	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();

    // do stuff
    return 0;
}