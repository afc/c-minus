#ifndef _CODE_H_
#define _CODE_H_

#define AX 0		// ACCUMULATOR A

#define BX 1		// ACCUMULATOR B

#define PC 2		// PROGRAM COUNTER

#define BP 3		// BASE(GLOBALS) POINTER

#define FP 4		// FRAME POINTER

#define TP 5		// TEMP POINTER

// EMITS A COMMENT INTO CODE FILE
void emit_comment(cchar *);

// emit_ro() EMITS A REGISTER-ONLY TVM INSTRUCTION
// op		OPCODE
//  r		TAR REGISTER
//  s		1ST SRC REGISTER
//  t		2ND SRC REGISTER
//  c		EMIT COMMENT IF trace_code SET
void emit_ro(cchar *op, int r, int s, int t, cchar *c);

// emit_rm() EMITS A REGISTER-TO-MEMORY TVM INSTRUCTION
// op		OPCODE
//  r		TARGET REGISTER
//  d		OFFSET OF BASE REGISTER
//  s		BASE REGISTER
void emit_rm(cchar *op, int r, int d, int s, cchar *);

// emit_skip SKIPS `steps' FOR LATER BACKPATCH
// RETURNS CURRENT INSTRUCTION LOCATION IF `steps' IS ZERO
uint emit_skip(uint steps);

// BACKPATCH TO `loc' THAT PREVIOUSLY SKIPPED
void emit_backup(uint loc);

// RESTORE CURRENT LOCATION TO PREVIOUSLY HIGHEST UNEMITTED LOCATION
void emit_restore(void);

// emit_rm_abs() CONVERTS AN ABSOLUTE REFERENCE TO A PC-RELATIVE
// REFERENCE WHEN EMITTING A REGISTER-TO-MEMEORY TVM INSTRUCTION
// op		OPCODE
//  r		TARGET REGISTER
//  a		ABSOLUTE LOCATION IN MEMORY
void emit_rm_abs(cchar *op, int r, int a, cchar *);

#endif
