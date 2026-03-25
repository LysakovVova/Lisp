#include "helper.h"
#include <stdio.h>

void help_plus() {
	printf("Usage: (+ expr1 expr2 ...)\n");
	printf("Returns the sum of all the expressions. If no arguments are given, returns 0.\n");
	printf("Examples:\n(+ 1 2 3) returns 6\n(+) returns 0\n(+ 5) returns 5\n");
}

void help_sub() {
	printf("Usage: (- expr1 expr2 ...)\n");
	printf("Returns the result of subtracting all the subsequent expressions from the first one. If only one argument is given, returns its negation.\n");
	printf("Examples:\n(- 10 3 2) returns 5\n(- 5) returns -5\n(-) is an error\n");
}

void help_mul() {
	printf("Usage: (* expr1 expr2 ...)\n");
	printf("Returns the product of all the expressions. If no arguments are given, returns 1.\n");
	printf("Examples:\n(* 2 3 4) returns 24\n(*) returns 1\n(* 5) returns 5\n");
}

void help_div() {
	printf("Usage: (/ expr1 expr2 ...)\n");
	printf("Returns the result of dividing the first expression by all the subsequent expressions. If only one argument is given, returns its reciprocal.\n");
	printf("Examples:\n(/ 10 2 5) returns 1\n(/ 4) returns 0.25\n(/) is an error\n");
}

void help_define() {
	printf("Usage: (define name value)\n");
	printf("Defines a new variable or function with the given name and value.\n");
	printf("Examples:\n(define x 10) defines a variable x with value 10\n(define (square n) (* n n)) defines a function square\n");
}

void help_lambda() {
	printf("Usage: (lambda (param1 param2 ...) body)\n");
	printf("Creates an anonymous function with the given parameters and body.\n");
	printf("Examples:\n((lambda (x) (* x x)) 5) returns 25\n(define square (lambda (x) (* x x))) then (square 5) returns 25\n");
}

void help_eq_number() {
	printf("Usage: (= expr1 expr2)\n");
	printf("Returns #t if the two expressions are the same number, #f otherwise.\n");
	printf("Examples:\n(= 5 5) returns #t\n(= 5 6) returns #f\n(= 5 '5) returns #f\n");
}

void help_greater() {
	printf("Usage: (> expr1 expr2)\n");
	printf("Returns #t if the first expression is greater than the second, #f otherwise.\n");
	printf("Examples:\n(> 5 3) returns #t\n(> 3 5) returns #f\n(> 5 5) returns #f\n");
}

void help_less() {
	printf("Usage: (< expr1 expr2)\n");
	printf("Returns #t if the first expression is less than the second, #f otherwise.\n");
	printf("Examples:\n(< 3 5) returns #t\n(< 5 3) returns #f\n(< 5 5) returns #f\n");
}

void help_greater_eq() {
	printf("Usage: (>= expr1 expr2)\n");
	printf("Returns #t if the first expression is greater than or equal to the second, #f otherwise.\n");
	printf("Examples:\n(>= 5 3) returns #t\n(>= 3 5) returns #f\n(>= 5 5) returns #t\n");
}

void help_less_eq() {
	printf("Usage: (<= expr1 expr2)\n");
	printf("Returns #t if the first expression is less than or equal to the second, #f otherwise.\n");
	printf("Examples:\n(<= 3 5) returns #t\n(<= 5 3) returns #f\n(<= 5 5) returns #t\n");
}

void help_if() {
	printf("Usage: (if condition then_expr else_expr)\n");
	printf("Evaluates the condition expression, if it returns #t, evaluates and returns then_expr, otherwise evaluates and returns else_expr.\n");
	printf("Examples:\n(if (> 5 3) 'yes 'no) returns 'yes\n(if (< 5 3) 'yes 'no) returns 'no\n");
}

void help_cond() {
	printf("Usage: (cond (condition1 expr1) (condition2 expr2) ...)\n");
	printf("Evaluates each condition in order, and returns the value of the corresponding expression for the first condition that evaluates to #t. If no conditions are true, returns #f.\n");
	printf("Example: (cond ((> 5 3) 'greater) ((< 5 3) 'less) (else 'equal)) returns 'greater.\n");

}

void help_case() {
	printf("Usage: (case expr (value1 expr1) (value2 expr2) ...)\n");
	printf("Evaluates the expression and compares it to each value in order. If a match is found, evaluates and returns the corresponding expression. If no match is found, returns #f.\n");
	printf("Example: (case 3 ((1) 'one) ((2) 'two) ((3) 'three)) returns 'three.\n");
}

void halp_not() {
	printf("Usage: (not expr)\n");
	printf("Returns #t if the expression evaluates to #f, and #f otherwise.\n");
	printf("Examples:\n(not #t) returns #f\n(not #f) returns #t\n(not 0) returns #f\n(not 1) returns #f\n");
}

void help_and() {
	printf("Usage: (and expr1 expr2 ...)\n");
	printf("Returns #t if all expressions evaluate to #t, and #f otherwise. If no expressions are given, returns #t.\n");
	printf("Examples:\n(and #t #t) returns #t\n(and #t #f) returns #f\n(and) returns #t\n");
}

void help_or() {
	printf("Usage: (or expr1 expr2 ...)\n");
	printf("Returns #t if at least one expression evaluates to #t, and #f otherwise. If no expressions are given, returns #f.\n");
	printf("Examples:\n(or #t #f) returns #t\n(or #f #f) returns #f\n(or) returns #f\n");
}

void help_quote() {
	printf("Usage: (quote expr) or 'expr\n");
	printf("Returns the expression without evaluating it.\n");
	printf("Examples:\n(quote (1 2 3)) returns (1 2 3)\n'(1 2 3) also returns (1 2 3)\n");

}

void help_car() {
	printf("Usage: (car expr)\n");
	printf("Returns the first element of a list.\n");
	printf("Examples:\n(car '(1 2 3)) returns 1\n(car '()) is an error\n");
}

void help_cdr() {
	printf("Usage: (cdr expr)\n");
	printf("Returns the list without its first element.\n");
	printf("Examples:\n(cdr '(1 2 3)) returns (2 3)\n(cdr '(1)) returns ()\n(cdr '()) is an error\n");
}

void help_cons() {
	printf("Usage: (cons expr1 expr2)\n");
	printf("Returns a new list with expr1 as the first element and expr2 as the rest of the list.\n");
	printf("Examples:\n(cons 1 '(2 3)) returns (1 2 3)\n(cons 1 '()) returns (1)\n(cons 1 2) returns (1 . 2)\n");
}

void help_null_check() {
	printf("Usage: (null? expr)\n");
	printf("Returns #t if the expression is an empty list, and #f otherwise.\n");
	printf("Examples:\n(null? '()) returns #t\n(null? '(1 2 3)) returns #f\n(null? 0) returns #f\n");
}

void help_number_check() {
	printf("Usage: (number? expr)\n");
	printf("Returns #t if the expression is a number, and #f otherwise.\n");
	printf("Examples:\n(number? 5) returns #t\n(number? '5) returns #f\n(number? '()) returns #f\n");

}

void help_cymbol_check() {
	printf("Usage: (symbol? expr)\n");
	printf("Returns #t if the expression is a symbol, and #f otherwise.\n");
	printf("Examples:\n(symbol? 'x) returns #t\n(symbol? 5) returns #f\n(symbol? '()) returns #f\n");
}

void help_list_check() {
	printf("Usage: (list? expr)\n");
	printf("Returns #t if the expression is a list, and #f otherwise.\n");
	printf("Examples:\n(list? '(1 2 3)) returns #t\n(list? '()) returns #t\n(list? 5) returns #f\n");
}

void help_boolean_check() {
	printf("Usage: (boolean? expr)\n");
	printf("Returns #t if the expression is a boolean, and #f otherwise.\n");
	printf("Examples:\n(boolean? #t) returns #t\n(boolean? #f) returns #t\n(boolean? 0) returns #f\n");
}

void help_begin() {
	printf("Usage: (begin expr1 expr2 ...)\n");
	printf("Evaluates each expression in order and returns the value of the last expression.\n");
	printf("Example: (begin (display \"Hello, \") (display \"world!\") (+ 1 2)) prints \"Hello, world!\" and returns 3.\n");
}

void help_let() {
	printf("Usage: (let ((var1 val1) (var2 val2) ...) body)\n");
	printf("Creates a new environment with the given variable bindings and evaluates the body in that environment.\n");
	printf("Example: (let ((x 10) (y 20)) (+ x y)) returns 30.\n");
}

void help_let_star() {
	printf("Usage: (let* ((var1 val1) (var2 val2) ...) body)\n");
	printf("Creates a new environment with the given variable bindings, where each variable can refer to the previously defined variables in the same let* expression, and evaluates the body in that environment.\n");
	printf("Example: (let* ((x 10) (y (+ x 5))) (* x y)) returns 150.\n");
}

void help_letrec() {
	printf("Usage: (letrec ((var1 val1) (var2 val2) ...) body)\n");
	printf("Creates a new environment with the given variable bindings, where each variable can refer to any of the variables defined in the same letrec expression, including itself, and evaluates the body in that environment. This is typically used for defining recursive functions.\n");
	printf("Example: (letrec ((fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))) (x 5)) (fact x)) returns 120.\n");
}

void help_set_val() {
	printf("Usage: (set! var value)\n");
	printf("Updates the value of an existing variable in the current environment or any of its parent environments. If the variable does not exist, it is an error.\n");
	printf("Example: (define x 10) (set! x 20) returns 20, (set! y 5) is an error if y is not defined.\n");
}

void help_set_memo_limit() {
	printf("Usage: (set-memo-limit n)\n");
	printf("Sets the maximum number of entries in the memoization cache to n. This can help control memory usage when using memoized functions.\nby default: 256");
	printf("Example: (set-memo-limit 100) sets the memoization cache limit to 100 entries.\n");
}

void help_eq() {
	printf("Usage: (eq? expr1 expr2)\n");
	printf("Returns #t if the two expressions are the same object in memory, and #f otherwise.\n");
	printf("Examples:\n(eq? 'x 'x) returns #t\n(eq? '(1 2) '(1 2)) returns #f\n(let ((a '(1 2))) (eq? a a)) returns #t\n");
}

void help_eqv() {
	printf("Usage: (eqv? expr1 expr2)\n");
	printf("Returns #t if the two expressions are the same object in memory or are considered equivalent (e.g., two numbers with the same value), and #f otherwise.\n");
	printf("Examples:\n(eqv? 'x 'x) returns #t\n(eqv? '(1 2) '(1 2)) returns #f\n(eqv? 5 5) returns #t\n");
}

void help_equal() {
	printf("Usage: (equal? expr1 expr2)\n");
	printf("Returns #t if the two expressions are structurally equal (e.g., two lists with the same elements), and #f otherwise.\n");
	printf("Examples:\n(equal? '(1 2) '(1 2)) returns #t\n(equal? '(1 2) '(2 1)) returns #f\n(equal? 5 5) returns #t\n");
}

void help_pair_check() {
	printf("Usage: (pair? expr)\n");
	printf("Returns #t if the expression is a pair (i.e., a non-empty list), and #f otherwise.\n");
	printf("Examples:\n(pair? '(1 . 2)) returns #t\n(pair? '(1 2)) returns #t\n(pair? '()) returns #f\n");
}

void help_list() {
	printf("Usage: (list expr1 expr2 ...)\n");
	printf("Returns a new list containing the given expressions.\n");
	printf("Examples:\n(list 1 2 (+ 1 2)) returns (1 2 3)\n(list) returns ()\n");
}

void help_append() {
	printf("Usage: (append list1 list2 ...)\n");
	printf("Returns a new list that is the concatenation of the given lists. All arguments must be proper lists (not dotted pairs).\n");
	printf("Examples:\n(append '(1 2) '(3 4)) returns (1 2 3 4)\n(append '(1) '(2) '(3)) returns (1 2 3)\n(append) returns ()\n");
}

void help_length() {
	printf("Usage: (length lst)\n");
	printf("Returns the number of elements in the list. The argument must be a proper list (not a dotted pair).\n");
	printf("Examples:\n(length '(1 2 3)) returns 3\n(length '()) returns 0\n(length '(1 . 2)) is an error\n");
}

void help_map() {
	printf("Usage: (map func lst1 lst2 ...)\n");
	printf("Returns a new list where each element is the result of applying the function to the corresponding elements of the given lists. All lists must have the same length.\n");
	printf("Examples:\n(map + '(1 2 3) '(4 5 6)) returns (5 7 9)\n(map list '(1 2) '(3 4)) returns ((1 3) (2 4))\n");
}

void help_for_each() {
	printf("Usage: (for-each func lst1 lst2 ...)\n");
	printf("Applies the function to the corresponding elements of the given lists for side effects. All lists must have the same length. Returns unspecified value.\n");
	printf("Examples:\n(for-each display '(1 2 3)) prints 123\n(for-each (lambda (x) (display x) (newline)) '(1 2 3)) prints each number on a new line\n");
}

void help_apply() {
	printf("Usage: (apply func arg1 arg2 ... args)\n");
	printf("Applies the function to the given arguments, where the last argument is a list of additional arguments to be passed to the function.\n");
	printf("Examples:\n(apply + 1 2 '(3 4)) returns 10\n(apply list 1 2 '(3 4)) returns (1 2 3 4)\n");
}

void help_display() {
	printf("Usage: (display expr)\n");
	printf("Evaluates the expression and prints its value to the standard output without a newline.\n");
	printf("Example: (display \"Hello, \") (display \"world!\") prints \"Hello, world!\".\n");
}

void help_newline() {
	printf("Usage: (newline)\n");
	printf("Prints a newline character to the standard output.\n");
	printf("Example: (newline) prints a newline.\n");
}