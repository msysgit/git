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

static inline int entry_equals(const struct hashmap *map,
		const struct hashmap_entry *e1, const struct hashmap_entry *e2,
		const void *keydata)
{
	return (e1 == e2) || (e1->hash == e2->hash && !(*map->cmpfn)(e1, e2, keydata));
}

static inline unsigned int bucket(const struct hashmap *map,
		const struct hashmap_entry *key)
{
	return key->hash & (map->tablesize - 1);
}

static void rehash(struct hashmap *map, unsigned int newsize)
{
	unsigned int i, oldsize = map->tablesize;
	struct hashmap_entry **oldtable = map->table;

	map->tablesize = newsize;
	map->table = xcalloc(sizeof(struct hashmap_entry*), map->tablesize);
	for (i = 0; i < oldsize; i++) {
		struct hashmap_entry *e = oldtable[i];
		while (e) {
			struct hashmap_entry *next = e->next;
			unsigned int b = bucket(map, e);
			e->next = map->table[b];
			map->table[b] = e;
			e = next;
		}
	}
	free(oldtable);
}

static inline struct hashmap_entry **find_entry_ptr(const struct hashmap *map,
		const struct hashmap_entry *key, const void *keydata)
{
	struct hashmap_entry **e = &map->table[bucket(map, key)];
	while (*e && !entry_equals(map, *e, key, keydata))
		e = &(*e)->next;
	return e;
}

static int always_equal(const void *unused1, const void *unused2, const void *unused3)
{
	return 0;
}

void hashmap_init(struct hashmap *map, hashmap_cmp_fn equals_function,
		size_t initial_size)
{
	map->size = 0;
	map->cmpfn = equals_function ? equals_function : always_equal;
	/* calculate initial table size and allocate the table */
	map->tablesize = HASHMAP_INITIAL_SIZE;
	initial_size *= HASHMAP_GROW_AT;
	while (initial_size > map->tablesize)
		map->tablesize <<= HASHMAP_GROW;
	map->table = xcalloc(sizeof(struct hashmap_entry*), map->tablesize);
}

void hashmap_free(struct hashmap *map, hashmap_free_fn free_function)
{
	if (!map || !map->table)
		return;
	if (free_function) {
		struct hashmap_iter iter;
		struct hashmap_entry *e;
		hashmap_iter_init(map, &iter);
		while ((e = hashmap_iter_next(&iter)))
			(*free_function)(e);
	}
	free(map->table);
	memset(map, 0, sizeof(*map));
}

void *hashmap_get(const struct hashmap *map, const void *key, const void *keydata)
{
	return *find_entry_ptr(map, key, keydata);
}

void *hashmap_get_next(const struct hashmap *map, const void *entry)
{
	struct hashmap_entry *e = ((struct hashmap_entry*) entry)->next;
	for (; e; e = e->next)
		if (entry_equals(map, entry, e, NULL))
			return e;
	return NULL;
}

void hashmap_add(struct hashmap *map, void *entry)
{
	unsigned int b = bucket(map, entry);

	/* add entry */
	((struct hashmap_entry*) entry)->next = map->table[b];
	map->table[b] = entry;

	/* fix size and rehash if appropriate */
	map->size++;
	if (map->size * HASHMAP_GROW_AT > map->tablesize)
		rehash(map, map->tablesize << HASHMAP_GROW);
}

void *hashmap_remove(struct hashmap *map, const void *key, const void *keydata)
{
	struct hashmap_entry *old;
	struct hashmap_entry **e = find_entry_ptr(map, key, keydata);
	if (!*e)
		return NULL;

	/* remove existing entry */
	old = *e;
	*e = old->next;
	old->next = NULL;

	/* fix size and rehash if appropriate */
	map->size--;
	if (map->tablesize > HASHMAP_INITIAL_SIZE &&
	    map->size * HASHMAP_SHRINK_AT < map->tablesize)
		rehash(map, map->tablesize >> HASHMAP_GROW);
	return old;
}

void *hashmap_put(struct hashmap *map, void *entry)
{
	struct hashmap_entry *old = hashmap_remove(map, entry, NULL);
	hashmap_add(map, entry);
	return old;
}

void hashmap_iter_init(struct hashmap *map, struct hashmap_iter *iter)
{
	iter->map = map;
	iter->tablepos = 0;
	iter->next = NULL;
}

void *hashmap_iter_next(struct hashmap_iter *iter)
{
	struct hashmap_entry *current = iter->next;
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
