#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_types.h"

void init_bst(bst* v);
void free_hash();

void insert(bst* v, const char* key, long long value); // вставка/изменение элемента
bool get(bst* v, const char* key, long long* out_value); // получение элемента, возвращает false, если элемента нет
bool contains(bst* v, const char* key); // проверка наличия элемента
void erase(bst* v, const char* key); // удаление элемента, ничего не делает, если элемента нет

void print_bst(bst* v); // печать всех элементов в виде "key: value\n", элементы печатаются в порядке возрастания ключей
void free_bst(bst* v); // освобождение всех ресурсов, выделенных для хранения элементов надо вызвать free_hash() после того, как больше не будет использоваться ни один bst