#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <stdbool.h>

typedef enum {
    LPAREN,
    RPAREN,
    NUMBER,
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
	VAL_SYMBOL,
	VAL_LIST
} ValueType;

typedef struct Value Value;

struct Value {
	ValueType type;
	union {
		long long number;
		char* symbol;
		struct {
			Value** items;
			int count;
			int cap;
		} list;
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
	long long value;
}node;

typedef struct bst {
	node* head;
}bst;

typedef struct Env {
	struct Env* parent;
	bst variables;
} Env;

#endif
