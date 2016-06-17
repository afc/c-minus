#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#define REVISION "2016/06/14"
#define VERSION  "v0.0.1a"

#define COPYRIGHT 												\
	"A toy-like simplified C compiler named C-\n" 				\
	"Last-modified: " REVISION " Version: " VERSION "\n"  		\
	"Acknowledgments: \n"										\
	"Idea mainly borrowed from K. C. Louden's C- project\n" 	\
	"Code mainly borrowed from K. C. Louden's TINY compiler\n" 	\
	"Parts borrowed from Ben Fowler's C- compiler project\n\n"

#define USAGE													\
	"Usage: cm [options] -f file\n"								\
	"Options: \n"												\
	"   -h     Display this information\n"						\
	"   -e     Echo the source code as parsing proceeds\n" 		\
	"   -s     Display lexeme occured for debug output\n" 		\
	"   -p     Display parser debug output information\n" 		\
	"   -a     Display semantic analyzer debug information\n" 	\
	"   -c     Display code generation debug information\n" 	\
	"   -f     Specify which file do you want to compile\n\n" 	\
	"Note that -f option cannot be omitted\n"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

//#include <stdbool.h>
#ifndef bool
    typedef enum {false, true} bool;
#endif

typedef unsigned int uint;
typedef unsigned char tint;		// TINY INTEGER
typedef const char cchar;

typedef enum token_t {
	ENDFILE, ERROR, 
	ELSE, IF, INT, RETURN, VOID, WHILE, 
	ID, NUM, 
	PLUS, MINUS, TIMES, DIVIDE, 
	LT, LE, GT, GE, EQ, NE, ASSIGN, 
	SEMI, COMMA, LPAREN, RPAREN, 
	LSQUARE, RSQUARE, LBRACE, RBRACE
} token_t;

extern cchar *token_map[];	// FROM utils.c

// THOSE EXTERNAL VARIBLES IMPORTED FROM `main.c'
extern FILE* src;				// SOURCE CODE
extern FILE* lst;				// LISTING OUTPUT
extern FILE* out;				// CODE FOR VM

extern uint lineno;				// SOURCE LINE-NO FOR LISTING
extern uint linecol;

#define LENGTH 128
char median[LENGTH];			// COOPERATE WITH PANIC()

#define PANIC(ERRNO, MSG)		 				\
	fprintf( 									\
		lst, 									\
		"[!] ERROR (NO.%u): %s at %u:%u\n",		\
		ERRNO, MSG, lineno, linecol				\
	),											\
	is_error = true

typedef enum { N_DECL, N_STMT, N_EXPR } node_k;
typedef enum { D_SCA, D_VEC, D_FUN } decl_k;
typedef enum { S_IF, S_WHILE, S_RETURN, S_CALL, S_COMPOUND } stmt_k;
typedef enum { E_OP, E_NUM, E_ID, E_ASSIGN } expr_k;

// expr_t IS USED FOR TYPE-CHECKING
typedef enum { ET_VOID, ET_INT, ET_BOOL, ET_ARR, ET_FUNC } expr_t;

#define MAX_CHILDS 3			// ALL USED IFF. stmt_k IS S_IF

typedef struct node_t {
	struct node_t *child[MAX_CHILDS];
	struct node_t *sibling;
	
	uint lineno;
	node_k nodekind;
	
	union {
		decl_k decl;
		stmt_k stmt;
		expr_k expr;
	} whichkind;
	
	token_t op;
	int val;		// NOT ONLY FOR int, ALSO FOR DIMENSION OF int[]
	char *name;
	
	union {
		expr_t func_ret_type;
		expr_t var_data_type;
	};
	
	// TYPE-CHECKER MARKS
	struct node_t *declaration;
	expr_t expr_type;
	
	bool is_parameter;
	bool is_global;
	
	// HERE'RE TWO CGEN-RELATED THINGS LEFTED
    uint offset;        // VAR OFFSET IN STACK OR FUNC LOCATION IN INST[]
	uint local_size;
} node_t;

extern bool echo_source;

extern bool trace_scan;

extern bool trace_parse;

extern bool trace_analyze;

extern bool trace_code;

extern bool is_error;

#endif
