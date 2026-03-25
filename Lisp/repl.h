#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "expr.h"
#include "eval.h"
#include "env.h"
#include "base_types.h"


void read_line(const char* line, Env* env);