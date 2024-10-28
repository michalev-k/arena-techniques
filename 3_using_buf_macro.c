#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc++20-designator"
#pragma clang diagnostic ignored "-Wc99-extensions"

#include "arenas.c"
#include "stuff.c"

#define define_buf(TYPE) typedef struct TYPE##_buffer { TYPE *data; uint32_t count; } TYPE##_buffer
#define buf(TYPE) TYPE##_buffer
#define buf_alloc(ARENA, COUNT, TYPE) (struct TYPE##_buffer){ .data = (TYPE*)alloc_aligned(ARENA, COUNT * sizeof(TYPE), alignof(TYPE)), .count = 0 }
#define buf_push(BUFFER, EL) (BUFFER).data[(BUFFER).count++] = EL
#define buf_pop(BUFFER) (BUFFER).data[--(BUFFER).count]
#define get(BUFFER, ID) ((BUFFER).data[ID])
#define len(BUFFER) ((BUFFER).count)
#define buf_finish(ARENA, BUFFER) (finish_array(ARENA, BUFFER.data, BUFFER.count), (BUFFER))

// with buffer-macros we need to define the types we use them for upfront
// this may seem unfortunate for some, but this is C
// a good version of this would just use code generation which also produces a generic macro

define_buf(uint32_t);
define_buf(uint32_t_buffer);
define_buf(Entity);

buf(uint32_t) query_aabb(Arena *arena, Bvh *bvh, AABB aabb) {
	buf(uint32_t) touched = buf_alloc(arena, bvh->leavesCount, uint32_t);
	buf(uint32_t) nodeStack = buf_alloc(arena, bvh->nodeCount, uint32_t);

	buf_push(nodeStack, bvh->root);

	while (nodeStack.count > 0) {
		uint32_t candidateId = buf_pop(nodeStack);
		Node *candidate = bvh->nodes + candidateId;

		if (!aabb_intersects_aabb(candidate->aabb, aabb)) {
			continue;
		}

		if(is_leaf(candidate)) {
			buf_push(touched, candidate->identifier);
		}
		else {
			buf_push(nodeStack, candidate->right);
			buf_push(nodeStack, candidate->left);
		}
	}
	
	return buf_finish(arena, touched);
}

buf(uint32_t) find_collisions_for_entity(Arena *arena, Bvh *bvh, buf(Entity) entities, uint32_t id) {
	Entity *entity = &get(entities, id);
	buf(uint32_t) collisions = buf_alloc(arena, len(entities), uint32_t);
	
	buf(uint32_t) touchingAABBs = query_aabb(arena, bvh, entity->ab);

	for (int j = 0; j < len(touchingAABBs); j++) {
		uint32_t mayCollideId = get(touchingAABBs, j);
			
		if (mayCollideId == id) {
			continue;
		}

		if (entity_collides(entities.data, id, mayCollideId)) {
			buf_push(collisions, mayCollideId);
		}
	}

	// we can call finish array	
	return buf_finish(arena, collisions);
}

void print_entity_collisions_buf(buf(Entity) entities, buf(uint32_t_buffer) entityCollisions) {

	for (int i = 0; i < entities.count; i++) {
		Entity *entity = &get(entities, i);

		printf("Entity %d at (%f %f %f) with radius %f collides with:\n\t", i, entity->position.x, entity->position.y, entity->position.z, entity->radius);
		for (int j = 0; j < len(get(entityCollisions, i)); j++) {
			printf("%d, ", get(get(entityCollisions, i), j));
		}
		printf("\n");
	}
}

// this version is like the previous one, but it uses a buffer macro to make it so that we dont need to pass around counts

int main(void) {
	Arena arena = arena_create(GB(32));
	
	Bvh bvh = init_bvh(&arena);
	buf(Entity) entities;
	entities.data = create_random_entities(&arena, &bvh, 32);
	entities.count = 32;
	
	Arena tempArena = split_arena(&arena, GB(4));

	// if we use the generated name, we can even do buffers of buffers
	buf(uint32_t_buffer) entityCollisions = buf_alloc(&tempArena, entities.count, uint32_t_buffer);
	
	for (int i = 0; i < len(entities); i++) {
		entityCollisions.data[i] = find_collisions_for_entity(&tempArena, &bvh, entities, i);
	}

	// finally we do something with the gathered collisions
	print_entity_collisions_buf(entities, entityCollisions);

	// this version may seem a bit nicer to some
	// debugging may become worse with macro-overuse, so if you are worried about that, dedicated code-generation is recommended

	// this still has the problem that we are worst-case commiting
	// again, becuase the memory is most likely going to be used by what happens next, it may not turn out to be a problem
	// but we do want to handle the case where we there is no good upper limit, or where the worst case is really big

	// well, we never had to use one single arena.

	arena_clear(&tempArena);
	printf("done\n");
}


