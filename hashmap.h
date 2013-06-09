#ifndef HASHMAP_H
#define HASHMAP_H

/*
 * Generic implementation of hash-based key value mappings.
 * Supports basic operations get, put, remove and iteration.
 *
 * Also contains a set of ready-to-use hash functions for strings, using the
 * FNV-1 algorithm (see http://www.isthe.com/chongo/tech/comp/fnv).
 */

/*
 * Case-sensitive FNV-1 hash of 0-terminated string.
 * str: the string
 * returns hash code
 */
extern unsigned int strhash(const char *buf);

/*
 * Case-insensitive FNV-1 hash of 0-terminated string.
 * str: the string
 * returns hash code
 */
extern unsigned int strihash(const char *buf);

/*
 * Case-sensitive FNV-1 hash of a memory block.
 * buf: start of the memory block
 * len: length of the memory block
 * returns hash code
 */
extern unsigned int memhash(const void *buf, size_t len);

/*
 * Case-insensitive FNV-1 hash of a memory block.
 * buf: start of the memory block
 * len: length of the memory block
 * returns hash code
 */
extern unsigned int memihash(const void *buf, size_t len);

/*
 * Hashmap entry data structure, intended to be used as first member of user
 * data structures. Consists of a pointer and an int. Ideally it should be
 * followed by an int-sized member to prevent unused memory on 64-bit systems
 * due to alignment.
 */
typedef struct hashmap_entry {
	struct hashmap_entry *next;
	unsigned int hash;
} hashmap_entry;

/*
 * User-supplied function to test two hashmap entries for equality, shall
 * return 0 if the entries are equal. This function is always called with
 * non-NULL parameters that have the same hash code.
 */
typedef int (*hashmap_cmp_fn)(const hashmap_entry*, const hashmap_entry*);

/*
 * User-supplied function to free a hashmap entry.
 */
typedef void (*hashmap_free_fn)(const hashmap_entry*);

/*
 * Hashmap data structure, use with hashmap_* functions.
 */
typedef struct hashmap {
	hashmap_entry **table;
	hashmap_cmp_fn cmpfn;
	unsigned int size, tablesize;
} hashmap;

/*
 * Hashmap iterator data structure, use with hasmap_iter_* functions.
 */
typedef struct hashmap_iter {
	hashmap *map;
	hashmap_entry *next;
	unsigned int tablepos;
} hashmap_iter;

/*
 * Initializes a hashmap_entry structure.
 * entry: pointer to the entry to initialize
 * hash: hash code of the entry
 */
static inline void hashmap_entry_init(hashmap_entry *entry, int hash)
{
	entry->hash = hash;
	entry->next = NULL;
}

/*
 * Initializes a hashmap structure.
 * map: hashmap to initialize
 * equals_function: function to test equality of hashmap entries
 * initial_size: number of initial entries, or 0 if unknown
 */
extern void hashmap_init(hashmap *map, hashmap_cmp_fn equals_function,
		size_t initial_size);

/*
 * Frees a hashmap structure and allocated memory.
 * map: hashmap to free
 * free_function: optional function to free the hashmap entries
 */
extern void hashmap_free(hashmap *map, hashmap_free_fn free_function);

/*
 * Returns the hashmap entry for the specified key, or NULL if not found.
 * map: the hashmap
 * key: key of the entry to look up
 * returns matching hashmap entry, or NULL if not found
 */
extern hashmap_entry *hashmap_get(const hashmap *map, const hashmap_entry *key);

/*
 * Adds or replaces a hashmap entry.
 * map: the hashmap
 * entry: the entry to add or replace
 * returns previous entry, or NULL if the entry is new
 */
extern hashmap_entry *hashmap_put(hashmap *map, hashmap_entry *entry);

/*
 * Removes a hashmap entry matching the specified key.
 * map: the hashmap
 * key: key of the entry to remove
 * returns removed entry, or NULL if not found
 */
extern hashmap_entry *hashmap_remove(hashmap *map, const hashmap_entry *key);

/*
 * Initializes a hashmap iterator structure.
 * map: the hashmap
 * iter: hashmap iterator structure
 */
extern void hashmap_iter_init(hashmap *map, hashmap_iter *iter);

/**
 * Returns the next hashmap entry.
 * iter: hashmap iterator
 * returns next entry, or NULL if there are no more entries
 */
extern hashmap_entry *hashmap_iter_next(hashmap_iter *iter);

/**
 * Initializes a hashmap iterator and returns the first hashmap entry.
 * map: the hashmap
 * iter: hashmap iterator
 * returns first entry, or NULL if there are no entries
 */
static inline hashmap_entry *hashmap_iter_first(hashmap *map,
		hashmap_iter *iter)
{
	hashmap_iter_init(map, iter);
	return hashmap_iter_next(iter);
}

#endif
