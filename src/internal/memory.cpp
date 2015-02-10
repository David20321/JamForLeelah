#include "internal/memory.h"
#include "platform_sdl/error.h"
#include <cstdio>
#include <cstdlib>

void* StackMemoryBlock::Alloc(int requested_size) {
	if(stack_pt + requested_size < size && stack_blocks < kMaxBlocks-2){
		++stack_blocks;
		stack_block_pts[stack_blocks] = stack_block_pts[stack_blocks-1] + requested_size;
		return (void*)((int)mem + stack_block_pts[stack_blocks]);
	} else {
		return NULL;
	}
}

void StackMemoryBlock::Free() {
	if(stack_blocks){
		--stack_blocks;
	} else {
		FormattedError("Memory stack underflow", "Calling Free() on StackMemoryBlock with no stack elements");
		exit(1);
	}
}

StackMemoryBlock::StackMemoryBlock(void* p_mem, int p_size)
	:mem(p_mem),
	 size(p_size),
	 stack_blocks(0)
{
	stack_block_pts[0] = 0;
}
