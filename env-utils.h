#ifndef ENV_UTILS_H
#define ENV_UTILS_H

typedef struct _env_buffer env_buffer;

struct _env_buffer
{
	int size;
	char** env;
};

int env_buffer_init(env_buffer *self, const char * const *start_env);
void env_buffer_close(env_buffer *self);
int env_buffer_add(env_buffer *self, const char *entry);
char **env_buffer_copy_env(env_buffer *self);
void env_buffer_free_env(char **env);
char *env_buffer_to_str(char **env);
void env_buffer_free_str(char *str);
#endif
