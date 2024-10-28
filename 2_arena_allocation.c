#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc++20-designator"
#pragma clang diagnostic ignored "-Wc99-extensions"

#include "arenas.c"
#include "stuff.c"

uint32_t* query_aabb(Arena *arena, Bvh *bvh, AABB aabb, uint32_t *touchedCountOut) {
	// query aabb can just cheaply allocate the memory it needs
	uint32_t *touched = alloc(arena, bvh->leavesCount, uint32_t);
	uint32_t *nodeStack = alloc(arena, bvh->nodeCount, uint32_t);

	uint32_t touchedCount = 0;
	uint32_t stackCount = 1;
	nodeStack[0] = bvh->root;

	while (stackCount > 0) {
		uint32_t candidateId = nodeStack[--stackCount];
		Node *candidate = bvh->nodes + candidateId;		

		if (!aabb_intersects_aabb(candidate->aabb, aabb)) {
			continue;
		}

		if(is_leaf(candidate)) {
			touched[touchedCount++] = candidate->identifier;
		}
		else {
			nodeStack[stackCount++] = candidate->right;
			nodeStack[stackCount++] = candidate->left;
		}
	}

	*touchedCountOut = touchedCount;

	// when we are done, we can shrink the arena to only include the array with a function like this
	// the next allocation in this arena would start at the end of the array
	finish_array(arena, touched, touchedCount);
	return touched;
}


uint32_t* find_collisions_for_entity(Arena *arena, Bvh *bvh, Entity *entities, uint32_t id, uint32_t *collisionsCountOut) {
	Entity *entity = entities + id;
	
	// again, the function can take care of its own memory management
	uint32_t *collisions = alloc(arena, bvh->leavesCount, uint32_t);

	uint32_t touchingAABBsCount = 0;
	uint32_t *touchingAABBs = query_aabb(arena, bvh, entity->ab, &touchingAABBsCount);

	uint32_t collisionsCount = 0;
	for (int j = 0; j < touchingAABBsCount; j++) {
		uint32_t mayCollideId = touchingAABBs[j];
			
		if (mayCollideId == id) {
			continue;
		}

		if (entity_collides(entities, id, mayCollideId)) {
			collisions[collisionsCount++] = mayCollideId;
		}
	}

	*collisionsCountOut = collisionsCount;
	
	// we can call finish array
	finish_array(arena, collisions, collisionsCount);
	return collisions;
}


int main(void) {
	Arena arena = arena_create(GB(32));
	
	uint32_t entityCount = 32;
	Bvh bvh = init_bvh(&arena);
	Entity *entities = create_random_entities(&arena, &bvh, entityCount);
	
	Arena tempArena = split_arena(&arena, GB(4));

	uint32_t **entityCollisions = alloc(&tempArena, entityCount, uint32_t*);
	uint32_t *entityCollisionsCounts = alloc(&tempArena, entityCount, uint32_t);

	// this version passes arenas instead
	
	for (int i = 0; i < entityCount; i++) {
		// we need to only pass an arena to this function
		entityCollisions[i] = find_collisions_for_entity(&tempArena, &bvh, entities, i, entityCollisionsCounts + i);

		// in the case where we handle the collisions here, and dont need the memory anymore, 
		// we can call the funcion "arena_free" on the returned pointer to make the space available again
	}

	print_entity_collisions(entities, entityCount, entityCollisions, entityCollisionsCounts);

	// this version has the collisions neatly aligned and unneeded memory is removed
	// the downside is that we potentially commit a lot of memory that we are unlikely to actually need
	// globally seen this is not much of a problem as we are likely to reuse that memory anyhow
	// but there may be cases were always commiting for the worst-case is too costly

	arena_clear(&tempArena);
	printf("done\n");
}


