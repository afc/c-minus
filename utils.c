// ERRNO: [1, 10]
#include "globals.h"
#include "utils.h"

cchar *token_map[] = {
	"(EOF)", "(ERROR)", 
	"else", "if", "int", "return", "void", "while", 
	"(ID)", "(NUM)", 
	"+", "-", "*", "/", 
	"<", "<=", ">", ">=", "==", "!=", "=", 
	";", ",", "(", ")", 
	"[", "]", "{", "}"
};

static node_t *new_node(void);

char * copy_string(cchar *str)
{
	if (str == NULL) return NULL;
	const int n = strlen(str) + 1;
	char *cp_str = (char *) malloc( n * sizeof(char) );
	if (cp_str == NULL)
		PANIC(1, "Memory exhausted in copy_string()");
	else
		strcpy(cp_str, str);
	return cp_str;
}

static node_t *new_node(void)
{
	node_t *t = (node_t *) malloc( sizeof(node_t) );
	if (t == NULL) {
		PANIC(2, "Memory exhausted in new_node()");
		return NULL;
	}
	for (tint i = 0; i < MAX_CHILDS; i++)
		t->child[i] = NULL;
	t->sibling = NULL;
	t->lineno = lineno;
	t->val = 0;
	t->name = NULL;
	t->declaration = NULL;
	t->expr_type = ET_VOID;
	t->is_parameter = false;
	t->is_global = false;
	t->offset = 0;
	t->local_size = 0;
	return t;
}

node_t * new_decl_node(decl_k kind)
{
	node_t *t = new_node();
	if (t != NULL) {
		t->nodekind = N_DECL;
		t->whichkind.decl = kind;
	}
	return t;
}

node_t * new_stmt_node(stmt_k kind)
{
	node_t *t = new_node();
	if (t != NULL) {
		t->nodekind = N_STMT;
		t->whichkind.stmt = kind;
	}
	return t;
}

node_t * new_expr_node(expr_k kind)
{
	node_t *t = new_node();
	if (t != NULL) {
		t->nodekind = N_EXPR;
		t->whichkind.expr = kind;
	}
	return t;
}

cchar *type_map(expr_t type)
{
	switch (type) {
		case ET_VOID:	return "void";
		case ET_INT:	return "int";
		case ET_BOOL:	return "bool";
		case ET_ARR:	return "int[]";
		case ET_FUNC:	return "function";
	}
	return "(unknown)";
}

#define LINE_LENG 79

void draw_ruler(cchar *str)
{
	if (str == NULL) return ;
	uint str_leng, half_i, dashes;
	str_leng = str[0] ? strlen(str) : 0;
	if (str_leng >= LINE_LENG) {
		puts(str);
		return ;
	}
	half_i = (LINE_LENG - str_leng) / 2;
	
	for (uint i = 0; i < half_i; i++)
		fprintf(lst, "-");
	
	if (str_leng > 0)
		fprintf(lst, "%s", str);
	
	dashes = LINE_LENG - str_leng;
	for (uint i = half_i; i < dashes; i++)
		fprintf(lst, "-");
	
	fprintf(lst, "\n");
}

void print_token(token_t token, cchar* lexeme)
{
	switch (token) {
		case ERROR:		fprintf(lst, "ERROR: %s", lexeme);	break;
		
		case ELSE:
		case IF:
		case INT:
		case RETURN:
		case VOID:
		case WHILE:		fprintf(lst, "Keyword: %s", lexeme);	break;
		
		case ID:		fprintf(lst, "ID, name: %s", lexeme);	break;
		case NUM:		fprintf(lst, "NUM, val: %s", lexeme);	break;
		
		case ENDFILE:
		case PLUS:
		case MINUS:
		case TIMES:
		case DIVIDE:
		case LT:
		case LE:
		case GT:
		case GE:
		case EQ:
		case NE:
		case ASSIGN:
		case SEMI:
		case COMMA:
		case LPAREN:
		case RPAREN:
		case LSQUARE:
		case RSQUARE:
		case LBRACE:
		case RBRACE:	fprintf(lst, token_map[token]);			break;
		
		default:
			sprintf(median, "Unknown token %d `%s'", token, lexeme);
			PANIC(3, median);
		break;
	}
}

static uint indentno = 0;
static inline void indent(bool flg)
{ indentno += (flg ? 4 : -4); }

static void print_spaces(void)
{
	tint i = indentno;
	while (i--)
		fprintf(lst, " ");
}

void print_tree(const node_t *tree)
{
	indent(true);
	while (tree != NULL) {
		print_spaces();
		if (tree->nodekind == N_DECL)
			switch (tree->whichkind.decl) {
				case D_SCA:
					fprintf(
						lst, "%s: %s type: %s\n", 
						(tree->is_parameter ? "parameter" : "variable"), 
						tree->name, type_map(tree->var_data_type)
					);
				break;
				
				case D_VEC:
					fprintf(
						lst, "%s: %s type: %s[%d]\n", 
						(tree->is_parameter ? "parameter" : "variable"), 
						tree->name, type_map(tree->var_data_type), tree->val
					);
				break;
				
				case D_FUN:
					fprintf(
						lst, "%s: %s type: %s\n", 
					 	"function", tree->name, 
 						type_map(tree->func_ret_type)
					);
				break;
				
				default: PANIC(4, "Unknown declaration type"); break;
			}
		else if (tree->nodekind == N_STMT)
			switch (tree->whichkind.stmt) {
				case S_IF:
					fprintf(lst, "if:\n");
				break;
				
				case S_WHILE:
					fprintf(lst, "while:\n");
				break;
				
				case S_RETURN:
					fprintf(lst, "return:\n");
				break;
				
				case S_CALL:
					fprintf(lst, "call: %s\n", tree->name);
				break;
				
				case S_COMPOUND:
					fprintf(lst, "(compound):\n");
				break;
				
				default: PANIC(5, "Unknown statement type"); break;
			}
		else if (tree->nodekind == N_EXPR)
			switch (tree->whichkind.expr) {
				case E_OP:
					fprintf(lst, "operator: ");
					print_token(tree->op, "");
					fprintf(lst, "\n");
				break;
				
				case E_NUM:
					fprintf(lst, "constant: %d\n", tree->val);
				break;
				
				case E_ID:
					fprintf(lst, "identifier: %s\n", tree->name);
				break;
				
				case E_ASSIGN:
					fprintf(lst, "assign:\n");
				break;
				
				default: PANIC(6, "Unknown expression type"); break;
			}
		else
			PANIC(7, "Unknown node type");
		
		for (tint i = 0; i < MAX_CHILDS; i++)
			print_tree(tree->child[i]);
		tree = tree->sibling;
	}
	indent(false);
}

void reclaim_memory(node_t *t)
{
	// A FORM OF `HEAD-RECURSIVE' :-(
	// TO JUDGE tree->sibling AND tree->child[i]
	// CAN REDUCE THE NUMBER OF FUNCTION CALL
	if (t->sibling)
		reclaim_memory(t->sibling);
	for (tint i = 0; i < MAX_CHILDS; i++)
		if (t->child[i])
			reclaim_memory(t->child[i]);
	if (t->nodekind == N_DECL)
		free(t->name);
	else if (t->nodekind == N_STMT && t->whichkind.stmt == S_CALL)
		free(t->name);
	else if (t->nodekind == N_EXPR && t->whichkind.expr == E_ID)
		free(t->name);
	// DUE TO THE POST-TRAVERSAL -- NOW WE CAN DROP THE WHOLE NODE
	free(t);
}
