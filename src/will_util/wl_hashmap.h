#ifndef WL_HASHMAP_H
#define WL_HASHMAP_H

#include <stdint.h>

typedef struct hashmap_entry {
	char *key;
	void *data;
	struct hashmap_entry *next;
} hashmap_entry_t;

typedef struct {
	uint32_t hash_seed;
	uint32_t size;
	hashmap_entry_t **buckets;
	uint32_t *size_of_buckets;
} hashmap_t;

void hashmap_free(hashmap_t *map);
void hashmap_init(hashmap_t *map, uint32_t size, uint32_t hash_seed);
int hashmap_add(hashmap_t *map, const char *key, void *data);
hashmap_entry_t *hashmap_get(hashmap_t *map, const char *key);

#endif
