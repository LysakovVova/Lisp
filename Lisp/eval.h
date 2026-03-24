#ifndef EVAL_H
#define EVAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "expr.h"
#include "env.h"

long long eval_number(Value* expr, int* ok, Env* env);
Value* eval(Value* expr, Env* env);

#endif#endif