#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <code_generation.h>
#include <debug.h>
#include <errors.h>
#include <parser.h>

int generating_breakpoint = 0;

#define NODEBUG

#ifdef NODEBUG
#define DEBUG(...) /* */
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

static void emit_code_scope(code_block_t *code, scope_t *sc);

/* ----------------------------------------------------------------------------
 * create error, and insert into errors.h log
 */
static void error(YYLTYPE *loc, const char *format, ...) {
  GLOBAL_ast->error_occured++;
  error_t *err = error_t_new();
  int n;
  get_printed_length(format, n);
  append_error_msg(err, "%s %d %d: ", loc->fn, loc->fl, loc->fc);
  va_list args;
  va_start(args, format);
  append_error_vmsg(err, n, format, args);
  va_end(args);
  printf("here\n");
  emit_error_handle(err, GLOBAL_ast->error_handler, GLOBAL_ast->error_handler_data);
}

/* ----------------------------------------------------------------------------
 * implementation of code_block_t
 */

CONSTRUCTOR(code_block_t) {
  ALLOC_VAR(r, code_block_t)
  r->data = (uint8_t *)malloc(16);
  r->pos = 0;
  r->size = 16;
  return r;
}

DESTRUCTOR(code_block_t) {
  if (r == NULL) return;
  free(r->data);
  free(r);
}

// low-level push data
void code_block_push(code_block_t *code, uint8_t *data, uint32_t len) {
  if (len == 0) return;
  while (len >= code->size - code->pos) {
    code->data = (uint8_t *)realloc(code->data, 2 * code->size);
    code->size *= 2;
  }
  memcpy(code->data + code->pos, data, len);
  code->pos += len;
}

// copy src to dst
void add_code_block(code_block_t *dst, code_block_t *src) {
  if (src->pos > 0) code_block_push(dst, src->data, src->pos);
}

void add_noop(code_block_t *out) {
  uint8_t buf[1];
  buf[0] = NOOP;
  code_block_push(out, buf, 1);
}

void add_step_in(code_block_t *out) {
  add_noop(out);
  add_instr(out, STEP_IN, 0);
}

// add one instruction with parameters to code block
void add_instr(code_block_t *out, int code, ...) {
  static uint8_t buf[4096];
  int len = 0;
  va_list args;
  va_start(args, code);

  while (code != NOOP) {
    buf[len++] = code;
    switch (code) { // remember how switch works
      case PUSHC:
      case JMP:
      case CALL:
      case JOIN_JMP:
        lval(buf + len, int32_t) = va_arg(args, int);
        len += 4;
        break;
      case PUSHB:
      case IDX:
        lval(buf + len, uint8_t) = va_arg(args, int);
        len += 1;
        break;
      default:
        break;
    }
    code = va_arg(args, int);
  }
  code_block_push(out, buf, len);
  va_end(args);
}

/* ----------------------------------------------------------------------------
 * assign addresses to variables
 *
 * for a scope, consider all nodes
 *  handle nodes with possible sub-scopes
 *  assign address to each variable
 *
 * base is the currently first free address
 *
 * closing a scope ends the visibility of the variables, so the base returned
 * from assign_scope_variable_addresses is not incremented
 */
static int assign_single_variable_address(uint32_t base, variable_t *var) {
  var->addr = base;
  if (var->num_dim > 0)
    base += 4 * (var->num_dim + 2);
  else
    base += var->base_type->size;
  DEBUG("'%s' %lx %u\n", var->name, (unsigned long)var, var->addr);
  return base;
}

static int assign_node_variable_addresses(uint32_t base, ast_node_t *node);

static int assign_scope_variable_addresses(uint32_t base, scope_t *sc) {
  int b = base;
  for (ast_node_t *p = sc->items; p; p = p->next)
    b = assign_node_variable_addresses(b, p);
  return base;
}

static int assign_node_variable_addresses(uint32_t base, ast_node_t *node) {
  switch (node->node_type) {
    case AST_NODE_VARIABLE:
      base = assign_single_variable_address(base, node->val.v);
      break;
    case AST_NODE_SCOPE: {
      assign_scope_variable_addresses(base, node->val.sc);
    } break;
    case AST_NODE_STATEMENT:
      if (node->val.s->variant == STMT_FOR ||
          node->val.s->variant == STMT_PARDO)
        base = assign_node_variable_addresses(base, node->val.s->par[0]);
      if (node->val.s->variant == STMT_COND ||
          node->val.s->variant == STMT_WHILE || node->val.s->variant == STMT_DO)
        base = assign_node_variable_addresses(base, node->val.s->par[1]);
      break;
  }
  return base;
}

/* ----------------------------------------------------------------------------
 * size (in bytes) of an inferred type (defined in ast.h )
 */
static int inferred_type_size(inferred_type_t *t) {
  if (t->compound) {
    int n = 0;
    list_for(tt, inferred_type_item_t, t->list) {
      n += inferred_type_size(tt->type);
    }
    list_for_end;
    return n;
  } else
    return t->type->size;
}

/* ----------------------------------------------------------------------------
 * this expression can be used to assign to
 */
static int is_lval_expression(expression_t *ex) {
  switch (ex->variant) {
    case EXPR_ARRAY_ELEMENT:
      return 1;
    case EXPR_VAR_NAME:
      if (ex->val.v->var->num_dim > 0) return 0;
      return 1;
    case EXPR_CAST:
      return is_lval_expression(ex->val.c->ex->val.e);
    case EXPR_SPECIFIER:
      return is_lval_expression(ex->val.s->ex->val.e);
    default:
      return 0;
  }
}

/* ----------------------------------------------------------------------------
 * is this an expression that creates address relative to heap?
 */
static int expr_on_heap(expression_t *ex) {
  switch (ex->variant) {
    case EXPR_ARRAY_ELEMENT:
      return 1;
      break;
    case EXPR_CAST:
      return expr_on_heap(ex->val.c->ex->val.e);
      break;
    case EXPR_SPECIFIER:
      return expr_on_heap(ex->val.s->ex->val.e);
      break;
    default:
      return 0;
  }
}

/* ----------------------------------------------------------------------------
 * base address of a variable (if it is in a function, add FBASE)
 */
static void emit_code_var_addr(code_block_t *code, variable_t *var) {
  add_instr(code, PUSHC, var->addr, 0);
  if (var->scope->fn) add_instr(code, FBASE, 0);
}

/* ----------------------------------------------------------------------------
 * the stack contains the address; emit code to load the variable
 *
 * if the variable is array, load from heap
 * transfer the value of the variable's type
 */
static void emit_code_load_value(code_block_t *code, int on_heap, int type_size,
                                 uint8_t *type_layout) {
  int ld4 = (on_heap) ? LDCH : LDC;
  int ld1 = (on_heap) ? LDBH : LDB;

  if (type_size > 1) add_instr(code, S2A, 0);

  int offs = 0;
  for (int i = 0; i < type_size - 1; i++)
    offs += (type_layout[i] == TYPE_CHAR) ? 1 : 4;

  for (int i = type_size - 1; i >= 0; i--) {
    if (offs > 0) add_instr(code, PUSHC, offs, ADD_INT, 0);
    add_instr(code, (type_layout[i] == TYPE_CHAR) ? ld1 : ld4, 0);
    if (i > 0) {
      add_instr(code, A2S, 0);
      offs -= (type_layout[i - 1] == TYPE_CHAR) ? 1 : 4;
    }
  }

  if (type_size > 1) add_instr(code, POPA, 0);
}

/* ----------------------------------------------------------------------------
 * the stack contains the address; emit code to store the variable
 *
 * if the variable is array, store to heap
 * perform conversion casts on the way
 */
static int conversion_needed(int cast) {
  int from = (cast & CONVERT_FROM_FLOAT) ? TYPE_FLOAT : TYPE_INT;
  int to = (cast & CONVERT_TO_FLOAT) ? TYPE_FLOAT : TYPE_INT;
  return from != to;
}

static void emit_code_store_value(code_block_t *code, int on_heap, int *casts,
                                  int n_casts) {
  int st4 = (on_heap) ? STCH : STC;
  int st1 = (on_heap) ? STBH : STB;

  // special case of single value
  if (n_casts == 1) {
    if (conversion_needed(casts[0])) {
      int conv = (casts[0] & CONVERT_TO_FLOAT) ? INT2FLOAT : FLOAT2INT;
      add_instr(code, S2A, POP, conv, A2S, POPA, 0);
    }
    add_instr(code, (casts[0] & CONVERT_TO_CHAR) ? st1 : st4, 0);
    return;
  }

  add_instr(code, S2A, 0);
  for (int i = 0, offs = 0; i < n_casts; i++) {
    if (offs > 0) add_instr(code, PUSHC, offs, ADD_INT, 0);
    if (conversion_needed(casts[i])) {
      int conv = (casts[i] & CONVERT_TO_FLOAT) ? INT2FLOAT : FLOAT2INT;
      add_instr(code, S2A, POP, conv, A2S, POPA, 0);
    }
    add_instr(code, (casts[i] & CONVERT_TO_CHAR) ? st1 : st4, 0);
    if (i < n_casts - 1) add_instr(code, A2S, 0);
    if (casts[i] & CONVERT_TO_CHAR)
      offs++;
    else
      offs += 4;
  }
  add_instr(code, POPA, 0);
}

/* ----------------------------------------------------------------------------
 * the stack contains the value; emit code to convert the variable
 *
 */
static void emit_code_cast_value(code_block_t *code, int *casts, int n_casts) {
  int needed = 0;
  for (int i = 0; i < n_casts; i++)
    if (conversion_needed(casts[i])) needed = 1;
  if (!needed) return;
  for (int i = 0; i < n_casts; i++) {
    if (conversion_needed(casts[i])) {
      int conv = (casts[i] & CONVERT_TO_FLOAT) ? INT2FLOAT : FLOAT2INT;
      add_instr(code, conv, 0);
    }
    if (i < n_casts - 1) add_instr(code, S2A, POP, 0);
  }
  for (int i = 0; i < n_casts - 1; i++) add_instr(code, A2S, POPA, 0);
}

/* ----------------------------------------------------------------------------
 * the stack contains value of type t, remove it
 *
 */
static void emit_code_remove_type(code_block_t *code, static_type_t *t) {
  int n = static_type_layout(t, NULL);
  for (int i = 0; i < n; i++) add_instr(code, POP, 0);
}

/* ----------------------------------------------------------------------------
 * the stack contains value of type tm->parent, make it so
 * that only tm->type part remains
 *
 */
static void emit_code_select_specifier(code_block_t *code,
                                       static_type_member_t *tm) {
  int n = static_type_layout(tm->type, NULL);

  for (static_type_member_t *x = tm->parent->members; x; x = x->next)
    if (x != tm)
      emit_code_remove_type(code, x->type);
    else
      for (int i = 0; i < n; i++) add_instr(code, S2A, POP, 0);

  for (int i = 0; i < n; i++) add_instr(code, A2S, POPA, 0);
}

/* ----------------------------------------------------------------------------
 * generate code for an expression
 *
 * the code, when executed, leaves on stack a value:
 *  - if addr=1, and the expression is a lvalue, leave on stack the
 * address relative to frame (variable) or heap (array)
 *  - otherwise, leave on stack the value
 *
 *  - if clear=0, remove anything from stack
 */

static void emit_code_expression(code_block_t *code, ast_node_t *exn, int addr,
                                 int clear) {
  exn->emitted = 1;
  expression_t *ex = exn->val.e;
  switch (ex->variant) {
    // --------------------------------------
    case EXPR_LITERAL:
      if (addr) {
        error(&(exn->loc), "cannot take address of expression");
        return;
      }
      if (!clear) {
        for (int n = inferred_type_size(ex->type); n > 0;) {
          if (n >= 4) {
            n -= 4;
            add_instr(code, PUSHC, lval(ex->val.l + n, int32_t), 0);
          } else {
            n--;
            add_instr(code, PUSHB, lval(ex->val.l + n, int8_t), 0);
          }
        }
      }
      break;

    // --------------------------------------
    case EXPR_CALL: {
      if (addr) {
        error(&(exn->loc), "cannot take address of expression");
        return;
      }
      int np = length(ex->val.f->params);
      if (length(ex->val.f->fn->params) != np) {
        error(&(exn->loc), "wrong number of parameters in call to '%s'",
              ex->val.f->fn->name);
        break;
      }
      for (int i = np - 1; i >= 0; --i) {
        ast_node_t *p = ex->val.f->params;
        ast_node_t *dst = ex->val.f->fn->params;

        for (int j = 0; j < i; j++) {
          p = p->next;
          dst = dst->next;
        }

        assert(p);
        assert(p->node_type == AST_NODE_EXPRESSION);

        int *casts, n_casts;
        if (inferred_type_compatible(dst->val.v->base_type, p->val.e->type,
                                     &casts, &n_casts)) {
          emit_code_expression(code, p, 0, 0);
          emit_code_cast_value(code, casts, n_casts);
          free(casts);
        } else {
          error(&(p->loc), "incompatible type in function call");
          return;
        }
      }

      if (!strcmp(ex->val.f->fn->name, "sqrt")) {
        add_instr(code, SQRT, 0);
      } else if (!strcmp(ex->val.f->fn->name, "sqrtf")) {
        add_instr(code, SQRTF, 0);
      } else if (!strcmp(ex->val.f->fn->name, "log")) {
        add_instr(code, LOG, 0);
      } else if (!strcmp(ex->val.f->fn->name, "logf")) {
        add_instr(code, LOGF, 0);
      } else {
        if (!ex->val.f->fn->root_scope) {
          error(&(exn->loc), "function was only declared without definition");
          return;
        }
        add_instr(code, CALL, ex->val.f->fn->n, 0);
      }
      if (clear) emit_code_remove_type(code, ex->val.f->fn->out_type);

    } break;

    // --------------------------------------
    case EXPR_VAR_NAME:
      if (clear) break;
      emit_code_var_addr(code, ex->val.v->var);
      if (!addr) {
        // we want the value
        int nd = ex->val.v->var->num_dim;
        if (nd == 0) {
          uint8_t *layout,
              ts = static_type_layout(ex->val.v->var->base_type, &layout);
          emit_code_load_value(code, 0, ts, layout);
          free(layout);
        } else {
          // the value of an array is num_dim + 2 ints
          // stack: base, nd, dim1,...,dim_nd,.... bottom
          add_instr(code, S2A, 0);
          for (int i = 0; i <= nd + 1; i++) {
            if (i < nd + 1)
              add_instr(code, PUSHC, 4 * (nd + 1 - i), ADD_INT, LDC, A2S, 0);
            else
              add_instr(code, LDC, 0);
          }
          add_instr(code, POPA, 0);
        }
      }
      break;

    // --------------------------------------
    case EXPR_CAST: {
      ast_node_t *oexn = ex->val.c->ex;
      static_type_t *nt = ex->val.c->type;

      if (oexn->val.e->variant == EXPR_VAR_NAME &&
          oexn->val.e->val.v->var->num_dim > 0) {
        error(&(exn->loc), "cannot cast array");
        break;
      }
      int *casts, n_casts;
      if (!inferred_type_compatible(nt, oexn->val.e->type, &casts, &n_casts)) {
        error(&(exn->loc), "incompatible types for cast");
        break;
      }
      emit_code_expression(code, oexn, addr, clear);
      if (!addr && !clear) emit_code_cast_value(code, casts, n_casts);
      if (casts) free(casts);
    } break;

    // --------------------------------------
    case EXPR_INITIALIZER: {
      if (addr) {
        error(&(exn->loc), "cannot take address of expression");
        return;
      }
      int n = length(ex->val.i);
      ast_node_t **tmp = (ast_node_t **)malloc(n * sizeof(ast_node_t *));
      ast_node_t *nd = ex->val.i;
      for (int i = 0; i < n; i++, nd = nd->next) tmp[n - i - 1] = nd;
      for (int i = 0; i < n; i++)
        emit_code_expression(code, tmp[i], addr, clear);
      free(tmp);
    } break;

    // --------------------------------------
    case EXPR_ARRAY_ELEMENT: {
      int n = ex->val.v->var->num_dim;
      if (n == 0) {
        error(&(exn->loc), "%s is not an array", ex->val.v->var->name);
        return;
      }

      for (ast_node_t *x = ex->val.v->params; x; x = x->next) {
        emit_code_expression(code, x, 0, clear);
        if (!clear && n > 1) add_instr(code, S2A, POP, 0);
      }
      if (!clear && n > 1)
        for (int i = 0; i < n; i++) add_instr(code, A2S, POPA, 0);
      if (!clear) {
        emit_code_var_addr(code, ex->val.v->var);
        add_instr(code, S2A, IDX, n, PUSHC, ex->val.v->var->base_type->size,
                  MULT_INT, A2S, POPA, LDC, ADD_INT, 0);
        if (!addr) {
          uint8_t *layout,
              ts = static_type_layout(ex->val.v->var->base_type, &layout);
          emit_code_load_value(code, 1, ts, layout);
          free(layout);
        }
      }
    } break;

    // --------------------------------------
    case EXPR_SIZEOF: {
      if (addr) {
        error(&(exn->loc), "cannot take address of expression");
        return;
      }
      variable_t *v = ex->val.v->var;
      emit_code_expression(code, ex->val.v->params, 0, 0);
      emit_code_var_addr(code, v);
      add_instr(code, SIZE, 0);
      if (clear) add_instr(code, POP, 0);
    } break;

    // --------------------------------------
    case EXPR_BINARY:
      // no operation is allowed on arrays
      if (ex->val.o->first->val.e->variant == EXPR_VAR_NAME &&
          ex->val.o->first->val.e->val.v->var->num_dim > 0) {
        error(&(ex->val.o->first->loc), "operation not permitted for arrays");
        return;
      }
      if (ex->val.o->second->val.e->variant == EXPR_VAR_NAME &&
          ex->val.o->second->val.e->val.v->var->num_dim > 0) {
        error(&(ex->val.o->second->loc), "operation not permitted for arrays");
        return;
      }
      // ..........
      // assignment
      if (assign_oper(ex->val.o->oper)) {
        if (!is_lval_expression(ex->val.o->first->val.e)) {
          error(&(exn->loc), "expression is not assignable");
          return;
        }
        int load, store;
        if (expr_on_heap(ex->val.o->first->val.e)) {
          load = LDCH;
          store = STCH;
        } else {
          load = LDC;
          store = STC;
        }
        if (ex->val.o->oper != '=') {
          // combined assignment

          // must be numeric
          if (ex->type->compound || ex->type->type->members) {
            error(&(exn->loc), "operation not supported on compound types");
            return;
          }
          int t = static_type_basic(ex->type->type);
          int t2 = static_type_basic(ex->val.o->second->val.e->type->type);

          if ((t == TYPE_FLOAT || t2 == TYPE_FLOAT) &&
              ex->val.o->oper == TOK_MOD_ASSIGN) {
            error(&(exn->loc), "remainder not supported on floats");
            return;
          }
          emit_code_expression(code, ex->val.o->second, 0, 0);
          if (t2 == TYPE_FLOAT && t == TYPE_INT)
            add_instr(code, FLOAT2INT, 0);
          else if (t2 == TYPE_INT && t == TYPE_FLOAT)
            add_instr(code, INT2FLOAT, 0);

          emit_code_expression(code, ex->val.o->first, 1, 0);

          int op;
          switch (ex->val.o->oper) {
            case TOK_PLUS_ASSIGN:
              op = (t == TYPE_INT) ? ADD_INT : ADD_FLOAT;
              break;
            case TOK_MINUS_ASSIGN:
              op = (t == TYPE_INT) ? SUB_INT : SUB_FLOAT;
              break;
            case TOK_TIMES_ASSIGN:
              op = (t == TYPE_INT) ? MULT_INT : MULT_FLOAT;
              break;
            case TOK_DIV_ASSIGN:
              op = (t == TYPE_INT) ? DIV_INT : DIV_FLOAT;
              break;
            case TOK_MOD_ASSIGN:
              op = MOD_INT;
              break;
          }
          add_instr(code, S2A, load, op, 0);
          if (addr) {
            add_instr(code, A2S, store, 0);
            if (!clear) add_instr(code, A2S, 0);
            add_instr(code, POPA, 0);
          } else {
            if (!clear) add_instr(code, S2A, SWA, 0);
            add_instr(code, A2S, POPA, store, 0);
            if (!clear) add_instr(code, A2S, POPA, 0);
          }
        } else {
          // assignment
          int *casts = NULL, n_casts;
          ast_node_t *l = ex->val.o->first, *r = ex->val.o->second;
          if (l->val.e->type->compound) {
            error(&(l->loc),
                  "This should not have happened, we are all doomed now.");
            assert(0);
          }

          // the types should be identical (checked in parser.y)
          // so this is basically only to create a uniform array of casts
          // (or if we wanted more implicit conversions in the future....)
          if (inferred_type_compatible(l->val.e->type->type, r->val.e->type,
                                       &casts, &n_casts)) {
            emit_code_expression(code, r, 0, 0);
            emit_code_expression(code, l, 1, 0);
            if (!clear) add_instr(code, S2A, 0);
            emit_code_store_value(code, expr_on_heap(l->val.e), casts, n_casts);
            if (!clear) {
              add_instr(code, A2S, POPA, 0);
              if (!addr) {
                uint8_t *layout;
                int ts = inferred_type_layout(l->val.e->type, &layout);
                emit_code_load_value(code, expr_on_heap(l->val.e), ts, layout);
                free(layout);
              }
            }
          } else {
            error(&(exn->loc),
                  "assignment of incompatible types (maybe add explicit cast)");
          }
          if (casts) free(casts);
        }

      } else if (numeric_oper(ex->val.o->oper)) {
        // ..........
        // binary op other that assignment
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int t = static_type_basic(ex->type->type);
        int t1 = static_type_basic(ex->val.o->first->val.e->type->type);
        int t2 = static_type_basic(ex->val.o->second->val.e->type->type);

        if (t == TYPE_FLOAT && ex->val.o->oper == '%') {
          error(&(exn->loc), "remainder not supported on floats");
          return;
        }

        if (t == TYPE_FLOAT &&
            (ex->val.o->oper == '|' || ex->val.o->oper == '&' ||
             ex->val.o->oper == '~')) {
          error(&(exn->loc), "bitwise operator not supported on floats");
          return;
        }

        int op;
        switch (ex->val.o->oper) {
          case '+':
            op = (t == TYPE_INT) ? ADD_INT : ADD_FLOAT;
            break;
          case '-':
            op = (t == TYPE_INT) ? SUB_INT : SUB_FLOAT;
            break;
          case '*':
            op = (t == TYPE_INT) ? MULT_INT : MULT_FLOAT;
            break;
          case '^':
            op = (t == TYPE_INT) ? POW_INT : POW_FLOAT;
            break;
          case '/':
            op = (t == TYPE_INT) ? DIV_INT : DIV_FLOAT;
            break;
          case '%':
            op = MOD_INT;
            break;
          case '|':
            op = BIT_OR;
            break;
          case '&':
            op = BIT_AND;
            break;
          case '~':
            op = BIT_XOR;
            break;
        }
        emit_code_expression(code, ex->val.o->second, 0, 0);
        if (t2 == TYPE_INT && t == TYPE_FLOAT) add_instr(code, INT2FLOAT, 0);

        emit_code_expression(code, ex->val.o->first, 0, 0);
        if (t1 == TYPE_INT && t == TYPE_FLOAT) add_instr(code, INT2FLOAT, 0);

        add_instr(code, op, 0);
        if (clear) add_instr(code, POP, 0);

      } else if (comparison_oper(ex->val.o->oper)) {
        // ..........
        // comparison
        if (addr) {
          error(&(exn->loc), "cannot take address of expression");
          return;
        }
        if (!ex->type->compound && ex->type->type->members &&
            (ex->val.o->oper == TOK_EQ || ex->val.o->oper == TOK_NEQ)) {
          // comparison of compound type - both operands have the same type
          uint8_t *layout, ts;
          ts = inferred_type_layout(ex->type, &layout);
          emit_code_expression(code, ex->val.o->second, 0, 0);
          for (int i = 0; i < ts; i++) add_instr(code, S2A, POP, 0);
          add_instr(code, RVA, 0);
          emit_code_expression(code, ex->val.o->first, 0, 0);
          for (int i = 0; i < ts; i++) {
            add_instr(code, A2S, POPA, 0);
            if (layout[i] == TYPE_FLOAT)
              add_instr(code, EQ_FLOAT, 0);
            else
              add_instr(code, EQ_INT, 0);
            if (i > 0) add_instr(code, AND, 0);
            if (i < ts - 1) add_instr(code, SWS, 0);
          }
          if (clear) add_instr(code, POP, 0);
          break;
        }
        // comparison of numbers
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int t = static_type_basic(ex->type->type);
        assert(t == TYPE_INT);
        int t1 = static_type_basic(ex->val.o->first->val.e->type->type);
        int t2 = static_type_basic(ex->val.o->second->val.e->type->type);
        int conv = ((t1 == TYPE_FLOAT) || (t2 == TYPE_FLOAT));
        int op;
        switch (ex->val.o->oper) {
          case TOK_EQ:
            op = (conv ? EQ_FLOAT : EQ_INT);
            break;
          case TOK_NEQ:
            op = (conv ? EQ_FLOAT : EQ_INT);
            break;
          case TOK_LEQ:
            op = (conv ? LEQ_FLOAT : LEQ_INT);
            break;
          case TOK_GEQ:
            op = (conv ? GEQ_FLOAT : GEQ_INT);
            break;
          case '<':
            op = (conv ? LT_FLOAT : LT_INT);
            break;
          case '>':
            op = (conv ? GT_FLOAT : GT_INT);
            break;
        }
        emit_code_expression(code, ex->val.o->second, 0, 0);
        if (conv && t2 == TYPE_INT) add_instr(code, INT2FLOAT, 0);
        emit_code_expression(code, ex->val.o->first, 0, 0);
        if (conv && t1 == TYPE_INT) add_instr(code, INT2FLOAT, 0);
        add_instr(code, op, 0);
        if (ex->val.o->oper == TOK_NEQ) add_instr(code, NOT, 0);
        if (clear) add_instr(code, POP, 0);

      } else if (ex->val.o->oper == TOK_AND || ex->val.o->oper == TOK_OR) {
        // ..........
        // logical and or
        if (addr) {
          error(&(exn->loc), "cannot take address of expression");
          return;
        }
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int t = static_type_basic(ex->type->type);
        assert(t == TYPE_INT);
        int t1 = static_type_basic(ex->val.o->first->val.e->type->type);
        int t2 = static_type_basic(ex->val.o->second->val.e->type->type);
        if (t1 == TYPE_FLOAT && t2 == TYPE_FLOAT) {
          error(&(exn->loc), "logical operation needs integral type");
          return;
        }
        emit_code_expression(code, ex->val.o->second, 0, 0);
        emit_code_expression(code, ex->val.o->first, 0, 0);
        if (ex->val.o->oper == TOK_AND)
          add_instr(code, AND, 0);
        else
          add_instr(code, OR, 0);
        if (clear) add_instr(code, POP, 0);
      }
      break;

    // --------------------------------------
    case EXPR_PREFIX:
      // no operation is allowed on arrays
      if (ex->val.o->first->val.e->variant == EXPR_VAR_NAME &&
          ex->val.o->first->val.e->val.v->var->num_dim > 0) {
        error(&(ex->val.o->first->loc), "operation not permitted for arrays");
        return;
      }
      if (ex->type->compound || ex->type->type->members) {
        error(&(exn->loc), "operation not supported on compound types");
        return;
      }
      if (ex->val.o->oper == TOK_DEC || ex->val.o->oper == TOK_INC) {
        // ..........
        // inc dec
        int onheap = expr_on_heap(ex->val.o->first->val.e);
        int load = onheap ? LDCH : LDC;
        int store = onheap ? STCH : STC;
        int type = static_type_basic(ex->type->type);
        int op;
        if (ex->val.o->oper == TOK_DEC)
          op = (type == TYPE_FLOAT) ? SUB_FLOAT : SUB_INT;
        else
          op = (type == TYPE_FLOAT) ? ADD_FLOAT : ADD_INT;
        add_instr(code, PUSHB, 1, 0);
        emit_code_expression(code, ex->val.o->first, 1, 0);
        add_instr(code, S2A, load, op, 0);
        if (!clear && !addr) add_instr(code, S2A, SWA, 0);
        add_instr(code, A2S, store, 0);
        if (clear || !addr) add_instr(code, POPA, 0);
        if (!clear) add_instr(code, A2S, POPA, 0);
      } else if (ex->val.o->oper == '-') {
        // ..........
        // unary -
        if (addr) {
          emit_code_expression(code, ex->val.o->first, 1, 0);
          add_instr(code, S2A, 0);
          int onheap = expr_on_heap(ex->val.o->first->val.e);
          int load = onheap ? LDCH : LDC;
          int store = onheap ? STCH : STC;
          add_instr(code, load, 0);
          //TODO! what about store??
        } else
          emit_code_expression(code, ex->val.o->first, 0, 0);
        add_instr(code, PUSHB, 0, 0);
        if (static_type_basic(ex->val.o->first->val.e->type->type) ==
            TYPE_FLOAT)
          add_instr(code, SUB_FLOAT, 0);
        else
          add_instr(code, SUB_INT, 0);
        if (clear || addr) add_instr(code, POP, 0);
        if (addr) add_instr(code, A2S, POPA, 0);
      } else if (ex->val.o->oper == '!') {
        // ..........
        // not
        if (addr) {
          error(&(exn->loc), "cannot take address of expression");
          return;
        }
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int t = static_type_basic(ex->type->type);
        if (t != TYPE_INT) {
          error(&(exn->loc), "operation needs integral type");
          return;
        }
        emit_code_expression(code, ex->val.o->first, 0, clear);
        if (!clear) add_instr(code, NOT, 0);
      };
      break;
    // --------------------------------------
    case EXPR_POSTFIX:
      // no operation is allowed on arrays
      if (ex->val.o->first->val.e->variant == EXPR_VAR_NAME &&
          ex->val.o->first->val.e->val.v->var->num_dim > 0) {
        error(&(ex->val.o->first->loc), "operation not permitted for arrays");
        return;
      }
      if (ex->val.o->oper == TOK_DEC || ex->val.o->oper == TOK_INC) {
        // ..........
        // inc dec
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int type = static_type_basic(ex->type->type);

        int onheap = expr_on_heap(ex->val.o->first->val.e);
        int load = onheap ? LDCH : LDC;
        int store = onheap ? STCH : STC;

        int op;
        if (ex->val.o->oper == TOK_DEC)
          op = (type == TYPE_FLOAT) ? SUB_FLOAT : SUB_INT;
        else
          op = (type == TYPE_FLOAT) ? ADD_FLOAT : ADD_INT;

        add_instr(code, PUSHB, 1, 0);
        emit_code_expression(code, ex->val.o->first, 1, 0);
        add_instr(code, S2A, load, 0);
        if (!clear) add_instr(code, S2A, SWA, 0);
        add_instr(code, op, A2S, POPA, store, 0);
        if (!clear) add_instr(code, A2S, POPA, 0);
      } else if (ex->val.o->oper == TOK_LAST_BIT) {
        // ..........
        // last bit
        if (addr) {
          error(&(exn->loc), "cannot take address of expression");
          return;
        }
        if (ex->type->compound || ex->type->type->members) {
          error(&(exn->loc), "operation not supported on compound types");
          return;
        }
        int t = static_type_basic(ex->type->type);
        if (t != TYPE_INT) {
          error(&(exn->loc), "operation needs integral type");
          return;
        }
        emit_code_expression(code, ex->val.o->first, 0, clear);
        if (!clear) add_instr(code, LAST_BIT, 0);
      }
      break;
    // --------------------------------------
    case EXPR_SPECIFIER: {
      if (is_lval_expression(ex->val.s->ex->val.e)) {
        emit_code_expression(code, ex->val.s->ex, 1, 0);
        add_instr(code, PUSHC, ex->val.s->memb->offset, ADD_INT, 0);
        if (!addr) {
          uint8_t *layout, ts = inferred_type_layout(ex->type, &layout);
          emit_code_load_value(code, expr_on_heap(ex), ts, layout);
          free(layout);
        }
      } else if (ex->val.s->ex->val.e->type->compound) {
        error(&(exn->loc), "cannot apply specifier to compound type");
        return;
      } else {
        emit_code_expression(code, ex->val.s->ex, 0, 0);
        emit_code_select_specifier(code, ex->val.s->memb);
      }
    } break;
    // --------------------------------------
    case EXPR_SORT: {
      if (addr) {
        error(&(exn->loc), "cannot take address of expression");
        return;
      }
      expression_t *spec = ex->val.v->params->val.e;
      if (spec->type->compound || spec->type->type->members) {
        error(&(exn->loc), "can sort only based on numeric key");
        return;
      }
      int t = static_type_basic(spec->type->type);
      add_instr(code, PUSHB, t, 0);

      int offs = 0;
      for (expression_t *p = spec; p->variant == EXPR_SPECIFIER;
           p = p->val.s->ex->val.e)
        offs += p->val.s->memb->offset;

      add_instr(code, PUSHC, offs, PUSHC, ex->val.v->var->base_type->size, 0);

      emit_code_var_addr(code, ex->val.v->var);
      add_instr(code, SORT, 0);
    } break;
  }
}

/* ----------------------------------------------------------------------------
 * generate code for an AST node
 */
static void emit_code_node(code_block_t *code, ast_node_t *node) {
  if (!node || node->emitted) return;
  node->emitted = 1;
  node->code_from = code->pos;
  switch (node->node_type) {
    // ................................
    case AST_NODE_VARIABLE:
      add_step_in(code);
      // init array
      if (node->val.v->num_dim > 0) {
        variable_t *v = node->val.v;
        // store number of dimensions
        add_instr(code, PUSHC, v->num_dim, 0);
        emit_code_var_addr(code, v);
        add_instr(code, PUSHB, 4, ADD_INT, STC, 0);

        ast_node_t *rng = v->ranges;
        if (!rng) break;  // input array - will be handled during loading

        // store number of elements in each dimension
        for (int i = 0; i < v->num_dim; i++) {
          emit_code_expression(code, rng, 0, 0);
          add_instr(code, S2A, 0);
          emit_code_var_addr(code, v);
          add_instr(code, PUSHC, 4 * (i + 2), ADD_INT, STC, 0);
          rng = rng->next;
        }

        // alloc base
        add_instr(code, PUSHB, 1, 0);
        for (int i = 0; i < v->num_dim; i++)
          add_instr(code, A2S, POPA, MULT_INT, 0);
        add_instr(code, PUSHC, v->base_type->size, MULT_INT, ALLOC, 0);
        emit_code_var_addr(code, v);
        add_instr(code, STC, 0);
      }
      // handle initializer
      if (node->val.v->initializer) {
        variable_t *var = node->val.v;
        ast_node_t *init = node->val.v->initializer;  // initializer ast_node

        if (node->val.v->num_dim == 0) {
          // scalar variables are implicitly converted if the layout is
          // equivalent
          int *casts, n_casts;
          if (inferred_type_compatible(var->base_type, init->val.e->type,
                                       &casts, &n_casts)) {
            emit_code_expression(code, init, 0, 0);
            emit_code_var_addr(code, var);
            emit_code_store_value(code, 0, casts, n_casts);
            free(casts);
          } else
            error(&(node->loc),
                  "incompatible types in initializer (maybe add explicit "
                  "cast)");

        } else {
          error(&(node->loc), "initializers are not supported for arrays");
        }
      } else if (node->val.v->num_dim == 0 && node->val.v->need_init) {
        // variables are zero-initialized
        // (internal: important for thread memory allcoation)
        uint8_t *layout;
        int n = static_type_layout(node->val.v->base_type, &layout);
        for (int i = 0, offs = 0; i < n; i++) {
          add_instr(code, PUSHB, 0, 0);
          emit_code_var_addr(code, node->val.v);
          if (offs > 0) add_instr(code, PUSHC, offs, ADD_INT, 0);
          add_instr(code, (layout[i] == TYPE_CHAR) ? STB : STC, 0);
          offs += (layout[i] == TYPE_CHAR) ? 1 : 4;
        }
        free(layout);
      }
      add_instr(code, STEP_OUT, 0);
      break;
    // ................................
    case AST_NODE_EXPRESSION:
      add_step_in(code);
      emit_code_expression(code, node, 0, 1);
      add_instr(code, STEP_OUT, 0);
      break;
    // ................................
    case AST_NODE_STATEMENT:
      switch (node->val.s->variant) {
        case STMT_FOR: {
          if (!node->val.s->par[0]) return;
          add_instr(code, MEM_MARK, 0);
          ast_node_t *A = node->val.s->par[0]->val.sc->items;
          ast_node_t *B = A->next;
          ast_node_t *C = B->next;
          ast_node_t *D = C->next;
          if (B->val.e->type->compound ||
              !static_type_equal(B->val.e->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc), "condition must be of integral type");
            return;
          }
          emit_code_node(code, A);
          int ret = code->pos;
          add_step_in(code);
          emit_code_expression(code, B, 0, 0);
          add_instr(code, STEP_OUT, 0);
          add_instr(code, SPLIT, 0);
          if (D) emit_code_node(code, D);
          emit_code_node(code, C);
          add_instr(code, JMP, 10, JOIN, JOIN_JMP, 10, JOIN, 0);
          add_instr(code, JOIN_JMP, ret - code->pos - 1, MEM_FREE, 0);
        } break;
        case STMT_WHILE: {
          if (!node->val.s->par[0] || !node->val.s->par[1]) return;
          int ret = code->pos;
          if (node->val.s->par[0]->val.e->type->compound ||
              !static_type_equal(node->val.s->par[0]->val.e->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc), "condition must be of integral type");
            return;
          }
          add_step_in(code);
          emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, STEP_OUT, 0);
          add_instr(code, SPLIT, 0);
          emit_code_node(code, node->val.s->par[1]->val.sc->items);
          add_instr(code, JMP, 10, JOIN, JOIN_JMP, 10, JOIN, 0);
          add_instr(code, JOIN_JMP, ret - code->pos - 1, MEM_FREE, 0);
        }; break;
        case STMT_DO: {
          if (!node->val.s->par[0] || !node->val.s->par[1]) return;
          int ret = code->pos;
          if (node->val.s->par[0]->val.e->type->compound ||
              !static_type_equal(node->val.s->par[0]->val.e->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc), "condition must be of integral type");
            return;
          }
          emit_code_node(code, node->val.s->par[1]->val.sc->items);
          add_step_in(code);
          emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, STEP_OUT, 0);
          add_instr(code, SPLIT, 0);
          add_instr(code, JMP, 10, JOIN, JOIN_JMP, 10, JOIN, 0);
          add_instr(code, JOIN_JMP, ret - code->pos - 1, 0);
        }; break;
        case STMT_PARDO: {
          if (!node->val.s->par[0] || !node->val.s->par[1]) return;
          if (node->val.s->par[1]->val.e->type->compound ||
              !static_type_equal(node->val.s->par[1]->val.e->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc), "condition must be of integral type");
            return;
          }
          add_step_in(code);
          emit_code_expression(code, node->val.s->par[1], 0, 0);
          add_instr(code, STEP_OUT, 0);
          emit_code_var_addr(code, node->val.s->par[0]->val.sc->items->val.v);
          add_instr(code, FORK, 0);
          emit_code_node(code, node->val.s->par[0]);
          add_instr(code, JOIN, 0);
        } break;
        case STMT_COND: {
          if (!node->val.s->par[0] || !node->val.s->par[1]) return;
          if (node->val.s->par[0]->val.e->type->compound ||
              !static_type_equal(node->val.s->par[0]->val.e->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc), "condition must be of integral type");
            return;
          }
          add_step_in(code);
          emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, STEP_OUT, 0);
          add_instr(code, SPLIT, 0);
          emit_code_node(code, node->val.s->par[1]->val.sc->items);
          add_instr(code, JOIN, 0);
          emit_code_node(code, node->val.s->par[1]->val.sc->items->next);
          add_instr(code, JOIN, 0);
        } break;
        case STMT_RETURN: {
          function_t *fn = node->val.s->ret_fn;
          int need_fn_cleanup = 0;
          if (generating_breakpoint && !fn) {
            need_fn_cleanup = 1;
            fn = function_t_new("breakpoint_f");
            fn->out_type = GLOBAL_ast->__type__int->val.t;
          }
          if (!fn) {
            error(&(node->loc), "return statement outside of function");
            return;
          }
          add_step_in(code);
          if (node->val.s->par[0]) {
            int *casts, n_casts;
            if (inferred_type_compatible(fn->out_type,
                                         node->val.s->par[0]->val.e->type,
                                         &casts, &n_casts)) {
              emit_code_expression(code, node->val.s->par[0], 0, 0);
              emit_code_cast_value(code, casts, n_casts);
              free(casts);
            } else {
              error(&(node->loc),
                    "incompatible types in return satement: function %s should "
                    "return %s",
                    fn->name,
                    fn->out_type->name);
              return;
            }
          } else if (strcmp(fn->out_type->name, "void")) {
            error(&(node->loc),
                  "return statement without parameter in a function "
                  "returning %s",
                  fn->out_type->name);
            return;
          }
          if (need_fn_cleanup) {
            function_t_delete(fn);
          }
          add_instr(code, SETR, 0);
          add_instr(code, STEP_OUT, 0);
        } break;
        case STMT_BREAKPOINT: {
          expression_t *ex = node->val.s->par[0]->val.e;
          if (ex->type->compound || !static_type_equal(ex->type->type, GLOBAL_ast->__type__int->val.t)) {
            error(&(node->loc),
                  "breakpoint condition must be of integral type");
            return;
          }
          emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, BREAK, 0);
        } break;
      }
      break;
    // ................................
    case AST_NODE_SCOPE:
      emit_code_scope(code, node->val.sc);
      break;
    default:
      break;
  }
  node->code_to = code->pos - 1;
  // printf("%d:%d:%d:%d %d %d-%d\n", node->loc.fl, node->loc.fc, node->loc.ll, node->loc.lc, node->node_type, node->code_from, node->code_to);
}

/* ----------------------------------------------------------------------------
 * generate code for scope
 */
static void emit_code_scope(code_block_t *code, scope_t *sc) {
  add_instr(code, MEM_MARK, 0);
  for (ast_node_t *p = sc->items; p; p = p->next) emit_code_node(code, p);
  add_instr(code, MEM_FREE, 0);
}

/* ----------------------------------------------------------------------------
 * used from emit_code_function to check if there is a return statement
 * within pardo - this is forbidden
 */
static void check_pardo_return(ast_node_t *node, int inside) {
  switch (node->node_type) {
    case AST_NODE_SCOPE: {
      for (ast_node_t *it = node->val.sc->items; it; it = it->next)
        check_pardo_return(it, inside);
    } break;
    case AST_NODE_STATEMENT: {
      switch (node->val.s->variant) {
        case STMT_COND:
        case STMT_WHILE:
        case STMT_DO:
          check_pardo_return(node->val.s->par[1], inside);
          break;
        case STMT_FOR:
          check_pardo_return(node->val.s->par[0], inside);
          break;
        case STMT_PARDO:
          check_pardo_return(node->val.s->par[0], 1);
          break;
        case STMT_RETURN:
          if (inside) {
            error(&(node->loc), "return statement inside pardo");
            return;
          }
      }
    } break;
  }
}

/* ----------------------------------------------------------------------------
 * generate code for function
 */

static void emit_code_function(code_block_t *code, ast_node_t *fn) {
  assert(fn->node_type == AST_NODE_FUNCTION);

  fn->code_from = code->pos;
  // load parameters
  // on the stack are values
  DEBUG("emit_code_function %s (addr: %d)\n", fn->val.f->name, code->pos);
  for (ast_node_t *p = fn->val.f->params; p; p = p->next) {
    if (p->val.v->num_dim == 0) {
      add_instr(code, PUSHC, p->val.v->addr, FBASE, 0);
      int *casts, n_casts;
      static_type_compatible(p->val.v->base_type, p->val.v->base_type, &casts,
                             &n_casts);
      emit_code_store_value(code, 0, casts, n_casts);
      if (casts) free(casts);
    } else {
      for (int i = 0; i < p->val.v->num_dim + 2; i++)
        add_instr(code, PUSHC, p->val.v->addr + 4 * i, FBASE, STC, 0);
    }
  }

  // error for return within pardo
  for (ast_node_t *n = fn->val.f->root_scope->items; n; n = n->next)
    check_pardo_return(n, 0);

  // error  for end of control without return

  emit_code_scope(code, fn->val.f->root_scope);
  add_instr(code, RETURN, 0);
  fn->code_to = code->pos;
}

/* ----------------------------------------------------------------------------
 * write the input/output variables section of the binary file
 */
static void write_io_variables(writer_t *out, int flag) {
  int n = 0;
  for (ast_node_t *x = GLOBAL_ast->root_scope->items; x; x = x->next)
    if (x->node_type == AST_NODE_VARIABLE && x->val.v->io_flag == flag) n++;
  out_raw(out, &n, 4);
  for (ast_node_t *x = GLOBAL_ast->root_scope->items; x; x = x->next)
    if (x->node_type == AST_NODE_VARIABLE && x->val.v->io_flag == flag) {
      out_raw(out, &(x->val.v->addr), 4);
      out_raw(out, &(x->val.v->num_dim), 4);
      uint8_t *layout;
      uint8_t ts = static_type_layout(x->val.v->base_type, &layout);
      out_raw(out, &ts, 1);
      for (int i = 0; i < ts; i++) {
        out_raw(out, &(layout[i]), 1);
      }
      free(layout);
    }
}

/* ----------------------------------------------------------------------------
 * main entry
 */
int emit_code(ast_t *ast, writer_t *out, int no_debug) {
  GLOBAL_ast = ast;

  // just for debugging: write all types
  DEBUG("types\n");
  for (ast_node_t *t = GLOBAL_ast->types; t; t = t->next) {
    static_type_t *type = t->val.t;
    DEBUG("%s (size %d) = [ ", type->name, type->size);
    for (static_type_member_t *m = type->members; m; m = m->next) {
      DEBUG("%s %s", m->type->name, m->name);
      if (m->next) DEBUG(", ");
    }
    DEBUG(" ]\n");
  }

  {
    // give each function its ID (used in the FNMAP) section of the code
    // assign addresses to variables within in the function
    // (parameters are located on the lowest addresses)
    int n = 0;
    for (ast_node_t *fn = GLOBAL_ast->functions; fn; fn = fn->next) {
      assert(fn->node_type == AST_NODE_FUNCTION);
      if (!fn->val.f->root_scope) {
        DEBUG("function %s without scope\n", fn->val.f->name);
        continue;
      }
      fn->val.f->n = n++;
      uint32_t base = 0;
      DEBUG("\nfunction #%d: %s\n", fn->val.f->n, fn->val.f->name);
      for (ast_node_t *p = fn->val.f->params; p; p = p->next)
        base = assign_single_variable_address(base, p->val.v);
      DEBUG("items:\n");
      assign_scope_variable_addresses(base, fn->val.f->root_scope);
      DEBUG("function #%d done\n\n", fn->val.f->n);
    }
  }

  // global variables have lowest addresses, even if they are defined
  // late in the source
  uint32_t base = 0;
  DEBUG("root variables\n");
  for (ast_node_t *p = GLOBAL_ast->root_scope->items; p; p = p->next)
    if (p->node_type == AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);

  // assign addresses to variables in subscopes
  DEBUG("root subscopes\n");
  for (ast_node_t *p = GLOBAL_ast->root_scope->items; p; p = p->next)
    if (p->node_type != AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);

  // main part - generate the code block
  code_block_t *code = code_block_t_new();
  emit_code_scope(code, GLOBAL_ast->root_scope);
  add_instr(code, ENDVM, 0);

  // add code for functions at the end
  for (ast_node_t *fn = GLOBAL_ast->functions; fn; fn = fn->next)
    if (fn->val.f->root_scope) {
      fn->val.f->addr = code->pos;
      emit_code_function(code, fn);
    }

  // compute the size of the memory used by global variables
  uint32_t global_size = 0;
  for (ast_node_t *nd = GLOBAL_ast->root_scope->items; nd; nd = nd->next)
    if (nd->node_type == AST_NODE_VARIABLE) {
      variable_t *var = nd->val.v;
      uint32_t sz = var->addr;
      if (var->num_dim == 0)
        sz += var->base_type->size;
      else
        sz += 4 * (2 + var->num_dim);
      if (sz > global_size) global_size = sz;
    }

  if (GLOBAL_ast->error_occured) {
    code_block_t_delete(code);
    return GLOBAL_ast->error_occured;
  }

  // write the binary file (see code.h)
  {  
    uint8_t section;

    {
      section = SECTION_HEADER;
      out_raw(out, &section, 1);
      int version = 1;
      out_raw(out, &version, 1);
      out_raw(out, &global_size, 4);
      uint8_t mm;
      switch (GLOBAL_ast->mem_mode) {
        case TOK_MODE_EREW:
          mm = MEM_MODE_EREW;
          break;
        case TOK_MODE_CCRCW:
          mm = MEM_MODE_CCRCW;
          break;
        default:
          mm = MEM_MODE_CREW;
      }
      out_raw(out, &mm, 1);
    }

    {
      section = SECTION_INPUT;
      out_raw(out, &section, 1);
      write_io_variables(out, IO_FLAG_IN);
    }

    {
      section = SECTION_OUTPUT;
      out_raw(out, &section, 1);
      write_io_variables(out, IO_FLAG_OUT);
    }

    {
      section = SECTION_FNMAP;
      out_raw(out, &section, 1);
      uint32_t n = 0;
      for (ast_node_t *fn = GLOBAL_ast->functions; fn; fn = fn->next)
        if (fn->val.f->root_scope) n++;
      out_raw(out, &n, 4);
      for (ast_node_t *fn = GLOBAL_ast->functions; fn; fn = fn->next)
        if (fn->val.f->root_scope) {
          out_raw(out, &(fn->val.f->addr), 4);
          int32_t out_size = fn->val.f->out_type->size;
          for (ast_node_t *p = fn->val.f->params; p; p = p->next)
            if (p->val.v->num_dim == 0)
              out_size -= p->val.v->base_type->size;
            else
              out_size -= 4 * (2 + p->val.v->num_dim);
          out_raw(out, &(out_size), 4);
        }
    }

    // emit the debug information from debug.h
    if (!no_debug) emit_debug_section(out, GLOBAL_ast, code->pos + 1);

    {
      section = SECTION_CODE;
      out_raw(out, &section, 1);
      out_raw(out, code->data, code->pos);
    }
  }

  code_block_t_delete(code);
  return 0;
}

int emit_code_scope_section(ast_t *ast, scope_t *scope, writer_t *out) {
  GLOBAL_ast = ast;

  // global variables have lowest addresses, even if they are defined
  // late in the source
  uint32_t base = 0;
  DEBUG("root variables\n");
  for (ast_node_t *p = GLOBAL_ast->root_scope->items; p; p = p->next)
    if (p->node_type == AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);

  // assign addresses to variables in subscopes
  DEBUG("root subscopes\n");
  for (ast_node_t *p = GLOBAL_ast->root_scope->items; p; p = p->next)
    if (p->node_type != AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);
  
  int b = 1000;
  for (ast_node_t *p = scope->items; p; p = p->next)
    b = assign_node_variable_addresses(b, p);

  // main part - generate the code block
  code_block_t *code = code_block_t_new();
  emit_code_scope(code, scope);
  add_instr(code, ENDVM, 0);

  if (GLOBAL_ast->error_occured) {
    code_block_t_delete(code);
    return GLOBAL_ast->error_occured;
  }

  out_raw(out, code->data, code->pos);

  code_block_t_delete(code);
  return 0;
}

#undef DEBUG
