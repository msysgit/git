/*
 * Generic implementation of hash-based key value mappings.
 */
#include "cache.h"
#include "hashmap.h"

#define FNV32_BASE ((unsigned int) 0x811c9dc5)
#define FNV32_PRIME ((unsigned int) 0x01000193)

unsigned int strhash(const char *str)
{
	unsigned int c, hash = FNV32_BASE;
	while ((c = (unsigned char) *str++))
		hash = (hash * FNV32_PRIME) ^ c;
	return hash;
}

unsigned int strihash(const char *str)
{
	unsigned int c, hash = FNV32_BASE;
	while ((c = (unsigned char) *str++)) {
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		hash = (hash * FNV32_PRIME) ^ c;
	}
	return hash;
}

unsigned int memhash(const void *buf, size_t len)
{
	unsigned int hash = FNV32_BASE;
	unsigned char *ucbuf = (unsigned char*) buf;
	while (len--) {
		unsigned int c = *ucbuf++;
		hash = (hash * FNV32_PRIME) ^ c;
	}
	return hash;
}

unsigned int memihash(const void *buf, size_t len)
{
	unsigned int hash = FNV32_BASE;
	unsigned char *ucbuf = (unsigned char*) buf;
	while (len--) {
		unsigned int c = *ucbuf++;
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		hash = (hash * FNV32_PRIME) ^ c;
	}
	return hash;
}

#define HASHMAP_INITIAL_SIZE 64
/* grow / shrink by 2^2 */
#define HASHMAP_GROW 2
/* grow if > 80% full (to 20%) */
#define HASHMAP_GROW_AT 1.25
/* shrink if < 16.6% full (to 66.6%) */
#define HASHMAP_SHRINK_AT 6

static inline int entry_equals(const hashmap *map, const hashmap_entry *e1,
		const hashmap_entry *e2)
{
	return (e1 == e2) || (e1->hash == e2->hash && !(*map->cmpfn)(e1, e2));
}

static inline unsigned int bucket(const hashmap *map, const hashmap_entry *key)
{
	return key->hash & (map->tablesize - 1);
}

static void rehash(hashmap *map, unsigned int newsize)
{
	unsigned int i, oldsize = map->tablesize;
	hashmap_entry **oldtable = map->table;

	map->tablesize = newsize;
	map->table = xcalloc(sizeof(hashmap_entry*), map->tablesize);
	for (i = 0; i < oldsize; i++) {
		hashmap_entry *e = oldtable[i];
		while (e) {
			hashmap_entry *next = e->next;
			unsigned int b = bucket(map, e);
			e->next = map->table[b];
			map->table[b] = e;
			e = next;
		}
	}
	free(oldtable);
}

void hashmap_init(hashmap *map, hashmap_cmp_fn equals_function,
		size_t initial_size)
{
	map->size = 0;
	map->cmpfn = equals_function;
	/* calculate initial table size and allocate the table */
	map->tablesize = HASHMAP_INITIAL_SIZE;
	initial_size *= HASHMAP_GROW_AT;
	while (initial_size > map->tablesize)
		map->tablesize <<= HASHMAP_GROW;
	map->table = xcalloc(sizeof(hashmap_entry*), map->tablesize);
}

void hashmap_free(hashmap *map, hashmap_free_fn free_function)
{
	if (!map || !map->table)
		return;
	if (free_function) {
		hashmap_iter iter;
		hashmap_entry *e;
		hashmap_iter_init(map, &iter);
		while ((e = hashmap_iter_next(&iter)))
			(*free_function)(e);
	}
	free(map->table);
	memset(map, 0, sizeof(*map));
}

hashmap_entry *hashmap_get(const hashmap *map, const hashmap_entry *key)
{
	hashmap_entry *e = map->table[bucket(map, key)];
	while (e && !entry_equals(map, e, key))
		e = e->next;
	return e;
}

hashmap_entry *hashmap_put(hashmap *map, hashmap_entry *entry)
{
	unsigned int b = bucket(map, entry);
	hashmap_entry *last = NULL, *e = map->table[b];

	/* find entry */
	while (e && !entry_equals(map, e, entry)) {
		last = e;
		e = e->next;
	}

	if (!e) {
		/* not found, add entry */
		entry->next = map->table[b];
		map->table[b] = entry;

		/* fix size and rehash if appropriate */
		map->size++;
		if (map->size * HASHMAP_GROW_AT > map->tablesize)
			rehash(map, map->tablesize << HASHMAP_GROW);
	} else if (e != entry) {
		/* replace found entry */
		if (last)
			last->next = entry;
		else
			map->table[b] = entry;
		entry->next = e->next;
		e->next = NULL;
	}
	return e;
}

hashmap_entry *hashmap_remove(hashmap *map, const hashmap_entry *key)
{
	unsigned int b = bucket(map, key);
	hashmap_entry *last = NULL, *e = map->table[b];

	/* find entry */
	while (e && !entry_equals(map, e, key)) {
		last = e;
		e = e->next;
	}

	if (e) {
		/* remove found entry */
		if (last)
			last->next = e->next;
		else
			map->table[b] = e->next;
		e->next = NULL;

		/* fix size and rehash if appropriate */
		map->size--;
		if (map->tablesize > HASHMAP_INITIAL_SIZE && map->size
				* HASHMAP_SHRINK_AT < map->tablesize)
			rehash(map, map->tablesize >> HASHMAP_GROW);
	}
	return e;
}

void hashmap_iter_init(hashmap *map, hashmap_iter *iter)
{
	iter->map = map;
	iter->tablepos = 0;
	iter->next = NULL;
}

hashmap_entry *hashmap_iter_next(hashmap_iter *iter)
{
	hashmap_entry *current = iter->next;
	for (;;) {
		if (current) {
			iter->next = current->next;
			return current;
		}

		if (iter->tablepos >= iter->map->tablesize)
			return NULL;

		current = iter->map->table[iter->tablepos++];
	}
}
