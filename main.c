#include <getopt.h>
#include "globals.h"

// RVALUE OF #define CANNOT BE enum-TYPE
// E.G. false AND true -- SO I MAKE BOTH
// OF THEM INTO MACRO-TYPE WITH UPPER-CASE

#ifndef FALSE
	#define FALSE 0
#endif
#ifndef TRUE
	#define TRUE 1
#endif

#define NO_PARSE	FALSE

#define NO_ANALYZE 	FALSE

#define NO_CODE 	FALSE

#include "utils.h"

#if NO_PARSE
	#define BUILDTYPE "SCANNER ONLY"
	#include "scan.h"
#else
	#include "parse.h"
	#if NO_ANALYZE
		#define BUILDTYPE "SCANNER/PARSER ONLY"
	#else
		#include "analyze.h"
		#if NO_CODE
			#define BUILDTYPE "SCANNER/PARSER/ANALYZER ONLY"
		#else
			#define BUILDTYPE "COMPLETE COMPILER"
			#include "cgen.h"
		#endif
	#endif
#endif

static char input[LENGTH];

FILE* src;							// SOURCE CODE
FILE* lst;							// LISTING OUTPUT
FILE* out;							// CODE FOR VM

uint lineno = 0;
uint linecol = 0;

bool echo_source 	= true;
bool trace_scan		= true;
bool trace_parse 	= true;
bool trace_analyze 	= true;
bool trace_code		= true;

bool is_error 		= false;

bool parse_cmd(int, char *[]);
void usage(void);

int main(int argc, char *argv[])
{
	node_t *syntax_tree;
	
#ifdef DEBUG
	FILE *fin = freopen("_input.txt", "r", stdin);
	if (fin == NULL) {
        printf("Cannot open `_input.txt' in DEBUG mode\n");
        exit(-1);
    }
	printf(">> ");
	gets(input);
	argv[1] = input;
#else
	if (parse_cmd(argc, argv) == false) {
		usage();
		exit(-2);
	}
#endif
	
	strncpy(input, argv[1], LENGTH-1);
	input[LENGTH-1] = '\0';
	
	src = fopen(input, "r");
	
	if (src == NULL) {
		sprintf(median, "Cannot open `%s'", input);
		PANIC(1, median);
		exit(-2);
	}
	
	lst = stderr;	// LIST TO STANDARD OUTPUT
	
	fprintf(lst, COPYRIGHT);
	fprintf(lst, "Compiler built as " BUILDTYPE "\n");
	fprintf(lst, "[*] C- COMPILATION: `%s'\n", input);
	
#if NO_PARSE
	while (get_token() != ENDFILE)
		continue;
#else
	fprintf(lst, "[*] Parsing source code...\n");
	syntax_tree = parse();
	if (trace_parse) {
		fprintf(lst, "[*] Dumping syntax tree...\n");
		print_tree(syntax_tree);
	}
	#if !NO_ANALYZE
		if (!is_error) {
			fprintf(lst, "[*] Building symbol table...\n");
			build_symtab(syntax_tree);
			fprintf(lst, "[*] Performing type checking...\n");
			type_check(syntax_tree);
		}
		#if !NO_CODE
			if (!is_error) {
				strcat(input, ".tm");
				code_gen(syntax_tree, input);
				if (!is_error) {
					if (trace_code)
						fprintf(lst, "[*] TM code generation done, no error\n");
					fprintf(lst, "TM code written to `%s'\n", input);
				}
			}
		#endif
	#endif
#endif
	
	if (is_error)
		fprintf(lst, "[!] ERROR ENCOUNTERED before %d:%d\n", lineno, linecol);
	else
		fprintf(lst, "COMPILATION COMPLETED for %d line(s)\n", lineno);
	
	reclaim_memory(syntax_tree);	// CLEAN UP MEMORIES
	
	return 0;
}

bool parse_cmd(int argc, char *argv[])
{
	char ch;
	bool got_src = false;
	opterr = 0;		// SUPRESS getopt()'S DEFAULT
					//    ERROR-HANDLING BEHAVIOR
	while ((ch = getopt(argc, argv, "hespacf")) != EOF) {
		switch (ch) {
			case 'h':
				fprintf(stderr, USAGE);
				exit(0);
			break;
			case 'e':  echo_source   = true;  break;
			case 's':  trace_scan    = true;  break;
			case 'p':  trace_parse   = true;  break;
			case 'a':  trace_analyze = true;  break;
			case 'c':  trace_code    = true;  break;
			case 'f':
				// ONLY PARSE 1 FILE EACH SESSION
				if (got_src) return false;
				got_src = true;
				strncpy(input, optarg, LENGTH);
			break;
			default: return false;
		}
	}
	// SOURCE CODE CAN'T BE OMITTED
	if (got_src == false)
		return false;
	return true;
}

inline void usage(void)
{
	fprintf(stderr, COPYRIGHT);
	fprintf(stderr, USAGE);
}
