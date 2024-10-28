#include <float.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

// stuff for constructing the bvh

float f_min(float a, float b) {
	return (a <= b) ? a : b;
}

float f_max(float a, float b) {
	return (a > b) ? a : b;
}

float square(float a) {
	return a * a;
}

typedef struct Vector {
	float x, y, z;
} Vector;

typedef struct AABB {
	Vector min;
	Vector max;
} AABB;

Vector add(Vector u, Vector v) {
	u.x += v.x;
	u.y += v.y;
	u.z += v.z;
	return u;
}

Vector sub(Vector u, Vector v) {
	u.x -= v.x;
	u.y -= v.y;
	u.z -= v.z;
	return u;
}

Vector mulf(Vector v, float t) {
	v.x *= t;
	v.y *= t;
	v.z *= t;
	return v;
}

Vector addf(Vector v, float f) {
	v.x += f;
	v.y += f;
	v.z += f;
	return v;
}

Vector subf(Vector v, float f) {
	v.x -= f;
	v.y -= f;
	v.z -= f;
	return v;
}

float squarelen(Vector r) {
	return square(r.x) + square(r.y) + square(r.z);
}

float random_float(void) {
	return (float)rand()/(float)(RAND_MAX);
}

Vector random_vector(void) {
	float rfx = random_float();
	float rfy = random_float();
	float rfz = random_float();

	rfx -= 0.5f;
	rfy -= 0.5f;
	rfz -= 0.5f;

	Vector v = {rfx, rfy, rfz};
	return v;
}

bool aabb_contains(AABB o, AABB i) {
	if (i.max.x > o.max.x || i.max.y > o.max.y || i.max.z > o.max.z) return false;
	if (i.min.x < o.min.x || i.min.y < o.min.y || i.min.z < o.min.z) return false;
	return true;
}

bool aabb_intersects_aabb(AABB a, AABB b) {
	if (a.max.x < b.min.x) return false;
	if (b.max.x < a.min.x) return false;

	if (a.max.y < b.min.y) return false;
	if (b.max.y < a.min.y) return false;

	if (a.max.z < b.min.z) return false;
	if (b.max.z < a.min.z) return false;

	return true;
}

AABB aabb_merge(AABB a, AABB b) {
	Vector max;
	Vector min;

	max.x = f_max(a.max.x, b.max.x);
	max.y = f_max(a.max.y, b.max.y);
	max.z = f_max(a.max.z, b.max.z);

	min.x = f_min(a.min.x, b.min.x);
	min.y = f_min(a.min.y, b.min.y);
	min.z = f_min(a.min.z, b.min.z);

	return (AABB){min, max};
}

float aabb_surface_area(AABB aabb) {
	float surface;
	Vector ex = sub(aabb.max, aabb.min);

	surface =  ex.x * ex.y * 2.0f;
	surface += ex.x * ex.z * 2.0f;
	surface += ex.y * ex.z * 2.0f;
	
	return surface;
}


typedef struct Node {
	AABB aabb;
	uint32_t parent;
	uint32_t left;
	uint32_t right;
	uint32_t identifier;
} Node;

typedef struct Bvh {
	Arena arena;
	Node *nodes;
	uint32_t nodeCount;
	uint32_t root;
	uint32_t leavesCount;

	// please ignore for now
	Arena nodeStack;
} Bvh;

bool is_leaf(Node *node) {
	return (node->left == 0 || node->right == 0);
}

void fix_upwards(Bvh *p, uint32_t nodeId) {
	while (nodeId != 0) {
		Node *node = p->nodes + nodeId;
		Node *left = p->nodes + node->left;
		Node *right = p->nodes + node->right;
		node->aabb = aabb_merge(left->aabb, right->aabb);
		nodeId = node->parent;
	}
}

uint32_t find_sibling_for_aabb(Bvh *b, AABB aabb) {
	assert(b->root != 0);
	Node *nodes = b->nodes;

	uint32_t currentId = b->root;
	
	while (1) {
		Node *current = nodes + currentId;

		if (is_leaf(current)) {
			break;
		}
		
		Node *left = nodes + current->left;
		Node *right = nodes + current->right;

		float newParentNodeCost = aabb_surface_area(aabb_merge(aabb, current->aabb));
		float minimumPushdownCost = 2.0f * (newParentNodeCost - aabb_surface_area(current->aabb));

		float costLeft = minimumPushdownCost + aabb_surface_area(aabb_merge(aabb, left->aabb));
		float costRight = minimumPushdownCost + aabb_surface_area(aabb_merge(aabb, right->aabb));

		if (!is_leaf(left)) {
			costLeft -=  aabb_surface_area(left->aabb);
		}

		if (!is_leaf(right)) {
			costRight -=  aabb_surface_area(right->aabb);
		}

		if (newParentNodeCost < costLeft && newParentNodeCost < costRight) {
			break;
		}

		if (costLeft < costRight) {
			currentId = current->left;
		} else {
			currentId = current->right;
		}
	}

	return currentId;
}

uint32_t push_node(Bvh *b) {
	alloc(&b->arena, 1, Node);
	return b->nodeCount++;
}

uint32_t insert_node(Bvh *b, uint32_t identifier, AABB aabb) {
	b->leavesCount++;
	Node *nodes = b->nodes;
	Arena *arena = &b->arena;

	uint32_t nodeId = push_node(b);
	Node *node = nodes + nodeId;
	*node = (Node){ .aabb = aabb, .identifier = identifier };
	
	if (b->root == 0) {
		b->root = nodeId;
		return nodeId;
	}

	uint32_t siblingId = find_sibling_for_aabb(b, aabb);
	Node *sibling = nodes + siblingId;

	uint32_t siblingParentId = sibling->parent; 
	Node *siblingParent = nodes + siblingParentId;

	uint32_t newParentId = push_node(b);
	Node *newParent = nodes + newParentId;

	*newParent = (Node){
		.aabb = aabb_merge(aabb, sibling->aabb),
		.parent = siblingParentId,
		.left = siblingId,
		.right = nodeId
	};

	node->parent = newParentId;
	sibling->parent = newParentId;

	if (siblingParentId == 0) {
		// sibling was root
		b->root = newParentId;
	}
	else if (siblingParent->left == siblingId) {
		siblingParent->left = newParentId;
	}
	else {
		siblingParent->right = newParentId;
	}

	fix_upwards(b, newParentId);
	return nodeId;
}

Bvh init_bvh(Arena *arena) {

	Bvh bvh = {0};
	{
		bvh.arena = split_arena(arena, GB(2));
		bvh.nodeStack = split_arena(arena, GB(2));
		bvh.nodes = (Node*)bvh.arena.start;
		
		// NULL node
		bvh.nodeCount = 1;
		zalloc(&bvh.arena, 1, Node);
	}

	return bvh;
}

// entities are just circles
typedef struct Entity {
	Vector position;
	float radius;
	AABB ab;
} Entity;

bool entity_collides(Entity *entities, uint32_t a, uint32_t b) {
	Entity *e = entities + a;
	Entity *f = entities + b;

	float distance = squarelen(sub(e->position, f->position));
	return distance < square(e->radius + f->radius);
}

void print_entity_collisions(Entity *entities, uint32_t entitiesCount, uint32_t **entityCollisions, uint32_t *entityCollisionsCount) {
	for (int i = 0; i < entitiesCount; i++) {
		Entity *entity = entities + i;

		printf("Entity %d at (%f %f %f) with radius %f collides with:\n\t", i, entity->position.x, entity->position.y, entity->position.z, entity->radius);
		for (int j = 0; j < entityCollisionsCount[i]; j++) {
			printf("%d, ", entityCollisions[i][j]);
		}
		printf("\n");
	}
}

Entity *create_random_entities(Arena *arena, Bvh *bvh, uint32_t entityCount) {
	Entity *entities = alloc(arena, entityCount, Entity);

	for (int i = 0; i < entityCount; i++) {
		Vector position = random_vector();
		float radius = random_float() * 0.3f;
		
		AABB ab = { subf(position, radius), addf(position, radius) };
		uint32_t nodeId = insert_node(bvh, i, ab);

		entities[i] = (Entity){
			.position = position,
			.radius = radius,
			.ab = ab,
		};
	}

	return entities;
}
