/**
 * @file ast_debug_print.h
 * @brief Print the ast_t structure for debug purposes. 
 *
 * \deprecated Outdated, needs update to the current language
 * specification. 
 *
 * <code>wtc</code> still supports it with <code>-D</code> option
 * 
 * 
 */
#ifndef __AST_DEBUG_PRINT__H__
#define __AST_DEBUG_PRINT__H__

#include "ast.h"
#include "writer.h"

//! print the AST tree to the writer
void ast_debug_print(ast_t*ast, writer_t *wrt);

void ast_debug_set_writer(writer_t *wrt);
void ast_debug_print_token(int op);
void ast_debug_print_expr_params(int ofs, expression_t *e);
void ast_debug_print_expr_props(expression_t *e);
void ast_debug_print_node(int ofs, ast_node_t *node);
void ast_debug_print_scope(int ofs, scope_t *s);

#endif
