#ifndef PARSER_H
#define PARSER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_types.h"

void free_value(Value* v);
Value* make_number(long long x);
char* my_strdup(const char* s);
Value* make_symbol(const char* s);
Value* make_list(void);
void list_push(Value* list, Value* item);

Value* read_expr(Line_token* tokens, int* pos);
void print_value(Value* v);
void free_value(Value* v);

#endif
