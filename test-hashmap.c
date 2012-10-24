#include "cache.h"
#include "hashmap.h"
#include <stdio.h>

typedef struct test_entry
{
	hashmap_entry ent;
	char *key;
	char *value;
} test_entry;

static int test_entry_cmp(const test_entry *e1, const test_entry *e2)
{
	return strcmp(e1->key, e2->key);
}

static int test_entry_cmp_icase(const test_entry *e1, const test_entry *e2)
{
	return strcasecmp(e1->key, e2->key);
}

static void perf_hashmap(unsigned int size, unsigned int rounds)
{
	hashmap map;
	char buf[16];
	char **strings;
	test_entry *entries, *e;
	unsigned int i, j;

	strings = malloc(size * sizeof(char*));
	entries = malloc(size * sizeof(test_entry));
	for (i = 0; i < size; i++) {
		snprintf(buf, sizeof(buf), "%i", i);
		strings[i] = strdup(buf);
		entries[i].key = strings[i];
		entries[i].value = strings[i];
	}

	for (j = 0; j < rounds; j++) {
		// initialize the map
		hashmap_init(&map, (hashmap_cmp_fn) test_entry_cmp, 0);

		// add entries
		for (i = 0; i < size; i++) {
			hashmap_entry_init(&entries[i].ent, strhash(strings[i]));
			e = (test_entry*) hashmap_put(&map, &entries[i].ent);
			if (e)
				printf("duplicate: %s\n", strings[i]);
		}

		hashmap_free(&map, NULL);
	}
}

typedef struct hash_entry
{
	char *key;
	char *value;
	struct hash_entry *next;
} hash_entry;

static void perf_hashtable(unsigned int size, unsigned int rounds)
{
	struct hash_table map;
	char buf[16];
	char **strings;
	hash_entry *entries, **res, *e;
	unsigned int i, j;

	strings = malloc(size * sizeof(char*));
	entries = malloc(size * sizeof(hash_entry));
	for (i = 0; i < size; i++) {
		snprintf(buf, sizeof(buf), "%i", i);
		strings[i] = strdup(buf);
		entries[i].key = strings[i];
		entries[i].value = strings[i];
	}

	for (j = 0; j < rounds; j++) {
		// initialize the map
		init_hash(&map);

		// add entries
		for (i = 0; i < size; i++) {
			res = (hash_entry**) insert_hash(
				strhash(entries[i].key), &entries[i], &map);
			if (res) {
				e = *res;
				while (e && strcmp(e->key, strings[i]))
					e = e->next;
				if (e)
					printf("duplicate: %s\n", strings[i]);

				entries[i].next = *res;
				*res = &entries[i];
			}
		}

		free_hash(&map);
	}
}


#define DELIM " \t\r\n"

/*
 * Read stdin line by line and print result of commands to stdout:
 *
 * hash key -> strhash(key) memhash(key) strihash(key) memihash(key)
 * put key value -> NULL / old value
 * get key -> NULL / value
 * remove key -> NULL / old value
 * iterate -> key1 value1\nkey2 value2\n...
 * size -> tablesize numentries
 */
int main(int argc, const char *argv[])
{
	char line[1024];
	hashmap map;
	int icase;

	/* init hash map */
	icase = argc > 1 && !strcmp("ignorecase", argv[1]);
	hashmap_init(&map, (hashmap_cmp_fn) (icase ? test_entry_cmp_icase
			: test_entry_cmp), 0);

	/* process commands from stdin */
	while (fgets(line, sizeof(line), stdin)) {
		char *cmd, *p1 = NULL, *p2 = NULL;
		int l1 = 0, l2 = 0, hash = 0;
		test_entry *entry;

		/* break line into command and up to two parameters */
		cmd = strtok(line, DELIM);
		/* ignore empty lines */
		if (!cmd || *cmd == '#')
			continue;

		p1 = strtok(NULL, DELIM);
		if (p1) {
			l1 = strlen(p1);
			hash = icase ? strihash(p1) : strhash(p1);
			p2 = strtok(NULL, DELIM);
			if (p2)
				l2 = strlen(p2);
		}

		if (!strcmp("hash", cmd) && l1) {

			/* print results of different hash functions */
			printf("%u %u %u %u\n", strhash(p1), memhash(p1, l1),
					strihash(p1), memihash(p1, l1));

		} else if (!strcmp("put", cmd) && l1 && l2) {

			/* create entry with key = p1, value = p2 */
			entry = malloc(sizeof(test_entry) + l1 + l2 + 2);
			hashmap_entry_init(&entry->ent, hash);
			entry->key = ((char*)entry) + sizeof(test_entry);
			entry->value = entry->key + l1 + 1;
			memcpy(entry->key, p1, l1 + 1);
			memcpy(entry->value, p2, l2 + 1);

			/* add to hashmap */
			entry = (test_entry*) hashmap_put(&map, &entry->ent);

			/* print and free replaced entry, if any */
			puts(entry ? entry->value : "NULL");
			free(entry);

		} else if (!strcmp("get", cmd) && l1) {

			/* setup static key */
			test_entry key;
			hashmap_entry_init(&key.ent, hash);
			key.key = p1;

			/* lookup entry in hashmap */
			entry = (test_entry*) hashmap_get(&map, &key.ent);

			/* print result */
			puts(entry ? entry->value : "NULL");

		} else if (!strcmp("remove", cmd) && l1) {

			/* setup static key */
			test_entry key;
			hashmap_entry_init(&key.ent, hash);
			key.key = p1;

			/* remove entry from hashmap */
			entry = (test_entry*) hashmap_remove(&map, &key.ent);

			/* print result and free entry*/
			puts(entry ? entry->value : "NULL");
			free(entry);

		} else if (!strcmp("iterate", cmd)) {

			hashmap_iter iter;
			hashmap_iter_init(&map, &iter);
			while ((entry = (test_entry*) hashmap_iter_next(&iter)))
				printf("%s %s\n", entry->key, entry->value);

		} else if (!strcmp("size", cmd)) {

			/* print table sizes */
			printf("%u %u\n", map.tablesize, map.size);

		} else if (!strcmp("perfhashmap", cmd) && l1 && l2) {

			perf_hashmap(atoi(p1), atoi(p2));

		} else if (!strcmp("perfhashtable", cmd) && l1 && l2) {

			perf_hashtable(atoi(p1), atoi(p2));

		} else {

			printf("Unknown command %s\n", cmd);

		}
	}

	hashmap_free(&map, (hashmap_free_fn) free);
	return 0;
}
