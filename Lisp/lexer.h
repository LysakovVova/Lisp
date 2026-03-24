#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_types.h"

void read_number(char* line, int* idx, long long* ans);
void read_symbol(char* line, int* idx, char** ans);
void read_string(char* line, int* idx, char** ans, bool* ok);
void reader(Line_token* vec, char* line);
void print_token(Line_token* vec);
void free_tokens(Line_token* vec);

#endif