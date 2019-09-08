#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <vm.h>

extern int EXEC_DEBUG;
#include <stdio.h>  // for debug

extern const char *const instr_names[];

int ipow(int base, int exp) {
  int result = 1;
  while (exp) {
    if (exp & 1) result *= base;
    exp /= 2;
    base *= base;
  }
  return result;
}

CONSTRUCTOR(stack_t) {
  ALLOC_VAR(r, stack_t)
  r->data = (uint8_t *)calloc(16, 1);
  r->top = 0;
  r->size = 16;
  return r;
}

DESTRUCTOR(stack_t) {
  if (r == NULL) return;
  if (r->data) free(r->data);
  free(r);
}

void stack_t_push(stack_t *s, void *data, uint32_t len) {
  while (s->size - s->top <= len) {
    s->size *= 2;
    s->data = (uint8_t *)realloc(s->data, s->size);
  }
  memcpy((void *)(s->data + s->top), data, len);
  s->top += len;
}

void stack_t_alloc(stack_t *s, uint32_t len) {
  while (s->size - s->top <= len) {
    s->size *= 2;
    s->data = (uint8_t *)realloc(s->data, s->size);
  }
  s->top += len;
}

void stack_t_pop(stack_t *s, void *data, uint32_t len) {
  memcpy(data, (void *)(s->data + s->top - len), len);
  s->top -= len;
}

CONSTRUCTOR(thread_t) {
  ALLOC_VAR(r, thread_t)
  r->mem_base = 0;
  r->op_stack = stack_t_new();
  r->acc_stack = stack_t_new();
  r->mem = stack_t_new();
  r->parent = NULL;
  r->refcnt = 1;
  return r;
}

thread_t *clone_thread(thread_t *src) {
  thread_t *r = thread_t_new();
  r->parent = src;
  r->mem_base = src->mem_base + src->mem->top;
  // maybe copy op and acc ?
  // should not bee needed
  return r;
}

DESTRUCTOR(thread_t) {
  if (r == NULL) return;
  r->refcnt--;
  if (r->refcnt <= 0) {
    stack_t_delete(r->op_stack);
    stack_t_delete(r->acc_stack);
    stack_t_delete(r->mem);
    free(r);
  }
}

CONSTRUCTOR(frame_t, uint32_t base) {
  ALLOC_VAR(r, frame_t)

  r->base = base;
  r->heap_mark = stack_t_new();
  r->mem_mark = stack_t_new();
  return r;
}

DESTRUCTOR(frame_t) {
  if (r == NULL) return;
  stack_t_delete(r->heap_mark);
  stack_t_delete(r->mem_mark);
  free(r);
}

/* create runtime */
#define GET(type, var, b)         \
  {                               \
    if (pos + (b) > len) exit(1); \
    var = *((type *)(in + pos));  \
    pos += b;                     \
  }

CONSTRUCTOR(runtime_t, uint8_t *in, int len) {
  ALLOC_VAR(r, runtime_t)

  r->heap = stack_t_new();
  r->threads = stack_t_new();
  r->frames = stack_t_new();

  r->W = r->T = r->pc = r->virtual_grps = 0;
  r->n_thr = 1;

  frame_t *tf = frame_t_new(0);
  stack_t_push(r->frames, (void *)(&tf), sizeof(frame_t *));

  thread_t *main_thread = thread_t_new();
  main_thread->mem_base = 0;
  main_thread->refcnt = 1;
  stack_t *grp = stack_t_new();
  stack_t_push(grp, (void *)(&main_thread), sizeof(thread_t *));
  stack_t_push(r->threads, (void *)(&grp), sizeof(stack_t *));
  
  r->thr = STACK(STACK(r->threads, stack_t *)[0], thread_t *);
  r->frame = STACK(r->frames, frame_t *)[0];

  // parse input file
  uint8_t section;
  for (int pos = 0; pos < len;) {
    GET(uint8_t, section, 1);
    switch (section) {
      case SECTION_HEADER: {
        uint8_t version;
        uint32_t global_size;
        GET(uint8_t, version, 1)
        GET(uint32_t, global_size, 4)
        stack_t_alloc(main_thread->mem, global_size);
      } break;
      case SECTION_INPUT:
        GET(uint32_t, r->n_in_vars, 4)
        r->in_vars = (input_layout_item_t *)malloc(r->n_in_vars *
                                                   sizeof(input_layout_item_t));
        for (int i = 0; i < r->n_in_vars; i++) {
          GET(uint32_t, r->in_vars[i].addr, 4)
          GET(uint8_t, r->in_vars[i].num_dim, 1)
          GET(uint8_t, r->in_vars[i].n_elems, 1)
          r->in_vars[i].elems = (uint8_t *)malloc(r->in_vars[i].n_elems);
          for (int j = 0; j < r->in_vars[i].n_elems; j++)
            GET(uint8_t, r->in_vars[i].elems[j], 1);
        }
        break;
      case SECTION_OUTPUT:
        GET(uint32_t, r->n_out_vars, 4)
        r->out_vars = (input_layout_item_t *)malloc(
            r->n_out_vars * sizeof(input_layout_item_t));
        for (int i = 0; i < r->n_out_vars; i++) {
          GET(uint32_t, r->out_vars[i].addr, 4)
          GET(uint8_t, r->out_vars[i].num_dim, 1)
          GET(uint8_t, r->out_vars[i].n_elems, 1)
          r->out_vars[i].elems = (uint8_t *)malloc(r->out_vars[i].n_elems);
          for (int j = 0; j < r->out_vars[i].n_elems; j++)
            GET(uint8_t, r->out_vars[i].elems[j], 1);
        }
        break;
      case SECTION_FNMAP: {
        GET(uint32_t, r->fcnt, 4);
        if (r->fcnt > 0)
          r->fnmap = (uint32_t *)malloc(r->fcnt * 4);
        else
          r->fnmap = NULL;
        for (uint32_t i = 0; i < r->fcnt; i++) GET(uint32_t, r->fnmap[i], 4);
      } break;
      case SECTION_CODE:
        r->code_size = len - pos;
        r->code = (uint8_t *)malloc(len - pos);
        memcpy(r->code, in + pos, len - pos);
        pos = len;
        break;
    }
  }
  return r;
}

#undef GET

DESTRUCTOR(runtime_t) {
  if (r == NULL) return;
  if (r->n_in_vars > 0) {
    for (int i = 0; i < r->n_in_vars; i++)
      if (r->in_vars[i].elems) free(r->in_vars[i].elems);
    free(r->in_vars);
  }
  if (r->n_out_vars > 0) {
    for (int i = 0; i < r->n_out_vars; i++)
      if (r->out_vars[i].elems) free(r->out_vars[i].elems);
    free(r->out_vars);
  }
  if (r->code) free(r->code);

  int n_grps = STACK_SIZE(r->threads, stack_t *);
  for (int i = 0; i < n_grps; i++) {
    int gsize = STACK_SIZE(STACK(r->threads, stack_t *)[i], thread_t *);
    for (int j = 0; j < gsize; j++)
      thread_t_delete(STACK(STACK(r->threads, stack_t *)[i], thread_t *)[j]);
    stack_t_delete(STACK(r->threads, stack_t *)[i]);
  }

  // FIXME: delete contents of threads,frames
  stack_t_delete(r->threads);
  stack_t_delete(r->frames);
  if (r->fnmap) free(r->fnmap);
  free(r);
}

#define _PUSH(var, len) stack_t_push(env->thr[t]->op_stack, (void *)(&(var)), len)
#define _POP(var, len) stack_t_pop(env->thr[t]->op_stack, (void *)(&(var)), len)

void *get_addr(thread_t *thr, uint32_t addr, uint32_t len) {
  if (thr->mem_base + thr->mem->top <= len ||
      addr + len > thr->mem_base + thr->mem->top)
    stack_t_alloc(thr->mem, addr + len - thr->mem_base - thr->mem->top);
  while (addr < thr->mem_base) thr = thr->parent;
  return (void *)(thr->mem->data + (addr - thr->mem_base));
}

static void mem_mark(frame_t *frame, runtime_t *env, int n_thr,
                     thread_t **thr) {
  stack_t_push(frame->heap_mark, (void *)&(env->heap->top), 4);
  // TODO: assert all threads have the same memtop
  stack_t_push(frame->mem_mark, (void *)&(thr[0]->mem->top), 4);
}

static void mem_free(frame_t *frame, runtime_t *env, int n_thr,
                     thread_t **thr) {
  stack_t_pop(frame->heap_mark, (void *)&(env->heap->top), 4);
  uint32_t memtop;
  stack_t_pop(frame->mem_mark, (void *)&memtop, 4);
  for (int t = 0; t < n_thr; t++) thr[t]->mem->top = memtop;
}

int execute(runtime_t *env, int limit) {

  if (EXEC_DEBUG) printf("code size: %d\n", env->code_size);

  while (1) {
    if (limit>0) limit--;
    if (limit == 0) return 1;
    uint8_t opcode = lval(env->code + env->pc, uint8_t);
    if (opcode == ENDVM) break;
    env->pc++;

    if (EXEC_DEBUG) {
      printf("\n%3d: %s", env->pc - 1, instr_names[opcode]);
      switch (opcode) {
        case PUSHC:
        case JMP:
          printf(" %d", lval(&env->code[env->pc], int32_t));
          break;
        case CALL:
          printf(" %d", lval(&env->code[env->pc], uint32_t));
          break;
        case PUSHB:
        case IDX:
          printf(" %d", lval(&env->code[env->pc], uint8_t));
          break;
      }
    }

    switch (opcode) {
      case MEM_MARK:
        if (env->n_thr > 0) mem_mark(env->frame, env, env->n_thr, env->thr);
        break;
      case MEM_FREE:
        if (env->n_thr > 0) mem_free(env->frame, env, env->n_thr, env->thr);
        break;
      case FORK:
        if (env->n_thr > 0) {
          env->W++;
          env->T++;

          stack_t *grp = stack_t_new();

          for (int t = 0; t < env->n_thr; t++) {
            uint32_t a, n;
            _POP(a, 4);
            _POP(n, 4);
            for (int j = 0; j < n; j++) {
              thread_t *nt = clone_thread(env->thr[t]);
              lval(get_addr(nt, a, 4), int32_t) = j;
              stack_t_push(grp, (void *)(&nt), sizeof(thread_t *));
            }
          }
          stack_t_push(env->threads, (void *)(&grp), sizeof(stack_t *));
          env->thr = STACK(grp, thread_t *);
          env->n_thr = STACK_SIZE(grp, thread_t *);
        } else
          env->virtual_grps++;
        break;
      case SPLIT: {
        if (env->n_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->n_thr > 0) {
          stack_t *nonzero = stack_t_new();
          stack_t *zero = stack_t_new();
          for (int t = 0; t < env->n_thr; t++) {
            int32_t a;
            _POP(a, 4);
            env->thr[t]->refcnt++;
            if (a == 0)
              stack_t_push(zero, (void *)(&(env->thr[t])), sizeof(thread_t *));
            else
              stack_t_push(nonzero, (void *)(&(env->thr[t])), sizeof(thread_t *));
          }
          stack_t_push(env->threads, (void *)(&nonzero), sizeof(stack_t *));
          stack_t_push(env->threads, (void *)(&zero), sizeof(stack_t *));
          env->thr = STACK(zero, thread_t *);
          env->n_thr = STACK_SIZE(zero, thread_t *);
        } else
          env->virtual_grps += 2;
      } break;
      case JOIN: {
        if (env->n_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->virtual_grps > 0)
          env->virtual_grps--;
        else {
          for (int t = 0; t < env->n_thr; t++) thread_t_delete(env->thr[t]);
          int n_grps = STACK_SIZE(env->threads, stack_t *) - 1;
          stack_t_delete(STACK(env->threads, stack_t *)[n_grps]);
          stack_t *tmp;
          stack_t_pop(env->threads, &tmp, sizeof(stack_t *));
          env->thr = STACK(STACK(env->threads, stack_t *)[n_grps - 1], thread_t *);
          env->n_thr = STACK_SIZE(STACK(env->threads, stack_t *)[n_grps - 1],
                                  thread_t *);
        }
      } break;

      case JMP:  // jump if nonempty group
        if (env->n_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->n_thr > 0) {
          env->pc += lval(env->code + env->pc, int32_t);
        } else
          env->pc += 4;
        break;

      case CALL:
        if (env->n_thr > 0) {
          env->W++;
          env->T++;
          uint32_t ra = env->pc + 4;

          frame_t *nf = frame_t_new(env->thr[0]->mem->top + env->thr[0]->mem_base);
          nf->ret_addr = ra;
          mem_mark(env->frame, env, env->n_thr, env->thr);

          stack_t_push(env->frames, (void *)&nf, sizeof(frame_t *));
          env->frame = nf;

          env->pc = env->fnmap[lval(env->code + env->pc, uint32_t)];
        }
        break;

      case RETURN:
        if (env->n_thr > 0) {
          env->W++;
          env->T++;

          env->pc = env->frame->ret_addr;

          frame_t *of;
          stack_t_pop(env->frames, (void *)&of, sizeof(frame_t *));
          frame_t_delete(of);

          env->frame = STACK_TOP(env->frames, frame_t *);
          mem_free(env->frame, env, env->n_thr, env->thr);
        }
        break;

      default:
        if (env->n_thr > 0) {
          env->W += env->n_thr;
          env->T++;
        }
        for (int t = 0; t < env->n_thr; t++, env->W++) switch (opcode) {
            case SIZE: {
              uint32_t a, d;
              _POP(a, 4);
              _POP(d, 4);
              uint32_t max = lval(get_addr(env->thr[t], a + 4, 4), uint32_t);
              if (d >= max) {
                fprintf(stderr, "bad array dimension\n");
                exit(1);
              }
              uint32_t size =
                  lval(get_addr(env->thr[t], a + 4 * (d + 2), 4), uint32_t);
              _PUSH(size, 4);
            } break;
            case PUSHC:
              _PUSH(env->code[env->pc], 4);
              break;

            case PUSHB: {
              uint32_t v = lval(env->code + env->pc, uint8_t);
              _PUSH(v, 4);
            } break;

            case FBASE:
              _PUSH(env->frame->base, 4);
              break;

            case ALLOC: {
              uint32_t c;
              _POP(c, 4);
              _PUSH(env->heap->top, 4);
              stack_t_alloc(env->heap, c);
            } break;

            case LDC: {
              uint32_t a;
              _POP(a, 4);
              stack_t_push(env->thr[t]->op_stack, get_addr(env->thr[t], a, 4), 4);
            } break;

            case LDB: {
              uint32_t a;
              _POP(a, 4);
              int32_t w = lval(get_addr(env->thr[t], a, 1), uint8_t);
              _PUSH(w, 4);
            } break;

            case STC: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              lval(get_addr(env->thr[t], a, 4), int32_t) = v;
            } break;

            case STB: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              lval(get_addr(env->thr[t], a, 1), uint8_t) = (uint8_t)v;
            } break;

            case LDCH: {
              uint32_t a;
              _POP(a, 4);
              stack_t_push(env->thr[t]->op_stack, (env->heap->data + a), 4);
            } break;

            case LDBH: {
              uint32_t a;
              _POP(a, 4);
              int32_t w = lval(env->heap->data + a, uint8_t);
              _PUSH(w, 4);
            } break;

            case STCH: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              lval(env->heap->data + a, int32_t) = v;
            } break;

            case STBH: {
              uint32_t a;
              int32_t v;
              uint8_t w;
              _POP(a, 4);
              _POP(v, 4);
              w = v;
              lval(env->heap->data + a, int32_t) = w;
            } break;

            case INT2FLOAT: {
              int32_t a;
              float b;
              _POP(a, 4);
              b = a;
              _PUSH(b, 4);
            } break;

            case FLOAT2INT: {
              int32_t a;
              float b;
              _POP(b, 4);
              a = b;
              _PUSH(a, 4);
            } break;

            case IDX: {
              uint8_t nd = lval(&env->code[env->pc], uint8_t);
              uint32_t addr;
              _POP(addr, 4);

              for (int i = 0; i < nd; i++) {
                env->arr_sizes[i] =
                    lval(get_addr(env->thr[t], addr + 4 * (i + 2), 4), uint32_t);
                uint32_t v;
                _POP(v, 4);
                env->arr_offs[i] = v;
                if (v >= env->arr_sizes[i]) {
                  fprintf(stderr, "range check error\n");
                  // dump_memory(env);
                  exit(1);
                }
              }
              uint32_t res = 0;
              for (int i = 0; i < nd; i++)
                res = res * env->arr_sizes[i] + env->arr_offs[i];
              _PUSH(res, 4);
            } break;

            case SWS: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              _PUSH(a, 4);
              _PUSH(b, 4);
            } break;

            case POP: {
              uint32_t tmp;
              _POP(tmp, 4);
            } break;

            case A2S: {
              _PUSH(STACK_TOP(env->thr[t]->acc_stack, int32_t), 4);
            } break;

            case POPA: {
              int32_t val;
              stack_t_pop(env->thr[t]->acc_stack, &val, 4);
            } break;

            case S2A:
              stack_t_push(env->thr[t]->acc_stack,
                           (void *)(&STACK_TOP(env->thr[t]->op_stack, int32_t)), 4);
              break;

            case RVA: {
              int n = env->thr[t]->acc_stack->top / 4;
              for (int i = 0; i < (int)(n / 2); i++) {
                int32_t a = lval(env->thr[t]->acc_stack->data + 4 * i, int32_t);
                lval(env->thr[t]->acc_stack->data + 4 * i, int32_t) =
                    lval(env->thr[t]->acc_stack->data + 4 * (n - i - 1), int32_t);
                lval(env->thr[t]->acc_stack->data + 4 * (n - i - 1), int32_t) = a;
              }
            } break;

            case SWA: {
              int n = env->thr[t]->acc_stack->top / 4;
              int32_t a = lval(env->thr[t]->acc_stack->data + 4 * (n - 2), int32_t);
              lval(env->thr[t]->acc_stack->data + 4 * (n - 2), int32_t) =
                  lval(env->thr[t]->acc_stack->data + 4 * (n - 1), int32_t);
              lval(env->thr[t]->acc_stack->data + 4 * (n - 1), int32_t) = a;
            } break;

            case ADD_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a += b;
              _PUSH(a, 4);
            } break;
            case SUB_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a -= b;
              _PUSH(a, 4);
            } break;
            case SUB_FLOAT: {
              float a, b;
              _POP(a, 4);
              _POP(b, 4);
              a -= b;
              _PUSH(a, 4);
            } break;
            case DIV_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a /= b;
              _PUSH(a, 4);
            } break;
            case MOD_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a %= b;
              _PUSH(a, 4);
            } break;
            case MULT_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              b *= a;
              _PUSH(b, 4);
            } break;
            case POW_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              b = ipow(a, b);
              _PUSH(b, 4);
            } break;
            case OR: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a || b);
              _PUSH(a, 4);
            } break;
            case AND: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = a || b;
              _PUSH(a, 4);
            } break;
            case NOT: {
              int32_t a;
              _POP(a, 4);
              a = !a;
              _PUSH(a, 4);
            } break;
            case EQ_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a == b);
              _PUSH(a, 4);
            } break;
            case GT_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a > b);
              _PUSH(a, 4);
            } break;

            case GEQ_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a >= b);
              _PUSH(a, 4);
            } break;
            case LT_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a < b);
              _PUSH(a, 4);
            } break;

            case LEQ_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a = (a <= b);
              _PUSH(a, 4);
            } break;

            default:
              fprintf(
                  stderr,
                  "fatal error, unknown instruction %s (opcode %0x) at %u\n",
                  instr_names[opcode], opcode, env->pc - 1);
              exit(1);
          }
        switch (opcode) {
          case PUSHC:
            env->pc += 4;
            break;
          case PUSHB:
          case IDX:
            env->pc++;
            break;
        }
    }

    if (EXEC_DEBUG) {
      printf("\nthread groups: ");
      for (int i = 0; i < STACK_SIZE(env->threads, stack_t *); i++)
        printf(" %lu ",
               STACK_SIZE(STACK(env->threads, stack_t *)[i], thread_t *));
      printf("\n     env->n_thr=%2d\n", env->n_thr);
      if (env->n_thr > 0) {
        printf("fbase=%d\n", env->frame->base);
        for (int t = 0; t < env->n_thr; t++) {
          printf("mem_base=%d size=%d", env->thr[t]->mem_base, env->thr[t]->mem->top);
          printf("     [");
          for (int i = 0; i < env->thr[t]->op_stack->top; i++)
            printf("%d ", env->thr[t]->op_stack->data[i]);
          printf("]\n");
        }
      } else
        printf("\n");
    }
  }
  return 0;
}
#undef _PUSH
#undef _POP
