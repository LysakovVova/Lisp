#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "expr.h"
#include "eval.h"
#include "map.h"


int main() {
	Line_token vec;

	Env* global_env = create_env(NULL);

	char* line = (char*)malloc(sizeof(char) * 5000);

	while (fgets(line, 5000, stdin)) {
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
}