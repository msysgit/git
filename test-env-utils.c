#include "cache.h"
#include "env-utils.h"
#include <stddef.h>
#include <stdio.h>
int
main(int argc,
	char **argv)
{
	int result;
	env_buffer env_buf;
	char **env;
	env = NULL;
	result = 0;
	result = env_buffer_init(&env_buf, NULL);
	printf("env_buffer_init finished\n");
	if (result == 0) {
		result = env_buffer_add(&env_buf, "env1=value1"); 
		printf("env_buffer_add finished\n");
	}
	if (result == 0) {
		result = env_buffer_add(&env_buf, "env2=value2");
		printf("env_buffer_add finished\n");
	}
	if (result == 0) {
		result = env_buf.size == 2 ? 0 : -1;
	}
	if (result == 0)
	{
		env = env_buffer_copy_env(&env_buf);
	}
	if (result == 0) {
		char *flat_env;

		flat_env = env_buffer_to_str(env);
		printf("%s\n", flat_env);
		env_buffer_free_str(flat_env);
	}
	if (env) {
		env_buffer_free_env(env);
	}
	env_buffer_close(&env_buf);
	return result;
}
