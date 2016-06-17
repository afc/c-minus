// ERRNO: [21, 40]
#include "globals.h"
#include "utils.h"
#include "scan.h"
#include "parse.h"

static token_t token;

static node_t *decl_list(void);
static node_t *decl(void);
static node_t *var_decl(void);
static node_t *para_list(void);
static node_t *para(void);

static node_t *comp_stmt(void);
static node_t *local_decl(void);
static node_t *stmt_list(void);
static node_t *statement(void);
static node_t *if_stmt(void);
static node_t *while_stmt(void);
static node_t *return_stmt(void);
static node_t *expr_stmt(void);

static node_t *expression(void);
static node_t *mixed_expr(node_t *);
static node_t *add_expr(node_t *);
static node_t *term(node_t *);
static node_t *factor(node_t *);
static node_t *id_stmt(void);
static node_t *args(void);

static inline void syntax_error(uint err_no, cchar *err_msg)
{ PANIC(err_no, err_msg); }

static void match_token(token_t excepted)
{
	if (token == excepted) {
		token = get_token();
		return ;
	}
	sprintf(median, "Unexcepted token -> `%s'", lexeme);
	if (excepted >= ENDFILE && excepted <= RBRACE)
		sprintf(
			median+strlen(median), 		// ACTS LIKE strcat()
			" Excepted -> `%s'", 
			token_map[excepted]
		);
	else
		sprintf(
			median+strlen(median), 
			" And excepted a unknown token type -> %d", 
			excepted
		);
	// I WON'T MOVE TO NEXT TOKEN
	syntax_error(21, median);
}

static expr_t match_atom()
{
	expr_t etype = ET_VOID;
	switch (token) {
		case INT:
			etype = ET_INT;
			token = get_token();
		break;
		
		case VOID:
			etype = ET_VOID;
			token = get_token();
		break;
		
		default:
			sprintf(median, "Excepted a atom type", token_map[token]);
			if (token >= ENDFILE && token <= RBRACE)
				sprintf(
					median+strlen(median), 
					" Got `%s'", 
					token_map[token]
				);
			syntax_error(22, median);
			// I WON'T MOVE TO NEXT TOKEN
		break;
	}
	
	return etype;
}

#define IS_ATOM(X) ((X) == INT || (X) == VOID)

node_t * parse(void)
{
	node_t *t;
	token = get_token();	// FETCH NEXT TOKEN
	t = decl_list();
	if (token != ENDFILE)
		syntax_error(23, "Unexcepted symbol at end of file");
	return t;
}

static node_t * decl_list(void)
{
	node_t *t = decl();
	node_t *u = t;
	while (token != ENDFILE) {
		node_t *v = decl();
		if (v && u) {
			u->sibling = v;
			u = v;
		}
	}
	return t;
}

static node_t * decl(void)
{
	node_t *t = NULL;
	expr_t dec_type;
	char *name = NULL;		// BETTER BE NULL
	
	dec_type = match_atom();
	if (token == ID)
		name = copy_string(lexeme);
	match_token(ID);
	
	switch (token) {
		case SEMI:
			// NOTE THAT IF name IS NULL
			// WE MAY NEEDN'T TO UPDATE t IMMEDIATELY
			// WE MAY JUDGE IF name IS NULL
			if (name != NULL)			// MATCHED ID
				t = new_decl_node(D_SCA);
			if (t != NULL) {
				t->var_data_type = dec_type;
				t->name = name;
				t->is_global = true;
			}
			match_token(SEMI);
		break;
		
		case LSQUARE:
			if (name != NULL)
				t = new_decl_node(D_VEC);
			if (t != NULL) {
				//~t->var_data_type = dec_type;
				t->var_data_type = ET_ARR;
				t->name = name;
				t->is_global = true;
			}
			match_token(LSQUARE);
			if (t != NULL) {
				t->val = atoi(lexeme);
				if (t->val <= 0) {
					sprintf(median, "Invalid array size -> %d", t->val);
					syntax_error(24, median);
				}
			}
			match_token(NUM);
			match_token(RSQUARE);
			match_token(SEMI);
		break;
		
		case LPAREN:
			if (name != NULL)
				t = new_decl_node(D_FUN);
			if (t != NULL) {
				t->func_ret_type = dec_type;
				t->name = name;
				// IT'S MEANLESS
				t->is_global = true;
			}
			match_token(LPAREN);
			if (t != NULL)
				t->child[0] = para_list();
			match_token(RPAREN);
			if (t != NULL)
				t->child[1] = comp_stmt();
		break;
		
		default:
			sprintf(median, "Unexctped token -> `%s'", lexeme);
			syntax_error(25, median);
			// MUST FETCH NEXT TOKEN -- OR CAUSE A DEAH-LOOP
			// IF TOKEN NOT IN (SEMI, LSQUARE, LPAREN) AND
			// IN LOOP OF decl_list()
			token = get_token();
		break;
	}
	
	return t;
}

static node_t * var_decl(void)
{
	// THIS FUNCTION HIGH ACTS SOMEWHAT LIKE decl()
	node_t *t = NULL;
	expr_t dec_type;
	char *name = NULL;
	
	dec_type = match_atom();
	if (token == ID)
		name = copy_string(lexeme);
	match_token(ID);
	
	switch (token) {
		case SEMI:
			if (name != NULL)
				t = new_decl_node(D_SCA);
			if (t != NULL) {
				t->var_data_type = dec_type;
				t->name = name;
			}
			match_token(SEMI);
		break;
		
		case LSQUARE:
			if (name != NULL)
				t = new_decl_node(D_VEC);
			if (t != NULL) {
				//~t->var_data_type = dec_type;
				t->var_data_type = ET_ARR;
				t->name = name;
			}
			match_token(LSQUARE);
			if (t != NULL) {
				t->val = atoi(lexeme);
				if (t->val <= 0) {
					sprintf(median, "Invalid array size `%d'", t->val);
					syntax_error(26, median);
				}
			}
			match_token(NUM);
			match_token(RSQUARE);
			match_token(SEMI);
		break;
		
		default:
			sprintf(median, "Unexctped token -> `%s'", lexeme);
			syntax_error(27, median);
			token = get_token();
		break;
	}
	
	return t;
}

static node_t * para_list(void)
{
	// NO ARGUMENT
	if (token == VOID || token == RPAREN) {
		if (token == VOID)
			match_token(VOID);
		return NULL;
	}
	
	node_t *t = para();
	node_t *u = t;
	if (t != NULL)
		while (token == COMMA) {
			match_token(COMMA);
			node_t *v = para();
			if (v != NULL) {
				u->sibling = v;
				u = v;
			}
		}
	
	return t;
}

static node_t * para(void)
{
	node_t *t;
	expr_t para_type;
	char *name = NULL;
	
	para_type = match_atom();
	if (token == ID)
		name = copy_string(lexeme);
	match_token(ID);
	
	if (token == LSQUARE) {
		match_token(LSQUARE);
		// ARRAY ARGUMENT MUST AS FORM OF foo[]
		match_token(RSQUARE);
		if (name != NULL)
			t = new_decl_node(D_VEC);
		t->var_data_type = ET_ARR;
	} else {
		if (name != NULL)
			t = new_decl_node(D_SCA);
		t->var_data_type = para_type;
	}
	
	if (t != NULL) {
		t->name = name;
		t->val = 0;
		//~t->var_data_type = para_type;
		t->is_parameter = true;
	}
	
	return t;
}

static node_t * comp_stmt(void)
{
	node_t *t = NULL;
	match_token(LBRACE);
	if ( (token != RBRACE) && (t = new_stmt_node(S_COMPOUND)) ) {
		if (IS_ATOM(token))
			t->child[0] = local_decl();
		if (token != RBRACE)
			t->child[1] = stmt_list();
	}
	match_token(RBRACE);
	return t;
}

static node_t * local_decl(void)
{
	node_t *t = NULL;
	if (IS_ATOM(token))
		t = var_decl();
	node_t *u = t;
	if (t != NULL)
		while (IS_ATOM(token)) {
			node_t *v = var_decl();
			if (v != NULL) {
				u->sibling = v;
				u = v;
			}
		}
	return t;
}

static node_t * stmt_list(void)
{
	if (token == RBRACE)
		return NULL;
	node_t *t = statement();
	node_t *u = t;
	while (token != RBRACE) {
		node_t *v = statement();
		if (v && u) {
			u->sibling = v;
			u = v;
		}
	}
	return t;
}

static node_t * statement(void)
{
	node_t *t = NULL;
	switch (token) {
		case IF:		t = if_stmt();			break;
		case WHILE:		t = while_stmt();		break;
		case RETURN:	t = return_stmt();		break;
		case LBRACE:	t = comp_stmt();		break;
		
		case ID:
		case NUM:
		case LPAREN:
		case SEMI:		t = expr_stmt();		break;
		
		default:
			sprintf(median, "Unexctped token -> `%s'", lexeme);
			syntax_error(28, median);
			token = get_token();
		break;
	}
	return t;
}

static node_t * if_stmt(void)
{
	node_t *t;
	node_t *expr;
	node_t *tcase;
	node_t *fcase = NULL;
	match_token(IF);
	match_token(LPAREN);
	expr = expression();
	match_token(RPAREN);
	tcase = statement();
	
	if (token == ELSE) {
		match_token(ELSE);
		fcase = statement();
	}
	t = new_stmt_node(S_IF);
	if (t != NULL) {
		t->child[0] = expr;
		t->child[1] = tcase;
		t->child[2] = fcase;
	}
	
	return t;
}

static node_t * while_stmt(void)
{
	node_t *t;
	node_t *expr;
	node_t *stmt;
	match_token(WHILE);
	match_token(LPAREN);
	expr = expression();
	match_token(RPAREN);
	stmt = statement();
	
	t = new_stmt_node(S_WHILE);
	if (t != NULL) {
		t->child[0] = expr;
		t->child[1] = stmt;
	}
	
	return t;
}

static node_t * return_stmt(void)
{
	match_token(RETURN);
	node_t *t;
	node_t *expr = NULL;
	t = new_stmt_node(S_RETURN);
	if (token != SEMI)
		expr = expression();
	if (t != NULL)
		t->child[0] = expr;
	match_token(SEMI);
	return t;
}

static node_t * expr_stmt(void)
{
	node_t *t = NULL;
	if (token == SEMI)
		match_token(SEMI);
	else if (token != RBRACE) {
		t = expression();
		match_token(SEMI);
	}
	return t;
}

static node_t * expression(void)
{
	node_t *t = NULL;
	node_t *lvalue = NULL;
	node_t *rvalue = NULL;
	bool got_lvalue = false;
	if (token == ID) {
		lvalue = id_stmt();
		got_lvalue = true;
	}
	
	if (got_lvalue == true && token == ASSIGN) {
		if (lvalue && lvalue->nodekind == N_EXPR 
				&& lvalue->whichkind.expr == E_ID) {
			match_token(ASSIGN);
			rvalue = expression();
			t = new_expr_node(E_ASSIGN);
			if (t != NULL) {
				t->child[0] = lvalue;
				t->child[1] = rvalue;
			}
		} else {
			syntax_error(29, "Attempting to assign to something not a lvalue");
			token = get_token();
		}
	} else
		t = mixed_expr(lvalue);			// PASS-DOWN
	
	return t;
}

/*
	THERE'S NO ROLL BACK FUNCTION -- SO WE NEED USE TO A 
	PASS DOWN ARGUMENT TO RECORD `ID' TOKEN ALREADY PARSED
		static node_t *mixed_expr(node_t *);
		static node_t *add_expr(node_t *);
		static node_t *term(node_t *);
		static node_t *factor(node_t *);
*/

static node_t * mixed_expr(node_t *pass)
{
	node_t *t;
	node_t *lexpr = NULL;
	node_t *rexpr = NULL;
	token_t op;
	
	lexpr = add_expr(pass);				// PASS-DOWN
	
	// MULTI-COMPARISON NOT ALLOWED
	if (token >= LT && token <= NE) {
		op = token;
		match_token(token);
		rexpr = add_expr(NULL);			// NOT INHERITED
		
		t = new_expr_node(E_OP);
		if (t != NULL) {
			t->child[0] = lexpr;
			t->child[1] = rexpr;
			t->op = op;
		}
	} else
		t = lexpr;
	
	return t;
}

static node_t * add_expr(node_t *pass)
{
	node_t *t = term(pass);
	node_t *u;
	// MULTI +/- SUPPORTED
	while (token == PLUS || token == MINUS) {
		u = new_expr_node(E_OP);
		// NOTE THAT IF MEMORY ALREADY EXHAUSTED 
		// FURTHER CALL OF new_expr_node IS FUTILE
		// SO I SIMPLY JUMP OUT THIS LOOP
		if (u == NULL)
			break;
		u->child[0] = t;
		u->op = token;
		t = u;
		match_token(token);
		t->child[1] = term(NULL);
	}
	return t;
}

static node_t * term(node_t *pass)
{
	node_t *t = factor(pass);
	node_t *u;
	while (token == TIMES || token == DIVIDE) {
		u = new_expr_node(E_OP);
		if (u == NULL)
			break;			//continue;
		u->child[0] = t;
		u->op = token;
		t = u;
		match_token(token);
		t->child[1] = factor(NULL);
	}
	return t;
}

static node_t * factor(node_t *pass)
{
	// THE END POINT OF PASS-DOWN CALL
	// NOTE THAT IF pass NOT NULL -- WHICH MEANS THAT
	// IT'S PASS-DOWN CALL -- SINCE WE ALREADY PARSED ID
	// in id_stmt() -- SO WE NEEDN'T FURTHER CALL OF IT
	// WHICH WE CAN NOW RETURN pass ITSELF
	if (pass != NULL) return pass;
	
	node_t *t = NULL;
	if (token == ID)
		t = id_stmt();
	else if (token == LPAREN) {
		match_token(LPAREN);
		t = expression();
		match_token(RPAREN);
	}
	else if (token == NUM) {
		t = new_expr_node(E_NUM);
		if (t != NULL) {
			t->val = atoi(lexeme);
			t->var_data_type = ET_INT;
		}
		match_token(NUM);
	}
	else {
		sprintf(median, "Unexctped token -> `%s'", lexeme);
		syntax_error(30, median);
		token = get_token();
	}
	
	return t;
}

static node_t *id_stmt(void)
{
	node_t *t;
	node_t *expr = NULL;
	node_t *argv = NULL;
	char *name = NULL;
	
	if (token == ID)
		name = copy_string(lexeme);
	match_token(ID);
	
	if (token == LPAREN) {
		match_token(LPAREN);
		argv = args();
		match_token(RPAREN);
		
		if (name != NULL)
			t = new_stmt_node(S_CALL);
		if (t != NULL) {
			t->child[0] = argv;
			t->name = name;
		}
	}
	else {
		if (token == LSQUARE) {
			match_token(LSQUARE);
			expr = expression();
			match_token(RSQUARE);
		}
		
		if (name != NULL)
			t = new_expr_node(E_ID);
		if (t != NULL) {
			t->child[0] = expr;
			t->name = name;
		}
	}
	
	return t;
}

static node_t * args(void)
{
	if (token == RPAREN) return NULL;
	node_t *t = expression();
	node_t *u = t;
	while (token == COMMA) {
		match_token(COMMA);
		node_t *v = expression();
		if (v && u) {
			u->sibling = v;
			u = v;
		}
	}
	return t;
}
