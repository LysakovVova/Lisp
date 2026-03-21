#include "expr.h"

Value* make_number(long long x) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_NUMBER;
	v->number = x;
	return v;
}
static char* my_strdup(const char* s) {
	int n = 0;
	while (s[n]) n++;

	char* copy = (char*)malloc((n + 1) * sizeof(char));
	for (int i = 0; i <= n; i++) {
		copy[i] = s[i];
	}
	return copy;
}

Value* make_symbol(const char* s) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_SYMBOL;
	v->symbol = my_strdup(s);
	return v;
}
Value* make_list(void) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_LIST;
	v->list.count = 0;
	v->list.cap = 2;
	v->list.items = (Value**)malloc(v->list.cap * sizeof(Value*));
	return v;
}
void list_push(Value* list, Value* item) {
	if (list->list.count == list->list.cap) {
		list->list.cap *= 2;
		list->list.items = (Value**)realloc(list->list.items, list->list.cap * sizeof(Value*));
	}
	list->list.items[list->list.count++] = item;
}


Value* read_expr(Line_token* tokens, int* pos) {
	Token cur = tokens->a[*pos];

	if (cur.type == NUMBER) {
		(*pos)++;
		return make_number(cur.number);
	}

	if (cur.type == SYMBOL) {
		(*pos)++;
		return make_symbol(cur.text);
	}

	if (cur.type == LPAREN) {
		(*pos)++;
		Value* list = make_list();

		while (tokens->a[*pos].type != RPAREN) {
			if (tokens->a[*pos].type == END) {
				printf("Error: expected ')'\n");
				free_value(list);
				return NULL;
			}

			Value* elem = read_expr(tokens, pos);
			if (elem == NULL) {
				free_value(list);
				return NULL;
			}
			list_push(list, elem);
		}

		(*pos)++;
		return list;
	}

	printf("Error: unexpected token\n");
	return NULL;
}

void print_value(Value* v) {
	if (v->type == VAL_NUMBER) {
		printf("%lld", v->number);
	}
	else if (v->type == VAL_SYMBOL) {
		printf("%s", v->symbol);
	}
	else if (v->type == VAL_LIST) {
		printf("(");
		for (int i = 0; i < v->list.count; i++) {
			if (i > 0) printf(" ");
			print_value(v->list.items[i]);
		}
		printf(")");
	}
}

void free_value(Value* v) {
	if (v == NULL) return;

	if (v->type == VAL_SYMBOL) {
		free(v->symbol);
	}
	else if (v->type == VAL_LIST) {
		for (int i = 0; i < v->list.count; i++) {
			free_value(v->list.items[i]);
		}
		free(v->list.items);
	}

	free(v);
}