/**
 * @file code_generation.h
 * @brief given ast_t, generate binary code, and write it to a writer_t
 */
#ifndef __CODE_GENERATION_H__
#define __CODE_GENERATION_H__

#include <ast.h>
#include <writer.h>

/**
 * @brief resizable block of code
 *
 * so far it is used only internally, but exposed in the header anyway
 */
typedef struct {
  uint8_t *data;  //!< base memory
  int pos,        //!< position of the writing head
      size;       //!< currently allocated size
} code_block_t;

//! allocate
CONSTRUCTOR(code_block_t);
//! free memory
DESTRUCTOR(code_block_t);

//! low-level push data
void code_block_push(code_block_t *code, uint8_t *data, uint32_t len);
//! copy src to dst
void add_code_block(code_block_t *dst, code_block_t *src);
//! add one instruction (see code.h) with possible parameters to code block
void add_instr(code_block_t *out, int code, ...);

/**
 * this is the main interface (if `no_debug` is set, no debug info is written
 * to the binary)
 *
 * uses the errors.h mechanism for announcing errors
 */
int emit_code(ast_t *_ast, writer_t *out, int no_debug);

int emit_code_scope_section(ast_t *_ast, scope_t *scope, writer_t *out);

#endif
