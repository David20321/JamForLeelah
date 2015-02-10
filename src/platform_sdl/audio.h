#pragma once
#ifndef PLATFORM_SDL_AUDIO_HPP
#define PLATFORM_SDL_AUDIO_HPP

#include <SDL.h>

class StackMemoryBlock;

struct AudioContext {
	SDL_AudioDeviceID device_id;
	SDL_AudioSpec audio_spec;
	void* curr_buffer;
	void* back_buffer;
	int buffer_read_byte;
	int buffer_size;
};

void UpdateAudio(AudioContext* audio_context);
void InitAudio(AudioContext* context, StackMemoryBlock *stack_memory_block);

#endif