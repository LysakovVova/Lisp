#include "map.h"
#include "expr.h"

static pair_hash MOD = { 1000000007LL, 1000000009LL };
static pair_hash BASE = { 31, 53 };

static long long* powers1;
static long long* powers2;

static void init_hash() {
	static bool initialized = false;
	if (initialized) return;
	initialized = true;
	powers1 = (long long*)malloc(sizeof(long long) * 10000);
	powers2 = (long long*)malloc(sizeof(long long) * 10000);
	powers1[0] = 1;
	powers2[0] = 1;
	for (int i = 1; i < 10000; i++) {
		powers1[i] = (powers1[i - 1] * BASE.first) % MOD.first;
		powers2[i] = (powers2[i - 1] * BASE.second) % MOD.second;
	}
}

static long long hash_string(const char* s) {
	long long h1 = 0, h2 = 0;
	for (int i = 0; s[i]; i++) {
		h1 = (h1 + (s[i] - 'a' + 1) * powers1[i]) % MOD.first;
		h2 = (h2 + (s[i] - 'a' + 1) * powers2[i]) % MOD.second;
	}
	return (h1 << 32) | h2;
}

void free_hash() {
	free(powers1);
	free(powers2);
	powers1 = NULL;
	powers2 = NULL;
}

static int random_int(int a, int b) {
	return a + rand() % (b - a + 1);
}

void init_bst(bst* v) {
	v->head = NULL;
	init_hash();
}

static node* rotate_r(node* p) {
	node* l = p->left;
	p->left = l->right;
	l->right = p;
	return l;
}

static node* rotate_l(node* p) {
	node* r = p->right;
	p->right = r->left;
	r->left = p;
	return r;
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

static int cmp_key(long long hash, const char* key, node* n) {
	if (hash < n->key) return -1;
	if (hash > n->key) return 1;
	return strcmp(key, n->key_string);
}

static node* create_node(const char* key, Value* value) {
	node* n = (node*)malloc(sizeof(node));
	n->prio = random_int(0, 1000000000);
	n->key = hash_string(key);
	n->key_string = my_strdup(key);
	n->value = value;
	n->left = NULL;
	n->right = NULL;
	return n;
}

static node* ins(node* root, node* x) {
	if (!root) {
		return x;
	}

	int cmp = cmp_key(x->key, x->key_string, root);
	if (cmp == 0) {
		free_value(root->value);
		root->value = x->value;
		free(x->key_string);
		free(x);
		return root;
	}

	if (cmp < 0) {
		root->left = ins(root->left, x);
		if (root->left->prio > root->prio) {
			root = rotate_r(root);
		}
	}
	else {
		root->right = ins(root->right, x);
		if (root->right->prio > root->prio) {
			root = rotate_l(root);
		}
	}
	return root;
}

void insert(bst* v, const char* key, Value* value) {
	node* n = create_node(key, value);
	v->head = ins(v->head, n);
}

static node* find_node(node* root, long long hash, const char* key) {
	while (root) {
		int cmp = cmp_key(hash, key, root);
		if (cmp == 0) return root;
		if (cmp < 0) root = root->left;
		else root = root->right;
	}
	return NULL;
}

bool get(bst* v, const char* key, Value** out_value) {
	long long h = hash_string(key);
	node* n = find_node(v->head, h, key);
	if (!n) return false;
	if (out_value) *out_value = n->value;
	return true;
}

bool contains(bst* v, const char* key) {
	return get(v, key, NULL);
}

static node* merge(node* l, node* r) {
	if (!l) return r;
	if (!r) return l;

	if (l->prio > r->prio) {
		l->right = merge(l->right, r);
		return l;
	}
	else {
		r->left = merge(l, r->left);
		return r;
	}
}

static node* er(node* root, long long h, const char* key) {
	if (!root) return NULL;

	int cmp = cmp_key(h, key, root);
	if (cmp < 0) {
		root->left = er(root->left, h, key);
		return root;
	}
	if (cmp > 0) {
		root->right = er(root->right, h, key);
		return root;
	}

	node* l = root->left;
	node* r = root->right;
	free(root->key_string);
	free_value(root->value);
	free(root);
	return merge(l, r);
}

void erase(bst* v, const char* key) {
	v->head = er(v->head, hash_string(key), key);
}

static void print_b(node* root) {
	if (!root) return;
	print_b(root->left);
	printf("%s : ", root->key_string);
	print_value(root->value);
	printf("\n");
	print_b(root->right);
}

void print_bst(bst* v) {
	print_b(v->head);
	printf("\n");
}

static void free_nodes(node* root) {
	if (!root) return;
	free_nodes(root->left);
	free_nodes(root->right);
	free(root->key_string);
	free_value(root->value);
	free(root);
}

void free_bst(bst* v) {
	free_nodes(v->head);
	v->head = NULL;
}