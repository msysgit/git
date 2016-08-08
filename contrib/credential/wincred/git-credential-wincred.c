/*
 * A git credential helper that interface with Windows' Credential Manager
 *
 */
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* common helpers */

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static void die(const char *err, ...)
{
	char msg[4096];
	va_list params;
	va_start(params, err);
	vsnprintf(msg, sizeof(msg), err, params);
	fprintf(stderr, "%s\n", msg);
	va_end(params);
	exit(1);
}

static void *xmalloc(size_t size)
{
	void *ret = malloc(size);
	if (!ret && !size)
		ret = malloc(1);
	if (!ret)
		die("Out of memory");
	return ret;
}

/* MinGW doesn't have wincred.h, so we need to define stuff */

typedef struct _CREDENTIAL_ATTRIBUTEW {
	LPWSTR Keyword;
	DWORD  Flags;
	DWORD  ValueSize;
	LPBYTE Value;
} CREDENTIAL_ATTRIBUTEW, *PCREDENTIAL_ATTRIBUTEW;

typedef struct _CREDENTIALW {
	DWORD                  Flags;
	DWORD                  Type;
	LPWSTR                 TargetName;
	LPWSTR                 Comment;
	FILETIME               LastWritten;
	DWORD                  CredentialBlobSize;
	LPBYTE                 CredentialBlob;
	DWORD                  Persist;
	DWORD                  AttributeCount;
	PCREDENTIAL_ATTRIBUTEW Attributes;
	LPWSTR                 TargetAlias;
	LPWSTR                 UserName;
} CREDENTIALW, *PCREDENTIALW;

#define CRED_TYPE_GENERIC 1
#define CRED_PERSIST_SESSION 1
#define CRED_PERSIST_LOCAL_MACHINE 2
#define CRED_MAX_ATTRIBUTES 64

typedef BOOL (WINAPI *CredWriteWT)(PCREDENTIALW, DWORD);
typedef BOOL (WINAPI *CredEnumerateWT)(LPCWSTR, DWORD, DWORD *,
				       PCREDENTIALW **);
typedef VOID (WINAPI *CredFreeT)(PVOID);
typedef BOOL (WINAPI *CredDeleteWT)(LPCWSTR, DWORD, DWORD);

static HMODULE advapi;
static CredWriteWT CredWriteW;
static CredEnumerateWT CredEnumerateW;
static CredFreeT CredFree;
static CredDeleteWT CredDeleteW;

static void load_cred_funcs(void)
{
	/* load DLLs */
	advapi = LoadLibrary("advapi32.dll");
	if (!advapi)
		die("failed to load advapi32.dll");

	/* get function pointers */
	CredWriteW = (CredWriteWT)GetProcAddress(advapi, "CredWriteW");
	CredEnumerateW = (CredEnumerateWT)GetProcAddress(advapi,
			 "CredEnumerateW");
	CredFree = (CredFreeT)GetProcAddress(advapi, "CredFree");
	CredDeleteW = (CredDeleteWT)GetProcAddress(advapi, "CredDeleteW");
	if (!CredWriteW || !CredEnumerateW || !CredFree || !CredDeleteW)
		die("failed to load functions");
}

static WCHAR *wusername, *password, *protocol, *host, *path, target[1024];

static void write_item(const char *what, LPCWSTR wbuf, int wlen)
{
	char *buf;

	if (!wbuf || !wlen) {
		printf("%s=\n", what);
		return;
	}

	int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, NULL, 0, NULL,
				      FALSE);
	buf = xmalloc(len);

	if (!WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, buf, len, NULL, FALSE))
		die("WideCharToMultiByte failed!");

	printf("%s=", what);
	fwrite(buf, 1, len, stdout);
	putchar('\n');
	free(buf);
}

/*
 * Match an (optional) expected string and a delimiter in the target string,
 * consuming the matched text by updating the target pointer.
 */
static int match_part(LPCWSTR *ptarget, LPCWSTR want, LPCWSTR delim)
{
	LPCWSTR delim_pos, start = *ptarget;
	int len;

	/* find start of delimiter (or end-of-string if delim is empty) */
	if (*delim)
		delim_pos = wcsstr(start, delim);
	else
		delim_pos = start + wcslen(start);

	/*
	 * match text up to delimiter, or end of string (e.g. the '/' after
	 * host is optional if not followed by a path)
	 */
	if (delim_pos)
		len = delim_pos - start;
	else
		len = wcslen(start);

	/* update ptarget if we either found a delimiter or need a match */
	if (delim_pos || want)
		*ptarget = delim_pos ? delim_pos + wcslen(delim) : start + len;

	return !want || (!wcsncmp(want, start, len) && !want[len]);
}

static int match_cred(const CREDENTIALW *cred)
{
	LPCWSTR target = cred->TargetName;
	if (wusername && wcscmp(wusername, cred->UserName ? cred->UserName : L""))
		return 0;

	return match_part(&target, L"git", L":") &&
	       match_part(&target, protocol, L"://") &&
	       match_part(&target, wusername, L"@") &&
	       match_part(&target, host, L"/") &&
	       match_part(&target, path, L"");
}

static void get_credential(void)
{
	CREDENTIALW **creds;
	DWORD num_creds;
	int i;

	if (!CredEnumerateW(L"git:*", 0, &num_creds, &creds))
		return;

	/* search for the first credential that matches username */
	for (i = 0; i < num_creds; ++i)
		if (match_cred(creds[i])) {
			write_item("username", creds[i]->UserName,
				   creds[i]->UserName ? wcslen(creds[i]->UserName) : 0);
			write_item("password",
				   (LPCWSTR)creds[i]->CredentialBlob,
				   creds[i]->CredentialBlobSize / sizeof(WCHAR));
			break;
		}

	CredFree(creds);
}

static const char* OPTION_KEY = "--type";
static const char* PERSIST_SESSION_SHORT = "-s";
static const char* PERSIST_SESSION = "session";

static void store_credential(const char *type)
{
	CREDENTIALW cred;

	if (!wusername || !password)
		return;

	// default value, keep in local computer.
	DWORD persit = CRED_PERSIST_LOCAL_MACHINE;

	if ((type != NULL) && (!strncmp(type, PERSIST_SESSION, strlen(type)) || !strncmp(type, PERSIST_SESSION_SHORT, strlen(type)))) {
		persit = CRED_PERSIST_SESSION;
	}

	cred.Flags = 0;
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = target;
	cred.Comment = L"saved by git-credential-wincred";
	cred.CredentialBlobSize = (wcslen(password)) * sizeof(WCHAR);
	cred.CredentialBlob = (LPVOID)password;
	cred.Persist = persit;
	cred.AttributeCount = 0;
	cred.Attributes = NULL;
	cred.TargetAlias = NULL;
	cred.UserName = wusername;

	if (!CredWriteW(&cred, 0))
		die("CredWrite failed");
}

static void erase_credential(void)
{
	CREDENTIALW **creds;
	DWORD num_creds;
	int i;

	if (!CredEnumerateW(L"git:*", 0, &num_creds, &creds))
		return;

	for (i = 0; i < num_creds; ++i) {
		if (match_cred(creds[i]))
			CredDeleteW(creds[i]->TargetName, creds[i]->Type, 0);
	}

	CredFree(creds);
}

static WCHAR *utf8_to_utf16_dup(const char *str)
{
	int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	WCHAR *wstr = xmalloc(sizeof(WCHAR) * wlen);
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wlen);
	return wstr;
}

static void read_credential(void)
{
	char buf[1024];

	while (fgets(buf, sizeof(buf), stdin)) {
		char *v;
		int len = strlen(buf);
		/* strip trailing CR / LF */
		while (len && strchr("\r\n", buf[len - 1]))
			buf[--len] = 0;

		if (!*buf)
			break;

		v = strchr(buf, '=');
		if (!v)
			die("bad input: %s", buf);
		*v++ = '\0';

		if (!strcmp(buf, "protocol"))
			protocol = utf8_to_utf16_dup(v);
		else if (!strcmp(buf, "host"))
			host = utf8_to_utf16_dup(v);
		else if (!strcmp(buf, "path"))
			path = utf8_to_utf16_dup(v);
		else if (!strcmp(buf, "username")) {
			wusername = utf8_to_utf16_dup(v);
		} else if (!strcmp(buf, "password"))
			password = utf8_to_utf16_dup(v);
		else
			die("unrecognized input");
	}
}

static char** split(char* str, const char delim)
{
	char** result = 0;
	size_t count = 0;
	char* tmp = str;
	char* last_comma = 0;
	char delims[2];
	delims[0] = delim;
	delims[1] = 0;

	/* Count how many elements will be extracted. */
	while (*tmp) {
		if (delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	/* Add space for trailing token. */
	count += last_comma < (str + strlen(str) - 1);

	/* Add space for terminating null string so caller
	knows where the list of returned strings ends. */
	count++;

	result = malloc(sizeof(char*) * count);

	if (result) {
		size_t idx = 0;
		char* token = strtok(str, delims);

		while (token) {
			assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delims);
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}

int main(int argc, char *argv[])
{
	char *type = NULL;
	const char *action;
	const char* usage = "usage: git-credential-wincred [--type=session]|[-s] <get|store|erase>\n";
	char** tokens;

	/* min length is 2, max length is 4.*/
	if ((argc < 2) || (argc > 3)) {
		die(usage);
	}

	// the last one is <get|store|erase>
	action = argv[argc - 1];

	if (3 == argc) {
		/* argv[1] is [--type=session]|[-s] */
		char* option = argv[1];

		if (strchr(option, '=')) {
			tokens = split(argv[1], '=');
			char* key = *(tokens);
			if (strncmp(key, OPTION_KEY, strlen(key))) {
				die(usage);
			}
			char* value = *(tokens + 1);
			if (strncmp(value, PERSIST_SESSION, strlen(value))) {
				die(usage);
			}
			if (value != NULL) {
				type = value;
			}
		} else {
			if (strncmp(option, PERSIST_SESSION_SHORT, strlen(option))) {
				die(usage);
			}
			type = option;
		}
	}

	/* git use binary pipes to avoid CRLF-issues */
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);

	read_credential();

	load_cred_funcs();

	if (!protocol || !(host || path))
		return 0;

	/* prepare 'target', the unique key for the credential */
	wcscpy(target, L"git:");
	wcsncat(target, protocol, ARRAY_SIZE(target));
	wcsncat(target, L"://", ARRAY_SIZE(target));
	if (wusername) {
		wcsncat(target, wusername, ARRAY_SIZE(target));
		wcsncat(target, L"@", ARRAY_SIZE(target));
	}
	if (host)
		wcsncat(target, host, ARRAY_SIZE(target));
	if (path) {
		wcsncat(target, L"/", ARRAY_SIZE(target));
		wcsncat(target, path, ARRAY_SIZE(target));
	}

	if (!strcmp(action, "get"))
		get_credential();
	else if (!strcmp(action, "store"))
		store_credential(type);
	else if (!strcmp(action, "erase"))
		erase_credential();
	/* otherwise, ignore unknown action */
	return 0;
}
