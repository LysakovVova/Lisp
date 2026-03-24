#include "env.h"

Env* create_env(Env* parent) {
	Env* env = (Env*)malloc(sizeof(Env));
	env->parent = parent;
	init_bst(&env->variables);
	return env;
}

Value* env_get(Env* env, const char* name) {
	for (Env* cur = env; cur != NULL; cur = cur->parent) {
		Value* stored = NULL;
		bool found = get(&cur->variables, name, &stored);
		if (found) {
			return copy_value(stored);
		}
	}
	return NULL;
}

void env_set(Env* env, const char* name, Value* value) {
	insert(&env->variables, name, value);
}

void free_env(Env* env) {
	if (env == NULL) return;
	free_bst(&env->variables);
	free(env);
}
