#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc++20-designator"
#pragma clang diagnostic ignored "-Wc99-extensions"

#include "arenas.c"
#include "stuff.c"

// for clearness sake I have not wrapped the following into a reusable macro

// but we could

typedef struct uint32_t_arena {
	uint32_t *data;
	uint32_t count;
	uint32_t cap;
} uint32_t_arena;

void push(uint32_t_arena *a, uint32_t el) {
	assert(a->count < a->cap);
	grow_mem((char*)(a->data + a->count), (char*)(a->data + (a->count + 1)));
	a->data[a->count++] = el;
	return;
}

uint32_t pop(uint32_t_arena *a) {
	assert(a->count > 0);
	a->count--;
	return a->data[a->count];
}

uint32_t_arena splitoff(Arena *parent, uint32_t cap) {
	uint32_t_arena child;
	child.data = (uint32_t*)align_forward((uintptr_t)parent->next, alignof(uint32_t)); 
	child.count = 0;
	child.cap = cap;
	char *end = (char*)(child.data + child.cap);

	split_mem(parent->next, end);
	parent->next = end;
	return child;
}

uint32_t_arena finish(Arena *parent, uint32_t_arena *child) {
	assert((uintptr_t)parent->start <= (uintptr_t)child->data);
	assert((uintptr_t)child->data <= (uintptr_t)parent->next);
	child->cap = child->count;
	parent->next = (char*)(child->data + child->count);
	return *child;
}

uint32_t get(uint32_t_arena a, uint32_t id) {
	return a.data[id];
}

// the core idea is that anything can be an arena as long as it uses split_mem and grow_mem

uint32_t_arena query_aabb(Arena *arena, Bvh *bvh, AABB aabb) {
	uint32_t_arena touched = splitoff(arena, bvh->leavesCount);
	uint32_t_arena stack = splitoff(arena, bvh->nodeCount);

	push(&stack, bvh->root);

	while (stack.count > 0) {
		uint32_t candidateId = pop(&stack);
		Node *candidate = bvh->nodes + candidateId;

		if (!aabb_intersects_aabb(candidate->aabb, aabb)) {
			continue;
		}

		if(is_leaf(candidate)) {
			push(&touched, candidate->identifier);
		}
		else {
			push(&stack, candidate->right);
			push(&stack, candidate->left);
		}
	}

	return finish(arena, &touched);
}

// looks pretty good!

uint32_t_arena find_collisions_for_entity(Arena *arena, Bvh *bvh, Entity *entities, uint32_t id) {
	Entity *entity = entities + id;
	uint32_t_arena collisions = splitoff(arena, bvh->leavesCount);

	uint32_t_arena touchingAABBs = query_aabb(arena, bvh, entity->ab);
	
	for (int j = 0; j < touchingAABBs.count; j++) {
		uint32_t mayCollideId = get(touchingAABBs, j);
			
		if (mayCollideId == id) {
			continue;
		}

		if (entity_collides(entities, id, mayCollideId)) {
			push(&collisions, mayCollideId);
		}
	}

	return finish(arena, &collisions);
}


void print_entity_collisions_arena(Entity *entities, uint32_t entitiesCount, uint32_t_arena *entityCollisions) {
	for (int i = 0; i < entitiesCount; i++) {
		Entity *entity = entities + i;

		printf("Entity %d at (%f %f %f) with radius %f collides with:\n\t", i, entity->position.x, entity->position.y, entity->position.z, entity->radius);
		for (int j = 0; j < entityCollisions[i].count; j++) {
			printf("%d, ", entityCollisions[i].data[j]);
		}
		printf("\n");
	}
}

int main(void) {
	Arena arena = arena_create(GB(32));
	
	// printinf this takes forever, but we can also handle many entities without problem
	uint32_t entityCount = 10240;
	Bvh bvh = init_bvh(&arena);
	Entity *entities = create_random_entities(&arena, &bvh, entityCount);
	
	Arena tempArena = split_arena(&arena, GB(4));
	
	uint32_t_arena *entityCollisions = alloc(&tempArena, entityCount, uint32_t_arena);
	
	for (int i = 0; i < entityCount; i++) {
		entityCollisions[i] = find_collisions_for_entity(&tempArena, &bvh, entities, i);
	}

	print_entity_collisions_arena(entities, entityCount, entityCollisions);

	// we eliminated drawbacks one by one
	// remaining are:
	// 1. we need to pass an arena to functions
	// -> create a mechanism to get an arena local to the thread or something
	//    I leave this as an exercise to the reader
	// 2. we have to have code for each type, which we can generate, potentially via macros
	//    but still then we have to use .data or get() and similar to access elements
	// the second, which I personally dont think of as too big of a problem. 
	// this is C after all.
	// but one thing we can do is mitigating this by hiding information behind the pointer similar to the stretchy buffers technique

	arena_clear(&tempArena);
	printf("done\n");
}


