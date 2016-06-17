// ERRNO: [41, 50]
#include "symtab.h"
#include "utils.h"

#define MAX_TAB_SIZE 257

// USED IN name OF hash_t TO
// SEPARATE DIFFERENT SCOPE
#define SCOPE_SEPARATOR ""

static hash_t * hashtable[MAX_TAB_SIZE];

static hash_t * scoped_list;

static uint scopeno;

static hash_t * new_symbol_node(
	cchar *, 
	node_t *, 
	uint
);

static uint hash_func(cchar *);

static void symtab_error(cchar *);

static void format_symbol_type(const node_t *);

static void do_dump_current_scope(hash_t *);

void init_symtab(void)
{
	memset(hashtable, (int) NULL, sizeof(hash_t *) * MAX_TAB_SIZE);
	scoped_list = NULL;
}

void insert_symbol(cchar *name, node_t *node, uint line)
{
	if (is_symbol_declared(name)) {
		sprintf(median, "Duplicate identifier `%s'", name);
		symtab_error(median);
		return ;
	}
	hash_t *new_node, *temp;
	uint hashno = hash_func(name);
	new_node = new_symbol_node(name, node, line);
	// APPEND IT TO HEAD OF `hashtable[hashno]'
	if (new_node != NULL) {
		temp = hashtable[hashno];
		hashtable[hashno] = new_node;
		new_node->next = temp;
	}
	// APPEND IT TO HEAD OF scoped_list
	new_node = new_symbol_node(name, node, line);
	if (new_node != NULL) {
		temp = scoped_list;
		scoped_list = new_node;
		new_node->next = temp;
	}
}

bool is_symbol_declared(cchar *name)
{
	// SCAN CURRENT SCOPE FOR DUPLICATE DECLARATION
	hash_t *cursor = scoped_list;
	while (cursor != NULL && 
		strcmp(cursor->name, SCOPE_SEPARATOR) != 0) {
		if (strcmp(cursor->name, name) == 0)
			return true;
		cursor = cursor->next;
	}
	return false;
}

hash_t * lookup_symbol(cchar *name)
{
	uint hashno = hash_func(name);
	hash_t *cursor = hashtable[hashno];
	while (cursor != NULL) {
		if (strcmp(cursor->name, name) == 0)
			return cursor;
		cursor = cursor->next;
	}
	return NULL;
}

void dump_current_scope(void)
{
	hash_t *cursor = scoped_list;
	if (cursor != NULL && 
		strcmp(cursor->name, SCOPE_SEPARATOR) != 0)
		do_dump_current_scope(cursor);
}

void enter_scope(void)
{
	hash_t *new_node, *temp;
	new_node = new_symbol_node(SCOPE_SEPARATOR, NULL, 0);
	if (new_node == NULL) return ;
	// APPEND new_node TO HEAD OF scoped_list
	temp = scoped_list;
	scoped_list = new_node;
	new_node->next = temp;
	scopeno++;
}

void leave_scope(void)
{
	hash_t *hashptr, *temp;
	uint hashno;
	while (scoped_list != NULL && 
			strcmp(scoped_list->name, SCOPE_SEPARATOR) != 0) {
		hashno = hash_func(scoped_list->name);
		hashptr = hashtable[hashno];
		
		// NOTE THAT:
		// SYMBOLS WERE INSERTED INTO THE hashtable AND scoped_list
		// IN THE SAME ORDER -- SO THE NAME IN THE hashptr AND
		// scoped_list MUST THE SAME -- USING assert() TO TEST IT
		//
		// ALSO NOTE:
		// WHEN SYMBOLS INSERTED INTO hashtable AND scoped_list
		// WE SIMPLY MAKE scoped_list POINT TO THE CORRESPONDING
		// hashtable[hashno] SO THEY MUST BE THE SAME -- SO WE
		// NEEDN'T TO JUDGE IF THEY HAVE THE SAME NAME OF STH. ELSE
		//
		// BESIDES:
		// WE SIMPLY JUDGE name -- THERE ALSO HAVE decl AND
		// decl_line IN IT -- WHICH WE HAVEN'T JUDGE IF THE SAME
		assert(hashptr != NULL && 
			strcmp(hashptr->name, scoped_list->name) == 0);
		
		// DELETE FROM hashtable
		temp = hashptr->next;
		free(hashptr);
		hashtable[hashno] = temp;
		
		// DELETE FROM scoped_list
		temp = scoped_list->next;
		free(scoped_list);
		scoped_list = temp;
	}
	
	// DELETE SCOPE SEPARATOR
	assert(scoped_list != NULL && 
		strcmp(scoped_list->name, SCOPE_SEPARATOR) == 0);
	temp = scoped_list->next;
	free(scoped_list);
	scoped_list = temp;
	scopeno--;
}

static hash_t * new_symbol_node(
	cchar *name, 
	node_t *decl, 
	uint line
)
{
	hash_t *node = (hash_t *) malloc(sizeof(hash_t));
	if (node == NULL) {
		PANIC(41, "Memory exhausted in new_symbol_node()");
		is_error = true;
		return NULL;
	}
	node->name = copy_string(name);
	node->decl = decl;
	node->decl_line = line;
	node->next = NULL;
	return node;
}

#define SHIFT 2

static uint hash_func(cchar *key)
{
	uint value = 0;
	for (uint i = 0; key[i] != '\0'; i++)
		value = (value << SHIFT) + key[i];
	if (value >= MAX_TAB_SIZE)
		value %= MAX_TAB_SIZE;
	return value;
}

static void symtab_error(cchar *msg)
{
	sprintf(median, "Symbol table -> %s", msg);
	PANIC(42, median);
	is_error = true;
}

static void format_symbol_type(const node_t *node)
{
	if (node == NULL || node->nodekind != N_DECL) {
		sprintf(
			median, 
			"Excepted a declaration type Got `%s'", 
			node->name
		);
		return ;
	}
	switch (node->whichkind.decl) {
		case D_SCA:
			sprintf(median, "Scalar of type `%s'", 
					type_map(node->var_data_type));
		break;
		
		case D_VEC:
			sprintf(median, "Array of type `%s' with %u elements", 
					type_map(node->var_data_type), node->val);
		break;
		
		case D_FUN:
			sprintf(median, "Function with return type `%s'", 
					type_map(node->func_ret_type));
		break;
		
		default:
			strcpy(median, "UNKNOWN TYPE");
		break;
	}
}

#define MAX_ID_LEN 64	// MAX_TOK_SIZE = 64

static void do_dump_current_scope(hash_t *cursor)
{	// PADDED NAME
	if (cursor->next != NULL && 
		strcmp(cursor->next->name, SCOPE_SEPARATOR) != 0)
		do_dump_current_scope(cursor->next);
	format_symbol_type(cursor->decl);
	fprintf(
		lst, "%-8d%-12s%-9d%c\t      %-30s\n", 
		scopeno, cursor->name, cursor->decl_line, 
		cursor->decl->is_parameter ? 'Y' : 'N', 
		median
	);
}
