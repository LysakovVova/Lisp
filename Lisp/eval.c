#include "eval.h"

static Value* apply_begin(Value* expr, Env* env);
static Value* eval_impl(Value* expr, Env* env);
static BuiltinFn find_builtin(const char* op);

static int g_eval_depth = 0;
static const int MAX_EVAL_DEPTH = 700;

static bool eval_enter(void) {
	if (g_eval_depth >= MAX_EVAL_DEPTH) {
		printf("Error: recursion limit exceeded (%d)\n", MAX_EVAL_DEPTH);
		return false;
	}
	g_eval_depth++;
	return true;
}

static void eval_leave(void) {
	if (g_eval_depth > 0) g_eval_depth--;
}

static bool is_truthy_value(const Value* v) {
	return !(v->type == VAL_BOLIAN && v->bolian == false);
}

static bool eval_truthy(Value* expr, Env* env, bool* out) {
	Value* v = eval(expr, env);
	if (v == NULL) return false;
	*out = is_truthy_value(v);
	free_value(v);
	return true;
}

static bst g_memo_cache;
static bool g_memo_inited = false;
static int g_memo_cache_size = 0;
static int g_memo_cache_limit = 256;

static void memo_init(void) {
	if (g_memo_inited) return;
	init_bst(&g_memo_cache);
	g_memo_inited = true;
}

static void memo_clear_all(void) {
	if (!g_memo_inited) return;
	free_bst(&g_memo_cache);
	init_bst(&g_memo_cache);
	g_memo_cache_size = 0;
}

static bool key_append_char(char** buf, int* len, int* cap, char c) {
	if (*len + 2 >= *cap) {
		int new_cap = (*cap == 0 ? 64 : (*cap * 2));
		char* tmp = (char*)realloc(*buf, new_cap);
		if (tmp == NULL) return false;
		*buf = tmp;
		*cap = new_cap;
	}
	(*buf)[(*len)++] = c;
	(*buf)[*len] = '\0';
	return true;
}

static bool key_append_cstr(char** buf, int* len, int* cap, const char* s) {
	for (int i = 0; s[i] != '\0'; ++i) {
		if (!key_append_char(buf, len, cap, s[i])) return false;
	}
	return true;
}

static bool key_append_ll(char** buf, int* len, int* cap, long long x) {
	char tmp[64];
	snprintf(tmp, sizeof(tmp), "%lld", x);
	return key_append_cstr(buf, len, cap, tmp);
}

static bool key_append_escaped(char** buf, int* len, int* cap, const char* s) {
	for (int i = 0; s[i] != '\0'; ++i) {
		char c = s[i];
		if (c == '\\' || c == '|' || c == ',' || c == '[' || c == ']') {
			if (!key_append_char(buf, len, cap, '\\')) return false;
		}
		if (!key_append_char(buf, len, cap, c)) return false;
	}
	return true;
}

static bool serialize_value_for_key(const Value* v, char** buf, int* len, int* cap) {
	if (v == NULL) return false;

	switch (v->type) {
	case VAL_NUMBER:
		return key_append_char(buf, len, cap, 'N') && key_append_ll(buf, len, cap, v->number);

	case VAL_STRING:
		return key_append_char(buf, len, cap, 'T') && key_append_escaped(buf, len, cap, v->string);

	case VAL_BOLIAN:
		return key_append_char(buf, len, cap, 'B') && key_append_char(buf, len, cap, v->bolian ? '1' : '0');

	case VAL_SYMBOL:
		return key_append_char(buf, len, cap, 'S') && key_append_escaped(buf, len, cap, v->symbol);

	case VAL_LIST:
		if (!key_append_char(buf, len, cap, '[')) return false;
		for (int i = 0; i < v->list.count; ++i) {
			if (i > 0 && !key_append_char(buf, len, cap, ',')) return false;
			if (!serialize_value_for_key(v->list.items[i], buf, len, cap)) return false;
		}
		if (v->list.dotted) {
			if (!key_append_cstr(buf, len, cap, "|.")) return false;
			if (!serialize_value_for_key(v->list.tail, buf, len, cap)) return false;
		}
		return key_append_char(buf, len, cap, ']');

	default:
		return false;
	}
}

static bool is_impure_op(const char* s) {
	return strcmp(s, "set!") == 0 ||
		strcmp(s, "define") == 0 ||
		strcmp(s, "display") == 0 ||
		strcmp(s, "newline") == 0 ||
		strcmp(s, "for-each") == 0;
}

static bool has_impure_effects(Value* expr) {
	if (expr == NULL) return false;
	if (expr->type != VAL_LIST) return false;

	if (expr->list.count > 0 && expr->list.items[0]->type == VAL_SYMBOL) {
		const char* op = expr->list.items[0]->symbol;
		if (is_impure_op(op)) return true;
		if (strcmp(op, "quote") == 0) return false;
	}

	for (int i = 0; i < expr->list.count; ++i) {
		if (has_impure_effects(expr->list.items[i])) return true;
	}
	if (expr->list.dotted && has_impure_effects(expr->list.tail)) return true;
	return false;
}

static bool symbol_in_params(Value* params, const char* name) {
	if (params == NULL || params->type != VAL_LIST) return false;
	for (int i = 0; i < params->list.count; ++i) {
		Value* p = params->list.items[i];
		if (p->type == VAL_SYMBOL && strcmp(p->symbol, name) == 0) return true;
	}
	return false;
}

typedef struct LocalScope {
	Value* bindings;
	struct LocalScope* parent;
} LocalScope;

static bool symbol_in_bindings(Value* bindings, const char* name) {
	if (bindings == NULL || bindings->type != VAL_LIST) return false;
	for (int i = 0; i < bindings->list.count; ++i) {
		Value* b = bindings->list.items[i];
		if (b->type != VAL_LIST || b->list.count < 1) continue;
		Value* n = b->list.items[0];
		if (n->type == VAL_SYMBOL && strcmp(n->symbol, name) == 0) return true;
	}
	return false;
}

static bool symbol_in_local_scope(LocalScope* scope, const char* name) {
	for (LocalScope* cur = scope; cur != NULL; cur = cur->parent) {
		if (symbol_in_bindings(cur->bindings, name)) return true;
	}
	return false;
}

static bool has_external_symbols_impl(Value* expr, Value* params, const char* self_name, LocalScope* scope) {
	if (expr == NULL) return false;

	if (expr->type == VAL_SYMBOL) {
		if (strcmp(expr->symbol, "else") == 0) return false;
		if (symbol_in_params(params, expr->symbol)) return false;
		if (symbol_in_local_scope(scope, expr->symbol)) return false;
		if (self_name != NULL && strcmp(expr->symbol, self_name) == 0) return false;
		if (find_builtin(expr->symbol) != NULL) return false;
		return true;
	}

	if (expr->type != VAL_LIST) return false;

	if (expr->list.count > 0 && expr->list.items[0]->type == VAL_SYMBOL &&
		strcmp(expr->list.items[0]->symbol, "quote") == 0) {
		return false;
	}

	if (expr->list.count > 0 && expr->list.items[0]->type == VAL_SYMBOL) {
		const char* op = expr->list.items[0]->symbol;

		if (strcmp(op, "cond") == 0) {
			for (int i = 1; i < expr->list.count; ++i) {
				Value* clause = expr->list.items[i];
				if (clause->type != VAL_LIST || clause->list.count < 1) continue;

				int start = 0;
				if (clause->list.items[0]->type == VAL_SYMBOL && strcmp(clause->list.items[0]->symbol, "else") == 0) {
					start = 1;
				}

				for (int j = start; j < clause->list.count; ++j) {
					if (has_external_symbols_impl(clause->list.items[j], params, self_name, scope)) return true;
				}
			}
			return false;
		}

		if (strcmp(op, "case") == 0) {
			if (expr->list.count > 1 && has_external_symbols_impl(expr->list.items[1], params, self_name, scope)) return true;
			for (int i = 2; i < expr->list.count; ++i) {
				Value* clause = expr->list.items[i];
				if (clause->type != VAL_LIST || clause->list.count < 1) continue;
				for (int j = 1; j < clause->list.count; ++j) {
					if (has_external_symbols_impl(clause->list.items[j], params, self_name, scope)) return true;
				}
			}
			return false;
		}

		if ((strcmp(op, "let") == 0 || strcmp(op, "let*") == 0 || strcmp(op, "letrec") == 0) && expr->list.count >= 3) {
			Value* bindings = expr->list.items[1];
			if (bindings->type == VAL_LIST) {
				for (int i = 0; i < bindings->list.count; ++i) {
					Value* b = bindings->list.items[i];
					if (b->type == VAL_LIST && b->list.count == 2) {
						if (has_external_symbols_impl(b->list.items[1], params, self_name, scope)) return true;
					}
				}
			}

			LocalScope inner;
			inner.bindings = (bindings->type == VAL_LIST ? bindings : NULL);
			inner.parent = scope;
			for (int i = 2; i < expr->list.count; ++i) {
				if (has_external_symbols_impl(expr->list.items[i], params, self_name, &inner)) return true;
			}
			return false;
		}
	}

	for (int i = 0; i < expr->list.count; ++i) {
		if (has_external_symbols_impl(expr->list.items[i], params, self_name, scope)) return true;
	}
	if (expr->list.dotted && has_external_symbols_impl(expr->list.tail, params, self_name, scope)) return true;
	return false;
}

static bool has_external_symbols(Value* expr, Value* params, const char* self_name) {
	return has_external_symbols_impl(expr, params, self_name, NULL);
}

static bool can_memoize_lambda_call(Value* fn, Value* call_expr) {
	if (fn == NULL || fn->type != VAL_LAMBDA) return false;
	if (call_expr->list.items[0]->type != VAL_SYMBOL) return false;
	const char* self_name = call_expr->list.items[0]->symbol;
	if (has_impure_effects(fn->lambda.body)) return false;
	if (has_external_symbols(fn->lambda.body, fn->lambda.params, self_name)) return false;
	return true;
}

static char* make_memo_key(const char* fn_name, Value** args, int arg_count) {
	char* key = NULL;
	int len = 0, cap = 0;

	if (!key_append_cstr(&key, &len, &cap, fn_name)) {
		free(key);
		return NULL;
	}
	if (!key_append_char(&key, &len, &cap, '|')) {
		free(key);
		return NULL;
	}

	for (int i = 0; i < arg_count; ++i) {
		if (i > 0 && !key_append_char(&key, &len, &cap, ';')) {
			free(key);
			return NULL;
		}
		if (!serialize_value_for_key(args[i], &key, &len, &cap)) {
			free(key);
			return NULL;
		}
	}

	return key;
}

static Value* apply_plus(Value* expr, Env* env) {
	if (expr->list.count == 1) {
		return make_number(0);
	}
	long long ans = 0;
	for (int i = 1; i < expr->list.count; i++) {
		int ok = 1;
		long long x = eval_number(expr->list.items[i], &ok, env);
		if (!ok) return NULL;
		ans += x;
	}
	return make_number(ans);
}

static Value* apply_mul(Value* expr, Env* env) {
	if (expr->list.count == 1) {
		return make_number(1);
	}
	long long ans = 1;
	for (int i = 1; i < expr->list.count; i++) {
		int ok = 1;
		long long x = eval_number(expr->list.items[i], &ok, env);
		if (!ok) return NULL;
		ans *= x;
	}
	return make_number(ans);
}

static Value* apply_sub(Value* expr, Env* env) {
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

static Value* apply_div(Value* expr, Env* env) {
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

static Value* apply_define(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: define expects exactly 2 arguments\n");
		return NULL;
	}

	Value* target = expr->list.items[1];

	// (define name value)
	if (target->type == VAL_SYMBOL) {
		Value* value = eval(expr->list.items[2], env);
		if (value == NULL) return NULL;

		Value* stored = copy_value(value);
		if (stored == NULL) {
			free_value(value);
			return NULL;
		}

		env_set(env, target->symbol, stored);
		memo_clear_all();
		return value;
	}

	// (define (fname p1 p2 ...) body)  =>  (define fname (lambda (p1 p2 ...) body)
	if (target->type == VAL_LIST) {
		if (target->list.count < 1) {
			printf("Error: function signature in define is empty\n");
			return NULL;
		}

		Value* fname = target->list.items[0];
		if (fname->type != VAL_SYMBOL) {
			printf("Error: function name in define must be a symbol\n");
			return NULL;
		}

		Value* params = make_list();
		for (int i = 1; i < target->list.count; i++) {
			Value* p = target->list.items[i];
			if (p->type != VAL_SYMBOL) {
				printf("Error: function parameters must be symbols\n");
				free_value(params);
				return NULL;
			}
			Value* p_copy = copy_value(p);
			if (p_copy == NULL) {
				free_value(params);
				return NULL;
			}
			list_push(params, p_copy);
		}

		Value* body_copy = copy_value(expr->list.items[2]);
		if (body_copy == NULL) {
			free_value(params);
			return NULL;
		}

		Value* lambda_value = make_lambda(params, body_copy, env);

		Value* stored = copy_value(lambda_value);
		if (stored == NULL) {
			free_value(lambda_value);
			return NULL;
		}

		env_set(env, fname->symbol, stored);
		memo_clear_all();
		return lambda_value;
	}

	printf("Error: first argument of define must be a symbol or a function signature list\n");
	return NULL;
}

static Value* apply_lambda(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: lambda expects parameters and at least 1 body expression\n");
		return NULL;
	}

	Value* params = expr->list.items[1];
	if (params->type != VAL_LIST) {
		printf("Error: lambda parameters must be a list\n");
		return NULL;
	}

	for (int i = 0; i < params->list.count; i++) {
		if (params->list.items[i]->type != VAL_SYMBOL) {
			printf("Error: lambda parameters must be symbols\n");
			return NULL;
		}
	}

	Value* params_copy = copy_value(params);
	Value* body_copy = NULL;

	if (expr->list.count == 3) {
		body_copy = copy_value(expr->list.items[2]);
	}
	else {
		body_copy = make_list();
		if (body_copy != NULL) {
			list_push(body_copy, make_symbol("begin"));
			for (int i = 2; i < expr->list.count; ++i) {
				Value* part = copy_value(expr->list.items[i]);
				if (part == NULL) {
					free_value(body_copy);
					body_copy = NULL;
					break;
				}
				list_push(body_copy, part);
			}
		}
	}

	if (params_copy == NULL || body_copy == NULL) {
		free_value(params_copy);
		free_value(body_copy);
		return NULL;
	}

	return make_lambda(params_copy, body_copy, env);
}

static Value* apply_compare(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: boolean function expects at least 2 arguments\n");
		return NULL;
	}

	const char* op = expr->list.items[0]->symbol;
	bool res = true;

	int ok = 1;
	long long first = eval_number(expr->list.items[1], &ok, env);
	if (!ok) return NULL;

	for (int i = 2; i < expr->list.count; i++) {
		long long second = eval_number(expr->list.items[i], &ok, env);
		if (!ok) return NULL;

		     if (strcmp(op, "=") == 0)  res = (res && (first == second));
		else if (strcmp(op, ">") == 0)  res = (res && (first > second));
		else if (strcmp(op, "<") == 0)  res = (res && (first < second));
		else if (strcmp(op, ">=") == 0) res = (res && (first >= second));
		else if (strcmp(op, "<=") == 0) res = (res && (first <= second));

		first = second;
	}

	return make_bolian(res);
}

static Value* apply_not(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: not expects exactly 1 argument\n");
		return NULL;
	}
	bool truthy = false;
	if (!eval_truthy(expr->list.items[1], env, &truthy)) {
		printf("Error: failed to evaluate argument of not\n");
		return NULL;
	}
	return make_bolian(!truthy);
}

static Value* apply_and(Value* expr, Env* env) {

	if (expr->list.count == 1) {
		return make_bolian(true);
	}

	Value* last = NULL;
	for (int i = 1; i < expr->list.count; i++) {
		if (last != NULL) free_value(last);
		last = eval(expr->list.items[i], env);
		if (last == NULL) {
			printf("Error: failed to evaluate argument of and\n");
			return NULL;
		}

		if (!is_truthy_value(last)) {
			return last;
		}
	}

	return last;
}

static Value* apply_or(Value* expr, Env* env) {

	if (expr->list.count == 1) {
		return make_bolian(false);
	}

	for (int i = 1; i < expr->list.count; i++) {
		Value* v = eval(expr->list.items[i], env);
		if (v == NULL) {
			printf("Error: failed to evaluate argument of or\n");
			return NULL;
		}

		if (is_truthy_value(v)) {
			return v;
		}

		free_value(v);
	}

	return make_bolian(false);
}

static Value* apply_if(Value* expr, Env* env) {
	if (expr->list.count != 3 && expr->list.count != 4) {
		printf("Error: if expects condition, then-expr, and optional else-expr\n");
		return NULL;
	}
	Value* cond = eval(expr->list.items[1], env);
	if (cond == NULL) {
		printf("Error: failed to evaluate condition in if\n");
		return NULL;
	}
	bool cond_value = is_truthy_value(cond);
	free_value(cond);

	if (cond_value) {
		return eval(expr->list.items[2], env);
	}
	if (expr->list.count != 4) {
		return make_void();
	}
	return eval(expr->list.items[3], env);
}

static Value* apply_quote(Value* expr, Env* env) {
	(void)env;
	if (expr->list.count != 2) {
		printf("Error: quote expects exactly 1 argument\n");
		return NULL;
	}
	return copy_value(expr->list.items[1]);
}

static Value* apply_car(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: car expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of car\n");
		return NULL;
	}
	if (v->type != VAL_LIST) {
		printf("Error: argument of car must be a list\n");
		free_value(v);
		return NULL;
	}
	if (v->list.count == 0) {
		printf("Error: argument of car cannot be an empty list\n");
		free_value(v);
		return NULL;
	}
	Value* res = copy_value(v->list.items[0]);
	free_value(v);
	return res;
}

static Value* apply_cdr(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: cdr expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of cdr\n");
		return NULL;
	}
	if (v->type != VAL_LIST) {
		printf("Error: argument of cdr must be a list\n");
		free_value(v);
		return NULL;
	}
	if (v->list.count == 0) {
		printf("Error: argument of cdr cannot be an empty list\n");
		free_value(v);
		return NULL;
	}

	if (v->list.count == 1 && v->list.dotted) {
		Value* res = copy_value(v->list.tail);
		free_value(v);
		return res;
	}

	Value* res = make_list();
	for (int i = 1; i < v->list.count; ++i) {
		Value* copy_val = copy_value(v->list.items[i]);
		if (copy_val == NULL) {
			free_value(res);
			free_value(v);
			return NULL;
		}
		list_push(res, copy_val);
	}

	if (v->list.dotted) {
		Value* tail_copy = copy_value(v->list.tail);
		if (tail_copy == NULL) {
			free_value(res);
			free_value(v);
			return NULL;
		}
		res->list.dotted = true;
		res->list.tail = tail_copy;
	}

	free_value(v);
	return res;
}

static Value* apply_cons(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: cons expects exactly 2 arguments\n");
		return NULL;
	}

	Value* car_val = eval(expr->list.items[1], env);
	if (car_val == NULL) {
		printf("Error: failed to evaluate first argument of cons\n");
		return NULL;
	}

	Value* cdr_val = eval(expr->list.items[2], env);
	if (cdr_val == NULL) {
		printf("Error: failed to evaluate second argument of cons\n");
		free_value(car_val);
		return NULL;
	}

	Value* res = make_list();
	if (res == NULL) {
		free_value(car_val);
		free_value(cdr_val);
		return NULL;
	}

	Value* first_copy = copy_value(car_val);
	if (first_copy == NULL) {
		free_value(res);
		free_value(car_val);
		free_value(cdr_val);
		return NULL;
	}
	list_push(res, first_copy);

	if (cdr_val->type == VAL_LIST) {
		for (int i = 0; i < cdr_val->list.count; ++i) {
			Value* copy_val = copy_value(cdr_val->list.items[i]);
			if (copy_val == NULL) {
				free_value(res);
				free_value(car_val);
				free_value(cdr_val);
				return NULL;
			}
			list_push(res, copy_val);
		}

		if (cdr_val->list.dotted) {
			Value* tail_copy = copy_value(cdr_val->list.tail);
			if (tail_copy == NULL) {
				free_value(res);
				free_value(car_val);
				free_value(cdr_val);
				return NULL;
			}
			res->list.dotted = true;
			res->list.tail = tail_copy;
		}
	}
	else {
		Value* tail_copy = copy_value(cdr_val);
		if (tail_copy == NULL) {
			free_value(res);
			free_value(car_val);
			free_value(cdr_val);
			return NULL;
		}
		res->list.dotted = true;
		res->list.tail = tail_copy;
	}

	free_value(car_val);
	free_value(cdr_val);
	return res;
}

static Value* apply_null_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: null? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of null?\n");
		return NULL;
	}
	if (v->type != VAL_LIST) {
		printf("Error: argument of null? must be a list\n");
		free_value(v);
		return NULL;
	}
	bool is_null = (v->list.count == 0 && !v->list.dotted);
	free_value(v);
	return make_bolian(is_null);
}

static Value* apply_number_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: number? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of number?\n");
		return NULL;
	}
	bool is_number = (v->type == VAL_NUMBER);
	free_value(v);
	return make_bolian(is_number);
}

static Value* apply_symbol_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: symbol? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of symbol?\n");
		return NULL;
	}
	bool is_symbol = (v->type == VAL_SYMBOL);
	free_value(v);
	return make_bolian(is_symbol);
}

static Value* apply_list_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: list? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of list?\n");
		return NULL;
	}
	bool is_list = (v->type == VAL_LIST && !v->list.dotted);
	free_value(v);
	return make_bolian(is_list);
}

static Value* apply_boolian_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: bolian? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of bolian?\n");
		return NULL;
	}
	bool is_bolian = (v->type == VAL_BOLIAN);
	free_value(v);
	return make_bolian(is_bolian);
}

static Value* apply_let(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: let expects bindings and at least 1 body expression\n");
		return NULL;
	}

	int body_start = 2;
	Value* name_sym = NULL;
	Value* bindings = expr->list.items[1];

	if (bindings->type == VAL_SYMBOL) {
		if (expr->list.count < 4) {
			printf("Error: named let expects name, bindings and at least 1 body expression\n");
			return NULL;
		}
		name_sym = bindings;
		bindings = expr->list.items[2];
		body_start = 3;
	}

	if (bindings->type != VAL_LIST) {
		printf("Error: let bindings must be a list\n");
		return NULL;
	}

	Env* let_env = create_env(env);

	if (name_sym != NULL) {
		Value* params = make_list();
		if (params == NULL) {
			free_env(let_env);
			return NULL;
		}

		for (int i = 0; i < bindings->list.count; ++i) {
			Value* binding = bindings->list.items[i];
			if (binding->type != VAL_LIST || binding->list.count != 2) {
				printf("Error: each let binding must be a list: (name value)\n");
				free_value(params);
				free_env(let_env);
				return NULL;
			}

			Value* pname = binding->list.items[0];
			if (pname->type != VAL_SYMBOL) {
				printf("Error: let binding name must be a symbol\n");
				free_value(params);
				free_env(let_env);
				return NULL;
			}

			Value* pname_copy = copy_value(pname);
			if (pname_copy == NULL) {
				free_value(params);
				free_env(let_env);
				return NULL;
			}
			list_push(params, pname_copy);
		}

		Value* body_expr = NULL;
		if (expr->list.count - body_start == 1) {
			body_expr = copy_value(expr->list.items[body_start]);
		}
		else {
			body_expr = make_list();
			if (body_expr != NULL) {
				list_push(body_expr, make_symbol("begin"));
				for (int i = body_start; i < expr->list.count; ++i) {
					Value* part = copy_value(expr->list.items[i]);
					if (part == NULL) {
						free_value(body_expr);
						body_expr = NULL;
						break;
					}
					list_push(body_expr, part);
				}
			}
		}

		if (body_expr == NULL) {
			free_value(params);
			free_env(let_env);
			return NULL;
		}

		Value* loop_lambda = make_lambda(params, body_expr, let_env);
		if (loop_lambda == NULL) {
			free_value(params);
			free_value(body_expr);
			free_env(let_env);
			return NULL;
		}

		env_set(let_env, name_sym->symbol, loop_lambda);
	}

	for (int i = 0; i < bindings->list.count; ++i) {
		Value* binding = bindings->list.items[i];
		if (binding->type != VAL_LIST || binding->list.count != 2) {
			printf("Error: each let binding must be a list: (name value)\n");
			free_env(let_env);
			return NULL;
		}

		Value* name = binding->list.items[0];
		if (name->type != VAL_SYMBOL) {
			printf("Error: let binding name must be a symbol\n");
			free_env(let_env);
			return NULL;
		}

		// let: значения считаются в исходном env, без доступа к новым binding'ам
		Value* value = eval(binding->list.items[1], env);
		if (value == NULL) {
			free_env(let_env);
			return NULL;
		}

		env_set(let_env, name->symbol, value);
	}

	Value* res = NULL;
	if (name_sym == NULL) {
		for (int i = body_start; i < expr->list.count; ++i) {
			if (res != NULL) free_value(res);
			res = eval(expr->list.items[i], let_env);
			if (res == NULL) {
				free_env(let_env);
				return NULL;
			}
		}
	}
	else {
		Value* call_expr = make_list();
		if (call_expr == NULL) {
			free_env(let_env);
			return NULL;
		}

		list_push(call_expr, make_symbol(name_sym->symbol));
		for (int i = 0; i < bindings->list.count; ++i) {
			Value* arg_copy = copy_value(bindings->list.items[i]->list.items[1]);
			if (arg_copy == NULL) {
				free_value(call_expr);
				free_env(let_env);
				return NULL;
			}
			list_push(call_expr, arg_copy);
		}

		res = eval(call_expr, let_env);
		free_value(call_expr);
		if (res == NULL) {
			free_env(let_env);
			return NULL;
		}
	}

	if (res == NULL || res->type != VAL_LAMBDA) {
		free_env(let_env);
	}

	return res;
}

static Value* apply_let_star(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: let* expects bindings and at least 1 body expression\n");
		return NULL;
	}

	Value* bindings = expr->list.items[1];
	if (bindings->type != VAL_LIST) {
		printf("Error: let* bindings must be a list\n");
		return NULL;
	}

	Env* let_env = create_env(env);

	for (int i = 0; i < bindings->list.count; ++i) {
		Value* binding = bindings->list.items[i];
		if (binding->type != VAL_LIST || binding->list.count != 2) {
			printf("Error: each let* binding must be a list: (name value)\n");
			free_env(let_env);
			return NULL;
		}

		Value* name = binding->list.items[0];
		if (name->type != VAL_SYMBOL) {
			printf("Error: let* binding name must be a symbol\n");
			free_env(let_env);
			return NULL;
		}

		// let*: значения считаются последовательно, в уже расширенном let_env
		Value* value = eval(binding->list.items[1], let_env);
		if (value == NULL) {
			free_env(let_env);
			return NULL;
		}

		env_set(let_env, name->symbol, value);
	}

	Value* res = NULL;
	for (int i = 2; i < expr->list.count; ++i) {
		if (res != NULL) free_value(res);
		res = eval(expr->list.items[i], let_env);
		if (res == NULL) {
			free_env(let_env);
			return NULL;
		}
	}

	if (res == NULL || res->type != VAL_LAMBDA) {
		free_env(let_env);
	}

	return res;
}

static Value* apply_letrec(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: letrec expects bindings and at least 1 body expression\n");
		return NULL;
	}

	Value* bindings = expr->list.items[1];
	if (bindings->type != VAL_LIST) {
		printf("Error: letrec bindings must be a list\n");
		return NULL;
	}

	Env* let_env = create_env(env);

	// 1) Регистрируем имена заранее, чтобы была взаимная рекурсия
	for (int i = 0; i < bindings->list.count; ++i) {
		Value* binding = bindings->list.items[i];
		if (binding->type != VAL_LIST || binding->list.count != 2) {
			printf("Error: each letrec binding must be a list: (name value)\n");
			free_env(let_env);
			return NULL;
		}

		Value* name = binding->list.items[0];
		if (name->type != VAL_SYMBOL) {
			printf("Error: letrec binding name must be a symbol\n");
			free_env(let_env);
			return NULL;
		}

		env_set(let_env, name->symbol, make_bolian(false));
	}

	// 2) Вычисляем значения в let_env и заменяем заглушки
	for (int i = 0; i < bindings->list.count; ++i) {
		Value* binding = bindings->list.items[i];
		Value* name = binding->list.items[0];

		Value* value = eval(binding->list.items[1], let_env);
		if (value == NULL) {
			free_env(let_env);
			return NULL;
		}

		if (value->type != VAL_LAMBDA) {
			printf("Error: letrec currently supports only lambda values\n");
			free_value(value);
			free_env(let_env);
			return NULL;
		}

		env_set(let_env, name->symbol, value);
	}

	Value* res = NULL;
	for (int i = 2; i < expr->list.count; ++i) {
		if (res != NULL) free_value(res);
		res = eval(expr->list.items[i], let_env);
		if (res == NULL) {
			free_env(let_env);
			return NULL;
		}
	}

	if (res == NULL || res->type != VAL_LAMBDA) {
		free_env(let_env);
	}

	return res;
}

static bool value_equal(const Value* a, const Value* b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	if (a->type != b->type) return false;

	switch (a->type) {
	case VAL_NUMBER:
		return a->number == b->number;

	case VAL_STRING:
		return strcmp(a->string, b->string) == 0;

	case VAL_BOLIAN:
		return a->bolian == b->bolian;

	case VAL_SYMBOL:
		return strcmp(a->symbol, b->symbol) == 0;

	case VAL_LIST:
		if (a->list.count != b->list.count) return false;
		if (a->list.dotted != b->list.dotted) return false;

		for (int i = 0; i < a->list.count; ++i) {
			if (!value_equal(a->list.items[i], b->list.items[i])) return false;
		}

		if (a->list.dotted) {
			return value_equal(a->list.tail, b->list.tail);
		}
		return true;

	case VAL_LAMBDA:
		return (a->lambda.closure == b->lambda.closure) &&
			value_equal(a->lambda.params, b->lambda.params) &&
			value_equal(a->lambda.body, b->lambda.body);
	}

	return false;
}

static bool value_eq(const Value* a, const Value* b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	if (a->type != b->type) return false;

	switch (a->type) {
	case VAL_STRING:
		return strcmp(a->string, b->string) == 0;

	case VAL_BOLIAN:
		return a->bolian == b->bolian;

	case VAL_SYMBOL:
		return strcmp(a->symbol, b->symbol) == 0;

	default:
		return false;
	}
}

static bool value_eqv(const Value* a, const Value* b) {
	if (a == b) return true;
	if (a == NULL || b == NULL) return false;
	if (a->type != b->type) return false;

	switch (a->type) {
	case VAL_NUMBER:
		return a->number == b->number;

	case VAL_STRING:
		return strcmp(a->string, b->string) == 0;

	case VAL_BOLIAN:
		return a->bolian == b->bolian;

	case VAL_SYMBOL:
		return strcmp(a->symbol, b->symbol) == 0;

	default:
		return false;
	}
}

static Value* apply_equal(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: equal? expects exactly 2 arguments\n");
		return NULL;
	}

	Value* v1 = eval(expr->list.items[1], env);
	if (v1 == NULL) return NULL;

	Value* v2 = eval(expr->list.items[2], env);
	if (v2 == NULL) {
		free_value(v1);
		return NULL;
	}

	bool eq = value_equal(v1, v2);
	free_value(v1);
	free_value(v2);

	return make_bolian(eq);
}

static Value* apply_eq(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: eq? expects exactly 2 arguments\n");
		return NULL;
	}

	Value* v1 = eval(expr->list.items[1], env);
	if (v1 == NULL) return NULL;

	Value* v2 = eval(expr->list.items[2], env);
	if (v2 == NULL) {
		free_value(v1);
		return NULL;
	}

	bool eq = value_eq(v1, v2);
	free_value(v1);
	free_value(v2);

	return make_bolian(eq);
}

static Value* apply_eqv(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: eqv? expects exactly 2 arguments\n");
		return NULL;
	}

	Value* v1 = eval(expr->list.items[1], env);
	if (v1 == NULL) return NULL;

	Value* v2 = eval(expr->list.items[2], env);
	if (v2 == NULL) {
		free_value(v1);
		return NULL;
	}

	bool eq = value_eqv(v1, v2);
	free_value(v1);
	free_value(v2);

	return make_bolian(eq);
}

static Value* apply_set_val(Value* expr, Env* env) {
	if (expr->list.count != 3) {
		printf("Error: set! expects exactly 2 arguments\n");
		return NULL;
	}
	Value* target = expr->list.items[1];
	if (target->type != VAL_SYMBOL) {
		printf("Error: first argument of set! must be a symbol\n");
		return NULL;
	}

	Value* value = eval(expr->list.items[2], env);
	if (value == NULL) return NULL;

	bool updated = false;
	for (Env* cur = env; cur != NULL; cur = cur->parent) {
		if (contains(&cur->variables, target->symbol)) {
			Value* stored = copy_value(value);
			if (stored == NULL) {
				free_value(value);
				return NULL;
			}
			env_set(cur, target->symbol, stored);
			updated = true;
			break;
		}
	}

	if (!updated) {
		printf("Error: symbol '%s' not defined, cannot set!\n", target->symbol);
		free_value(value);
		return NULL;
	}

	memo_clear_all();

	return value;
}

static Value* apply_set_memo_limit(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: set-memo-limit expects exactly 1 argument\n");
		return NULL;
	}

	int ok = 1;
	long long limit = eval_number(expr->list.items[1], &ok, env);
	if (!ok) return NULL;
	if (limit < 0) {
		printf("Error: set-memo-limit expects non-negative number\n");
		return NULL;
	}

	g_memo_cache_limit = (int)limit;
	if (g_memo_cache_limit == 0 || (g_memo_inited && g_memo_cache_size > g_memo_cache_limit)) {
		memo_clear_all();
	}

	return make_number(g_memo_cache_limit);
}

static Value* apply_pair_check(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: pair? expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of pair?\n");
		return NULL;
	}
	bool is_pair = (v->type == VAL_LIST && v->list.count > 0);
	free_value(v);
	return make_bolian(is_pair);
}

static Value* apply_list(Value* expr, Env* env) {
	Value* res = make_list();
	for (int i = 1; i < expr->list.count; ++i) {
		Value* v = eval(expr->list.items[i], env);
		if (v == NULL) {
			free_value(res);
			return NULL;
		}
		list_push(res, v);
	}
	return res;
}

static Value* apply_append(Value* expr, Env* env) {
	if (expr->list.count < 2) {
		printf("Error: append expects at least 1 argument\n");
		return NULL;
	}
	Value* res = make_list();
	for (int i = 1; i < expr->list.count; ++i) {
		Value* v = eval(expr->list.items[i], env);
		if (v == NULL) {
			free_value(res);
			return NULL;
		}
		if (v->type != VAL_LIST) {
			printf("Error: all arguments of append must be lists\n");
			free_value(v);
			free_value(res);
			return NULL;
		}
		if (v->list.dotted) {
			printf("Error: append expects proper lists, dotted pair given\n");
			free_value(v);
			free_value(res);
			return NULL;
		}
		for (int j = 0; j < v->list.count; ++j) {
			Value* copy_val = copy_value(v->list.items[j]);
			if (copy_val == NULL) {
				free_value(v);
				free_value(res);
				return NULL;
			}
			list_push(res, copy_val);
		}
		free_value(v);
	}
	return res;
}

static Value* apply_length(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: length expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of length\n");
		free_value(v);
		return NULL;
	}
	if (v->type != VAL_LIST) {
		printf("Error: argument of length must be a list\n");
		free_value(v);
		return NULL;
	}
	if (v->list.dotted) {
		printf("Error: length expects a proper list, dotted pair given\n");
		free_value(v);
		return NULL;
	}
	int len = v->list.count;
	free_value(v);
	return make_number(len);
}

static Value* apply_map(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: map expects a function and at least 1 list\n");
		return NULL;
	}

	Value* fun = NULL;
	Value* fun_expr = expr->list.items[1];
	if (fun_expr->type == VAL_SYMBOL && find_builtin(fun_expr->symbol) != NULL) {
		fun = copy_value(fun_expr);
	}
	else {
		fun = eval(fun_expr, env);
		if (fun == NULL) {
			printf("Error: failed to evaluate function argument of map\n");
			return NULL;
		}
		if (fun->type != VAL_LAMBDA) {
			printf("Error: first argument of map must be a function\n");
			free_value(fun);
			return NULL;
		}
	}

	int list_count = expr->list.count - 2;
	Value** lists = (Value**)malloc(sizeof(Value*) * list_count);
	if (lists == NULL) {
		printf("Error: out of memory\n");
		free_value(fun);
		return NULL;
	}

	int min_len = -1;
	for (int i = 0; i < list_count; ++i) {
		lists[i] = eval(expr->list.items[i + 2], env);
		if (lists[i] == NULL) {
			printf("Error: failed to evaluate list argument of map\n");
			for (int k = 0; k < i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}

		if (lists[i]->type != VAL_LIST) {
			printf("Error: all arguments after the first in map must be lists\n");
			for (int k = 0; k <= i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}
		if (lists[i]->list.dotted) {
			printf("Error: map expects proper lists, dotted pair given\n");
			for (int k = 0; k <= i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}

		if (min_len == -1 || lists[i]->list.count < min_len) {
			min_len = lists[i]->list.count;
		}
	}

	Value* res = make_list();
	for (int idx = 0; idx < min_len; ++idx) {
		Value* call_expr = make_list();
		list_push(call_expr, copy_value(fun));

		for (int li = 0; li < list_count; ++li) {
			Value* arg_copy = copy_value(lists[li]->list.items[idx]);
			if (arg_copy == NULL) {
				free_value(call_expr);
				for (int k = 0; k < list_count; ++k) free_value(lists[k]);
				free(lists);
				free_value(fun);
				free_value(res);
				return NULL;
			}
			list_push(call_expr, arg_copy);
		}

		Value* mapped_val = eval(call_expr, env);
		free_value(call_expr);
		if (mapped_val == NULL) {
			printf("Error: failed to evaluate function application in map\n");
			for (int k = 0; k < list_count; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			free_value(res);
			return NULL;
		}
		list_push(res, mapped_val);
	}

	for (int k = 0; k < list_count; ++k) free_value(lists[k]);
	free(lists);
	free_value(fun);
	return res;
}

static Value* apply_for_each(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: for-each expects a function and at least 1 list\n");
		return NULL;
	}

	Value* fun = NULL;
	Value* fun_expr = expr->list.items[1];
	if (fun_expr->type == VAL_SYMBOL && find_builtin(fun_expr->symbol) != NULL) {
		fun = copy_value(fun_expr);
	}
	else {
		fun = eval(fun_expr, env);
		if (fun == NULL) {
			printf("Error: failed to evaluate function argument of for-each\n");
			return NULL;
		}
		if (fun->type != VAL_LAMBDA) {
			printf("Error: first argument of for-each must be a function\n");
			free_value(fun);
			return NULL;
		}
	}

	int list_count = expr->list.count - 2;
	Value** lists = (Value**)malloc(sizeof(Value*) * list_count);
	if (lists == NULL) {
		printf("Error: out of memory\n");
		free_value(fun);
		return NULL;
	}

	int min_len = -1;
	for (int i = 0; i < list_count; ++i) {
		lists[i] = eval(expr->list.items[i + 2], env);
		if (lists[i] == NULL) {
			printf("Error: failed to evaluate list argument of for-each\n");
			for (int k = 0; k < i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}

		if (lists[i]->type != VAL_LIST) {
			printf("Error: all arguments after the first in for-each must be lists\n");
			for (int k = 0; k <= i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}
		if (lists[i]->list.dotted) {
			printf("Error: for-each expects proper lists, dotted pair given\n");
			for (int k = 0; k <= i; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}

		if (min_len == -1 || lists[i]->list.count < min_len) {
			min_len = lists[i]->list.count;
		}
	}

	for (int idx = 0; idx < min_len; ++idx) {
		Value* call_expr = make_list();
		list_push(call_expr, copy_value(fun));

		for (int li = 0; li < list_count; ++li) {
			Value* arg_copy = copy_value(lists[li]->list.items[idx]);
			if (arg_copy == NULL) {
				free_value(call_expr);
				for (int k = 0; k < list_count; ++k) free_value(lists[k]);
				free(lists);
				free_value(fun);
				return NULL;
			}
			list_push(call_expr, arg_copy);
		}

		Value* applied = eval(call_expr, env);
		free_value(call_expr);
		if (applied == NULL) {
			printf("Error: failed to evaluate function application in for-each\n");
			for (int k = 0; k < list_count; ++k) free_value(lists[k]);
			free(lists);
			free_value(fun);
			return NULL;
		}
		free_value(applied);
	}

	for (int k = 0; k < list_count; ++k) free_value(lists[k]);
	free(lists);
	free_value(fun);
	return make_void();
}

static Value* make_quoted_expr(Value* value) {
	Value* quoted = make_list();
	if (quoted == NULL) return NULL;

	Value* quote_sym = make_symbol("quote");
	if (quote_sym == NULL) {
		free_value(quoted);
		return NULL;
	}

	Value* value_copy = copy_value(value);
	if (value_copy == NULL) {
		free_value(quote_sym);
		free_value(quoted);
		return NULL;
	}

	list_push(quoted, quote_sym);
	list_push(quoted, value_copy);
	return quoted;
}

static Value* apply_apply(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: apply expects function and argument list\n");
		return NULL;
	}

	Value* tail_list = eval(expr->list.items[expr->list.count - 1], env);
	if (tail_list == NULL) {
		printf("Error: failed to evaluate last argument of apply\n");
		return NULL;
	}
	if (tail_list->type != VAL_LIST) {
		printf("Error: last argument of apply must be a list\n");
		free_value(tail_list);
		return NULL;
	}
	if (tail_list->list.dotted) {
		printf("Error: last argument of apply must be a proper list\n");
		free_value(tail_list);
		return NULL;
	}

	Value* call_expr = make_list();
	Value* fn_expr = copy_value(expr->list.items[1]);
	if (call_expr == NULL || fn_expr == NULL) {
		free_value(call_expr);
		free_value(fn_expr);
		free_value(tail_list);
		return NULL;
	}
	list_push(call_expr, fn_expr);

	for (int i = 2; i < expr->list.count - 1; ++i) {
		Value* arg_val = eval(expr->list.items[i], env);
		if (arg_val == NULL) {
			free_value(call_expr);
			free_value(tail_list);
			return NULL;
		}

		Value* quoted_arg = make_quoted_expr(arg_val);
		free_value(arg_val);
		if (quoted_arg == NULL) {
			free_value(call_expr);
			free_value(tail_list);
			return NULL;
		}
		list_push(call_expr, quoted_arg);
	}

	for (int i = 0; i < tail_list->list.count; ++i) {
		Value* quoted_arg = make_quoted_expr(tail_list->list.items[i]);
		if (quoted_arg == NULL) {
			free_value(call_expr);
			free_value(tail_list);
			return NULL;
		}
		list_push(call_expr, quoted_arg);
	}

	Value* result = eval(call_expr, env);
	free_value(call_expr);
	free_value(tail_list);
	return result;
}

static Value* appluy_displey(Value* expr, Env* env) {
	if (expr->list.count != 2) {
		printf("Error: display expects exactly 1 argument\n");
		return NULL;
	}
	Value* v = eval(expr->list.items[1], env);
	if (v == NULL) {
		printf("Error: failed to evaluate argument of display\n");
		return NULL;
	}
	print_value(v);
	free(v);
	return make_void();
}

static Value* apply_newline(Value* expr, Value* env) {
	if (expr->list.count != 1) {
		printf("Error: newline expects no arguments\n");
		return NULL;
	}
	printf("\n");
	return make_void();
}

static Value* apply_cond(Value* expr, Env* env) {
	if (expr->list.count < 2) {
		printf("Error: cond expects at least 1 clause\n");
		return NULL;
	}

	for (int i = 1; i < expr->list.count; ++i) {
		Value* clause = expr->list.items[i];
		if (clause->type != VAL_LIST || clause->list.count < 1) {
			printf("Error: each cond clause must be a non-empty list\n");
			return NULL;
		}

		Value* test_expr = clause->list.items[0];
		bool is_else = (test_expr->type == VAL_SYMBOL && strcmp(test_expr->symbol, "else") == 0);
		if (is_else && i != expr->list.count - 1) {
			printf("Error: else clause in cond must be the last clause\n");
			return NULL;
		}

		Value* test_value = NULL;
		bool matched = false;

		if (is_else) {
			matched = true;
		}
		else {
			test_value = eval(test_expr, env);
			if (test_value == NULL) {
				printf("Error: failed to evaluate cond test\n");
				return NULL;
			}
			matched = is_truthy_value(test_value);
		}

		if (!matched) {
			if (test_value != NULL) free_value(test_value);
			continue;
		}

		// (cond (test)) -> вернуть значение test
		if (clause->list.count == 1) {
			if (is_else) return make_bolian(true);
			return test_value;
		}

		if (test_value != NULL) free_value(test_value);

		Value* res = NULL;
		for (int j = 1; j < clause->list.count; ++j) {
			if (res != NULL) free_value(res);
			res = eval(clause->list.items[j], env);
			if (res == NULL) {
				printf("Error: failed to evaluate expression in cond clause\n");
				return NULL;
			}
		}
		return res;
	}

	return make_bolian(false);
}

static Value* apply_case(Value* expr, Env* env) {
	if (expr->list.count < 3) {
		printf("Error: case expects key and at least 1 clause\n");
		return NULL;
	}

	Value* key = eval(expr->list.items[1], env);
	if (key == NULL) {
		printf("Error: failed to evaluate case key\n");
		return NULL;
	}

	for (int i = 2; i < expr->list.count; ++i) {
		Value* clause = expr->list.items[i];
		if (clause->type != VAL_LIST || clause->list.count < 1) {
			printf("Error: each case clause must be a non-empty list\n");
			free_value(key);
			return NULL;
		}

		Value* head = clause->list.items[0];
		bool is_else = (head->type == VAL_SYMBOL && strcmp(head->symbol, "else") == 0);
		if (is_else && i != expr->list.count - 1) {
			printf("Error: else clause in case must be the last clause\n");
			free_value(key);
			return NULL;
		}

		bool matched = false;
		if (is_else) {
			matched = true;
		}
		else {
			if (head->type != VAL_LIST) {
				printf("Error: case datum list must be a list\n");
				free_value(key);
				return NULL;
			}

			for (int j = 0; j < head->list.count; ++j) {
				if (value_eqv(key, head->list.items[j])) {
					matched = true;
					break;
				}
			}
		}

		if (!matched) continue;

		if (clause->list.count == 1) {
			free_value(key);
			if (is_else) return make_bolian(true);
			return make_bolian(false);
		}

		Value* res = NULL;
		for (int j = 1; j < clause->list.count; ++j) {
			if (res != NULL) free_value(res);
			res = eval(clause->list.items[j], env);
			if (res == NULL) {
				printf("Error: failed to evaluate expression in case clause\n");
				free_value(key);
				return NULL;
			}
		}

		free_value(key);
		return res;
	}

	free_value(key);
	return make_bolian(false);
}


BuiltinEntry g_builtins[] = {
	{ "+", apply_plus, help_plus },
	{ "*", apply_mul, help_mul },
	{ "-", apply_sub, help_sub },
	{ "/", apply_div, help_div },
	{ "define", apply_define, help_define },
	{ "lambda", apply_lambda, help_lambda },
	{ "=", apply_compare, help_eq_number },
	{ ">", apply_compare, help_greater },
	{ "<", apply_compare, help_less },
	{ ">=", apply_compare, help_greater_eq },
	{ "<=", apply_compare, help_less_eq },
	{ "if", apply_if, help_if },
	{ "cond", apply_cond, help_cond },
	{ "case", apply_case, help_case },
	{ "not", apply_not, halp_not },
	{ "and", apply_and, help_and },
	{ "or", apply_or, help_or },
	{ "quote", apply_quote, help_quote },
	{ "car", apply_car, help_car },
	{ "cdr", apply_cdr, help_cdr },
	{ "cons", apply_cons, help_cons },
	{ "null?", apply_null_check, help_null_check },
	{ "number?", apply_number_check, help_number_check },
	{ "symbol?", apply_symbol_check, help_cymbol_check },
	{ "list?", apply_list_check, help_list_check },
	{ "boolean?", apply_boolian_check, help_boolean_check },
	{ "begin", apply_begin, help_begin },
	{ "let", apply_let, help_let },
	{ "let*", apply_let_star, help_let_star },
	{ "letrec", apply_letrec, help_letrec },
	{ "set!", apply_set_val, help_set_val },
	{ "set-memo-limit", apply_set_memo_limit, help_set_memo_limit },
	{ "eq?", apply_eq, help_eq },
	{ "eqv?", apply_eqv, help_eqv },
	{ "equal?", apply_equal, help_equal },
	{ "pair?", apply_pair_check, help_pair_check },
	{ "list", apply_list, help_list },
	{ "append", apply_append, help_append },
	{ "length", apply_length, help_length },
	{ "map", apply_map, help_map },
	{ "for-each", apply_for_each, help_for_each },
	{ "apply", apply_apply, help_apply },
	{ "display", appluy_displey, help_display },
	{ "newline", apply_newline, help_newline },
};

BuiltinEntry* get_function() {
	int n = (int)(sizeof(g_builtins) / sizeof(g_builtins[0]));
	BuiltinEntry* names = (BuiltinEntry*)malloc(sizeof(BuiltinEntry) * (n + 1));
	if (names == NULL) {
		printf("Error: out of memory\n");
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		names[i] = g_builtins[i];
	}
	names[n].name = NULL;
	return names;
}

void helper_function(const char* name) {
	int n = (int)(sizeof(g_builtins) / sizeof(g_builtins[0]));
	for (int i = 0; i < n; i++) {
		if (strcmp(g_builtins[i].name, name) == 0) {
			if (g_builtins[i].help_fn != NULL) {
				g_builtins[i].help_fn();
			}
			else {
				printf("No help available for %s\n", name);
			}
			return;
		}
	}
	printf("No such function: %s\n", name);
}

static BuiltinFn find_builtin(const char* op) {
	int n = (int)(sizeof(g_builtins) / sizeof(g_builtins[0]));
	for (int i = 0; i < n; i++) {
		if (strcmp(g_builtins[i].name, op) == 0) return g_builtins[i].fn;
	}
	return NULL;
}

static Value* apply_begin(Value* expr, Env* env) {
	if (expr->list.count < 2) {
		printf("Error: begin expects at least 1 argument\n");
		return NULL;
	}
	Value* res = NULL;
	for (int i = 1; i < expr->list.count; ++i) {
		if (res != NULL) free_value(res);
		res = eval(expr->list.items[i], env);
		if (res == NULL) {
			printf("Error: failed to evaluate an expression in begin\n");
			return NULL;
		}
	}
	return res;
}

static Value* apply_lambda_call(Value* fn, Value* call_expr, Env* env) {
	if (fn->type != VAL_LAMBDA) {
		printf("Error: first element is not a function\n");
		return NULL;
	}

	if (fn->lambda.params == NULL || fn->lambda.params->type != VAL_LIST) {
		printf("Error: bad lambda parameters\n");
		return NULL;
	}

	int param_count = fn->lambda.params->list.count;
	int arg_count = call_expr->list.count - 1;
	if (param_count != arg_count) {
		printf("Error: lambda expected %d arguments, got %d\n", param_count, arg_count);
		return NULL;
	}

	for (int i = 0; i < param_count; i++) {
		Value* param = fn->lambda.params->list.items[i];
		if (param->type != VAL_SYMBOL) {
			printf("Error: lambda parameter must be a symbol\n");
			return NULL;
		}
	}

	Value** args = (Value**)malloc(sizeof(Value*) * param_count);
	if (args == NULL) {
		printf("Error: out of memory\n");
		return NULL;
	}

	for (int i = 0; i < param_count; ++i) {
		args[i] = eval(call_expr->list.items[i + 1], env);
		if (args[i] == NULL) {
			for (int k = 0; k < i; ++k) free_value(args[k]);
			free(args);
			return NULL;
		}
	}

	bool use_memo = can_memoize_lambda_call(fn, call_expr);
	char* memo_key = NULL;

	if (use_memo) {
		memo_key = make_memo_key(call_expr->list.items[0]->symbol, args, param_count);
		if (memo_key == NULL) {
			use_memo = false;
		}
		else {
			memo_init();
			Value* cached = NULL;
			if (get(&g_memo_cache, memo_key, &cached)) {
				Value* hit = copy_value(cached);
				for (int i = 0; i < param_count; ++i) free_value(args[i]);
				free(args);
				free(memo_key);
				return hit;
			}
		}
	}

	Env* call_env = create_env(fn->lambda.closure);
	for (int i = 0; i < param_count; i++) {
		Value* param = fn->lambda.params->list.items[i];
		env_set(call_env, param->symbol, args[i]);
	}
	free(args);

	Value* result = eval(fn->lambda.body, call_env);

	if (result != NULL && use_memo && memo_key != NULL && result->type != VAL_VOID) {
		Value* stored = copy_value(result);
		if (stored != NULL && g_memo_cache_limit > 0) {
			memo_init();
			if (g_memo_cache_size >= g_memo_cache_limit) {
				memo_clear_all();
			}
			insert(&g_memo_cache, memo_key, stored);
			g_memo_cache_size++;
		}
		else {
			free_value(stored);
		}
	}

	free(memo_key);

	if (result == NULL || result->type != VAL_LAMBDA) {
		free_env(call_env);
	}
	return result;
}


static Value* eval_impl(Value* expr, Env* env) {
	if (expr->type == VAL_NUMBER) {
		return make_number(expr->number);
	}

	if (expr->type == VAL_STRING) {
		return make_string(expr->string);
	}

	if (expr->type == VAL_SYMBOL) {
		Value* v = env_get(env, expr->symbol);
		if (v == NULL) {
			printf("Error: unknown symbol %s\n", expr->symbol);
			return NULL;
		}
		return v;
	}

	if (expr->type == VAL_BOLIAN) {
		return make_bolian(expr->bolian);
	}

	if (expr->type == VAL_LAMBDA) {
		return copy_value(expr);
	}

	if (expr->type == VAL_LIST) {
		if (expr->list.count == 0) {
			printf("Error: empty list\n");
			return NULL;
		}

		Value* first = expr->list.items[0];

		if (first->type == VAL_SYMBOL) {
			BuiltinFn fn = find_builtin(first->symbol);
			if (fn != NULL) return fn(expr, env);
		}

		Value* fn_value = eval(first, env);
		if (fn_value == NULL) return NULL;
		Value* result = apply_lambda_call(fn_value, expr, env);
		free_value(fn_value);
		return result;
	}

	printf("Error: bad expression\n");
	return NULL;
}

Value* eval(Value* expr, Env* env) {
	if (!eval_enter()) return NULL;
	Value* result = eval_impl(expr, env);
	eval_leave();
	return result;
}

long long eval_number(Value* expr, int* ok, Env* env) {
	Value* v = eval(expr, env);
	if (v == NULL) {
		*ok = 0;
		return 0;
	}

	if (v->type != VAL_NUMBER) {
		printf("Error: expected a number\n");
		free_value(v);
		*ok = 0;
		return 0;
	}

	long long x = v->number;
	free_value(v);
	*ok = 1;
	return x;
}

