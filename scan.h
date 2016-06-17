#ifndef _SCAN_H_
#define _SCAN_H_

#define MAX_REVERSED 6

#define MAX_TOK_SIZE 64

extern char lexeme[MAX_TOK_SIZE];		// scan.c

token_t get_token(void);

#endif
