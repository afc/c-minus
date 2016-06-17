#ifndef _UTILS_H_
#define _UTILS_H_

char * copy_string(cchar *);

node_t * new_decl_node(decl_k);

node_t * new_stmt_node(stmt_k);

node_t * new_expr_node(expr_k);

cchar *type_map(expr_t);

void draw_ruler(cchar *);

void print_token(token_t, cchar*);

void print_tree(const node_t *);

void reclaim_memory(node_t *);

#endif
