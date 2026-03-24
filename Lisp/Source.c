#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "expr.h"
#include "eval.h"
#include "env.h"

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


int main() {
	Line_token vec;

	Env* global_env = create_env(NULL);

	char* line = (char*)malloc(sizeof(char) * 5000);
	if (line == NULL) {
		printf("Error: out of memory\n");
		free_env(global_env);
		free_hash();
		return 1;
	}

	int len = 0;

	while (1) {
		printf(">>> ");
		line[0] = '\0';
		if (fgets(line, 5000, stdin) == NULL) {
			break;
		}

		while (1) {
			int st = check_syntax(line);
			if (st == -1) {
				printf("Syntax error: unexpected ')'\n");
				line[0] = '\0';
				break;
			}
			if (st == 0) {
				break;
			}

			len = (int)strlen(line);
			if (len >= 4999) {
				printf("Syntax error: input too long\n");
				line[0] = '\0';
				break;
			}

			printf("... ");
			if (fgets(line + len, 5000 - len, stdin) == NULL) {
				if (st == 2) printf("Syntax error: expected '\"'\n");
				else printf("Syntax error: expected ')'\n");
				line[0] = '\0';
				break;
			}
		}

		if (line[0] == '\0') {
			continue;
		}

		if (strcmp(line, "exit\n") == 0 || strcmp(line, "exit") == 0) {
			break;
		}

		reader(&vec, line);

		int pos = 0;
		Value* expr = read_expr(&vec, &pos);

		if (expr == NULL) {
			free_tokens(&vec);
			continue;
		}

		if (vec.a[pos].type != END) {
			printf("Syntax error: extra tokens after expression\n");
			free_value(expr);
			free_tokens(&vec);
			continue;
		}

		Value* result = eval(expr, global_env);

		if (result != NULL) {
			print_value(result);
			printf("\n");
			free_value(result);
		}

		free_value(expr);
		free_tokens(&vec);
	}

	free_env(global_env);
	free_hash();
	free(line);
	return 0;
}