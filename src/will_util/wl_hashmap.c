#include "wl_hashmap.h"
#include "murmur3_32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hashmap_free(hashmap_t *map) {
	int i;
	for (i = 0; i < map->size; i++) {
		hashmap_entry_t *bucket = map->buckets[i];
		hashmap_entry_t *next;
		while (bucket) {
			next = bucket->next;
			free(bucket->key);
			free(bucket->data);
			free(bucket);
			bucket = next;
		}
	}
	free(map->buckets);
}

void hashmap_init(hashmap_t *map, uint32_t size, uint32_t hash_seed) {
	map->size = size * 2;
	map->hash_seed = hash_seed;
	// http://planetmath.org/goodhashtableprimes
	uint32_t primes[] = { 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
			49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469,
			12582917, 25165843, 50331653, 100663319, 201326611, 402653189,
			805306457, 1610612741 };
	int n = sizeof(primes) / sizeof(primes[0]);
	while (--n > 0) {
		if (map->size > primes[n - 1]) {
			map->size = primes[n];
			break;
		}
	}
	map->buckets = calloc(map->size, sizeof(hashmap_entry_t *));
	map->size_of_buckets = calloc(map->size, sizeof(uint32_t));
	printf("[HASHMAP] size: %d (%d)\n", map->size, size);
}

int hashmap_add(hashmap_t *map, const char *key, void *data) {
	hashmap_entry_t *next = malloc(sizeof(hashmap_entry_t));
	if (next == NULL) {
		printf("[ERROR] hashmap_add malloc\n");
		return 0;
	}
	next->key = key;
	next->data = data;
	next->next = NULL;

	uint32_t len = strlen(key);
	uint32_t hash = murmur3_32(key, len, map->hash_seed);
	uint32_t index = hash % map->size;
	if (strcmp(key, "perpetual") == 0)
		printf("hashmap_add %s %d %d %d %d\n", key, len, hash,
				murmur3_32(key, len, map->hash_seed), map->size);
	hashmap_entry_t *bucket = map->buckets[index];
	if (bucket) {
		while (bucket->next)
			bucket = bucket->next;
		bucket->next = next;
	} else {
		map->buckets[index] = next;
	}
	map->size_of_buckets[index]++;
	return 1;
}

hashmap_entry_t *hashmap_get(hashmap_t *map, const char *key) {
	if (map == NULL || key == NULL)
		return NULL;

	uint32_t len = strlen(key);
	uint32_t hash = murmur3_32(key, len, map->hash_seed);
	uint32_t index = hash % map->size;
	hashmap_entry_t *bucket = map->buckets[index];
	int bucket_size = map->size_of_buckets[index];
	printf("hashmap_get %s %d %d %d %d\n", key, len, hash,
			murmur3_32(key, len, map->hash_seed), map->size);
	int i, j = 0;
	for (i = 0; i < bucket_size; i++, bucket = bucket->next) {
		int l = strlen(bucket->key);
		while (j < l && j < len && bucket->key[j] == key[j])
			j++;
		if (j == l && j == len)
			return bucket;
		if (key[j] < bucket->key[j])
			break;
	}
	return NULL;
}
