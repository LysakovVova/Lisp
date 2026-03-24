#ifndef PARSER_H
#define PARSER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_types.h"

void free_value(Value* v);
Value* make_number(long long x);
Value* make_bolian(bool b);
Value* make_lambda(Value* params, Value* body, Env* closure);
char* my_strdup(const char* s);
Value* make_symbol(const char* s);
Value* make_list(void);
void list_push(Value* list, Value* item);
Value* copy_value(Value* v);

Value* read_expr(Line_token* tokens, int* pos);
void print_value(Value* v);
void free_value(Value* v);

#endif
