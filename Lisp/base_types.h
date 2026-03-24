#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <stdbool.h>

typedef enum {
    LPAREN,
    RPAREN,
    QUOTE,
    NUMBER,
    STRING,
    SYMBOL,
    END
} TokenType;

typedef struct {
    TokenType type;
    char* text;
    long long number;
} Token;

typedef struct {
    Token* a;
    int cup;
    int n;
} Line_token;

typedef enum {
	VAL_NUMBER,
	VAL_STRING,
	VAL_SYMBOL,
	VAL_LIST,
	VAL_BOLIAN,
	VAL_LAMBDA,
	VAL_VOID,
} ValueType;

typedef struct Value Value;

typedef struct Env Env;

struct Value {
	ValueType type;
	union {
		long long number;
		bool bolian;
		char* string;
		char* symbol;
		struct {
			Value** items;
			int count;
			int cap;
			bool dotted;
			Value* tail;
		} list;
		struct {
			Value* params;
			Value* body;
			Env* closure;
		} lambda;
	};
};

typedef struct pair_hash {
	long long first;
	long long second;
} pair_hash;

typedef struct pair_bst {
	bool first;
	long long second;
} pair_bst;

typedef struct node {
	struct node* left;
	struct node* right;
	long long key;
	int prio;
	char* key_string;
	Value* value;
} node;

typedef struct bst {
	node* head;
} bst;

struct Env {
	struct Env* parent;
	bst variables;
};

#endif
