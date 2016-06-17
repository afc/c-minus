#include "cgen.h"
#include "code.h"

#define NIL 0		// PLACEHOLDER

static uint local_bound = 0;

static uint scope_depth = 0;

static bool has_outmost_ret = false;

static void do_code_gen(node_t *);

static void gen_func(node_t *);

static void calc_offset(node_t *);

static void calc_offset2(node_t *);

static void gen_stmt(node_t *);

static void gen_expr(node_t *, bool, bool);

static void gen_if(node_t *);

static void gen_while(node_t *);

static void gen_return(node_t *);

static void gen_call(node_t *);

void code_gen(node_t *tree, cchar *outfile)
{
	out = fopen(outfile, "w");
	if (out == NULL) {
		sprintf(median, "Cannot open `%s'", outfile);
		PANIC(1, median);
	}
	
	// IN TVM -- ALL REGISTERS INITIALIZED TO ZERO
	// AX BX PC BP FP TP INCLUDED
	
	emit_ro("HALT", NIL, NIL, NIL, "in case of no main()");
	
	// SPECIAL THINGS ABOUT `ST' AND `LD' OPERATION
	// WHEN s SET TO FP OR TP -- WE NEED EXTRA-ACTION
	// IN TM:
	//			  |  FP_REG	 |  TP_REG
	//		ST	  |    +1	 |    -1
	//	  --------|----------|---------
	//		LD	  |     0	 |    +1
	// WHICH ACTIONS ON TP ACTS LIKE A REAL STACK
	// HOWEVER FP ACTS SIGHTLY DIFFERENT -- `LD' NOT POP OUT
	// MOSTLY :. IT WILL RESTORED BY `RET'
	
	emit_rm("ST", AX, 0, TP, "backup PC to TP");
	emit_rm("ST", FP, 0, TP, "backup FP to TP");
	
	do_code_gen(tree);
	fclose(out);
}

static void do_code_gen(node_t *tree)
{
	// UPDATE BP TO RECORD SIZE OF PUBLIC-AREA
	uint public_bound = 0;
	node_t *temp = tree;
	while (tree != NULL) {
		if (tree->nodekind == N_DECL) {
			if (tree->whichkind.decl == D_SCA)
				tree->offset = public_bound++;
			else if (tree->whichkind.decl == D_VEC) {
				tree->offset = public_bound;
				public_bound += tree->val;
			}
		}
		// SPLITED BY SCOPE-0
		tree = tree->sibling;
	}
	if (public_bound != 0) {
		emit_ro("LDC", BP, public_bound, NIL, "update public area");
		emit_ro("LDC", FP, public_bound, NIL, "also apply to FP");
	}
	
	int jmp_main = emit_skip(1);	// PREPARE TO JUMP TO main()
	static uint entry_point = 0;	// LOCATION OF main()
	
	tree = temp;
	while (tree != NULL) {
		if (tree->nodekind == N_DECL && tree->whichkind.decl == D_FUN) {
			tree->offset = emit_skip(0);	// GET CURRENT INST. LINE
			if (entry_point == 0 && strcmp(tree->name, "main") == 0)
				entry_point = tree->offset;
			gen_func(tree);
		}
		tree = tree->sibling;
	}
	
	emit_backup(jmp_main);
	emit_rm("LDC", PC, entry_point, NIL, "jump to main()");
	emit_restore();
}

static void gen_func(node_t *tree)
{	// tree NOT NULL
	calc_offset(tree);
	gen_stmt(tree->child[1]);	// FUNCTION BODY
	
	scope_depth = 0;			// TO DEFAULT
	
	// WE NEED TO ASSURE HERE IS NOT `return ;' IN CURRENT FUNC.
	if (has_outmost_ret) {
		has_outmost_ret = false;	// ALSO TO DEFAULT
		return ;
	}
	
	// WE NEED A MANUALLY RETURN FOR NO RETURN-STMT
	sprintf(median, "general return inst. of %s()", tree->name);
	emit_ro("RET", NIL, NIL, NIL, median);
/*
	// THE ACTUAL OPERATION OF `RET' IS EQUAL TO:
	// DONE BY TVM
	emit_rm("LD", FP, 0, TP, "restore FP");
	emit_rm("LD", PC, 0, TP, "restore PC");
*/
}

static void calc_offset(node_t *tree)
{
	// COMPOUND-STMT -> INNER-DECL
	node_t *cursor = tree->child[1];
	local_bound = 0;
	if (cursor) cursor = cursor->child[0];
	while (cursor != NULL) {
		if (cursor->nodekind == N_DECL) {
			decl_k dtype = cursor->whichkind.decl;
			if (dtype == D_SCA || dtype == D_VEC) {
				cursor->offset = local_bound;
				local_bound += (dtype == D_SCA) ? 1 : cursor->val;
			}
		}
		cursor = cursor->sibling;
	}
	
	// COMPOUND-STMT -> SCOPED-DECL
	cursor = tree->child[1];
	if (cursor->child[1])
		calc_offset2(cursor->child[1]);
	
	// A AD-HOC METHOD TO UPDATES main()'S LOCAL AREA
	if (local_bound != 0 && strcmp(tree->name, "main") == 0)
		emit_ro("LDA", FP, local_bound, FP, "update main() local area");
	
	tree->local_size = local_bound;
	
	// CALCULATE PARAMETER OFFSET
	cursor = tree->child[0];
	while (cursor != NULL) {
		if (cursor->nodekind == N_DECL) {
			decl_k dtype = cursor->whichkind.decl;
			if (dtype == D_SCA || dtype == D_VEC)
				cursor->offset = local_bound++;
		}
		cursor = cursor->sibling;
	}
}

static void calc_offset2(node_t *cursor)
{
	while (cursor != NULL) {
		for (tint i = 0; i < MAX_CHILDS; i++)
			calc_offset2(cursor->child[i]);
		// SINCE `dtype' NOT D_FUN IT MUST BE `D_SCA' || `D_VEC'
		decl_k dtype = cursor->whichkind.decl;
		if (cursor->nodekind == N_DECL && dtype != D_FUN) {
			cursor->offset = local_bound;
			local_bound += (dtype == D_SCA) ? 1 : cursor->val;
		}
		cursor = cursor->sibling;
	}
}

static void gen_stmt(node_t *cursor)
{
	bool is_in_compound = false;
	while (cursor != NULL) {
		if (cursor->nodekind == N_EXPR && cursor->whichkind.expr == E_ASSIGN) {
			gen_expr(cursor->child[1], false, true);
			// NOW AX STORES ASSIGNEE -- PUSH IT INTO TP
			emit_rm("ST", AX, 0, TP, "[1] store assignee into TP");
			
			node_t *tmp = cursor->child[0]->declaration;
			if (tmp->nodekind == N_DECL && tmp->whichkind.decl == D_VEC && tmp->is_parameter)
				gen_expr(cursor->child[0], false, false);
			else
				gen_expr(cursor->child[0], true, false);
			// NOW AX STORES ASSIGNER -- POP THE ASSIGNEE INTO BX FROM TP
			emit_rm("LD", BX, 0, TP, "load assignee from TP");
			
			// stack[0 + reg[AX]] = reg[BX]
			emit_rm("ST", BX, 0, AX, "assigner in AX");
			
			//emit_rm("LDA", AX, 0, BX, "assign result into AX");
		} else if (cursor->nodekind == N_STMT)
			switch (cursor->whichkind.stmt) {
				case S_IF:			gen_if(cursor);			break;
				case S_WHILE:		gen_while(cursor);		break;
				case S_RETURN:		gen_return(cursor);		break;
				case S_CALL:
				// a label can only be part of a statement and a declaration is not a statement
				; // WE CAN ELIMINATE IT USING A LEADING SEMICOLON
					// SKIP 1 STEP FOR LATER CALC SKIP-STEP-LENGH
					int bak_loc = emit_skip(1);
					
					// WE MUST FIRST PRE-PUSH PC AND FP INTO STACK
					emit_rm("ST", BX, 0, TP, "backup PC to TP");
					emit_rm("ST", FP, 0, TP, "backup FP to TP");
					
					gen_call(cursor);
					
					int current_loc = emit_skip(0);
					// REWIND AND PUT SKIP-STEP-LENGH IN IT
					emit_backup(bak_loc);
					emit_rm("LDA", BX, current_loc-bak_loc-1, PC, "calculate skip-step length");
					
					emit_restore();
				break;
				case S_COMPOUND:
					// cursor->child[0] = (LOCAL_DECLS)
					// NOTE THAT: child[0] ALREADY 
					//   DONE BY calc_local_offset()
					// cursor->child[1] = (STATEMENTS)
					is_in_compound = true;
					scope_depth++;
					gen_stmt(cursor->child[1]);
				break;
			}
		else if (cursor->nodekind == N_EXPR)
			; //gen_expr(cursor, false, true);	// OPTIMIZATION
		cursor = cursor->sibling;
	}
	// WE'RE LEAVING A COMPOUND SCOPE
	if (is_in_compound) scope_depth--;
}

static void gen_expr(node_t *tree, bool need_addr, bool load_val)
{
	node_k nkind = tree->nodekind;
	if (nkind == N_STMT)
		if (tree->whichkind.stmt == S_CALL) {
			// HERE IS THE SIMILAR STRUCTURE AS IN gen_stmt()
			int bak_loc = emit_skip(1);
			
			emit_rm("ST", BX, 0, TP, "backup PC to TP");
			emit_rm("ST", FP, 0, TP, "backup FP to TP");
			
			gen_call(tree);
			int current_loc = emit_skip(0);
			emit_backup(bak_loc);
			emit_rm("LDA", BX, current_loc-bak_loc-1, PC, "calculate skip-step length");
			
			emit_restore();
			return ;
		}
	if (nkind != N_EXPR) return ;
	switch (tree->whichkind.expr) {
		case E_OP:
			// EVALUATE ORDER: LR
			gen_expr(tree->child[0], false, true);
			emit_rm("ST", AX, 0, TP, "store lhs to TP");
			gen_expr(tree->child[1], false, true);
			emit_rm("LD", BX, 0, TP, "load lhs to BX from TP");
			// AX: RSIDE   BX: LSIDE
			switch (tree->op) {
				case PLUS:
				case MINUS:
				case TIMES:
				case DIVIDE:
					;static cchar *opmap[] = {"ADD", "SUB", "MUL", "DIV"};
					cchar *p_op = opmap[tree->op - PLUS];
					sprintf(median, "AX = lhd %s rhs", p_op);
					emit_ro(p_op, AX, BX, AX, median);
				break;
				
				case LT:
				case LE:
				case GT:
				case GE:
				case EQ:
				case NE:
					emit_ro("SUB", AX, BX, AX, "AX = lhs - rhs");
					static cchar *jmap[] = {"JLT", "JLE", "JGT", "JGE", "JEQ", "JNE"};
					emit_rm(jmap[tree->op - LT], AX, 2, PC, "PC = PC+2 if satisfied");
					emit_rm("LDC", AX, 0, NIL, "comparison result 0");
					emit_rm("LDA", PC, 1, PC, "unconditional jump");
					emit_rm("LDC", AX, 1, NIL, "comparison result 1");
				break;
				
				default: break;
			}
		break;
		
		case E_NUM:
			emit_rm("LDC", AX, tree->val, NIL, "load constant");
		break;
		
		case E_ID:
			;static cchar *ld_map[] = {"LD", "LDA"};
			tint which_reg = tree->declaration->is_global ? BP : FP;
			if (tree->declaration->whichkind.decl == D_VEC) {
				if (tree->child[0] == NULL)		// ONLY POSSIBLE FOR FUNCTION INVOCATION
					emit_rm(ld_map[need_addr], AX, -(tree->declaration->offset + 1), which_reg, "load addr/val of [] into AX");
				else {
					gen_expr(tree->child[0], false, true);
					// NOW reg[AX] STORES ARRAY INDEX
					
					emit_rm(ld_map[need_addr], BX, -(tree->declaration->offset + 1), which_reg, "load addr/val of [*] into AX");
					emit_ro("SUB", AX, BX, AX, "calculate array's index");
					
					// WE NEED TO RESTORE ITS ORIGINAL VALUE TO CHECK IF IT NEEDS TO LOAD VALUE
					if (load_val)			// NEED VALUE ? -- THEN LOAD IT
						emit_rm("LD", AX, 0, AX, "load value of [*]");
				}
			}
			else if (tree->declaration->whichkind.decl == D_SCA)
				emit_rm(ld_map[need_addr], AX, -(tree->declaration->offset + 1), which_reg, "load addr/val of * into AX");
		break;
		
		case E_ASSIGN:
			gen_expr(tree->child[1], false, true);
			emit_rm("ST", AX, 0, TP, "[2] store assignee into TP");
			
			node_t *tmp = tree->child[0]->declaration;
			if (tmp->nodekind == N_DECL && tmp->whichkind.decl == D_VEC && tmp->is_parameter)
				gen_expr(tree->child[0], false, false);
			else
				gen_expr(tree->child[0], true, false);
			emit_rm("LD", BX, 0, TP, "load assignee from TP");
			// AX: LVALUE(BP-ADDR ALREADY ADDED IN AX)   BX: RVALUE
			
			// stack[0 + reg[AX]] = reg[BX]
			emit_rm("ST", BX, 0, AX, "assigner in AX");
			
			// reg[AX] = 0 + reg[BX]		RESULT IN reg[AX]
			emit_rm("LDA", AX, 0, BX, "assign result into AX");
		break;
	}
}

static void gen_if(node_t *tree)
{
	// IF-EXPR DOESN'T MEAN A NEW SCOPE
	gen_expr(tree->child[0], false, true);
	// NOW RESULT OF if IN AX
	int else_jmp_loc = emit_skip(1);
	
	// THEN-PART
	scope_depth++;
	gen_stmt(tree->child[1]);
	scope_depth--;
	int end_jmp_loc = emit_skip(1);
	int else_loc = emit_skip(0);
	emit_backup(else_jmp_loc);
	emit_rm_abs("JEQ", AX, else_loc, "jump to next block if AX is zero");
	emit_restore();
	
	// ELSE-PART
	scope_depth++;
	gen_stmt(tree->child[2]);
	scope_depth--;
	int end_loc = emit_skip(0);
	emit_backup(end_jmp_loc);
	emit_rm("LDC", PC, end_loc, NIL, "jump to end of if");
	emit_restore();
}

static void gen_while(node_t *tree)
{
	int while_head = emit_skip(0);
	// WHILE-EXPR (NO NEW SCOPE)
	gen_expr(tree->child[0], false, true);
	
	int jmp_while_end = emit_skip(1);
	// WHILE-BODY
	scope_depth++;
	gen_stmt(tree->child[1]);
	scope_depth--;
	
	int jmp_while_head = emit_skip(1);
	int while_end = emit_skip(0);
	
	emit_backup(jmp_while_end);
	emit_rm_abs("JEQ", AX, while_end, "jump to end if AX is zero");
	
	emit_backup(jmp_while_head);
	emit_rm("LDC", PC, while_head, NIL, "jump to head of while");
	
	emit_restore();
}

static void gen_return(node_t *tree)
{
	// ONLY WHEN THE RETURN IS THE OUT-MOST RETURN
	// 		CAN WE SET THE has_outmost_ret TO true!
	// OR LOGICALLY THERE IS A OUT-MOST RETURN-STMT
	
	// IN THE FOLLOWING FUNC THERE IS NO TRUE
	// OUT-MOST RETURN -- BUT EXISTS A LOGICALLY
	// OUT-MOST RETURN STATEMENT
	/*
		bool is_odd(int x)
		{
			if (x & 1 == 1)
				return true;
			else
				return false;
		}
	*/
	
	// WEAK-VERSION OF TRACING TRUE OUT-MOST RETURN-STMT IN FUNC
	if (scope_depth == 1) has_outmost_ret = true;
	
	// gen_expr(child[0]) AND RECONVER FP AND PC
	if (tree->child[0] != NULL)
		gen_expr(tree->child[0], false, true);
	// NOW reg[AX] CONTAINS RET-VALUE
	// NOTE THAT FOR A VOID-RET-TYPE FUNC
	// ITS `RET-VALUE' DEPENDS ON LAST OPERATION INTO AX
	
	// POP FP PC FROM reg[TP] -- AND RESTORE THEM
	emit_ro("RET", NIL, NIL, NIL, "restore FP and PC from TP");
}

static void gen_call(node_t *tree)
{
	uint pcot = 0;		// PARAMETER COUNT
	if (tree->child[0]) {
		// p IS POINT TO CALL'S ARGUMENT-LIST
		node_t *p = tree->child[0];
		bool is_need_addr, is_need_val;
		
		while (p != NULL) {
			is_need_addr = false;
			is_need_val = true;
			if (
				p->nodekind == N_EXPR &&
				p->whichkind.expr == E_ID && 
				p->declaration->whichkind.decl == D_VEC
			) {
				// DEFAULT VALUE IN CASE p IS A []
				// E.G. p->child[0] IS NULL
				is_need_addr = true;
				is_need_val = false;
				// IF ARGUMENT OF CALLEE p IS CALLER'S PARAMETER
				// THEN IT MEANS THAT ITS ACTUAL-ADDR IS ALREADY
				// BE IN STACK -- .: WE CAN'T LOAD ITS ADDR AGAIN
				// JUST USING `LD' TO LOAD ITS ACTUAL-ADDR
				if (p->declaration->is_parameter)
					is_need_addr = false;
				if (p->child[0] != NULL)
					is_need_val = true;
			}
			
			gen_expr(p, is_need_addr, is_need_val);
			// THERE IS NO NEED TO PUSH ARGUMENT OF output() INTO FP
			if (strcmp(tree->name, "output") == 0) {
				p = p->sibling;
				continue;
			}
			
			// :. PARAMETER WILL BE PUSHED IN THE FELLOWING CODE
			// reg[AX] STORES EACH ARGUMENT -- PUSH IT IN STACK
			//emit_rm("ST", AX, 0, FP, "push each argument into FP");
			
			emit_rm("ST", AX, 0, TP, "push each argument into TP");
			pcot++;		// RECORD PUSHED ARGUMENT
			
			// EACH ARGUMENT STORED IN sibling
			p = p->sibling;
		}
	}
	// NOW TO CHANGE PC TO CALLEE()
	if (strcmp(tree->name, "input") == 0) {
		emit_ro("IN", AX, NIL, NIL, "input integer from stdin");
		emit_ro("RET", NIL, NIL, NIL, "return stmt. of IN");
		return ;
	}
	if (strcmp(tree->name, "output") == 0) {
		// output() JUST READ ARGUMENT FROM AX -- :. ITS ARGUMENT
		// REQUIRES A INT-RET-TYPE FUNC AND WHEN THE FUNC RETURNS
		// ITS RESULT STORED IN AX -- SO WE CAN SAFELY USE IT
		emit_ro("OUT", AX, NIL, NIL, "output integer to stdout");
		emit_ro("RET", NIL, NIL, NIL, "return stmt. of OUT");
		return ;
	}
	
	// NOTE THAT WE PUSHED ARGUMENTS INTO TP -- AND 
	// WE NEED TO POP IT FROM TP AND PUSH IT INTO FP
	if (pcot != 0)
		emit_rm("LS", NIL, pcot, NIL, "load from TP and store argument to FP");
	
	// WE NEED TO UPDATE FP FOR LOCAL-VARIABLES(PARAMETER EXCLUDED)
	uint lsize = 0;
	if (tree->declaration) lsize = tree->declaration->local_size;
	if (lsize != 0 && tree->nodekind == N_STMT && tree->whichkind.stmt == S_CALL) {
		sprintf(median, "update %s()'s local size", tree->name);
		emit_ro("LDA", FP, lsize, FP, median);
	}
	
	sprintf(median, "jump to %s()", tree->declaration->name);
	emit_rm("LDC", PC, tree->declaration->offset, NIL, median);
}
