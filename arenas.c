
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdalign.h>
#include <stdint.h>
#include <assert.h>

#define KB(n) ((uint64_t)0x400 * (uint64_t)(n))
#define MB(n) ((uint64_t)0x100000 * (uint64_t)(n)) 
#define GB(n) ((uint64_t)0x40000000 * (uint64_t)(n))

#define PAGE_SIZE KB(4)
#define COMMIT_SIZE (128 * KB(4))

// In this arena implementation memory is divided into COMMIT_SIZE large blocks

uintptr_t align_backward(uintptr_t ptr, size_t alignment) {	
	return ptr & ~(alignment-1);
}

uintptr_t align_forward(uintptr_t next, size_t alignment) {	
	return (next + (alignment-1)) & ~(alignment-1);
}

uintptr_t amount_until(uintptr_t ptr, size_t alignment) {
	return ((ptr + (alignment-1)) & ~(alignment-1)) - ptr;
}

// when the allocation straddles a COMMIT_SIZE large block, we need to allocate more pages
void grow_mem(char *prev, char *next) {
	uintptr_t prevBlockEnd = align_forward((uintptr_t)prev, COMMIT_SIZE);
	uintptr_t nextBlockEnd = align_forward((uintptr_t)next, COMMIT_SIZE);
	if (prevBlockEnd != nextBlockEnd) {
		VirtualAlloc((void*)prevBlockEnd, nextBlockEnd - prevBlockEnd, MEM_COMMIT, PAGE_READWRITE);
	}
}

// when we split memory we have one of three choices
// 1. make the start of the split arena be aligned to the next block;
// -> cant neatly align split arrays
// 2. make the end of the split arena be aligned to the next block
// -> all arenas take up minimum COMMIT_SIZE
// 3. have arbitrary sized arenas
// -> arenas can potentially start in the middle of an uncommited block
//    in which case need to commit the page the arena started in
// choice 3 is what we do, and for this case we have the split_mem function
void split_mem(char *from, char *start) {
	uintptr_t fromBlock = align_backward((uintptr_t)from, COMMIT_SIZE);
	uintptr_t startBlock = align_backward((uintptr_t)start, COMMIT_SIZE);
	if (fromBlock != startBlock) {
		VirtualAlloc(start, amount_until((uintptr_t)start, COMMIT_SIZE), MEM_COMMIT, PAGE_READWRITE);
	}
}

typedef struct Arena {
	char *start;
	char *next;
	char *end;
} Arena;

Arena arena_create(size_t size) {
	Arena arena;
	arena.start = (char*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);	
	arena.next = arena.start;
	arena.end = arena.start + size;

	split_mem(NULL, arena.start);
	return arena;
}

void* alloc_aligned(Arena *arena, size_t size, size_t alignment) {
	char *prev = arena->next;
	char *astart = (char*)align_forward((uintptr_t)arena->next, alignment);
	char *next = astart + size;
	assert((uintptr_t)next <= (uintptr_t)arena->end);
	grow_mem(prev, next);
	arena->next = next;
	return (void*)astart;
}

Arena split_arena_aligned(Arena *arena, size_t amount, size_t alignment) {
	char *prev = arena->next;
	prev = (char*)align_forward((uintptr_t)prev, alignment);
	char *next = prev + amount;	

	assert((uintptr_t)next <= (uintptr_t)arena->end);
	split_mem(arena->next, next);
	
	Arena split = {
		.start = prev,
		.next = prev,
		.end = next,
	};
	
	arena->next = next;
	return split;
}

Arena split_arena(Arena *arena, size_t amount) {
	char *prev = arena->next;
	char *next = prev + amount;	

	assert((uintptr_t)next <= (uintptr_t)arena->end);
	split_mem(prev, next);
	
	Arena split = {
		.start = prev,
		.next = prev,
		.end = next,
	};
	
	arena->next = next;
	return split;
}


void* alloc_aligned_and_zero(Arena *arena, size_t size, size_t alignment) {
	void *p = alloc_aligned(arena, size, alignment);
	memset(p, 0, size);
	return p;
}

void * arena_begin_aligned(Arena *arena, size_t alignment) {
	return (void*)align_forward((uintptr_t)arena->next, alignment);
}

#define begin_aligned(ARENA, TYPE) (TYPE*)arena_begin_aligned(ARENA, alignof(TYPE))

#define alloc(ARENA, COUNT, TYPE) (TYPE*)alloc_aligned(ARENA, sizeof(TYPE) * COUNT, alignof(TYPE))
#define zalloc(ARENA, COUNT, TYPE) (TYPE*)alloc_aligned_and_zero(ARENA, sizeof(TYPE) * COUNT, alignof(TYPE))
#define split_type(ARENA, COUNT, TYPE) split_arena_aligned(ARENA, COUNT * sizeof(TYPE), alignof(TYPE))


// decommit ommitted

void arena_shrink_to_pointer(Arena *arena, void *ptr) {
	assert((uintptr_t)arena->start <= (uintptr_t)ptr);
	assert((uintptr_t)ptr <= (uintptr_t)arena->end);
	arena->next = ptr;
	return;
}

#define finish_array(ARENA, PTR, COUNT) arena_shrink_to_pointer(ARENA, PTR + COUNT)
#define arena_free(ARENA, PTR) arena_shrink_to_pointer(ARENA, PTR)

void* arena_pop_size(Arena *arena, size_t popSize) {
	// this wont work with decommiting
	// in that case we have to use memcpy
	char *popped = arena->next - popSize;
	assert((uintptr_t)arena->start <= (uintptr_t)popped);
	arena->next = popped;
	return popped;
}

#define arena_push_type(ARENA, TYPE) (TYPE*)alloc_aligned(ARENA, sizeof(TYPE), alignof(TYPE))
#define arena_pop_type(ARENA, TYPE) (TYPE*)arena_pop_size(ARENA, sizeof(TYPE))

void arena_clear(Arena *arena) {
	arena->next = arena->start;
	return;
}

// NOTE;
// it would also be a good idea to keep track of the commited region to decrease the use of VirtualAlloc
// though then care must be taken when calling the "finish" set of functions, 
// Potentially split off arenas wont necessarily have all their pages committed, so when calling finish, the committed region must also be reset
// for brevity I have not included this here
