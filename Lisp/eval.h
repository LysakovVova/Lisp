#ifndef EVAL_H
#define EVAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "map.h"
#include "expr.h"

Env* create_env(Env* parent);
Value* env_get(Env* env, const char* name);
void env_set(Env* env, const char* name, long long value);
void free_env(Env* env);

long long eval_number(Value* expr, int* ok, Env* env);
Value* eval(Value* expr, Env* env);

#endif