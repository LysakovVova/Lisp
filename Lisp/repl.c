#include "repl.h"

static void repl_load(const char* s, Env* env);

static int check_syntax(const char* s) {
	int balance = 0;
	bool in_string = false;
	bool escaped = false;
	for (int i = 0; s[i] != '\0'; i++) {
		if (in_string) {
			if (escaped) {
				escaped = false;
				continue;
			}
			if (s[i] == '\\') {
				escaped = true;
				continue;
			}
			if (s[i] == '"') {
				in_string = false;
			}
			continue;
		}

		if (s[i] == '"') {
			in_string = true;
			continue;
		}

		if (s[i] == '(') balance++;
		else if (s[i] == ')') balance--;
		if (balance < 0) return -1;
	}
	if (in_string) return 2;
	if (balance > 0) return 1;
	return 0;
}

void start_lexer(const char* line, Env* env) {
	Line_token vec;
	reader(&vec, line);
	int pos = 0;
	if (vec.n == 1) {
		free_tokens(&vec);
		return;
	}
	Value* expr = read_expr(&vec, &pos);
	if (expr == NULL) {
		free_tokens(&vec);
		return;
	}
	if (vec.a[pos].type != END) {
		printf("Syntax error: extra tokens after expression\n");
		free_value(expr);
		free_tokens(&vec);
		return;
	}
	Value* result = eval(expr, env);
	if (result != NULL) {
		print_value(result);
		printf("\n");
		free_value(result);
	}
	free_value(expr);
	free_tokens(&vec);
}

static void repl_help(const char* s, Env* env) {
	printf("Available commands:\n");
	printf(":help or :h - Show this help message\n");
	printf(":quit or :q or :exit - Exit the REPL\n");
	printf(":clear or :c - Clear the screen\n");
	printf(":env - Show current environment variables\n");
	printf(":functions or :f - Show defined functions\n");
	printf(":help-function or :hf - Show help for built-in function\n");
	printf(":load or :l - Load and execute a file\n");
}

static void repl_clear(const char* s, Env* env) {
	system("cls");
}

static void repl_env(const char* s, Env* env) {
	printf("Environment variables:\n");
	print_bst(&env->variables);
}

static void repl_functions(const char* s, Env* env) {
	printf("Defined values/functions in current environment:\n");

	BuiltinEntry*  names = get_function();
	
	for (int i = 0; names[i].name != NULL; i++) {
		printf("- %s\n", names[i].name);
	}
}

static void repl_help_function(const char* s, Env* env) {
	char* function_name = calloc(50, sizeof(char));

	if (function_name == NULL) {
		printf("Error: out of memory\n");
		return;
	}

	int i = 0;

	while (s[i] != '\0' && s[i] != ' ' && s[i] != '\n' && i < 49) {
		++i;
	}
	while (s[i] == ' ' || s[i] == '\n' && i < 49) {
		++i;
	}

	int n1 = 0;

	for (i; s[i] != '\0' && s[i] != ' ' && s[i] != '\n' && i < 49; i++, ++n1) {
		function_name[n1] = s[i];
		function_name[n1 + 1] = '\0';
	}


	helper_function(function_name);
	free(function_name);
}

static void repl_load(const char* s, Env* env) {
	char path[1024] = { 0 };
	int i = 0;

	while (s[i] != '\0' && s[i] != ' ' && s[i] != '\n') ++i;
	while ((s[i] == ' ' || s[i] == '\n' || s[i] == '\t') && s[i] != '\0') ++i;

	int p = 0;
	while (s[i] != '\0' && s[i] != '\n' && p < 1023) {
		path[p++] = s[i++];
	}
	path[p] = '\0';

	if (path[0] == '\0') {
		printf("Usage: :load <file-path>\n");
		return;
	}

	FILE* f = fopen(path, "r");
	if (f == NULL) {
		printf("Error: cannot open file '%s'\n", path);
		return;
	}

	char line[5000];
	char expr_buf[20000];
	expr_buf[0] = '\0';

	while (fgets(line, sizeof(line), f) != NULL) {
		if ((int)strlen(expr_buf) + (int)strlen(line) + 1 >= (int)sizeof(expr_buf)) {
			printf("Error: expression in file is too long\n");
			expr_buf[0] = '\0';
			continue;
		}

		strcat_s(expr_buf, sizeof(expr_buf), line);

		int st = check_syntax(expr_buf);
		if (st == -1) {
			printf("Syntax error in file: unexpected ')'\n");
			expr_buf[0] = '\0';
			continue;
		}
		if (st == 0) {
			start_lexer(expr_buf, env);
			expr_buf[0] = '\0';
		}
	}

	if (expr_buf[0] != '\0') {
		int st = check_syntax(expr_buf);
		if (st == 1) printf("Syntax error in file: expected ')'\n");
		else if (st == 2) printf("Syntax error in file: expected '\"'\n");
		else start_lexer(expr_buf, env);
	}

	fclose(f);
}

static void repl_exit(const char* s, Env* env) {
	exit(0);
}

static ReplEntry g_repl[] = {
	{ ":help", repl_help },
	{ ":h", repl_help },
	{ ":quit", repl_exit },
	{ ":q", repl_exit },
	{ ":exit", repl_exit },
	{ ":clear", repl_clear },
	{ ":c", repl_clear },
	{ ":env", repl_env },
	{ ":functions", repl_functions },
	{ ":f", repl_functions },
	{ ":help-function", repl_help_function },
	{ ":hf", repl_help_function },
	{ ":load", repl_load },
	{ ":l", repl_load },
	{ NULL, NULL },
};


static bool handle_builtin(const char* line, Env* env) {
	char* s = malloc(sizeof(char) * 20);
	if (s == NULL) return false;
	s[0] = '\0';



	for (int i = 0; line[i] != '\0' && line[i] != ' ' && line[i] != '\n' && i < 19; i++) {
		s[i] = line[i];
		s[i + 1] = '\0';
	}

	for (int i = 0; g_repl[i].name != NULL; i++) {
		if (strcmp(g_repl[i].name, s) == 0) {
			g_repl[i].fn(line, env);
			free(s);
			return true;
		}
	}
	free(s);
	return false;
}
	

void read_line(const char* line, Env* env) {
	static char acc[20000];
	static bool in_progress = false;

	if (!in_progress) {
		if (handle_builtin(line, env)) {
			return;
		}
		acc[0] = '\0';
	}

	if ((int)strlen(acc) + (int)strlen(line) + 1 >= (int)sizeof(acc)) {
		printf("Syntax error: input too long\n");
		acc[0] = '\0';
		in_progress = false;
		return;
	}

	strcat_s(acc, sizeof(acc), line);

	int st = check_syntax(acc);
	if (st == -1) {
		printf("Syntax error: unexpected ')'\n");
		acc[0] = '\0';
		in_progress = false;
		return;
	}

	if (st == 0) {
		start_lexer(acc, env);
		acc[0] = '\0';
		in_progress = false;
		return;
	}

	in_progress = true;
}