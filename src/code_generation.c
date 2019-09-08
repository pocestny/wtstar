#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <code_generation.h>
#include <errors.h>
#include <parser.h>

// #define NODEBUG

#ifdef NODEBUG
#define DEBUG(...) /* */
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

static ast_t *ast;
static int was_error = 0;

static void error(YYLTYPE *loc, const char *format, ...) {
  was_error = 1;
  error_t *err = error_t_new();
  int n;
  get_printed_length(format, n);
  append_error_msg(err, "%s %d %d: ", loc->fn, loc->fl, loc->fc);
  va_list args;
  va_start(args, format);
  append_error_vmsg(err, n, format, args);
  va_end(args);
  emit_error(err);
}

static int assign_oper(int oper) {
  if (oper == '=' || oper == TOK_PLUS_ASSIGN || oper == TOK_MINUS_ASSIGN)
    return 1;
  return 0;
}

static int binary_oper(int oper) {
  if (oper == '+' || oper == '-' || oper == '*' || oper == '^' || oper == '/')
    return 1;
  return 0;
}

static int comparison_oper(int oper) {
  if (oper == TOK_EQ || oper == TOK_NEQ || oper == TOK_GEQ || oper == TOK_LEQ ||
      oper == '<' || oper == '>')
    return 1;
  return 0;
}

static void emit_code_scope(code_block_t *code, scope_t *sc);

/* ----------------------------------------------------------------------------
 * resizable page of code
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

// add one instruction with parameters to code block
void add_instr(code_block_t *out, int code, ...) {
  static uint8_t buf[4096];
  int len = 0;
  va_list args;
  va_start(args, code);

  while (code != NOOP) {
    buf[len++] = code;
    switch (code) {
      case PUSHC:
      case JMP:
      case CALL:
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
 * assign_variable_addresses
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

static int assign_node_variable_addresses(uint32_t base, ast_node_t *node) {
  switch (node->node_type) {
    case AST_NODE_VARIABLE:
      base = assign_single_variable_address(base, node->val.v);
      break;
    case AST_NODE_SCOPE: {
      int b = base;
      for (ast_node_t *p = node->val.sc->items; p; p = p->next)
        b = assign_node_variable_addresses(b, p);
    } break;
    case AST_NODE_STATEMENT:
      if (node->val.s->variant == STMT_FOR ||
          node->val.s->variant == STMT_PARDO)
        base = assign_node_variable_addresses(base, node->val.s->par[0]);
      break;
  }
  return base;
}

static int assign_scope_variable_addresses(uint32_t base, scope_t *sc) {
  int b = base;
  for (ast_node_t *p = sc->items; p; p = p->next)
    b = assign_node_variable_addresses(b, p);
  return base;
}

/* ----------------------------------------------------------------------------
 * size (in bytes) of an inferred type
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
  if (var->scope->fn) add_instr(code, FBASE, ADD_INT, 0);
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
 * the stack contains value of type tm->parent, make it so
 * that only tm->type part remains
 *
 */
static void emit_code_remove_type(code_block_t *code, static_type_t *t) {
  int n = static_type_layout(t, NULL);
  for (int i = 0; i < n; i++) add_instr(code, POP, 0);
}

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
    case EXPR_LITERAL:
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

    case EXPR_CALL: {
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
      add_instr(code, CALL, ex->val.f->fn->n, 0);
    } break;

    case EXPR_VAR_NAME:
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

    case EXPR_INITIALIZER: {
      assert(!addr);
      int n = length(ex->val.i);
      ast_node_t **tmp = (ast_node_t **)malloc(n * sizeof(ast_node_t *));
      ast_node_t *nd = ex->val.i;
      for (int i = 0; i < n; i++, nd = nd->next) tmp[n - i - 1] = nd;
      for (int i = 0; i < n; i++)
        emit_code_expression(code, tmp[i], addr, clear);
      free(tmp);
    } break;

    case EXPR_ARRAY_ELEMENT: {
      int n = ex->val.v->var->num_dim;

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

    case EXPR_SIZEOF: {
      variable_t *v = ex->val.v->var;
      emit_code_expression(code, ex->val.v->params, 0, 0);
      emit_code_var_addr(code, v);
      add_instr(code, SIZE, 0);
    } break;
    case EXPR_BINARY:
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

        if (ex->val.o->oper == TOK_PLUS_ASSIGN ||
            ex->val.o->oper == TOK_MINUS_ASSIGN) {
          // TODO: check for numeric types
          emit_code_expression(code, ex->val.o->second, 0, 0);
          emit_code_expression(code, ex->val.o->first, 1, 0);
          int op = (ex->val.o->oper == TOK_PLUS_ASSIGN) ? ADD_INT : SUB_INT;
          add_instr(code, S2A, load, op, A2S, POPA, store, 0);
        } else {
          // just assign
          int *casts = NULL, n_casts;
          ast_node_t *l = ex->val.o->first, *r = ex->val.o->second;
          if (l->val.e->type->compound) {
            error(&(l->loc),
                  "This should not have happened, we are all doomed now.");
            assert(0);
          }
          if (inferred_type_compatible(l->val.e->type->type, r->val.e->type,
                                       &casts, &n_casts)) {
            emit_code_expression(code, r, 0, 0);
            emit_code_expression(code, l, 1, 0);
            emit_code_store_value(code, expr_on_heap(l->val.e), casts, n_casts);
          } else {
            error(&(exn->loc),
                  "assignment of incompatible types (maybe add explicit cast)");
          }
          if (casts) free(casts);
        }

      } else if (binary_oper(ex->val.o->oper)) {
        // ..........
        // binary op other that assignment

        // TODO check types
        emit_code_expression(code, ex->val.o->second, 0, 0);
        emit_code_expression(code, ex->val.o->first, 0, 0);
        int op;
        switch (ex->val.o->oper) {
          case '+':
            op = ADD_INT;
            break;
          case '-':
            op = SUB_INT;
            break;
          case '*':
            op = MULT_INT;
            break;
          case '^':
            op = POW_INT;
            break;
          case '/':
            op = DIV_INT;
            break;
        }
        add_instr(code, op, 0);
      } else if (comparison_oper(ex->val.o->oper)) {
        // ..........
        // comparison
        int op;
        switch (ex->val.o->oper) {
          case TOK_EQ:
            op = EQ_INT;
            break;
          case TOK_NEQ:
            op = EQ_INT;
            break;
          case TOK_LEQ:
            op = LEQ_INT;
            break;
          case TOK_GEQ:
            op = GEQ_INT;
            break;
          case '<':
            op = LT_INT;
            break;
          case '>':
            op = GT_INT;
            break;
        }

        emit_code_expression(code, ex->val.o->second, 0, 0);
        emit_code_expression(code, ex->val.o->first, 0, 0);
        add_instr(code, op, 0);
        if (ex->val.o->oper == TOK_NEQ) add_instr(code, NOT, 0);
      }

      break;
    case EXPR_PREFIX:
      if (ex->val.o->oper == TOK_DEC || ex->val.o->oper == TOK_INC) {
        int onheap = expr_on_heap(ex->val.o->first->val.e);
        int load = onheap ? LDCH : LDC;
        int store = onheap ? STCH : STC;
        int op = (ex->val.o->oper == TOK_DEC) ? SUB_INT : ADD_INT;

        add_instr(code, PUSHB, 1, 0);
        emit_code_expression(code, ex->val.o->first, 1, 0);
        add_instr(code, S2A, load, op, 0);
        if (!clear) add_instr(code, S2A, SWA, 0);
        add_instr(code, A2S, POPA, store, 0);
        if (!clear) add_instr(code, A2S, POPA, 0);
      } else if (ex->val.o->oper == '-') {
        emit_code_expression(code, ex->val.o->first, 0, 0);
        add_instr(code, PUSHB, 0, 0);
        if (static_type_basic(ex->val.o->first->val.e->type->type) ==
            TYPE_FLOAT)
          add_instr(code, SUB_FLOAT, 0);
        else
          add_instr(code, SUB_INT, 0);
      }
      break;
    case EXPR_POSTFIX:
      if (ex->val.o->oper == TOK_DEC || ex->val.o->oper == TOK_INC) {
        int onheap = expr_on_heap(ex->val.o->first->val.e);
        int load = onheap ? LDCH : LDC;
        int store = onheap ? STCH : STC;
        int op = (ex->val.o->oper == TOK_DEC) ? SUB_INT : ADD_INT;

        add_instr(code, PUSHB, 1, 0);
        emit_code_expression(code, ex->val.o->first, 1, 0);
        add_instr(code, S2A, load, 0);
        if (!clear) add_instr(code, S2A, SWA, 0);
        add_instr(code, op, A2S, POPA, store, 0);
        if (!clear) add_instr(code, A2S, POPA, 0);
      }
      break;
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
  }
}

/* ----------------------------------------------------------------------------
 * generate code for an AST node
 */
static void emit_code_node(code_block_t *code, ast_node_t *node) {
  if (node->emitted) return;
  node->emitted = 1;
  switch (node->node_type) {
    // ................................
    case AST_NODE_VARIABLE:
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
            error(
                &(node->loc),
                "incompatible types in initializer (maybe add explicit cast)");

        } else {
          error(&(node->loc), "initializers are not supported for arrays");
        }
      }
      break;
    // ................................
    case AST_NODE_EXPRESSION:
      emit_code_expression(code, node, 1, 1);
      break;
    // ................................
    case AST_NODE_STATEMENT:
      switch (node->val.s->variant) {
        case STMT_FOR: {
          add_instr(code, MEM_MARK, 0);
          ast_node_t *A = node->val.s->par[0]->val.sc->items;
          ast_node_t *B = A->next;
          ast_node_t *C = B->next;
          ast_node_t *D = C->next;

          emit_code_node(code, A);
          int ret = code->pos;
          emit_code_expression(code, B, 0, 0);
          add_instr(code, SPLIT, JOIN, 0);
          if (D) emit_code_node(code, D);
          emit_code_node(code, C);
          add_instr(code, JMP, 10, JOIN, JMP, 10, JOIN, 0);
          add_instr(code, JMP, ret - code->pos - 1, MEM_FREE, 0);
        } break;
        case STMT_PARDO: {
          emit_code_expression(code, node->val.s->par[1], 0, 0);
          emit_code_var_addr(code, node->val.s->par[0]->val.sc->items->val.v);
          add_instr(code, FORK, 0);
          emit_code_scope(code, node->val.s->par[0]->val.sc);
          add_instr(code, JOIN, 0);
        } break;
        case STMT_COND: {
          emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, SPLIT, 0);
          emit_code_node(code, node->next->next);
          add_instr(code, JOIN, 0);
          emit_code_node(code, node->next);
          add_instr(code, JOIN, 0);
        } break;
        case STMT_RETURN: {
          // FIXME: return within pardo has to join threads
          if (node->val.s->par[0])
            emit_code_expression(code, node->val.s->par[0], 0, 0);
          add_instr(code, RETURN, 0);
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
}

/* ----------------------------------------------------------------------------
 * generate code for scope
 */
static void emit_code_scope(code_block_t *code, scope_t *sc) {
  add_instr(code, MEM_MARK, 0);
  for (ast_node_t *p = sc->items; p; p = p->next) emit_code_node(code, p);
  add_instr(code, MEM_FREE, 0);
}

static void emit_code_function(code_block_t *code, ast_node_t *fn) {
  assert(fn->node_type == AST_NODE_FUNCTION);

  // load parameters
  // on the stack are values
  for (ast_node_t *p = fn->val.f->params; p; p = p->next) {
    if (p->val.v->num_dim == 0) {
      add_instr(code, PUSHC, p->val.v->addr, FBASE, ADD_INT, 0);
      int *casts, n_casts;
      static_type_compatible(p->val.v->base_type, p->val.v->base_type, &casts,
                             &n_casts);
      emit_code_store_value(code, 0, casts, n_casts);
      if (casts) free(casts);
    } else {
      for (int i = 0; i < p->val.v->num_dim + 2; i++)
        add_instr(code, PUSHC, p->val.v->addr + 4 * i, FBASE, ADD_INT, STC, 0);
    }
  }

  emit_code_scope(code, fn->val.f->root_scope);
}

/* ----------------------------------------------------------------------------
 */
static void write_io_variables(writer_t *out, int flag) {
  int n = 0;
  for (ast_node_t *x = ast->root_scope->items; x; x = x->next)
    if (x->node_type == AST_NODE_VARIABLE && x->val.v->io_flag == flag) n++;
  out_raw(out, &n, 4);
  for (ast_node_t *x = ast->root_scope->items; x; x = x->next)
    if (x->node_type == AST_NODE_VARIABLE && x->val.v->io_flag == flag) {
      out_raw(out, &(x->val.v->addr), 4);
      out_raw(out, &(x->val.v->num_dim), 1);
      uint8_t *layout;
      int ts = static_type_layout(x->val.v->base_type, &layout);
      out_raw(out, &ts, 1);
      for (int i = 0; i < ts; i++) out_raw(out, layout + i, 1);
      free(layout);
    }
}

/* ----------------------------------------------------------------------------
 */
void emit_code(ast_t *_ast, writer_t *out) {
  ast = _ast;

  DEBUG("types\n");
  for (ast_node_t *t = ast->types; t; t = t->next) {
    static_type_t *type = t->val.t;
    DEBUG("%s (size %d) = [ ", type->name, type->size);
    for (static_type_member_t *m = type->members; m; m = m->next) {
      DEBUG("%s %s", m->type->name, m->name);
      if (m->next) DEBUG(", ");
    }
    DEBUG(" ]\n");
  }

  {
    int n = 0;
    for (ast_node_t *fn = ast->functions; fn; fn = fn->next) {
      assert(fn->node_type == AST_NODE_FUNCTION);
      fn->val.f->n = n++;
      uint32_t base = 0;
      DEBUG("\nfunction #%d: %s\n", fn->val.f->n, fn->val.f->name);
      for (ast_node_t *p = fn->val.f->params; p; p = p->next)
        base = assign_single_variable_address(base, p->val.v);
      DEBUG("items:\n");
      assign_scope_variable_addresses(base, fn->val.f->root_scope);
      DEBUG("function #%d done\n", fn->val.f->n);
    }
  }

  uint32_t base = 0;
  DEBUG("root variables\n");
  for (ast_node_t *p = ast->root_scope->items; p; p = p->next)
    if (p->node_type == AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);
  DEBUG("root subscopes\n");
  for (ast_node_t *p = ast->root_scope->items; p; p = p->next)
    if (p->node_type != AST_NODE_VARIABLE)
      base = assign_node_variable_addresses(base, p);

  code_block_t *code = code_block_t_new();
  emit_code_scope(code, ast->root_scope);
  add_instr(code, ENDVM, 0);

  for (ast_node_t *fn = ast->functions; fn; fn = fn->next) {
    fn->val.f->addr = code->pos;
    emit_code_function(code, fn);
  }

  uint32_t global_size = 0;
  for (ast_node_t *nd = ast->root_scope->items; nd; nd = nd->next)
    if (nd->node_type == AST_NODE_VARIABLE) {
      variable_t *var = nd->val.v;
      uint32_t sz = var->addr;
      if (var->num_dim == 0)
        sz += var->base_type->size;
      else
        sz += 4 * (2 + var->num_dim);
      if (sz > global_size) global_size = sz;
    }

  if (!was_error) {
    uint8_t section;

    {
      section = SECTION_HEADER;
      out_raw(out, &section, 1);
      int version = 1;
      out_raw(out, &version, 1);
      out_raw(out, &global_size, 4);
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
      uint32_t n = length(ast->functions);
      out_raw(out, &n, 4);
      for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
        out_raw(out, &(fn->val.f->addr), 4);
    }

    {
      section = SECTION_CODE;
      out_raw(out, &section, 1);
      out_raw(out, code->data, code->pos);
    }
  }

  code_block_t_delete(code);

  DEBUG("emit_code done\n");
}

#undef DEBUG
