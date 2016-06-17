#ifndef _SYMTAB_H_
#define _SYMTAB_H_

#include "globals.h"

typedef struct hash_t {
	struct hash_t *next;
	char *name;
	node_t *decl;
	uint decl_line;
} hash_t;

void init_symtab(void);

void insert_symbol(cchar *, node_t *, uint);

bool is_symbol_declared(cchar *);

hash_t * lookup_symbol(cchar *);

void dump_current_scope(void);

void enter_scope(void);

void leave_scope(void);

#endif
