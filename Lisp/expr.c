#include "expr.h"

Value* make_number(long long x) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_NUMBER;
	v->number = x;
	return v;
}
Value* make_bolian(bool b) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_BOLIAN;
	v->bolian = b;
	return v;
}
Value* make_lambda(Value* params, Value* body, Env* closure) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_LAMBDA;
	v->lambda.params = params;
	v->lambda.body = body;
	v->lambda.closure = closure;
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

	if (strcmp("#t", s) == 0) {
		v->type = VAL_BOLIAN;
		v->bolian = true;
	}
	else if (strcmp("#f", s) == 0) {
		v->type = VAL_BOLIAN;
		v->bolian = false;
	}
	else {
		v->type = VAL_SYMBOL;
		v->symbol = my_strdup(s);
	}
	return v;
}
Value* make_list(void) {
	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_LIST;
	v->list.count = 0;
	v->list.cap = 2;
	v->list.items = (Value**)malloc(v->list.cap * sizeof(Value*));
	v->list.dotted = false;
	v->list.tail = NULL;
	return v;
}
void list_push(Value* list, Value* item) {
	if (list->list.count == list->list.cap) {
		list->list.cap *= 2;
		list->list.items = (Value**)realloc(list->list.items, list->list.cap * sizeof(Value*));
	}
	list->list.items[list->list.count++] = item;
}

Value* copy_value(Value* v) {
	if (v == NULL) return NULL;
	if (v->type == VAL_NUMBER) return make_number(v->number);
	if (v->type == VAL_BOLIAN) return make_bolian(v->bolian);
	if (v->type == VAL_SYMBOL) return make_symbol(v->symbol);

	if (v->type == VAL_LIST) {
		Value* res = make_list();
		for (int i = 0; i < v->list.count; ++i) {
			Value* copy_val = copy_value(v->list.items[i]);
			if (copy_val == NULL) {
				free_value(res);
				return NULL;
			}
			list_push(res, copy_val);
		}
		if (v->list.dotted) {
			Value* tail_copy = copy_value(v->list.tail);
			if (tail_copy == NULL) {
				free_value(res);
				return NULL;
			}
			res->list.dotted = true;
			res->list.tail = tail_copy;
		}
		return res;
	}

	if (v->type == VAL_LAMBDA) {
		Value* params = copy_value(v->lambda.params);
		Value* body = copy_value(v->lambda.body);
		if (params == NULL || body == NULL) {
			free_value(params);
			free_value(body);
			return NULL;
		}
		return make_lambda(params, body, v->lambda.closure);
	}

	return NULL;
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

	if (cur.type == QUOTE) {
		(*pos)++;
		if (tokens->a[*pos].type == END || tokens->a[*pos].type == RPAREN) {
			printf("Error: expected expression after '\''\n");
			return NULL;
		}

		Value* quoted_expr = read_expr(tokens, pos);
		if (quoted_expr == NULL) {
			return NULL;
		}

		Value* quote_form = make_list();
		list_push(quote_form, make_symbol("quote"));
		list_push(quote_form, quoted_expr);
		return quote_form;
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

			if (tokens->a[*pos].type == SYMBOL && strcmp(tokens->a[*pos].text, ".") == 0) {
				if (list->list.count == 0) {
					printf("Error: unexpected '.'\n");
					free_value(list);
					return NULL;
				}

				(*pos)++;
				Value* tail = read_expr(tokens, pos);
				if (tail == NULL) {
					free_value(list);
					return NULL;
				}

				if (tokens->a[*pos].type != RPAREN) {
					printf("Error: dotted pair must have exactly one tail expression\n");
					free_value(tail);
					free_value(list);
					return NULL;
				}

				list->list.dotted = true;
				list->list.tail = tail;
				break;
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
	else if (v->type == VAL_BOLIAN) {
		printf("%s", v->bolian ? "#t" : "#f");
	}
	else if (v->type == VAL_LAMBDA) {
		printf("<lambda>");
	}
	else if (v->type == VAL_LIST) {
		printf("(");
		for (int i = 0; i < v->list.count; i++) {
			if (i > 0) printf(" ");
			print_value(v->list.items[i]);
		}
		if (v->list.dotted) {
			if (v->list.count > 0) printf(" ");
			printf(". ");
			print_value(v->list.tail);
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
		if (v->list.dotted) {
			free_value(v->list.tail);
		}
	}
	else if (v->type == VAL_LAMBDA) {
		free_value(v->lambda.params);
		free_value(v->lambda.body);
	}

	free(v);
}