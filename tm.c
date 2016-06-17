#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#ifndef bool
    typedef enum {false, true} bool;
#endif
typedef const char cchar;

#define   CMD_SIZE   1024	// INSTRUCTION
#define   STK_SIZE   1024	// STACK
#define   REG_SIZE 		6	// REGISTER
#define   PC_REG 		2
#define   FP_REG 		4
#define   TP_REG 		5

#define   LINE_LIM    256
#define   WORD_LIM     32

typedef enum op_type {
	T_RO, 		// REG OPERANDS r, s, t
	T_RM, 		// REG r OFF d STK s
	T_RA		// REG r INT d + s
} op_type;

typedef enum op_code {
	
	// REGISTER-ONLY INSTRUCTION
	OP_HALT, 	// HALT -- OPERANDS ARE IGNORED
	OP_RET, 	// RESTORE FP AND PC -- OPERANDS INGORED
	OP_IN, 		// READ INT INTO reg[r] -- s, t IGNORED
	OP_OUT, 	// WRITE FROM reg[r] -- s, t IGNORED
	OP_ADD, 	// reg[r] = reg[s] + reg[t]
	OP_SUB, 	// reg[r] = reg[s] - reg[t]
	OP_MUL, 	// reg[r] = reg[s] * reg[t]
	OP_DIV, 	// reg[r] = reg[s] / reg[t]
	RO_LIM, 

	// REGISTER-TO-MEMORY
	OP_LD, 		// reg[r] = stk[d + reg[s]]
	OP_ST, 		// stk[d + reg[s]] = reg[r]
	RM_LIM, 
	
	// REGISTER-TO-ADDRESS
	OP_LS, 		// LD AND ST FOR d TIMES -- r, s INGORED
	OP_LDA, 	// reg[r] = d + reg[s]
	OP_LDC, 	// reg[r] = d -- s IGNORED
	OP_JLT,     // reg[PC_REG] = d + reg[s] IF reg[r] <  0
	OP_JLE,     // reg[PC_REG] = d + reg[s] IF reg[r] <= 0
	OP_JGT,     // reg[PC_REG] = d + reg[s] IF reg[r] >  0
	OP_JGE,     // reg[PC_REG] = d + reg[s] IF reg[r] >= 0
	OP_JEQ,     // reg[PC_REG] = d + reg[s] IF reg[r] == 0
	OP_JNE,     // reg[PC_REG] = d + reg[s] IF reg[r] != 0
	RA_LIM
} op_code;

typedef enum op_result {
   R_OKAY, 
   R_HALT, 
   R_CMD_ERR, 
   R_STK_ERR, 
   R_ZERO_DIV
} op_result;

typedef struct op_cmd {
	op_code cop;
	int carg1;
	int carg2;
	int carg3;
} op_cmd;

int iloc = 0;
int sloc = 0;
int trace_flag = false;
int icount_flag = false;

op_cmd instruction[CMD_SIZE];
int stack[STK_SIZE];
int reg[REG_SIZE];

static cchar * opcode_map[] = {
	// RO OPERANDS
	"HALT", "RET", "IN", "OUT", "ADD", "SUB", "MUL", "DIV", "???", 
	// RM
	"LD", "ST", "???", 
	// RA
	"LS", "LDA", "LDC", "JLT", "JLE", "JGT", "JGE", "JEQ", "JNE", "???"
};

static cchar *reg_map[] = {
	"AX", "BX", "PC", 
	"BP", "FP", "TP"
};

static cchar * result_map[] = {
	"OK", "Halted", "Instruction Memory Fault", 
	"Data Memory Fault", "Division by 0"
};

FILE *fin;

char buffer[LINE_LIM];
int len;
int col;
int num;
char word[WORD_LIM];
char ch;

op_type get_op_type(op_code c)
{
	if (c <= RO_LIM)
		return T_RO;
	else if (c <= RM_LIM)
		return T_RM;
	return T_RA;
}

void print_command(int loc)
{
	printf("%5d: ", loc);
	if (loc >= 0 && loc < CMD_SIZE) {
		printf("%6s%3d,", opcode_map[instruction[loc].cop], instruction[loc].carg1);
		switch (get_op_type(instruction[loc].cop)) {
			case T_RO:
				printf("%1d,%1d", instruction[loc].carg2, instruction[loc].carg3);
			break;
			
			case T_RM:
			case T_RA:
				printf("%3d(%1d)", instruction[loc].carg2, instruction[loc].carg3);
			break;
		}
		printf("\n");
	}
}

void get_chr(void)
{ ch = (++col < len) ? buffer[col] : ' '; }

bool get_nonblank(void)
{
	while (col < len && buffer[col] == ' ')
		col++;
	ch = (col < len) ? buffer[col] : ' ';
	return ch != ' ';
}

bool get_num(void)
{
	int sign;
	int term;
	int is_okay = false;
	num = 0;
	do {
		sign = 1;
		while (get_nonblank() && (ch == '+' || ch == '-')) {
			is_okay = false;
			if (ch == '-')
				sign = -sign;
			get_chr();
		}
		term = 0;
		get_nonblank();
		while (isdigit(ch)) {
			is_okay = true;
			term = term * 10 + (ch - '0');
			get_chr();
		}
		num = num + (term * sign);
	} while (get_nonblank() && (ch == '+' || ch == '-'));
	return is_okay;
}

bool get_word(void)
{
	int len = 0;
	if (get_nonblank ())
		while (isalnum(ch)) {
			if (len < WORD_LIM-1)
				word[len++] = ch;
			get_chr();
		}
	word[len] = '\0';
	return len != 0;
}

bool skip_chr(char c)
{
	if (get_nonblank() && ch == c) {
		get_chr();	// SKIP CURRENT -- GET NEXT
		return true;
	}
	return false;
}

bool is_eol(void)
{ return get_nonblank() == false; }

bool error(cchar *msg, int lineno, int cmdno)
{
	printf("Line %d", lineno);
	if (cmdno >= 0)
		printf(" (Instruction %d)", cmdno);
	printf("   %s\n", msg);
	return false;
}

int read_command(void)
{
	op_code op;
	int arg1, arg2, arg3;
	int loc, regno, lineno;
	// ALL REGISTERS INIT. ZERO
	for (regno = 0; regno < REG_SIZE; regno++)
		reg[regno] = 0;
	// BEGIN FROM POSITION 1
	reg[PC_REG] = 1;
	reg[TP_REG] = STK_SIZE-1;
	for (loc = 0; loc < STK_SIZE; loc++)
		stack[loc] = 0;
	for (loc = 0; loc < CMD_SIZE; loc++) {
		instruction[loc].cop = OP_HALT;
		instruction[loc].carg1 = 0;
		instruction[loc].carg2 = 0;
		instruction[loc].carg3 = 0;
	}
	lineno = 0;
	while (!feof(fin))
	{				// "\n\0" 2 BYTES
		fgets(buffer, LINE_LIM-2, fin);		// READ A NEW LINE
		col = 0;
		lineno++;
		len = strlen(buffer);
		if (buffer[len-1] == '\n')
			buffer[len-1] = '\0';
		else
			buffer[len] = '\0';
		// COMMENT BEGINS WITH *
		if (get_nonblank() && buffer[col] != '*') {
			// OPCODE FORM -> `LINENO: OP A, B, C'
			// READ LINENO
			if (!get_num())
				return error("Bad location", lineno, -1);
			loc = num;
			if (loc >= CMD_SIZE)
				return error("Location too large", lineno, loc);
			// SKIP `:' CHARACTER
			if (!skip_chr(':'))
				return error("Missing colon", lineno, loc);
			// GET OPERATOR
			if (!get_word())
				return error("Missing opcode", lineno, loc);
			// MATCH OPCODE
			op = OP_HALT;			// DEFAULT
			while (op < RA_LIM && strncmp(opcode_map[op], word, 5) != 0)
				op++;
			if (strncmp(opcode_map[op], word, 4) != 0)
				return error("Illegal opcode", lineno, loc);
			
			switch (get_op_type(op)) {
				// GET A, B, C INTO arg*
				case T_RO:
					if (!get_num() || num < 0 || num >= REG_SIZE)
						return error("Bad first register", lineno, loc);
					arg1 = num;
					if (!skip_chr(','))
						return error("Missing comma", lineno, loc);
					if (!get_num() || num < 0 || num >= REG_SIZE)
						return error("Bad second register", lineno, loc);
					arg2 = num;
					if (!skip_chr(',')) 
						return error("Missing comma", lineno, loc);
					if (!get_num() || num < 0 || num >= REG_SIZE)
						return error("Bad third register", lineno, loc);
					arg3 = num;
				break;
				
				case T_RM:
				case T_RA:
					if (!get_num() || num < 0 || num >= REG_SIZE)
						return error("Bad first register", lineno, loc);
					arg1 = num;
					if (!skip_chr(','))
						return error("Missing comma", lineno, loc);
					// THERE IS NO NEED TO CHECK IF arg2 OUT OF RANGE
					if (!get_num())
						return error("Bad displacement", lineno,loc);
					arg2 = num;
					if (!skip_chr('(') && !skip_chr(','))
						return error("Missing LParen", lineno, loc);
					if (!get_num() || num < 0 || num >= REG_SIZE)
						return error("Bad second register", lineno, loc);
					arg3 = num;
				break;
			}
			
			instruction[loc].cop = op;
			instruction[loc].carg1 = arg1;
			instruction[loc].carg2 = arg2;
			instruction[loc].carg3 = arg3;
		}
	}
	return true;
}

op_result step_command(void)
{
	op_cmd command;
	int pc;
	int r, s, t, a;
	bool is_ok = false;
	
	pc = reg[PC_REG];
	if (pc < 0 || pc >= CMD_SIZE)
		return R_CMD_ERR;
	reg[PC_REG] = pc + 1;	// UPDATE TO NEXT CMD.
	command = instruction[pc];
	switch (get_op_type(command.cop)) {
		case T_RO:
			r = command.carg1;
			s = command.carg2;
			t = command.carg3;
		break;
		
		case T_RM:
			r = command.carg1;
			s = command.carg3;
			a = command.carg2 + reg[s];
			if (a < 0 || a >= STK_SIZE) {
				printf("PC = %d a = %d\n", pc, a);
				return R_STK_ERR;
			}
		break;
		
		case T_RA:
			r = command.carg1;
			s = command.carg3;
			a = command.carg2 + reg[s];
			// HERE ALSO NEED A ERROR-CHECK
		break;
	}
	
	switch ( command.cop)
	{
		// RO
		case OP_HALT:
			printf("HALT: %1d,%1d,%1d\n", r, s, t);
			return R_HALT;
		break;
		
		case OP_RET:
			reg[FP_REG] = stack[++reg[TP_REG]];
			reg[PC_REG] = stack[++reg[TP_REG]];
		break;
		
		case OP_IN:
			while (!is_ok) {
				printf("Enter value for IN instruction: ");
				fflush(stdin);
				fflush(stdout);
				gets(buffer);
				len = strlen(buffer);
				col = 0;
				is_ok = get_num();
				if (!is_ok)
					printf("Illegal value\n");
				else
					reg[r] = num;
			}
		break;
		
		case OP_OUT:
			printf("OUT instruction prints: %d\n", reg[r]);
		break;
		
		case OP_ADD:  reg[r] = reg[s] + reg[t];  break;
		case OP_SUB:  reg[r] = reg[s] - reg[t];  break;
		case OP_MUL:  reg[r] = reg[s] * reg[t];  break;
		
		case OP_DIV:
			if (reg[t] == 0) return R_ZERO_DIV;
			reg[r] = reg[s] / reg[t];
		break;
		
		// RM   USING r, d(s)   a = d + reg[s]
		case OP_LD:
			// FP AND TP ACTUALLY ACTS LIKE STACK
			// 	FP WILL BE POPED BY `RET' TO RESTORE ORIGINAL-FP
		/*
			if (s == FP_REG)
				a = command.carg2 + (--reg[s]);
		*/
			if (s == TP_REG)
				a = command.carg2 + (++reg[s]);
			reg[r] = stack[a];
		break;
		
		case OP_ST:
			stack[a] = reg[r];
			if (s == FP_REG)
				reg[s]++;
			else if (s == TP_REG)
				reg[s]--;
		break;
		
		// RA
		case OP_LS:
			// NOW WE NEED LD AND ST FOR d TIMES
			// LOAD FROM TP AND STORE INTO FP
			;int d = command.carg2;
			while (d--)
				stack[reg[FP_REG]++] = stack[++reg[TP_REG]];
		break;
		
		case OP_LDA:	reg[r] = a;					break;
		case OP_LDC:	reg[r] = command.carg2;		break;
		
		case OP_JLT:   if (reg[r] <  0) reg[PC_REG] = a;  break;
		case OP_JLE:   if (reg[r] <= 0) reg[PC_REG] = a;  break;
		case OP_JGT:   if (reg[r] >  0) reg[PC_REG] = a;  break;
		case OP_JGE:   if (reg[r] >= 0) reg[PC_REG] = a;  break;
		case OP_JEQ:   if (reg[r] == 0) reg[PC_REG] = a;  break;
		case OP_JNE:   if (reg[r] != 0) reg[PC_REG] = a;  break;
		
		default: break;
	}
	
	return R_OKAY;
}

int do_command(void)
{
	int stepcot = 0, i;
	int printcot;
	op_result result;
	int regno, loc;
	do {
		//printf("Enter command: ");
		printf("(tm) ");
		fflush(stdin);
		fflush(stdout);
		gets(buffer);
		len = strlen(buffer);
		col = 0;
	}
	while (!get_word());
	
	char cmd = word[0];
	switch (cmd) {
		case 't':
			trace_flag = !trace_flag;
			printf("Tracing now ");
			puts(trace_flag ? "on." : "off.");
		break;
		
		case 'h':
			printf("Commands are:\n");
			printf("   s(tep <n>      "
				"Execute n (default 1) TM instructions\n");
			printf("   g(o            "
				"Execute TM instructions until HALT\n");
			printf("   r(egs          "
				"Print the contents of the registers\n");
			printf("   i(Mem <b <n>>  "
				"Print n instruction locations starting at b\n");
			printf("   d(Mem <b <n>>  "
				"Print n stack locations starting at b\n");
			printf("   t(race         "
				"Toggle instruction trace\n");
			printf("   p(rint         "
				"Toggle print of total instructions executed"
				" ('go' only)\n");
			printf("   c(lear         "
				"Reset simulator for new execution of program\n");
			printf("   h(elp          "
				"Cause this list of commands to be printed\n");
			printf("   q(uit          "
				"Terminate the simulation\n");
			printf("   9)             "
				"Print content from bp to fp.\n");
			printf("   0)             "
				"Print content from fp to bottom.\n");
		break;
		
		case '9':
			for (int i = 0; i < reg[FP_REG]; i++)
				printf("%d   ", stack[i]);
			puts("");
		break;
		
		case '0':
			for (int i = reg[TP_REG]+1; i < STK_SIZE; i++)
				printf("%d   ", stack[i]);
			puts("");
		break;
		
		case 'p':
			icount_flag = !icount_flag;
			printf("Printing instruction count now ");
			puts(icount_flag ? "on." : "off.");
		break;
		
		case 's':
			if (is_eol())
				stepcot = 1;
			else if (get_num())
				stepcot = abs(num);
			else
				printf("Step count?\n");
		break;
		
		case 'g': stepcot = 1; break;
		
		case 'r':
			for (i = 0; i < REG_SIZE; i++) {
				printf("%s: %4d   ", reg_map[i], reg[i]);
				if (i % 3 == 2) printf ("\n");
			}
		break;
		
		case 'i':
			printcot = 1;
			if (get_num()) {
				iloc = num;
				if (get_num()) printcot = num;
			}
			if (!is_eol())
				printf("Instruction locations?\n");
			else
			{
				while (iloc >= 0 && iloc < CMD_SIZE && printcot > 0) {
					print_command(iloc);
					iloc++;
					printcot--;
				}
			}
		break;
		
		case 'd':
			printcot = 1;
			if (get_num()) {
				sloc = num;
				if (get_num()) printcot = num;
			}
			if (!is_eol())
				printf("Data locations?\n");
			else
			{
				while (sloc >= 0 && sloc < STK_SIZE && printcot > 0) {
					printf("%5d: %5d\n", sloc, stack[sloc]);
					sloc++;
					printcot--;
				}
			}
		break;
		
		case 'c':
			iloc = 0;
			sloc = 0;
			stepcot = 0;
			for (regno = 0; regno < REG_SIZE; regno++)
				reg[regno] = 0;
			reg[PC_REG] = 1;
			reg[TP_REG] = STK_SIZE-1;
			for (loc = 0; loc < STK_SIZE; loc++)
				stack[loc] = 0;
		break;
		
		case 'q': return false; break;
		
		default: printf("Command %c unknown.\n", cmd); break;
	}
	
	result = R_OKAY;
	
	if (stepcot > 0) {
		if (cmd == 'g') {
			stepcot = 0;
			while (result == R_OKAY) {
				iloc = reg[PC_REG];
				if (trace_flag) print_command(iloc);
				result = step_command();
				stepcot++;
			}
			if ( icount_flag )
			printf("Number of instructions executed = %d\n",stepcot);
		}
		else {
			while (stepcot > 0 && result == R_OKAY) {
				iloc = reg[PC_REG];
				if (trace_flag) print_command(iloc);
				result = step_command();
				stepcot--;
			}
		}
		printf("%s\n", result_map[result]);
	}
	
	return true;
}

int main(int argc, char * argv[])
{
	char *fname;
	#ifndef DEBUG
		if (argc != 2) {
			printf("TM Usage: %s <filename>\n", argv[0]);
			exit(1);
		}
		fname = argv[1];
	#else
		int sfd = dup(STDIN_FILENO), sfd2;
		sfd2 = open("_input.txt", O_RDONLY);
		// REDIRECTING stdin
		dup2(sfd2, STDIN_FILENO);
		char readin[128];
		printf(">> ");
		gets(readin);
		fname = readin;
		int len = strlen(fname);
		if (strcmp(&fname[len-3], ".tm") != 0)
			strcat(fname, ".tm");
		// RECOVER REDIRECTED stdin
		dup2(sfd, STDIN_FILENO);
	#endif
	
	fin = fopen(fname, "r");
	
	if (fin == NULL) {
		printf("file '%s' not found\n", fname);
		exit(2);
	}
	printf("Codefile: %s\n", fname);
	
	if (!read_command())
		exit(3);
	fclose(fin);
	
	printf("TM simulation (enter h for help)...\n");
	bool is_done = false;
	while (is_done == false)
		is_done = !do_command();
	printf("Simulation done.\n");
	return 0;
}
