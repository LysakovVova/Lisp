#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void read_number(char* line, int* idx, long long* ans) {
	*ans = 0;
	bool flag = 0;

	if (line[*idx] == '+') {
		(*idx)++;
	}
	if (line[*idx] == '-') {
		(*idx)++;
		flag = 1;
	}

	while (line[*idx] >= '0' && line[*idx] <= '9') {
		*ans *= 10;
		*ans += (line[*idx] - '0');
		(*idx)++;
	}
	if (flag) *ans = -(*ans);
	return;
}

void read_symbol(char* line, int* idx, char** ans) {
	int cup = 2;
	char* s = (char*)malloc(cup * sizeof(char));
	int n = 0;

	while (line[*idx] &&
		line[*idx] != ' ' &&
		line[*idx] != '\t' &&
		line[*idx] != '\n' &&
		line[*idx] != '"' &&
		line[*idx] != '\'' &&
		line[*idx] != '(' &&
		line[*idx] != ')') {
		if (n + 1 >= cup) {
			cup *= 2;
			s = (char*)realloc(s, cup * sizeof(char));
		}
		s[n++] = line[*idx];
		(*idx)++;
	}

	s[n] = '\0';
	*ans = s;
}

void read_string(char* line, int* idx, char** ans, bool* ok) {
	*ok = true;
	(*idx)++;

	int cup = 16;
	char* s = (char*)malloc(cup * sizeof(char));
	if (s == NULL) {
		*ok = false;
		*ans = NULL;
		return;
	}

	int n = 0;
	while (line[*idx] && line[*idx] != '"') {
		char ch = line[*idx];

		if (ch == '\\') {
			(*idx)++;
			if (!line[*idx]) {
				free(s);
				*ok = false;
				*ans = NULL;
				return;
			}

			switch (line[*idx]) {
			case 'n': ch = '\n'; break;
			case 't': ch = '\t'; break;
			case '"': ch = '"'; break;
			case '\\': ch = '\\'; break;
			default: ch = line[*idx]; break;
			}
		}

		if (n + 1 >= cup) {
			cup *= 2;
			char* new_s = (char*)realloc(s, cup * sizeof(char));
			if (new_s == NULL) {
				free(s);
				*ok = false;
				*ans = NULL;
				return;
			}
			s = new_s;
		}

		s[n++] = ch;
		(*idx)++;
	}

	if (line[*idx] != '"') {
		free(s);
		*ok = false;
		*ans = NULL;
		return;
	}

	(*idx)++;
	s[n] = '\0';
	*ans = s;
}

void reader(Line_token* vec, char* line) {
	vec->n = 0;
	vec->cup = 2;
	vec->a = (Token*)malloc(vec->cup * sizeof(Token));


	int idx_line = 0;
	char cur;
	char next_simbol;
	bool lex_error = false;
	while (line[idx_line]) {
		if (line[idx_line] == ' ' || line[idx_line] == '\t' || line[idx_line] == '\n') {
			idx_line++;
			continue;
		}

		cur = line[idx_line];
		if (cur == '\0') break;
		next_simbol = line[idx_line + 1];

		if (vec->cup == vec->n) {
			vec->cup *= 2;
			vec->a = (Token*)realloc(vec->a, vec->cup * sizeof(Token));
		}

		if (cur == '(') {
			vec->a[vec->n].type = LPAREN;
			idx_line++;
		}
		else if (cur == ')') {
			vec->a[vec->n].type = RPAREN;
			idx_line++;
		}
		else if (cur == '\'') {
			vec->a[vec->n].type = QUOTE;
			idx_line++;
		}
		else if (
			(cur >= '0' && cur <= '9') ||
			(cur == '+' && next_simbol >= '0' && next_simbol <= '9') ||
			(cur == '-' && next_simbol >= '0' && next_simbol <= '9')
			) {
			vec->a[vec->n].type = NUMBER;
			read_number(line, &idx_line, &vec->a[vec->n].number);
		}
		else if (cur == '"') {
			bool ok = false;
			vec->a[vec->n].type = STRING;
			read_string(line, &idx_line, &vec->a[vec->n].text, &ok);
			if (!ok) {
				printf("Syntax error: invalid or unterminated string literal\n");
				lex_error = true;
				break;
			}
		}
		else {
			vec->a[vec->n].type = SYMBOL;
			read_symbol(line, &idx_line, &vec->a[vec->n].text);
		}

		vec->n++;
	}

	if (vec->cup == vec->n) {
		vec->cup *= 2;
		vec->a = (Token*)realloc(vec->a, vec->cup * sizeof(Token));
	}
	if (!lex_error && line[idx_line] == '\0') {
		vec->a[vec->n].type = END;
		vec->n++;
	}
	else if (lex_error) {
		if (vec->cup == vec->n) {
			vec->cup *= 2;
			vec->a = (Token*)realloc(vec->a, vec->cup * sizeof(Token));
		}
		vec->a[vec->n].type = END;
		vec->n++;
	}
}

void print_token(Line_token* vec) {
	for (int i = 0; i < vec->n; ++i) {
		printf("Token : ");
		switch (vec->a[i].type) {
		case LPAREN:
			printf("(");
			break;
		case RPAREN:
			printf(")");
			break;
		case NUMBER:
			printf("NUM = %lld", vec->a[i].number);
			break;
		case STRING:
			printf("STR = \"%s\"", vec->a[i].text);
			break;
		case QUOTE:
			printf("'");
			break;
		case SYMBOL:
			printf("SYMB = %s", vec->a[i].text);
			break;
		case END:
			printf("END");
			break;
		}
		printf("\n");
	}
}

void free_tokens(Line_token* vec) {
	for (int i = 0; i < vec->n; i++) {
		if (vec->a[i].type == SYMBOL || vec->a[i].type == STRING) {
			free(vec->a[i].text);
			vec->a[i].text = NULL;
		}
	}
	free(vec->a);
	vec->a = NULL;
	vec->n = 0;
	vec->cup = 0;
}