// ERRNO: [51, 60]
#include "analyze.h"
#include "symtab.h"
#include "utils.h"

static void do_build_symtab(node_t *);

static void checker_error(cchar *);

static void traverse(
	node_t *, 
	void (*)(node_t *), 
	void (*)(node_t *)
);

static void do_type_check(node_t *);

static void mark_globals(node_t *);

static void decl_builtins(void);

static bool para_check(const node_t *, const node_t *);

#define ____________ "PROTOTYPES AND I12NS"

void build_symtab(node_t *tree)
{
	if (trace_analyze) {
		draw_ruler("SYMBOL TABLE");
		fprintf(lst, "%-8s%-12s%-6s%-12s%-18s\n", "Scope", "Name", "Line", "Parameter", "Details");
	}
	
	decl_builtins();
	do_build_symtab(tree);
	
	// AT THIS POINT -- scopeno MUST BE 0
	// WHICH MEANS THAT THE OUT-MOST SCOPE
	// SO MARK THEM ALL TO GLOBALS
	mark_globals(tree);
	
	if (trace_analyze) {
		draw_ruler("GLOBALS");
		dump_current_scope();
		draw_ruler("");
	}
}

void inline type_check(node_t *tree)
{
	traverse(tree, NULL, do_type_check);
}

static void do_build_symtab(node_t *t)
{
	hash_t *lu_symbol;
	static node_t *enclosing_func = NULL;
	while (t != NULL) {
		if (t->nodekind == N_DECL)
			insert_symbol(t->name, t, t->lineno);
		
		if (t->nodekind == N_DECL && 
			t->whichkind.decl == D_FUN) {
			enclosing_func = t;
			if (trace_analyze)
				draw_ruler(t->name);
			enter_scope();
		}
		
		if (t->nodekind == N_STMT && 
			t->whichkind.stmt == S_COMPOUND)
			enter_scope();
		
		if ( (t->nodekind == N_EXPR && t->whichkind.expr == E_ID) || 
			(t->nodekind == N_STMT && t->whichkind.stmt == S_CALL) ) {
			lu_symbol = lookup_symbol(t->name);
			if (lu_symbol == NULL) {
				// NEED char err_msg[LENGTH] REMAINS UNKNOWN!
				sprintf(median, "Unknown Identifier `%s'", t->name);
				checker_error(median);
			}
			else
				t->declaration = lu_symbol->decl;
		}
		
		if (t->nodekind == N_STMT && t->whichkind.stmt == S_RETURN)
			t->declaration = enclosing_func;
		
		for (tint i = 0; i < MAX_CHILDS; i++)
			do_build_symtab(t->child[i]);
		
		if ( (t->nodekind == N_DECL && t->whichkind.decl == D_FUN) ||
			(t->nodekind == N_STMT && t->whichkind.stmt == S_COMPOUND) ) {
			if (trace_analyze)
				dump_current_scope();
			leave_scope();
		}
		
		t = t->sibling;
	}
}

static void checker_error(cchar *str)
{
	static char err_msg[LENGTH];
	strncpy(err_msg, str, LENGTH-1);
	err_msg[LENGTH-1] = '\0';
	sprintf(err_msg, "Type check -> %s", str);
	PANIC(51, err_msg);
	is_error = true;
}

static void traverse(
	node_t *tree, 
	void (*pre_order_func)(node_t *), 
	void (*post_order_func)(node_t *)
)
{
	tint i;
	while (tree != NULL) {
		// NOTE THAT HERE tree NOT NULL
		// SO IN pre_order_func() AND post_order_func()
		// WE NEEDN'T TO JUDGE IF tree IS NULL
		if (pre_order_func != NULL)
			pre_order_func(tree);
		for (i = 0; i < MAX_CHILDS; i++)
			traverse(tree->child[i], pre_order_func, post_order_func);
		if (post_order_func != NULL)
			post_order_func(tree);
		tree = tree->sibling;
	}
}

static void do_type_check(node_t *t)
{
	// t == NULL TRIGGERS RUN-TIME ERROR
	// FORTUNATELY ONLY traverse() CALLS ME
	// AND traverse() PROVIDES ME A NON-NULL t
	static char err_msg[LENGTH];
	switch (t->nodekind) {
		case N_DECL:
			switch (t->whichkind.decl) {
				case D_SCA: t->expr_type = t->var_data_type; break;
				
				case D_VEC: t->expr_type = ET_ARR; break;
				
				case D_FUN: t->expr_type = ET_FUNC; break;
				
				default:
					sprintf(err_msg, "Unknown declaration type in type-chcker");
					PANIC(52, err_msg);
				break;
			}
		break;
		
		case N_STMT:
			switch(t->whichkind.stmt) {
				case S_IF:
					if (t->child[0]->expr_type != ET_INT)
						checker_error("if-expression must be integer");
				break;
				
				case S_WHILE:
					if (t->child[0]->expr_type != ET_INT)
						checker_error("while-expression must be integer");
				break;
				
				case S_RETURN:
					if (t->declaration->func_ret_type == ET_INT) {
						if (t->child[0] == NULL || t->child[0]->expr_type != ET_INT)
							checker_error("return-expression is either void nor an integer");
					}
					else if (t->declaration->func_ret_type == ET_VOID) {
						if (t->child[0] != NULL)
							checker_error("void-return-expression must be void");
					}
				break;
				
				case S_CALL:
					if (para_check(t->declaration, t) == false) {
						sprintf(err_msg, "Formal and actual parameters "
							"of function `%s' are not matched", t->name);
						checker_error(err_msg);
					}
					t->expr_type = t->declaration->func_ret_type;
				break;
				
				case S_COMPOUND: t->expr_type = ET_VOID; break;
				
				default:
					sprintf(err_msg, "Unknown statement type in type-checker");
					PANIC(53, err_msg);
				break;
			}
		break;
		
		case N_EXPR:
			switch (t->whichkind.expr) {
				case E_OP:
					if (t->op >= PLUS && t->op <= DIVIDE) {
						expr_t child_0 = t->child[0]->expr_type;
						expr_t child_1 = t->child[1]->expr_type;
						if (child_0 == ET_INT && child_1 == ET_INT)
							t->expr_type = ET_INT;
						else
							checker_error("Arithmetic operators must have integer operands");
					}
					else if (t->op >= LT && t->op <= NE) {
						expr_t child_0 = t->child[0]->expr_type;
						expr_t child_1 = t->child[1]->expr_type;
						if (child_0 == ET_INT && child_1 == ET_INT)
							t->expr_type = ET_INT;
						else
							checker_error("Relational operators must have integer operands");
					}
					else
						checker_error("Unknown operator in type-checker");
				break;
				
				case E_NUM: t->expr_type = ET_INT; break;
				
				case E_ID:
					if (t->declaration->expr_type == ET_INT) {
						if (t->child[0] == NULL)
							t->expr_type = ET_INT;
						else
							checker_error("Cannot access a non-array element");
					}
					else if (t->declaration->expr_type == ET_ARR) {
						if (t->child[0] == NULL)
							t->expr_type = ET_ARR;
						else {
							if (t->child[0]->expr_type == ET_INT)
								t->expr_type = ET_INT;
							else {
								sprintf(err_msg, "Array `%s' must be indexed by a integer", t->name);
								checker_error(err_msg);
							}
						}
					}
					else
						checker_error("Illegal identifier type");
				break;
				
				case E_ASSIGN:
					if (t->child[0]->expr_type == ET_INT && t->child[1]->expr_type == ET_INT)
						t->expr_type = ET_INT;
					else
						checker_error("Both assigner and assignee must be integer");
				break;
				
				default:
					sprintf(err_msg, "Unknown expression type in type-checker");
					PANIC(54, err_msg);
				break;
			}
		break;
		
		default:
			sprintf(err_msg, "Unknown node type in type-chcker");
			PANIC(55, err_msg);
		break;
	}
}

static void mark_globals(node_t *tree)
{
	node_t *cursor = tree;
	while (cursor != NULL) {
		if (cursor->nodekind == N_DECL && (
			cursor->whichkind.decl == D_SCA ||
			cursor->whichkind.decl == D_VEC))
			cursor->is_global = true;
		cursor = cursor->sibling;
	}
}

static void decl_builtins(void)
{
	node_t *input, *output, *output_arg;
	
	// int input(void)
	input = new_decl_node(D_FUN);
	input->name = copy_string("input");
	input->func_ret_type = ET_INT;
	input->expr_type = ET_FUNC;
	input->local_size = 0;
	
	// void output(int)
	output_arg = new_decl_node(D_SCA);
	output_arg->name = copy_string("_argument");
	output_arg->var_data_type = ET_INT;
	output_arg->expr_type = ET_INT;
	// DON'T FORGET THIS
	output_arg->is_parameter = true;
	
	output = new_decl_node(D_FUN);
	output->name = copy_string("output");
	output->func_ret_type = ET_VOID;
	output->expr_type = ET_FUNC;
	output->child[0] = output_arg;
	output->local_size = 0;
	
	// MAKE THEM TO GLOBAL SCOPE
	insert_symbol("input", input, 0);
	insert_symbol("output", output, 0);
}

static bool para_check(const node_t *formal, const node_t *actual)
{
	node_t *p_formal = formal->child[0];
	node_t *p_actual = actual->child[0];
	while (p_formal != NULL && p_actual != NULL) {
		if (p_formal->expr_type != p_actual->expr_type)
			return false;
		p_formal = p_formal->sibling;
		p_actual = p_actual->sibling;
	}
	// IF ONE THEM ARE NULL -- BUT NOT BOTH
	//if ((p_formal == NULL && actual != NULL) ||
	//	(p_formal != NULL && actual == NULL))
	if ((int) p_formal ^ (int) p_actual)
		return false;
	return true;
}
