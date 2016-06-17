#include "globals.h"
#include "code.h"

static bool debug_flag = false;

static cchar *reg_map[] = {
	"AX", "BX", "PC", 
	"BP", "FP", "TP", ""
};

// LOCATION NUMBER OF CURRENT INSTRUCTION IN TVM
static uint emit_loc = 0;

// HIGHEST TVM LOCATION EMITTED SO FAR
// USED IN emit_skip() emit_backup() emit_restore()
static uint high_emit_loc = 0;

inline void emit_comment(cchar *str)
{ if (trace_code) fprintf(out, "* %s\n", str); }

void emit_ro(cchar *op, int r, int s, int t, cchar *c)
{
	if (debug_flag)
		fprintf(out, "%03d:  %-5s  %d,%s,%s ", emit_loc++, op, reg_map[r], reg_map[s], reg_map[t]);
	else
		fprintf(out, "%03d:  %-5s  %d,%d,%d ", emit_loc++, op, r, s, t);
	if (trace_code)
		fprintf(out, "\t\t%s", c);
	fprintf(out, "\n");
	if (high_emit_loc < emit_loc)
		high_emit_loc = emit_loc;
}

void emit_rm(cchar * op, int r, int d, int s, cchar *c)
{
	if (debug_flag)
		fprintf(out, "%03d:  %-5s  %s,%d(%s) ", emit_loc++, op, reg_map[r], d, reg_map[s]);
	else
		fprintf(out, "%03d:  %-5s  %d,%d(%d) ", emit_loc++, op, r, d, s);
	if (trace_code)
		fprintf(out, "\t\t%s", c);
	fprintf(out, "\n");
	if (high_emit_loc < emit_loc)
		high_emit_loc = emit_loc;
}

uint emit_skip(uint steps)
{
	uint old_loc = emit_loc;
	emit_loc += steps;
	if (high_emit_loc < emit_loc)
		high_emit_loc = emit_loc;
	return old_loc;
}

void emit_backup(uint loc)
{
	if (loc > high_emit_loc)
		emit_comment("glitch in emit_backup()");
	emit_loc = loc;
}

inline void emit_restore(void)
{ emit_loc = high_emit_loc; }

void emit_rm_abs(cchar *op, int r, int a, cchar *c)
{
	if (debug_flag)
		fprintf(out, "%03d:  %-5s  %s,%d(%s) ",
			emit_loc, op, reg_map[r], a-(emit_loc+1), reg_map[PC]);
	else
		fprintf(out, "%03d:  %-5s  %d,%d(%d) ", 
			emit_loc, op, r, a-(emit_loc+1), PC);
 	++emit_loc;
	if (trace_code) fprintf(out, "\t\t%s", c);
	fprintf(out, "\n");
	if (high_emit_loc < emit_loc)
		high_emit_loc = emit_loc;
}
