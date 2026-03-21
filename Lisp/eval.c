#include "eval.h"

Env* create_env(Env* parent) {
	Env* env = (Env*)malloc(sizeof(Env));
	env->parent = parent;
	init_bst(&env->variables);
	return env;
}

Value* env_get(Env* env, const char* name) {
	long long res;
	bool found = get(&env->variables, name, &res);
	if (!found) return NULL;

	Value* v = (Value*)malloc(sizeof(Value));
	v->type = VAL_NUMBER;
	v->number = res;
	return v;
}

void env_set(Env* env, const char* name, long long value) {
	insert(&env->variables, name, value);
}

void free_env(Env* env) {
	if (env->parent) {
		free_env(env->parent);
	}
	free_bst(&env->variables);
	free(env);
}



Value* eval(Value* expr, Env* env) {
	if (expr->type == VAL_NUMBER) {
		return make_number(expr->number);
	}

	if (expr->type == VAL_SYMBOL) {
		Value* v = env_get(env, expr->symbol);

		if (v == NULL) {
			printf("Error: unknown symbol %s\n", expr->symbol);
			return NULL;
		}
		return v;
	}

	if (expr->type == VAL_LIST) {
		if (expr->list.count == 0) {
			printf("Error: empty list\n");
			return NULL;
		}

		Value* first = expr->list.items[0];
		if (first->type != VAL_SYMBOL) {
			printf("Error: first element is not a function\n");
			return NULL;
		}

		char* op = first->symbol;

		if (strcmp(op, "+") == 0) {
			long long ans = 0;
			for (int i = 1; i < expr->list.count; i++) {
				int ok = 1;
				long long x = eval_number(expr->list.items[i], &ok, env);
				if (!ok) return NULL;
				ans += x;
			}
			return make_number(ans);
		}

		if (strcmp(op, "*") == 0) {
			long long ans = 1;
			for (int i = 1; i < expr->list.count; i++) {
				int ok = 1;
				long long x = eval_number(expr->list.items[i], &ok, env);
				if (!ok) return NULL;
				ans *= x;
			}
			return make_number(ans);
		}

		if (strcmp(op, "-") == 0) {
			if (expr->list.count < 2) {
				printf("Error: - expects at least 1 argument\n");
				return NULL;
			}

			int ok = 1;
			long long ans = eval_number(expr->list.items[1], &ok, env);
			if (!ok) return NULL;

			if (expr->list.count == 2) {
				return make_number(-ans);
			}

			for (int i = 2; i < expr->list.count; i++) {
				long long x = eval_number(expr->list.items[i], &ok, env);
				if (!ok) return NULL;
				ans -= x;
			}

			return make_number(ans);
		}

		if (strcmp(op, "/") == 0) {
			if (expr->list.count < 3) {
				printf("Error: / expects at least 2 arguments\n");
				return NULL;
			}

			int ok = 1;
			long long ans = eval_number(expr->list.items[1], &ok, env);
			if (!ok) return NULL;

			for (int i = 2; i < expr->list.count; i++) {
				long long x = eval_number(expr->list.items[i], &ok, env);
				if (!ok) return NULL;
				if (x == 0) {
					printf("Error: division by zero\n");
					return NULL;
				}
				ans /= x;
			}

			return make_number(ans);
		}

		if (strcmp("define", op) == 0) {
			if (expr->list.count != 3) {
				printf("Error: define expects exactly 2 arguments\n");
				return NULL;
			}
			Value* var = expr->list.items[1];
			if (var->type != VAL_SYMBOL) {
				printf("Error: first argument of define must be a symbol\n");
				return NULL;
			}
			int ok = 1;
			long long value = eval_number(expr->list.items[2], &ok, env);
			if (!ok) return NULL;
			env_set(env, var->symbol, value);
			return make_number(value); // return something
		}

		printf("Error: unknown function %s\n", op);
		return NULL;
	}

	printf("Error: bad expression\n");
	return NULL;
}

long long eval_number(Value* expr, int* ok, Env* env) {
	Value* v = eval(expr, env);
	if (v == NULL) {
		*ok = 0;
		return 0;
	}

	if (v->type == VAL_SYMBOL) {
		Value* sym_val = env_get(env, v->symbol);
		if (sym_val == NULL) {
			printf("Error: unknown symbol %s\n", v->symbol);
			*ok = 0;
			return 0;
		}
		v = sym_val;
	} else if (v->type != VAL_NUMBER) {
		printf("Error: expected a number, got something else\n");
		free_value(v);
		*ok = 0;
		return 0;
	}
	long long x = v->number;
	free_value(v);
	*ok = 1;
	return x;
}
