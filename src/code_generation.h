#ifndef __CODE_GENERATION_H__
#define __CODE_GENERATION_H__

#include "writer.h"
#include "ast.h"

void emit_code(ast_t *ast, writer_t *out, writer_t *log) ;

#endif
