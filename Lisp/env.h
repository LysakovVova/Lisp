#ifndef ENV_H
#define ENV_H

#include "map.h"
#include "expr.h"

Env* create_env(Env* parent);
Value* env_get(Env* env, const char* name);
void env_set(Env* env, const char* name, Value* value);
void free_env(Env* env);

#endif
