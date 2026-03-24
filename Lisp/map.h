#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_types.h"

void init_bst(bst* v);
void free_hash();

void insert(bst* v, const char* key, Value* value);
bool get(bst* v, const char* key, Value** out_value);
bool contains(bst* v, const char* key);
void erase(bst* v, const char* key);

void print_bst(bst* v);
void free_bst(bst* v);