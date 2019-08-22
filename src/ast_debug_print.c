#include "ast_debug_print.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

static writer_t *writer;

#define LOG(...) out_text(writer, __VA_ARGS__)
#define OFS(n) LOG("%*s", n, " ")

#define AST_NODE_NAME_BASE 52
static const char *const node_names[] = {
    "AST_NODE_STATIC_TYPE", "AST_NODE_VARIABLE",   "AST_NODE_SCOPE",
    "AST_NODE_FUNCTION",    "AST_NODE_EXPRESSION", "AST_NODE_STATEMENT"};

#define STMT_BASE 69
static const char *const stmt_names[] = {
    "STMT_COND",  "STMT_WHILE", "STMT_DO",       "STMT_FOR",
    "STMT_PARDO", "STMT_BREAK", "STMT_CONTINUE", "STMT_RETURN"};

#define EXPR_BASE 18
static const char *const expr_names[] = {
    "EXPR_EMPTY",         "EXPR_LITERAL",  "EXPR_INITIALIZER",    "EXPR_CALL",
    "EXPR_ARRAY_ELEMENT", "EXPR_VAR_NAME", "EXPR_SIZEOF",
    "EXPR_POSTFIX",       "EXPR_PREFIX",   "EXPR_BINARY",         "EXPR_CAST",
    "EXPR_SPECIFIER"};

static const char *const ioflag_names[] = {"IO_FLAG_NONE", "IO_FLAG_IN",
                                           "IO_FLAG_OUT"};

static void print_scope(int ofs, scope_t *s);
static void print_node(int ofs, ast_node_t *node);

static void print_token(int op) {
  switch (op) {
    case TOK_EQ:
      LOG("==");
      break;
    case TOK_NEQ:
      LOG("!=");
      break;
    case TOK_LEQ:
      LOG("<=");
      break;
    case TOK_GEQ:
      LOG(">=");
      break;
    case TOK_AND:
      LOG("&&");
      break;
    case TOK_OR:
      LOG("||");
      break;
    case TOK_SHL:
      LOG("<<");
      break;
    case TOK_SHR:
      LOG(">>");
      break;
    case TOK_FIRST_BIT:
      LOG("|~");
      break;
    case TOK_LAST_BIT:
      LOG("~|");
      break;
    case TOK_INC:
      LOG("++");
      break;
    case TOK_DEC:
      LOG("--");
      break;
    case TOK_PLUS_ASSIGN:
      LOG("+=");
      break;
    case TOK_MINUS_ASSIGN:
      LOG("-=");
      break;
    case TOK_TIMES_ASSIGN:
      LOG("*=");
      break;
    case TOK_DIV_ASSIGN:
      LOG("/=");
      break;
    case TOK_MOD_ASSIGN:
      LOG("%%=");
      break;
    case TOK_POW_ASSIGN:
      LOG("^=");
      break;
    case TOK_DONT_CARE:
      LOG("_");
      break;
    default:
      LOG("%c", (char)op);
  }
}

static void print_expr_params(int ofs, expression_t *e) {
  switch (e->variant) {
    case EXPR_BINARY:
      print_node(ofs, e->val.o->first);
      print_node(ofs, e->val.o->second);
      break;
    case EXPR_PREFIX:
    case EXPR_POSTFIX:
      print_node(ofs, e->val.o->first);
  }
}

static void print_expr_props(expression_t *e) {
  if (e->variant == EXPR_VAR_NAME) LOG("var:'%s' ", e->val.v->var->name);

  if (e->variant == EXPR_LITERAL) {
    if (expr_int(e)) LOG("value:%d ", *(int *)(e->val.l));
  }

  if (e->variant == EXPR_ARRAY_ELEMENT) {
    LOG("array:'%s' ", e->val.v->var->name);
  }

  if (e->variant == EXPR_SIZEOF) LOG("variable:'%s' ", e->val.v->var->name);

  if (e->variant == EXPR_BINARY || e->variant == EXPR_PREFIX ||
      e->variant == EXPR_POSTFIX) {
    LOG("'");
    print_token(e->val.o->oper);
    LOG("' ");
  }
}

static void print_node(int ofs, ast_node_t *node) {
  if (!node) {
    OFS(ofs);
    LOG("[NULL]\n");
    return;
  }
  OFS(ofs);
  LOG("[ %s ", node_names[node->node_type - AST_NODE_NAME_BASE]);

  if (node->node_type == AST_NODE_STATEMENT)
    LOG("%s ", stmt_names[node->val.s->variant - STMT_BASE]);

  if (node->node_type == AST_NODE_EXPRESSION) {
    LOG("%s ", expr_names[node->val.e->variant - EXPR_BASE]);
    print_expr_props(node->val.e);
    char *name = inferred_type_name(node->val.e->type);
    LOG("type:'%s' ", name);
    if (name) free(name);
  }

  LOG("this:%lx ", (unsigned long)node);
  if (ast_node_name(node)) LOG("name:'%s' ", ast_node_name(node));

  if (node->node_type == AST_NODE_VARIABLE) {
    LOG("%s type:'%s' ", ioflag_names[node->val.v->io_flag],
        node->val.v->base_type->name);
    if (node->val.v->num_dim > 0) {
      LOG("num_dim=%d ", node->val.v->num_dim);
    }
  }

  if (node->node_type == AST_NODE_STATIC_TYPE && node->val.t->members) {
    LOG("members:{ ");
    list_for(tm, static_type_member_t, node->val.t->members) {
      LOG("'%s':'%s' ", tm->type->name, tm->name);
    }
    list_for_end;
    LOG("} ");
    LOG("size: %d ", node->val.t->size);
  }

  LOG("next:%lx ", (unsigned long)node->next);

  if (node->node_type == AST_NODE_SCOPE) {
    LOG("\n");
    print_scope(ofs + 5, node->val.sc);
    OFS(ofs);
    LOG("]\n");
  } else
    LOG("]\n");

  if (node->node_type == AST_NODE_STATEMENT)
    for (int i = 0; i < 2; i++)
      if (node->val.s->par[i]) {
        OFS(ofs + 5);
        LOG("par%d\n", i);
        for (ast_node_t *nn = node->val.s->par[i]; nn; nn = nn->next)
          print_node(ofs + 5, nn);
      }

  if (node->node_type == AST_NODE_EXPRESSION)
    print_expr_params(ofs + 5, node->val.e);

  if (node->node_type == AST_NODE_VARIABLE) {
    for (ast_node_t *t = node->val.v->ranges; t; t = t->next)
      print_node(ofs + 5, t);
    if (node->val.v->initializer) {
      OFS(ofs + 5);
      LOG("init\n");
      print_node(ofs + 5, node->val.v->initializer);
    }
  }

  if (node->node_type == AST_NODE_EXPRESSION &&
      (node->val.e->variant == EXPR_ARRAY_ELEMENT ||
       node->val.e->variant == EXPR_SIZEOF ||
       node->val.e->variant == EXPR_CALL )) {
    for (ast_node_t *t = node->val.e->val.v->params; t; t = t->next)
      print_node(ofs + 5, t);
  }

  if (node->node_type == AST_NODE_EXPRESSION &&
      node->val.e->variant == EXPR_INITIALIZER) {
    print_node(ofs + 5, node->val.e->val.i);
  }
}

static void print_scope(int ofs, scope_t *s) {
  if (!s) {
    OFS(ofs);
    LOG("empty scope\n");
    return;
  }
  OFS(ofs);
  LOG("scope this:%lx parent:%lx\n", (unsigned long)s,
      (unsigned long)s->parent);
  if (s->fn) {
    OFS(ofs);
    LOG("params:\n");
    list_for(it, ast_node_t, s->fn->params) { print_node(ofs, it); }
    list_for_end;
  }
  OFS(ofs);
  LOG("items:\n");
  list_for(it, ast_node_t, s->items) { print_node(ofs, it); }
  list_for_end;
  OFS(ofs);
  LOG("scope end (%lx)\n", (unsigned long)s);
}

void ast_debug_print(ast_t *ast, writer_t *wrt) {
  writer = wrt;
  LOG("ast->types:\n");
  list_for(tt, ast_node_t, ast->types) print_node(0, tt);
  list_for_end;

  // functions
  list_for(tt, ast_node_t, ast->functions) {
    LOG("\n%s %s(", tt->val.f->out_type->name, tt->val.f->name);
    list_for(p, ast_node_t, tt->val.f->params) {
      LOG("%s %s", p->val.v->base_type->name, p->val.v->name);
    }
    list_for_end;
    LOG("):\n");
    print_scope(0, tt->val.f->root_scope);
    LOG("\n");
  }
  list_for_end;

  LOG("\nast->root_scope:\n");
  print_scope(0, ast->root_scope);
}

#undef LOG
#undef OFS
#undef AST_NODE_NAME_BASE
#undef STMT_BASE
#undef EXPR_BASE
