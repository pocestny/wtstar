#include <stdio.h>
#include <stdlib.h>

#include <ast_debug_print.h>
#include <parser.h> 

static writer_t *writer;

#define MSG(...) out_text(writer, __VA_ARGS__)
#define OFS(n) MSG("%*s", n, " ")

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
    "EXPR_EMPTY",  "EXPR_LITERAL",       "EXPR_INITIALIZER",
    "EXPR_CALL",   "EXPR_ARRAY_ELEMENT", "EXPR_VAR_NAME",
    "EXPR_SIZEOF", "EXPR_POSTFIX",       "EXPR_PREFIX",
    "EXPR_BINARY", "EXPR_CAST",          "EXPR_SPECIFIER"};

static const char *const ioflag_names[] = {"IO_FLAG_NONE", "IO_FLAG_IN",
                                           "IO_FLAG_OUT"};

static void print_scope(int ofs, scope_t *s);
static void print_node(int ofs, ast_node_t *node);

static void print_token(int op) {
  switch (op) {
    case TOK_EQ:
      MSG("==");
      break;
    case TOK_NEQ:
      MSG("!=");
      break;
    case TOK_LEQ:
      MSG("<=");
      break;
    case TOK_GEQ:
      MSG(">=");
      break;
    case TOK_AND:
      MSG("&&");
      break;
    case TOK_OR:
      MSG("||");
      break;
    case TOK_LAST_BIT:
      MSG("~|");
      break;
    case TOK_INC:
      MSG("++");
      break;
    case TOK_DEC:
      MSG("--");
      break;
    case TOK_PLUS_ASSIGN:
      MSG("+=");
      break;
    case TOK_MINUS_ASSIGN:
      MSG("-=");
      break;
    case TOK_TIMES_ASSIGN:
      MSG("*=");
      break;
    case TOK_DIV_ASSIGN:
      MSG("/=");
      break;
    case TOK_MOD_ASSIGN:
      MSG("%%=");
      break;
    case TOK_DONT_CARE:
      MSG("_");
      break;
    default:
      MSG("%c", (char)op);
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
  if (e->variant == EXPR_VAR_NAME) MSG("var:'%s' ", e->val.v->var->name);

  if (e->variant == EXPR_LITERAL) {
    if (expr_int(e)) MSG("value:%d ", *(int *)(e->val.l));
  }

  if (e->variant == EXPR_ARRAY_ELEMENT) {
    MSG("array:'%s' ", e->val.v->var->name);
  }

  if (e->variant == EXPR_SIZEOF) MSG("variable:'%s' ", e->val.v->var->name);

  if (e->variant == EXPR_BINARY || e->variant == EXPR_PREFIX ||
      e->variant == EXPR_POSTFIX) {
    MSG("'");
    print_token(e->val.o->oper);
    MSG("' ");
  }
}

static void print_node(int ofs, ast_node_t *node) {
  if (!node) {
    OFS(ofs);
    MSG("[NULL]\n");
    return;
  }
  OFS(ofs);
  MSG("[ %s ", node_names[node->node_type - AST_NODE_NAME_BASE]);

  if (node->node_type == AST_NODE_STATEMENT)
    MSG("%s ", stmt_names[node->val.s->variant - STMT_BASE]);

  if (node->node_type == AST_NODE_EXPRESSION) {
    MSG("%s ", expr_names[node->val.e->variant - EXPR_BASE]);
    print_expr_props(node->val.e);
    char *name = inferred_type_name(node->val.e->type);
    MSG("type:'%s' ", name);
    if (name) free(name);
  }

  MSG("this:%lx ", (unsigned long)node);
  if (ast_node_name(node)) MSG("name:'%s' ", ast_node_name(node));

  if (node->node_type == AST_NODE_VARIABLE) {
    MSG("%s type:'%s' ", ioflag_names[node->val.v->io_flag],
        node->val.v->base_type->name);
    if (node->val.v->num_dim > 0) {
      MSG("num_dim=%d ", node->val.v->num_dim);
    }
  }

  if (node->node_type == AST_NODE_STATIC_TYPE && node->val.t->members) {
    MSG("members:{ ");
    list_for(tm, static_type_member_t, node->val.t->members) {
      MSG("'%s':'%s' ", tm->type->name, tm->name);
    }
    list_for_end;
    MSG("} ");
    MSG("size: %d ", node->val.t->size);
  }

  MSG("next:%lx ", (unsigned long)node->next);

  if (node->node_type == AST_NODE_SCOPE) {
    MSG("\n");
    print_scope(ofs + 5, node->val.sc);
    OFS(ofs);
    MSG("]\n");
  } else
    MSG("]\n");

  if (node->node_type == AST_NODE_STATEMENT)
    for (int i = 0; i < 2; i++)
      if (node->val.s->par[i]) {
        OFS(ofs + 5);
        MSG("par%d\n", i);
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
      MSG("init\n");
      print_node(ofs + 5, node->val.v->initializer);
    }
  }

  if (node->node_type == AST_NODE_EXPRESSION &&
      (node->val.e->variant == EXPR_ARRAY_ELEMENT ||
       node->val.e->variant == EXPR_SIZEOF ||
       node->val.e->variant == EXPR_CALL)) {
    for (ast_node_t *t = node->val.e->val.v->params; t; t = t->next)
      print_node(ofs + 5, t);
  }

  if (node->node_type == AST_NODE_EXPRESSION &&
      node->val.e->variant == EXPR_INITIALIZER)
    for (ast_node_t *nd = node->val.e->val.i; nd; nd = nd->next) {
      print_node(ofs + 5, nd);
    }

  if (node->node_type == AST_NODE_EXPRESSION &&
      node->val.e->variant == EXPR_CAST) {
    print_node(ofs + 5, node->val.e->val.c->ex);
  }
}

static void print_scope(int ofs, scope_t *s) {
  if (!s) {
    OFS(ofs);
    MSG("empty scope\n");
    return;
  }
  OFS(ofs);
  MSG("scope this:%lx parent:%lx\n", (unsigned long)s,
      (unsigned long)s->parent);
  if (s->fn) {
    OFS(ofs);
    MSG("params:\n");
    list_for(it, ast_node_t, s->fn->params) { print_node(ofs, it); }
    list_for_end;
  }
  OFS(ofs);
  MSG("items:\n");
  list_for(it, ast_node_t, s->items) { print_node(ofs, it); }
  list_for_end;
  OFS(ofs);
  MSG("scope end (%lx)\n", (unsigned long)s);
}

void ast_debug_print(ast_t *ast, writer_t *wrt) {
  writer = wrt;
  MSG("ast->types:\n");
  list_for(tt, ast_node_t, ast->types) print_node(0, tt);
  list_for_end;

  // functions
  list_for(tt, ast_node_t, ast->functions) {
    MSG("\n%s %s(", tt->val.f->out_type->name, tt->val.f->name);
    list_for(p, ast_node_t, tt->val.f->params) {
      MSG("%s %s", p->val.v->base_type->name, p->val.v->name);
    }
    list_for_end;
    MSG("):\n");
    print_scope(0, tt->val.f->root_scope);
    MSG("\n");
  }
  list_for_end;

  MSG("\nast->root_scope:\n");
  print_scope(0, ast->root_scope);
}

#undef MSG
#undef OFS
#undef AST_NODE_NAME_BASE
#undef STMT_BASE
#undef EXPR_BASE
