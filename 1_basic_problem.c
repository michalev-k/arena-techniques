#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc++20-designator"
#pragma clang diagnostic ignored "-Wc99-extensions"

#include "arenas.c"
#include "stuff.c"

void query_aabb(Bvh *bvh, AABB aabb, uint32_t *touchedCountOut, uint32_t *touched, uint32_t *nodeStack) {
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
	return;
}

void find_collisions_for_entity(Bvh *bvh, Entity *entities, uint32_t id, uint32_t *collisions, uint32_t *collisionsCountOut, uint32_t *touchingAABBs, uint32_t *tempNodeStack) {
	Entity *entity = entities + id;

	uint32_t touchingAABBsCount = 0;
	query_aabb(bvh, entity->ab, &touchingAABBsCount, touchingAABBs, tempNodeStack);

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
	return;
}


int main(void) {
	Arena arena = arena_create(GB(32));
	
	uint32_t entityCount = 32;
	Bvh bvh = init_bvh(&arena);
	Entity *entities = create_random_entities(&arena, &bvh, entityCount);
	
	Arena tempArena = split_arena(&arena, GB(4));

	// this version uses statically sized buffers for everything
	// the functions are oblivious to the allocator
	
	// want to find collisions for all entities
	uint32_t **entityCollisions = alloc(&tempArena, entityCount, uint32_t*);
	uint32_t *entityCollisionsCounts = alloc(&tempArena, entityCount, uint32_t);
	
	// need to provide this memory...
	// and need to know the appropriate size here too...
	uint32_t *touchingAABBs = alloc(&tempArena, entityCount, uint32_t);
	
	// not smart enough to know that the upper limit of this is like 2 times the tree-height or something like that
	uint32_t *tempNodeStack = alloc(&tempArena, bvh.nodeCount, uint32_t); 
	
	for (int i = 0; i < entityCount; i++) {
		// we need to size this array for the worst case or alternatively provide a maximum and accept that we could drop collisions
		// for a game dropping collisions might be ok, but for a lot of software it isnt.
		// as we will see later, we dont have to make this tradeoff AND make our code easier to use
		entityCollisions[i] = alloc(&tempArena, entityCount, uint32_t);

		// we need to pass the buffers to the functions
		find_collisions_for_entity(&bvh, entities, i, entityCollisions[i], entityCollisionsCounts+i, touchingAABBs, tempNodeStack);
	}

	// finally we do something with the gathered collisions
	print_entity_collisions(entities, entityCount, entityCollisions, entityCollisionsCounts);

	arena_clear(&tempArena);
	printf("done\n");
}


