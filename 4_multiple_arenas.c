#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wc++20-designator"
#pragma clang diagnostic ignored "-Wc99-extensions"

#include "arenas.c"
#include "stuff.c"

// here we will use multiple arenas to allow our arrays to use memory proportional to what they need
// this can be useful in two cases:
// 1 The _theoretical_ worst case allocation amount is very large, but we are unlikely to hit that case
// -> but we can still support this case easily without doing anything special. 
//    Our program will just consume a lot of memory, potentially hitting swap-space.

// 2 The worst case is unknown, maybe due to external factors, or us just not caring enough to figure it out
// -> by providing a lot of address space we can handle these cases too and let memory grow up to a set limit

// to make things clear, we go back to not using the macro version, and write stuff out
// but we could provide macros or codegen to make all of this easier to use.
// Or do something entirely else that I will show later

// At the risk of making things seem more complicated than they are;
// To showcase them, I make use of three patterns in this code.
// 1. pushing onto arena.
// 2. dedicated arena.
// 3. arena splitting
// 

uint32_t* query_aabb(Arena *arena, Bvh *bvh, AABB aabb, uint32_t *touchedCountOut) {
	// here, we push items onto an arena one by one, using the passed arena like a stack
	// this means throughout we cant use the arena to allocate something else
	uint32_t *touched = begin_aligned(arena, uint32_t);
	uint32_t touchedCount = 0;

	// here is pattern 2; dedicated arena.
	// we are going to reuse this one anyway, so might keep one around
	Arena *stackArena = &bvh->nodeStack;
	uint32_t *stack = (uint32_t*)stackArena->start;
	*(arena_push_type(stackArena, uint32_t)) = bvh->root;

	uint32_t stackCount = 1;

	while (stackCount-- > 0) {
		uint32_t candidateId = *(arena_pop_type(stackArena, uint32_t));
		Node *candidate = bvh->nodes + candidateId;

		if (!aabb_intersects_aabb(candidate->aabb, aabb)) {
			continue;
		}

		if(is_leaf(candidate)) {
			touchedCount++;
			*(arena_push_type(arena, uint32_t)) = candidate->identifier;
		}
		else {
			stackCount += 2;
			*(arena_push_type(stackArena, uint32_t)) = candidate->right;
			*(arena_push_type(stackArena, uint32_t)) = candidate->left;
		}
	}

	*touchedCountOut = touchedCount;
	return touched;
}


uint32_t* find_collisions_for_entity(Arena *arena, Bvh *bvh, Entity *entities, uint32_t id, uint32_t *collisionsCountOut) {
	Entity *entity = entities + id;

	// Here we make use of the last pattern; arena splitting
	// given an arena, we split off a new arena with a given size; 
	// It does its own memory management, and the parent-arena can be used for allocations further down the line
	// This is the most flexible use of arenas, and we could have used the same technique for the above allocations
	// Only the dedicated arena has the advantage that it grows to the needed size and then doesnt need further commits in this case
	Arena collisionsArena = split_type(arena, bvh->leavesCount, uint32_t);
	uint32_t *collisions = (uint32_t*)collisionsArena.start;
	uint32_t collisionsCount = 0;
	
	uint32_t touchingAABBsCount = 0;
	uint32_t *touchingAABBs = query_aabb(arena, bvh, entity->ab, &touchingAABBsCount);

	for (int j = 0; j < touchingAABBsCount; j++) {
		uint32_t mayCollideId = touchingAABBs[j];
			
		if (mayCollideId == id) {
			continue;
		}

		if (entity_collides(entities, id, mayCollideId)) {			
			collisionsCount++;
			*(arena_push_type(&collisionsArena, uint32_t)) = mayCollideId;
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
	
	// want to find collisions for all entities
	uint32_t **entityCollisions = alloc(&tempArena, entityCount, uint32_t*);
	uint32_t *entityCollisionsCounts = alloc(&tempArena, entityCount, uint32_t);
	
	for (int i = 0; i < entityCount; i++) {
		// we need to only pass an arena to this function
		entityCollisions[i] = find_collisions_for_entity(&tempArena, &bvh, entities, i, entityCollisionsCounts + i);

		// in the case where we handle the collisions here, and dont need the memory anymore, 
		// we can call the funcion "arena_free" on the returned pointer to make the space available again
	}

	// finally we do something with the gathered collisions
	print_entity_collisions(entities, entityCount, entityCollisions, entityCollisionsCounts);

	// this version only uses memory proportionally to its needs
	// we also didnt need to pass multiple arenas up the callstack thanks to arena splitting
	
	// The code is a bit clumsy though. With some clever macros this might get a bit better or not.
	// But it does seem that we manage the same information manually multiple times.
	// memory-pointer <-> arena.start
	// array-count <-> arena.next - arena.start

	// we use this as an idea for the next version

	arena_clear(&tempArena);
	printf("done\n");
}


