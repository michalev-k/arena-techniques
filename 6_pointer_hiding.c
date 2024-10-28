// gonna use a few extensions to make everything neater
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wgnu-alignof-expression"

#include "arenas.c"
#include "stuff.c"

typedef struct HiddenArena {
	char *next;
	char *end;
} HiddenArena;

HiddenArena *get_hidden_arena_from_pointer(void *ptr) {
	return (HiddenArena*)((uintptr_t)ptr - sizeof(HiddenArena));
}

void* push_hidden_arena(void *ptr, size_t size, size_t alignment) {
	HiddenArena *hidden = get_hidden_arena_from_pointer(ptr);

	char *prev = hidden->next;
	char *astart = (char*)align_forward((uintptr_t)prev, alignment);
	char *next = astart + size;
	assert((uintptr_t)next <= (uintptr_t)hidden->end);
	grow_mem(prev, next);
	hidden->next = next;
	return (void*)astart;
}

void* pop_hidden_arena(void *ptr, size_t size) {
	HiddenArena *hidden = get_hidden_arena_from_pointer(ptr);

	char *popped = hidden->next - size;
	assert((uintptr_t)ptr <= (uintptr_t)popped);
	hidden->next = popped;
	return popped;
}

void* splitoff_hidden_arena(Arena *parent, size_t size, size_t alignment) {
	char *ptr = (char*)align_forward((uintptr_t)parent->next + sizeof(HiddenArena), alignment);
	HiddenArena *hidden = get_hidden_arena_from_pointer(ptr);
	hidden->next = ptr;
	hidden->end = ptr + size;

	grow_mem(parent->next, ptr);
	split_mem(parent->next, hidden->end);
	parent->next = hidden->end;
	return ptr;
}

void* finish_hidden_arena(Arena *parent, void *ptr) {
	HiddenArena *hidden = get_hidden_arena_from_pointer(ptr);
	parent->next = hidden->next;
	return ptr;
}

void* next_hidden_arena(void *ptr) {
	HiddenArena *hidden = get_hidden_arena_from_pointer(ptr);
	return hidden->next;
}

#define push(BUFFER, EL) do { *(typeof(BUFFER))(push_hidden_arena(BUFFER, sizeof(*(BUFFER)), alignof(*(BUFFER)))) = EL; } while(0) 
#define pop(BUFFER) *(typeof(BUFFER))(pop_hidden_arena(BUFFER, sizeof(*BUFFER)))
#define splitoff(PARENT, COUNT, TYPE) splitoff_hidden_arena(PARENT, COUNT * sizeof(TYPE), alignof(TYPE));
#define finish(PARENT, BUFFER) (typeof(BUFFER))finish_hidden_arena(PARENT, BUFFER)
#define len(BUFFER) (uintptr_t)(((typeof(BUFFER))next_hidden_arena(BUFFER)) - BUFFER)


// just for fun we use a global arena for this
Arena *arena;

uint32_t* query_aabb(Bvh *bvh, AABB aabb) {
	uint32_t *touched = splitoff(arena, bvh->leavesCount, uint32_t);
	uint32_t *stack = splitoff(arena, bvh->nodeCount, uint32_t);

	push(stack, bvh->root);

	while (len(stack) > 0) {
		uint32_t candidateId = pop(stack);
		Node *candidate = bvh->nodes + candidateId;

		if (!aabb_intersects_aabb(candidate->aabb, aabb)) {
			continue;
		}

		if(is_leaf(candidate)) {
			push(touched, candidate->identifier);
		}
		else {
			push(stack, candidate->right);
			push(stack, candidate->left);
		}
	}

	return finish(arena, touched);
}

uint32_t* find_collisions_for_entity(Bvh *bvh, Entity *entities, uint32_t id) {
	Entity *entity = entities + id;
	uint32_t *collisions = splitoff(arena, bvh->leavesCount, uint32_t);

	uint32_t *touchingAABBs = query_aabb(bvh, entity->ab);
	
	for (int j = 0; j < len(touchingAABBs); j++) {
		uint32_t mayCollideId = touchingAABBs[j];
			
		if (mayCollideId == id) {
			continue;
		}

		if (entity_collides(entities, id, mayCollideId)) {
			push(collisions, mayCollideId);
		}
	}

	return finish(arena, collisions);
}

void print_entity_collisions_arena(Entity *entities, uint32_t entitiesCount, uint32_t **entityCollisions) {
	for (int i = 0; i < entitiesCount; i++) {
		Entity *entity = entities + i;

		printf("Entity %d at (%f %f %f) with radius %f collides with:\n\t", i, entity->position.x, entity->position.y, entity->position.z, entity->radius);
		for (int j = 0; j < len(entityCollisions[i]); j++) {
			printf("%d, ", entityCollisions[i][j]);
		}
		printf("\n");
	}
}

int main(void) {
	// how about reserving a terabyte this time
	// works on my machine
	Arena memory = arena_create(GB(1024));
	arena = &memory;

	uint32_t entityCount = 32;
	Bvh bvh = init_bvh(arena);
	Entity *entities = create_random_entities(arena, &bvh, entityCount);
	
	uint32_t **entityCollisions = splitoff(arena, entityCount, uint32_t*);
	
	for (int i = 0; i < entityCount; i++) {
		entityCollisions[i] = find_collisions_for_entity(&bvh, entities, i);
	}

	print_entity_collisions_arena(entities, entityCount, entityCollisions);

	// this ones usage code is pretty cool and we can use the arrays like arrays
	// it does store extra information into the arena though and debugging will become worse
	// so I would probably recommend the code-gen version maybe paired with generic macros
	// but some people might enjoy this version too

	// now, as we see, we can do pretty much anything we want with arenas
	
	// but for computational tasks, stack management is pretty good actually
	// for long lived memory we might want some additional management

	// some more you could do:

	// we could store a reference to the parent arena in the hidden arena, and when we grow but dont have enough capacity, migrate the arena.

	// we could add a free-list so that we can reuse memory that became unneeded
	// this free-list might also be block based to allow for freeing of memory-ranges 

	// or really do any other allocation scheme on top of arenas.
	// but what would be the point of doing this inside of an arena? ->
	// we still have our grouped lifetimes, which we can use as values in our programming language.
	// we can pass them around, and then free them all together
	// we also prevent a lot of error cases related to memory management

	// maybe another time I will show how to program these.

	arena_clear(arena);
	printf("nice\n");
}


