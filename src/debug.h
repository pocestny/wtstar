#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <ast.h>
#include <writer.h>

void emit_debug_sections(writer_t *out, ast_t *ast, int _code_size);

#endif
