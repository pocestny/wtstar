#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <errors.h>
#include <hash.h>
#include <vm.h>

#define error(...)                      \
  {                                     \
    error_t *err = error_t_new();       \
    append_error_msg(err, __VA_ARGS__); \
    emit_error(err);                    \
  }

extern int EXEC_DEBUG;
#include <stdio.h>  // for debug

extern const char *const instr_names[];

// qsort_r has some portability issues, so just use global var instead
typedef struct {
  uint32_t offs, type;
} sort_param_t;

static sort_param_t sort_param;

static int sort_compare(const void *a, const void *b) {
  sort_param_t *p = &sort_param;
  uint8_t *A = ((uint8_t *)a) + p->offs;
  uint8_t *B = ((uint8_t *)b) + p->offs;

  switch (p->type) {
    case TYPE_INT: {
      int32_t x = *(int32_t *)(A);
      int32_t y = *(int32_t *)(B);
      return x - y;
    } break;
    case TYPE_FLOAT: {
      float x = *(float *)(A);
      float y = *(float *)(B);
      return x - y;
    } break;
    case TYPE_CHAR: {
      int8_t x = *(int8_t *)(A);
      int8_t y = *(int8_t *)(B);
      return x - y;
    } break;
  };
  return 0;
}

int ipow(int base, int exp) {
  int result = 1;
  while (exp) {
    if (exp & 1) result *= base;
    exp /= 2;
    base *= base;
  }
  return result;
}

// ceiling log_2
int ilog2(int n) {
  int pw = 0, res = 0;
  while (n) {
    if (n % 2 == 0) pw++;
    n >>= 1;
    res++;
  }
  if (pw == 1) res--;
  return res;
}

// from http://www.codecodex.com/wiki/Calculate_an_integer_square_root
unsigned long isqrt(unsigned long x)
{
    register unsigned long op, res, one;

    op = x;
    res = 0;

    /* "one" starts at the highest power of four <= than the argument. */
    one = 1 << 30;  /* second-to-top bit set */
    while (one > op) one >>= 2;

    while (one!= 0) {
        if (op >= res + one) {
            op -= res + one;
            res += one << 1;  // <-- faster than 2 * one
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
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
  r->returned = 0;
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

  r->mem_mode = MEM_MODE_CREW;

  r->heap = stack_t_new();
  r->threads = stack_t_new();
  r->frames = stack_t_new();

  r->W = r->T = r->pc = r->virtual_grps = 0;
  r->n_thr = r->a_thr = 1;

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
        GET(uint8_t,r->mem_mode,1)
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
          r->fnmap = (fnmap_t *)malloc(r->fcnt * sizeof(fnmap_t));
        else
          r->fnmap = NULL;
        for (uint32_t i = 0; i < r->fcnt; i++) {
          GET(uint32_t, r->fnmap[i].addr, 4);
          GET(int32_t, r->fnmap[i].out_size, 4);
        }

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

#define ACCESS_READ 35
#define ACCESS_WRITE 36
typedef struct {
  uint8_t access;
  int value_written;
} mem_check_value_t;

static mem_check_value_t *mem_check_value_t_new(uint8_t _access,
                                                int _value_written) {
  mem_check_value_t *r = (mem_check_value_t *)malloc(sizeof(mem_check_value_t));
  r->access = _access;
  r->value_written = _value_written;
  return r;
}

static void mem_check_value_deleter(void *a) { free((mem_check_value_t *)a); }

static int check_read_mem(runtime_t *env, hash_table_t *mem_used, void *addr) {
  if (env->mem_mode == MEM_MODE_EREW) {
    uint64_t key = (uint64_t)addr;
    if (hash_get(mem_used, key)) {
      error("read memory access violation");
      return 0;
    }
    hash_put(mem_used, key, mem_check_value_t_new(ACCESS_READ, 0));
  }
  return 1;
}

static int check_write_mem(runtime_t *env, hash_table_t *mem_used, void *addr,
                           int32_t value) {
  uint64_t key = (uint64_t)addr;
  mem_check_value_t *data = hash_get(mem_used, key);
  if (data &&
      (env->mem_mode != MEM_MODE_CCRCW || data->value_written != value)) {
    //printf("%x %d %d\n",env->mem_mode,data->value_written,value);
    error("write memory access violation.");

    return 0;
  }
  hash_put(mem_used, key, mem_check_value_t_new(ACCESS_WRITE, value));
  return 1;
}

#define _PUSH(var, len) \
  stack_t_push(env->thr[t]->op_stack, (void *)(&(var)), len)
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

static void perform_join(runtime_t *env) {
  for (int t = 0; t < env->n_thr; t++) thread_t_delete(env->thr[t]);
  int n_grps = STACK_SIZE(env->threads, stack_t *) - 1;
  stack_t_delete(STACK(env->threads, stack_t *)[n_grps]);
  stack_t *tmp;
  stack_t_pop(env->threads, &tmp, sizeof(stack_t *));
  env->thr = STACK(STACK(env->threads, stack_t *)[n_grps - 1], thread_t *);
  env->n_thr =
      STACK_SIZE(STACK(env->threads, stack_t *)[n_grps - 1], thread_t *);
  env->a_thr = 0;
  for (int t = 0; t < env->n_thr; t++)
    if (!env->thr[t]->returned) env->a_thr++;
}

int execute(runtime_t *env, int limit) {
  if (EXEC_DEBUG) printf("code size: %d\n", env->code_size);

  while (1) {
    if (limit > 0) limit--;
    if (limit == 0) return 1;
    uint8_t opcode = lval(env->code + env->pc, uint8_t);
    if (opcode == ENDVM) break;
    env->pc++;

    if (EXEC_DEBUG) {
      printf("\n%3d: %s", env->pc - 1, instr_names[opcode]);
      switch (opcode) {
        case PUSHC:
        case JMP:
        case JOIN_JMP:
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

    if (opcode == SORT) {
      int max = 0, sum = 0;
      for (int t = 0; t < env->n_thr; t++)
        if (!env->thr[t]->returned) {
          stack_t *s = env->thr[t]->op_stack;
          uint32_t a = lval(s->data + (s->top - 4), uint32_t);
          uint32_t n = lval(get_addr(env->thr[t], a + 8, 4), int32_t);
          if (n > max) max = n;
          sum += n * ilog2(n);
        }
      env->T += ilog2(max);
      env->W += sum;
    }

    switch (opcode) {
      case MEM_MARK:
        if (env->a_thr > 0) mem_mark(env->frame, env, env->n_thr, env->thr);
        break;
      case MEM_FREE:
        if (env->a_thr > 0) mem_free(env->frame, env, env->n_thr, env->thr);
        break;
      case FORK:
        if (env->a_thr > 0) {
          env->W++;
          env->T++;

          stack_t *grp = stack_t_new();

          for (int t = 0; t < env->n_thr; t++)
            if (!env->thr[t]->returned) {
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
          env->a_thr = env->n_thr;
        } else
          env->virtual_grps++;
        break;
      case SPLIT: {
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->a_thr > 0) {
          stack_t *nonzero = stack_t_new();
          stack_t *zero = stack_t_new();
          for (int t = 0; t < env->n_thr; t++)
            if (!env->thr[t]->returned) {
              int32_t a;
              _POP(a, 4);
              env->thr[t]->refcnt++;
              if (a == 0)
                stack_t_push(zero, (void *)(&(env->thr[t])),
                             sizeof(thread_t *));
              else
                stack_t_push(nonzero, (void *)(&(env->thr[t])),
                             sizeof(thread_t *));
            }
          stack_t_push(env->threads, (void *)(&nonzero), sizeof(stack_t *));
          stack_t_push(env->threads, (void *)(&zero), sizeof(stack_t *));
          env->thr = STACK(zero, thread_t *);
          env->n_thr = STACK_SIZE(zero, thread_t *);
          env->a_thr = env->n_thr;
        } else
          env->virtual_grps += 2;
      } break;

      case JOIN: {
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->virtual_grps > 0)
          env->virtual_grps--;
        else
          perform_join(env);
      } break;

      case JOIN_JMP: {
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->virtual_grps > 0)
          env->virtual_grps--;
        else
          perform_join(env);
        env->pc += lval(env->code + env->pc, int32_t);
      } break;

      case SETR: {
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->virtual_grps > 0)
          env->virtual_grps--;
        else {
          for (int t = 0; t < env->n_thr; t++) env->thr[t]->returned = 1;
          env->a_thr = 0;
          for (int t = 0; t < env->n_thr; t++)
            if (!env->thr[t]->returned) env->a_thr++;
        }
      } break;

      case JMP:  // jump if nonempty group
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
        }
        if (env->a_thr > 0) {
          env->pc += lval(env->code + env->pc, int32_t);
        } else
          env->pc += 4;
        break;

      case CALL:
        if (env->a_thr > 0) {
          env->W++;
          env->T++;
          uint32_t ra = env->pc + 4;

          // copy active to new group
          stack_t *grp = stack_t_new();
          for (int t = 0; t < env->n_thr; t++)
            if (!env->thr[t]->returned) {
              env->thr[t]->refcnt++;
              stack_t_push(grp, (void *)(&(env->thr[t])), sizeof(thread_t *));
            }
          stack_t_push(env->threads, (void *)(&grp), sizeof(stack_t *));
          env->thr = STACK(grp, thread_t *);
          env->n_thr = STACK_SIZE(grp, thread_t *);
          env->a_thr = env->n_thr;

          // create new frame
          frame_t *nf =
              frame_t_new(env->thr[0]->mem->top + env->thr[0]->mem_base);
          nf->ret_addr = ra;
          mem_mark(env->frame, env, env->n_thr, env->thr);

          stack_t_push(env->frames, (void *)&nf, sizeof(frame_t *));
          env->frame = nf;
          nf->op_stack_end =
              env->thr[0]->op_stack->top +
              env->fnmap[lval(env->code + env->pc, uint32_t)].out_size;

          // jump
          env->pc = env->fnmap[lval(env->code + env->pc, uint32_t)].addr;
        }
        break;

      case RETURN: {
        env->W += env->n_thr;
        env->T++;

        // fix op_stack
        int should = env->frame->op_stack_end;
        for (int t = 0; t < env->n_thr; t++) {
          uint8_t zero = 0;
          while (env->thr[t]->op_stack->top < should)
            stack_t_push(env->thr[t]->op_stack, (void *)(&zero), 1);
          while (env->thr[t]->op_stack->top > should)
            stack_t_pop(env->thr[t]->op_stack, (void *)(&zero), 1);
        }

        // clear flag and join
        for (int t = 0; t < env->n_thr; t++) env->thr[t]->returned = 0;
        perform_join(env);

        // jump
        env->pc = env->frame->ret_addr;

        // remove frame
        frame_t *of;
        stack_t_pop(env->frames, (void *)&of, sizeof(frame_t *));
        frame_t_delete(of);

        env->frame = STACK_TOP(env->frames, frame_t *);
        mem_free(env->frame, env, env->n_thr, env->thr);
      } break;

      default: {
        if (env->a_thr > 0) {
          env->W += env->a_thr;
          env->T++;
        }

        hash_table_t *mem_used =
            hash_table_t_new(env->a_thr, mem_check_value_deleter);

        for (int t = 0; t < env->n_thr; t++)
          if (!env->thr[t]->returned) switch (opcode) {
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

              case SIZE: {
                uint32_t a, d;
                _POP(a, 4);
                _POP(d, 4);
                uint32_t max = lval(get_addr(env->thr[t], a + 4, 4), uint32_t);
                if (d >= max) {
                  error("bad array dimension\n");
                  return -1;
                }
                uint32_t size =
                    lval(get_addr(env->thr[t], a + 4 * (d + 2), 4), uint32_t);
                _PUSH(size, 4);
              } break;

              case LDC: {
                uint32_t a;
                _POP(a, 4);
                void *addr = get_addr(env->thr[t], a, 4);
                stack_t_push(env->thr[t]->op_stack, addr, 4);
                check_read_mem(env, mem_used, addr);
              } break;

              case LDB: {
                uint32_t a;
                _POP(a, 4);
                void *addr = get_addr(env->thr[t], a, 1);
                int32_t w = lval(addr, uint8_t);
                _PUSH(w, 4);
                check_read_mem(env, mem_used, addr);
              } break;

              case STC: {
                uint32_t a;
                int32_t v;
                _POP(a, 4);
                _POP(v, 4);
                void *addr = get_addr(env->thr[t], a, 4);
                lval(addr, int32_t) = v;
                check_write_mem(env, mem_used, addr, v);
              } break;

              case STB: {
                uint32_t a;
                int32_t v;
                _POP(a, 4);
                _POP(v, 4);
                void *addr = get_addr(env->thr[t], a, 1);
                lval(addr, uint8_t) = (uint8_t)v;
                check_write_mem(env, mem_used, addr, v);
              } break;

              case LDCH: {
                uint32_t a;
                _POP(a, 4);
                void *addr=(void*)(env->heap->data + a);
                stack_t_push(env->thr[t]->op_stack, addr, 4);
                check_read_mem(env, mem_used, addr);
              } break;

              case LDBH: {
                uint32_t a;
                _POP(a, 4);
                void *addr=(void*)(env->heap->data + a);
                int32_t w = lval(addr, uint8_t);
                _PUSH(w, 4);
                check_read_mem(env, mem_used, addr);
              } break;

              case STCH: {
                uint32_t a;
                int32_t v;
                _POP(a, 4);
                _POP(v, 4);
                void *addr=(void*)(env->heap->data + a);
                lval(addr, int32_t) = v;
                check_write_mem(env, mem_used, addr, v);
              } break;

              case STBH: {
                uint32_t a;
                int32_t v;
                uint8_t w;
                _POP(a, 4);
                _POP(v, 4);
                w = v;
                void *addr=(void*)(env->heap->data + a);
                lval(addr, int32_t) = w;
                check_write_mem(env, mem_used, addr, v);
              } break;

              case IDX: {
                uint8_t nd = lval(&env->code[env->pc], uint8_t);
                uint32_t addr;
                _POP(addr, 4);

                for (int i = 0; i < nd; i++) {
                  env->arr_sizes[i] = lval(
                      get_addr(env->thr[t], addr + 4 * (i + 2), 4), uint32_t);
                  uint32_t v;
                  _POP(v, 4);
                  env->arr_offs[i] = v;
                  if (v >= env->arr_sizes[i]) {
                    error("range check error\n");
                    return -2;
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
                stack_t_push(
                    env->thr[t]->acc_stack,
                    (void *)(&STACK_TOP(env->thr[t]->op_stack, int32_t)), 4);
                break;

              case RVA: {
                int n = env->thr[t]->acc_stack->top / 4;
                for (int i = 0; i < (int)(n / 2); i++) {
                  int32_t a =
                      lval(env->thr[t]->acc_stack->data + 4 * i, int32_t);
                  lval(env->thr[t]->acc_stack->data + 4 * i, int32_t) = lval(
                      env->thr[t]->acc_stack->data + 4 * (n - i - 1), int32_t);
                  lval(env->thr[t]->acc_stack->data + 4 * (n - i - 1),
                       int32_t) = a;
                }
              } break;

              case SWA: {
                int n = env->thr[t]->acc_stack->top / 4;
                int32_t a =
                    lval(env->thr[t]->acc_stack->data + 4 * (n - 2), int32_t);
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

              case MULT_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                b *= a;
                _PUSH(b, 4);
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

              case ADD_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                a += b;
                _PUSH(a, 4);
              } break;

              case SUB_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                a -= b;
                _PUSH(a, 4);
              } break;

              case MULT_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                b *= a;
                _PUSH(b, 4);
              } break;

              case DIV_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                a /= b;
                _PUSH(a, 4);
              } break;

              case POW_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                b = ipow(a, b);
                _PUSH(b, 4);
              } break;

              case POW_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                b = pow(a, b);
                _PUSH(b, 4);
              } break;

              case NOT: {
                int32_t a;
                _POP(a, 4);
                a = !a;
                _PUSH(a, 4);
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
                a = a && b;
                _PUSH(a, 4);
              } break;

              case EQ_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                a = (a == b);
                _PUSH(a, 4);
              } break;

              case EQ_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                int32_t c = (a == b);
                _PUSH(c, 4);
              } break;

              case GT_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                a = (a > b);
                _PUSH(a, 4);
              } break;

              case GT_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                int32_t c = (a > b);
                _PUSH(c, 4);
              } break;

              case GEQ_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                a = (a >= b);
                _PUSH(a, 4);
              } break;

              case GEQ_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                int32_t c = (a >= b);
                _PUSH(c, 4);
              } break;

              case LT_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                a = (a < b);
                _PUSH(a, 4);
              } break;

              case LT_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                int32_t c = (a < b);
                _PUSH(c, 4);
              } break;

              case LEQ_INT: {
                int32_t a, b;
                _POP(a, 4);
                _POP(b, 4);
                a = (a <= b);
                _PUSH(a, 4);
              } break;

              case LEQ_FLOAT: {
                float a, b;
                _POP(a, 4);
                _POP(b, 4);
                int32_t c = (a <= b);
                _PUSH(c, 4);
              } break;

              case ALLOC: {
                uint32_t c;
                _POP(c, 4);
                _PUSH(env->heap->top, 4);
                stack_t_alloc(env->heap, c);
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

              case LAST_BIT: {
                int32_t a, b = 0;
                _POP(a, 4);
                while (a % 2 == 0) {
                  b++;
                  a >>= 1;
                }
                _PUSH(b, 4);
              } break;

              case LOGF: {
                float a;
                _POP(a, 4);
                a = logf(a)/logf(2);
                _PUSH(a, 4);
              } break;

              case LOG: {
                int32_t a;
                _POP(a, 4);
                a = ilog2(a);
                _PUSH(a, 4);
              } break;
                        
              case SQRT: {
                int32_t a;
                _POP(a, 4);
                int32_t b = isqrt(a);
                if (b*b!=a) b++; 
                _PUSH(b, 4);
              } break;
                        
              case SQRTF: {
                float a;
                _POP(a, 4);
                a = sqrtf(a);
                _PUSH(a, 4);
              } break;

              case SORT: {
                uint32_t a, size, offs, type;
                _POP(a, 4);
                _POP(size, 4);
                _POP(offs, 4);
                _POP(type, 4);
                sort_param.offs = offs;
                sort_param.type = type;
                uint32_t n = lval(get_addr(env->thr[t], a + 8, 4), uint32_t);
                uint32_t addr = lval(get_addr(env->thr[t], a, 4), uint32_t);
                void *base = (void *)(env->heap->data + addr);
                check_write_mem(env, mem_used, base, 1);
                
                qsort(base, n, size, sort_compare);
              } break;

              default:
                error("unknown instruction %s (opcode %0x) at %u\n",
                      instr_names[opcode], opcode, env->pc - 1);
                return -3;
            }  // end switch opcode for each thread
        hash_table_t_delete(mem_used);
      }  // end case per-thread instruction
        switch (opcode) {
          case PUSHC:
            env->pc += 4;
            break;
          case PUSHB:
          case IDX:
            env->pc++;
            break;
        }
    }  // end process operation

    if (EXEC_DEBUG) {
      printf("\nthread groups: ");
      for (int i = 0; i < STACK_SIZE(env->threads, stack_t *); i++)
        printf(" %lu ",
               STACK_SIZE(STACK(env->threads, stack_t *)[i], thread_t *));
      printf("\nenv->n_thr=%2d env->a_thr=%2d\n", env->n_thr, env->a_thr);
      if (env->a_thr > 0) {
        printf("fbase=%d\n", env->frame->base);
        for (int t = 0; t < env->n_thr; t++) {
          printf("mem_base=%d size=%d", env->thr[t]->mem_base,
                 env->thr[t]->mem->top);
          printf("     [");
          for (int i = 0; i < env->thr[t]->op_stack->top; i++)
            printf("%d ", env->thr[t]->op_stack->data[i]);
          printf("]\n");
          printf("W=%d T=%d\n\n", env->W, env->T);
        }
      } else
        printf("\n");
    }
  }  // end main loop
  return 0;
}
#undef _PUSH
#undef _POP
