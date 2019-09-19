/**
 * @file code_generation.h
 */
#ifndef __CODE_GENERATION_H__
#define __CODE_GENERATION_H__

#include <writer.h>
#include <ast.h>

/* ----------------------------------------------------------------------------
 * resizable page of code
 *
 * size  = currently allocated size
 * pos   = write position
 *
 * so far it is used only internally, but exposed in the header anyway
 */
typedef struct {
  uint8_t *data;
  int pos, size;
} code_block_t;

CONSTRUCTOR(code_block_t);
DESTRUCTOR(code_block_t);

// low-level push data
void code_block_push(code_block_t *code, uint8_t *data, uint32_t len);
// copy src to dst
void add_code_block(code_block_t *dst, code_block_t *src);
// add one instruction with parameters to code block
void add_instr(code_block_t *out, int code, ...);

//! this is the main interface
int emit_code(ast_t *_ast, writer_t *out) ;

#endif
