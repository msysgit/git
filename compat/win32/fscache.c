#include "../../git-compat-util.h"
#include "../../cache.h"
#include "../../hashmap.h"
#include "fscache.h"
#include "../win32.h"

#include <stdlib.h>
#include <stdio.h>
#include <direct.h>

typedef struct fsentry fsentry;

/*
 * An entry in the file system cache. Used for both entire directory listings
 * and file entries.
 */
struct fsentry {
	hashmap_entry hash;
	mode_t st_mode;
	/* Length of name. */
	unsigned short len;
	/*
	 * Name of the entry. For directory listings: relative path of the
	 * directory, without trailing '/' (empty for cwd()). For file entries:
	 * name of the file. Typically points to the end of the structure if
	 * the fsentry is allocated on the heap (see fse_alloc), or to a local
	 * variable if on the stack (see fsentry_init).
	 */
	const char *name;
	/* Pointer to the directory listing, or NULL for the listing itself. */
	fsentry *list;
	/* Pointer to the next file entry of the list. */
	fsentry *next;

	union {
		/* Reference count of the directory listing. */
		volatile long refcnt;
		struct {
			/* More stat members (only used for file entries). */
			off64_t st_size;
			time_t st_atime;
			time_t st_mtime;
			time_t st_ctime;
		};
	};
};

/*
 * Compares the paths of two fsentry structures for equality.
 */
static int fsentry_cmp(const fsentry *fse1, const fsentry *fse2)
{
	int res;
	if (fse1 == fse2)
		return 0;

	/* compare the list parts first */
	if (fse1->list != fse2->list && (res = fsentry_cmp(
			fse1->list ? fse1->list : fse1,
			fse2->list ? fse2->list	: fse2)))
		return res;

	/* if list parts are equal, compare len and name */
	if (fse1->len != fse2->len)
		return fse1->len - fse2->len;
	return strnicmp(fse1->name, fse2->name, fse1->len);
}

/*
 * Calculates the hash code of an fsentry structure's path.
 */
static unsigned int fsentry_hash(const fsentry *fse)
{
	unsigned int hash = fse->list ? fse->list->hash.hash : 0;
	return hash ^ memihash(fse->name, fse->len);
}

/*
 * Initialize an fsentry structure for use by fse_hash and fse_cmp.
 */
static void fsentry_init(fsentry *fse, fsentry *list, const char *name,
		size_t len)
{
	fse->list = list;
	fse->name = name;
	fse->len = len;
	hashmap_entry_init(&fse->hash, fsentry_hash(fse));
}

/*
 * Allocate an fsentry structure on the heap.
 */
static fsentry *fsentry_alloc(fsentry *list, const char *name, size_t len)
{
	/* overallocate fsentry and copy the name to the end */
	fsentry *fse = (fsentry*) xmalloc(sizeof(fsentry) + len + 1);
	char *nm = ((char*) fse) + sizeof(fsentry);
	memcpy(nm, name, len);
	nm[len] = 0;
	/* init the rest of the structure */
	fsentry_init(fse, list, nm, len);
	fse->next = NULL;
	fse->refcnt = 1;
	return fse;
}

/*
 * Add a reference to an fsentry.
 */
inline static void fsentry_addref(fsentry *fse)
{
	if (fse->list)
		fse = fse->list;

	InterlockedIncrement(&(fse->refcnt));
}

/*
 * Release the reference to an fsentry, frees the memory if its the last ref.
 */
static void fsentry_release(fsentry *fse)
{
	if (fse->list)
		fse = fse->list;

	if (InterlockedDecrement(&(fse->refcnt)))
		return;

	while (fse) {
		fsentry *next = fse->next;
		free(fse);
		fse = next;
	}
}

/*
 * Allocate and initialize an fsentry from a WIN32_FIND_DATA structure.
 */
static fsentry *fseentry_create_entry(fsentry *list,
		const WIN32_FIND_DATAW *fdata)
{
	char buf[MAX_PATH * 3];
	int len;
	fsentry *fse;
	len = xwcstoutf(buf, fdata->cFileName, ARRAY_SIZE(buf));

	fse = fsentry_alloc(list, buf, len);

	fse->st_mode = file_attr_to_st_mode(fdata->dwFileAttributes);
	fse->st_size = (((off64_t) (fdata->nFileSizeHigh)) << 32)
			| fdata->nFileSizeLow;
	fse->st_atime = filetime_to_time_t(&(fdata->ftLastAccessTime));
	fse->st_mtime = filetime_to_time_t(&(fdata->ftLastWriteTime));
	fse->st_ctime = filetime_to_time_t(&(fdata->ftCreationTime));

	return fse;
}

/*
 * Create an fsentry-based directory listing (similar to opendir / readdir).
 * Dir should not contain trailing '/'. Use an empty string for the current
 * directory (not "."!).
 */
static fsentry *fsentry_create_list(const fsentry *dir)
{
	wchar_t pattern[MAX_PATH + 2]; /* + 2 for '/' '*' */
	WIN32_FIND_DATAW fdata;
	HANDLE h;
	int wlen;
	fsentry *list, **phead;
	DWORD err;

	/* convert name to UTF-16 and check length < MAX_PATH */
	if ((wlen = xutftowcsn(pattern, dir->name, MAX_PATH, dir->len)) < 0) {
		if (errno == ERANGE)
			errno = ENAMETOOLONG;
		return NULL;
	}

	/* append optional '/' and wildcard '*' */
	if (wlen)
		pattern[wlen++] = '/';
	pattern[wlen++] = '*';
	pattern[wlen] = 0;

	/* open find handle */
	h = FindFirstFileW(pattern, &fdata);
	if (h == INVALID_HANDLE_VALUE) {
		err = GetLastError();
		errno = (err == ERROR_DIRECTORY) ? ENOTDIR : err_win_to_posix(err);
		return NULL;
	}

	/* allocate object to hold directory listing */
	list = fsentry_alloc(NULL, dir->name, dir->len);

	/* walk directory and build linked list of fsentry structures */
	phead = &list->next;
	do {
		*phead = fseentry_create_entry(list, &fdata);
		phead = &(*phead)->next;
	} while (FindNextFileW(h, &fdata));

	/* remember result of last FindNextFile, then close find handle */
	err = GetLastError();
	FindClose(h);

	/* return the list if we've got all the files */
	if (err == ERROR_NO_MORE_FILES)
		return list;

	/* otherwise free the list and return error */
	fsentry_release(list);
	errno = err_win_to_posix(err);
	return NULL;
}

static volatile long enabled = 0;
static hashmap map;
static CRITICAL_SECTION mutex;

/*
 * Initializes fscache, called on startup (mingw_startup).
 */
void fscache_init()
{
	InitializeCriticalSection(&mutex);
	hashmap_init(&map, (hashmap_cmp_fn) fsentry_cmp, 0);
}

/*
 * Adds a directory listing to the cache.
 */
static void fscache_add(fsentry *fse)
{
	if (fse->list)
		fse = fse->list;

	while (fse) {
		fsentry *old = (fsentry*) hashmap_put(&map, &fse->hash);

		/*
		 * synchronization in fscache_get should ensure that we never
		 * replace an existing entry...warn if this is broken
		 */
		if (old)
			warning("fscache: replacing existing entry %s/%s!",
				old->list ? old->list->name : "", old->name);

		fse = fse->next;
	}
}

/*
 * Removes a directory listing from the cache.
 */
static void fscache_remove(fsentry *fse)
{
	if (fse->list)
		fse = fse->list;

	while (fse) {
		hashmap_remove(&map, &fse->hash);
		fse = fse->next;
	}
}

/*
 * Clears the cache.
 */
static void fscache_clear()
{
	hashmap_iter iter;
	fsentry *fse;
	while ((fse = (fsentry*) hashmap_iter_first(&map, &iter))) {
		fscache_remove(fse);
		fsentry_release(fse);
	}
}

/*
 * Checks if the cache is enabled for the given path.
 */
inline static int fscache_enabled(const char *path)
{
	return enabled > 0 && !is_absolute_path(path);
}

/*
 * Enables or disables the cache. Note that the cache is read-only, changes to
 * the working directory are NOT reflected in the cache while enabled.
 */
int fscache_enable(int enable)
{
	int result;

	/* allow the cache to be disabled entirely */
	if (getenv("GIT_NOFSCACHE"))
		return 0;

	result = enable ? InterlockedIncrement(&enabled)
			: InterlockedDecrement(&enabled);

	/* clear the cache if disabled */
	if (!result) {
		EnterCriticalSection(&mutex);
		fscache_clear();
		LeaveCriticalSection(&mutex);
	}
	return result;
}

/*
 * Looks up or creates a cache entry for the specified key.
 */
static fsentry *fscache_get(fsentry *key)
{
	fsentry *fse;

	EnterCriticalSection(&mutex);
	/* check if entry is in cache */
	fse = (fsentry*) hashmap_get(&map, &key->hash);
	if (fse) {
		fsentry_addref(fse);
		LeaveCriticalSection(&mutex);
		return fse;
	}
	/* if looking for a file, check if directory listing is in cache */
	if (!fse && key->list) {
		fse = (fsentry*) hashmap_get(&map, &key->list->hash);
		if (fse) {
			LeaveCriticalSection(&mutex);
			/* dir entry without file entry -> file doesn't exist */
			errno = ENOENT;
			return NULL;
		}
	}

	/* create the directory listing (outside mutex!) */
	LeaveCriticalSection(&mutex);
	fse = fsentry_create_list(key->list ? key->list : key);
	if (!fse)
		return NULL;

	EnterCriticalSection(&mutex);
	/* add directory listing if it hasn't been added by some other thread */
	if (!hashmap_get(&map, &key->hash))
		fscache_add(fse);

	/* lookup file entry if requested (fse already points to directory) */
	if (key->list)
		fse = (fsentry*) hashmap_get(&map, &key->hash);

	/* return entry or ENOENT */
	if (fse)
		fsentry_addref(fse);
	else
		errno = ENOENT;

	LeaveCriticalSection(&mutex);
	return fse;
}

/*
 * Lstat replacement, uses the cache if enabled, otherwise redirects to
 * mingw_lstat.
 */
int fscache_lstat(const char *filename, struct stat *st)
{
	int dirlen, base, len;
	fsentry key[2], *fse;

	if (!fscache_enabled(filename))
		return mingw_lstat(filename, st);

	/* split filename into path + name */
	len = strlen(filename);
	if (len && is_dir_sep(filename[len - 1]))
		len--;
	base = len;
	while (base && !is_dir_sep(filename[base - 1]))
		base--;
	dirlen = base ? base - 1 : 0;

	/* lookup entry for path + name in cache */
	fsentry_init(key, NULL, filename, dirlen);
	fsentry_init(key + 1, key, filename + base, len - base);
	fse = fscache_get(key + 1);
	if (!fse)
		return -1;

	/* copy stat data */
	st->st_ino = 0;
	st->st_gid = 0;
	st->st_uid = 0;
	st->st_dev = 0;
	st->st_rdev = 0;
	st->st_nlink = 1;
	st->st_mode = fse->st_mode;
	st->st_size = fse->st_size;
	st->st_atime = fse->st_atime;
	st->st_mtime = fse->st_mtime;
	st->st_ctime = fse->st_ctime;

	/* don't forget to release fsentry */
	fsentry_release(fse);
	return 0;
}

/*
 * Opendir replacement, uses a directory listing from the cache if enabled,
 * otherwise creates a fresh directory listing.
 */
struct fscache_dirent *fscache_opendir(const char *dirname)
{
	fsentry key, *list;
	fscache_DIR *dir;

	/* prepare name (strip trailing '/', replace '.') */
	int len = strlen(dirname);
	if ((len == 1 && dirname[0] == '.') ||
	    (len && is_dir_sep(dirname[len - 1])))
		len--;

	/* create fresh directory listing or lookup in cache */
	fsentry_init(&key, NULL, dirname, len);
	if (!fscache_enabled(dirname))
		list = fsentry_create_list(&key);
	else
		list = fscache_get(&key);
	if (!list)
		return NULL;

	/* alloc and return DIR structure */
	dir = (fscache_DIR*) xmalloc(sizeof(fscache_DIR));
	dir->d_type = 0;
	dir->pfsentry = list;
	return dir;
}

/*
 * Readdir replacement.
 */
struct fscache_dirent *fscache_readdir(fscache_DIR *dir)
{
	fsentry *next = ((fsentry*) dir->pfsentry)->next;
	if (!next)
		return NULL;
	dir->pfsentry = next;
	dir->d_type = S_ISDIR(next->st_mode) ? DT_DIR : DT_REG;
	dir->d_name = next->name;
	return dir;
}

/*
 * Closedir replacement.
 */
void fscache_closedir(fscache_DIR *dir)
{
	fsentry_release((fsentry*) dir->pfsentry);
	free(dir);
}
