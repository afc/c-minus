// ERRNO: [11, 20]
#include "globals.h"
#include "utils.h"
#include "scan.h"

typedef enum state_t {
	START, INNUM, INID, 
	INLE, INEQ, INGE, INNE, 
	INDIV, INC99, INKR, OUTKR, 
	INEOF, DONE
} state_t;

char lexeme[MAX_TOK_SIZE];

#define BUF_SIZE 256
static char linebuf[BUF_SIZE];
static uint bufsize = 0;

static char next_char(void)
{
	if (linecol < bufsize)
		return linebuf[linecol++];
	
	lineno++;
	if (fgets(linebuf, BUF_SIZE-1, src)) {
		bufsize = strlen(linebuf);
		if (echo_source) {
			fprintf(lst, "%4d: %s", lineno, linebuf);
			// MAYBE linebuf REACHED THE LAST LINE
			if (linebuf[bufsize-1] != '\n')
				fprintf(lst, "\n");
		}
		linecol = 1;
		return linebuf[0];
	}
	return EOF;
}

static void roll_back(void)
{
	if (linecol != 0)
		linecol--;
	else
		PANIC(11, "Lineno reached zero and trying to roll back");
}

static token_t kw_match(cchar *str)
{
	tint len = strlen(str);
	// KEYWORDS LENGTH RANGE FROM 2 TO 6
	if (len < 2 || len > 6)
		return ID;
	token_t m, l = ELSE, r = WHILE;
	while (l <= r) {
		m = l + (r-l)/2;
		int cmp = strcmp(str, token_map[m]);
		if (cmp == 0)
			return m;
		(cmp > 0) ? (l = m+1) : (r = m-1);
	}
	return ID;
}

// HARD-WRITTEN VERSION OF get_token()
token_t get_token(void)
{
	uint lexeme_i = 0;
	token_t current_token;
	state_t state = START;
	// FLAG TO INDICATE IF SAVE TO lexeme
	bool tosave;
	while (state != DONE) {
		char c = next_char();
		tosave = true;
		
		switch (state) {
			case START:
				if (isdigit(c))
					state = INNUM;
				else if (isalpha(c) || c == '_')
					state = INID;
				else if (c == '<')
					state = INLE;
				else if (c == '=')
					state = INEQ;
				else if (c == '>')
					state = INGE;
				else if (c == '!')
					state = INNE;
				else if (isspace(c))
					tosave = false;
				else if (c == '/')
					state = INDIV;
				else {
					state = DONE;
					switch (c) {
						case EOF:
							// OUT-LAYER CODE WILL DEAL ~
						break;
						
						case '+':
							current_token = PLUS;
						break;
						
						case '-':
							current_token = MINUS;
						break;
						
						case '*':
							current_token = TIMES;
						break;
						
						case ';':
							current_token = SEMI;
						break;
						
						case ',':
							current_token = COMMA;
						break;
						
						case '(':
							current_token = LPAREN;
						break;
						
						case ')':
							current_token = RPAREN;
						break;
						
						case '[':
							current_token = LSQUARE;
						break;
						
						case ']':
							current_token = RSQUARE;
						break;
						
						case '{':
							current_token = LBRACE;
						break;
						
						case '}':
							current_token = RBRACE;
						break;
						
						default:
							current_token = ERROR;
						break;
					}
				}
			break;
			
			case INNUM:
				if ( isdigit(c) )
					break;
				else if (c == EOF)
					goto jmp_eof;
				roll_back();
				state = DONE;
				tosave = false;
				current_token = NUM;
			break;
			
			case INID:
				if ( isalnum(c) || c == '_' )
					break;
				else if (c == EOF)	// OOPS! -- MET EOF
					goto jmp_eof;
				roll_back();
				state = DONE;
				tosave = false;
				current_token = ID;
			break;
			
			case INLE:
				state = DONE;
				// PREPARE TO OVERWRITE
				current_token = LT;
				if (c == '=')
					current_token = LE;
				else if (c == EOF)
					goto jmp_eof;
				else
					roll_back();
			break;
			
			case INEQ:
				state = DONE;
				current_token = ASSIGN;
				if (c == '=')
					current_token = EQ;
				else if (c == EOF)
					goto jmp_eof;
				else
					roll_back();
			break;
			
			case INGE:
				state = DONE;
				current_token = GT;
				if (c == '=')
					current_token = GE;
				else if (c == EOF)
					goto jmp_eof;
				else
					roll_back();
			break;
			
			case INNE:
				state = DONE;
				current_token = ERROR;
				if (c == '=')
					current_token = NE;
				else if (c == EOF)
					goto jmp_eof;
				else {
					roll_back();
					tosave = false;
				}
			break;
			
			// THE FOLLOWING 4 CASES
			// DEAL WITH // ... AND /* ... */
			case INDIV:
				state = DONE;
				tosave = false;
				if (c == '*') {
					state = INKR;
					lexeme_i = 0;	// CLEAN UP SAVED BUFFER
				}
				else if (c == '/') {
					state = INC99;
					lexeme_i = 0;
				}
				else if (c == EOF)
					goto jmp_eof;
				else {
					roll_back();
					current_token = DIVIDE;
				}
			break;
			
			case INC99:
				tosave = false;
				if (c == EOF)
					goto jmp_eof;
				else if (c != '\n')
					break;
				state = START;
			break;
			
			case INKR:
				tosave = false;
				if (c == '*')
					state = OUTKR;
				else if (c == EOF)
					goto jmp_eof;
			break;
			
			case OUTKR:
				tosave = false;
				if (c == '/')
					state = START;		// REWIND TO THE START STATE
				else if (c == EOF)
					goto jmp_eof;
				else if (c != '*')
					state = INKR;		// REWIND ONE LEVEL(STATE)
			break;
			
			jmp_eof:					// GOTO LABEL
			case INEOF:
				lineno--;				// LINE AUTO-INCREASED ONE
				PANIC(12, "Met EOF in advance");
				state = DONE;
			break;
			
			case DONE:
			default:
				sprintf(median, "Unknown token: `%s'", lexeme);
				PANIC(13, median);
				state = DONE;
				current_token = ERROR;
			break;
		}								// END OF SWITCH
		
		if (c == EOF) {					// CHECK IN NON-START STATE
			tosave = false;
			current_token = ENDFILE;
		}
		
		if ( tosave && lexeme_i < MAX_TOK_SIZE )
			lexeme[lexeme_i++] = c;
		
		if (state == DONE) {
			lexeme[lexeme_i] = '\0';
			if (current_token == ID)	// CHECK IF IT'S A REVERSED WORD
				current_token = kw_match(lexeme);
		}
	}									// END OF WHILE
	
	if (trace_scan) {					// 8 SPACES AS \t
		fprintf(lst, "        %d: ", lineno);
		print_token(current_token, lexeme);
		fprintf(lst, "\n");
	}
	
	return current_token;
}
