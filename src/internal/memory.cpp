#include "internal/memory.h"
#include "platform_sdl/error.h"
#include <cstdio>
#include <cstdlib>
#include "SDL.h"

void* StackAllocator::Alloc(int requested_size) {
    if(stack_block_pts[stack_blocks] + requested_size < size && stack_blocks < kMaxBlocks-2){
        ++stack_blocks;
        stack_block_pts[stack_blocks] = stack_block_pts[stack_blocks-1] + requested_size;
        return (void*)((int)mem + stack_block_pts[stack_blocks-1]);
    } else {
        return NULL;
    }
}

void StackAllocator::Free(void* ptr) {
    if(stack_blocks){
        --stack_blocks;
        SDL_assert(ptr == (void*)((int)mem + stack_block_pts[stack_blocks]));
    } else {
        FormattedError("Memory stack underflow", "Calling Free() on StackMemoryBlock with no stack elements");
        exit(1);
    }
}

void StackAllocator::Init(void* p_mem, int p_size) {
    stack_block_pts[0] = 0;
    mem = p_mem;
    size = p_size;
    stack_blocks = 0;
}
