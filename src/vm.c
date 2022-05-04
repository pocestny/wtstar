#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errors.h>
#include <hash.h>
#include <reader.h>
#include <vm.h>

static int ___pc___;

static int _tid = 1;
static hash_table_t *_tid2thread = NULL;
int vm_print_colors = 0;

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
      if (x - y > 1e-15) return 1;
      if (x - y < 1e-15) return -1;
      return 0;
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
    exp >>= 1;
    base *= base;
  }
  return result;
}

// ceiling log_2
int ilog2(int n) {
  int pw = 0, res = -1;
  while (n) {
    if (n % 2 != 0) pw++;
    n >>= 1;
    res++;
  }
  if (pw > 1) res++;
  return res;
}

// from http://www.codecodex.com/wiki/Calculate_an_integer_square_root
unsigned long isqrt(unsigned long x) {
  register unsigned long op, res, one;

  op = x;
  res = 0;

  /* "one" starts at the highest power of four <= than the argument. */
  one = 1 << 30; /* second-to-top bit set */
  while (one > op) one >>= 2;

  while (one != 0) {
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
    memset(s->data + s->top, 0, s->size - s->top);
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
  r->bp_hit = 0;
  r->tid = _tid++;
  if (!_tid2thread) _tid2thread = hash_table_t_new(64, NULL);
  hash_put(_tid2thread, r->tid, r);
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

thread_t *get_thread(uint64_t tid) {
  if (!_tid2thread) return NULL;
  return hash_get(_tid2thread, tid);
}

DESTRUCTOR(thread_t) {
  if (r == NULL) return;
  r->refcnt--;
  if (r->refcnt <= 0) {
    if (!_tid2thread) _tid2thread = hash_table_t_new(64, NULL);
    hash_remove(_tid2thread, r->tid);
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

CONSTRUCTOR(virtual_machine_t, uint8_t *in, int len) {
  if (len <= 0)
    return NULL;
  // printf("machine constructor\n");
  ALLOC_VAR(r, virtual_machine_t)

  r->state = VM_READY;
  r->mem_mode = MEM_MODE_CREW;
  r->debug_info = NULL;

  r->heap = stack_t_new();
  r->threads = stack_t_new();
  r->frames = stack_t_new();

  r->W = r->T = r->pc = r->stored_pc = r->virtual_grps = r->last_global_pc = 0;
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
        // printf(">> section header\n");
        uint8_t version;
        GET(uint8_t, version, 1)
        GET(uint32_t, r->global_size, 4)
        stack_t_alloc(main_thread->mem, r->global_size);
        GET(uint8_t, r->mem_mode, 1)
      } break;
      case SECTION_INPUT:
        // printf(">> section input\n");
        GET(uint32_t, r->n_in_vars, 4)
        r->in_vars = (input_layout_item_t *)malloc(r->n_in_vars *
                                                   sizeof(input_layout_item_t));
        for (int i = 0; i < r->n_in_vars; i++) {
          GET(uint32_t, r->in_vars[i].addr, 4)
          GET(uint8_t, r->in_vars[i].num_dim, 4)
          GET(uint8_t, r->in_vars[i].n_elems, 1)
          r->in_vars[i].elems = (uint8_t *)malloc(r->in_vars[i].n_elems);
          for (int j = 0; j < r->in_vars[i].n_elems; j++)
            GET(uint8_t, r->in_vars[i].elems[j], 1);
        }
        break;
      case SECTION_OUTPUT:
        // printf(">> section output\n");
        GET(uint32_t, r->n_out_vars, 4)
        r->out_vars = (input_layout_item_t *)malloc(
            r->n_out_vars * sizeof(input_layout_item_t));
        for (int i = 0; i < r->n_out_vars; i++) {
          GET(uint32_t, r->out_vars[i].addr, 4)
          GET(uint8_t, r->out_vars[i].num_dim, 4)
          GET(uint8_t, r->out_vars[i].n_elems, 1)
          r->out_vars[i].elems = (uint8_t *)malloc(r->out_vars[i].n_elems);
          for (int j = 0; j < r->out_vars[i].n_elems; j++)
            GET(uint8_t, r->out_vars[i].elems[j], 1);
        }
        break;
      case SECTION_FNMAP: {
        // printf(">> section fnmap\n");
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
        // printf(">> section code\n");
        r->code_size = len - pos;
        r->code = (uint8_t *)malloc(len - pos);
        memcpy(r->code, in + pos, len - pos);
        pos = len;
        break;
      case SECTION_DEBUG:
        // printf(">> section debug\n");
        r->debug_info = debug_info_t_new(in, &pos, len);
        if (!r->debug_info) {
          virtual_machine_t_delete(r);
          return NULL;
        }
        break;
    }
  }

  r->bps = hash_table_t_new(32, (void (*)(void *))&breakpoint_t_delete);

  return r;
}

#undef GET

DESTRUCTOR(virtual_machine_t) {
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
  if (r->debug_info) debug_info_t_delete(r->debug_info);
  if (r->bps) hash_table_t_delete(r->bps);
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

static int check_read_mem(virtual_machine_t *env, hash_table_t *mem_used,
                          void *addr) {
  if (env->mem_mode == MEM_MODE_EREW) {
    uint64_t key = (uint64_t)addr;
    if (hash_get(mem_used, key)) {
      throw("read memory access violation");
      env->state = VM_ERROR;
      return 0;
    }
    hash_put(mem_used, key, mem_check_value_t_new(ACCESS_READ, 0));
  }
  return 1;
}

static int check_write_mem(virtual_machine_t *env, hash_table_t *mem_used,
                           void *addr, int32_t value) {
  uint64_t key = (uint64_t)addr;
  mem_check_value_t *data = hash_get(mem_used, key);
  if (data &&
      (env->mem_mode != MEM_MODE_CCRCW || data->value_written != value)) {
    printf("%x %d %d\n", env->mem_mode, data->value_written, value);
    throw("write memory access violation (%d).", ___pc___);
    env->state = VM_ERROR;
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

uint32_t get_nth_dimension_size(thread_t *t, uint32_t base, uint32_t dim) {
      return lval(get_addr(t, base + 4 * (2 + dim), 4), uint32_t);
}

static void mem_mark(frame_t *frame, virtual_machine_t *env, int n_thr,
                     thread_t **thr) {
  stack_t_push(frame->heap_mark, (void *)&(env->heap->top), 4);
  // TODO: assert all threads have the same memtop
  stack_t_push(frame->mem_mark, (void *)&(thr[0]->mem->top), 4);
}

static void mem_free(frame_t *frame, virtual_machine_t *env, int n_thr,
                     thread_t **thr) {
  stack_t_pop(frame->heap_mark, (void *)&(env->heap->top), 4);
  uint32_t memtop;
  stack_t_pop(frame->mem_mark, (void *)&memtop, 4);
  for (int t = 0; t < n_thr; t++) thr[t]->mem->top = memtop;
}

static void perform_join(virtual_machine_t *env) {
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

CONSTRUCTOR(
  breakpoint_t,
  uint32_t id,
  uint32_t bp_pos,
  uint32_t code_pos,
  uint32_t code_size
) {
  ALLOC_VAR(r, breakpoint_t);
  r->id = id;
  r->bp_pos = bp_pos;
  r->code_pos = code_pos;
  r->code_size = code_size;
  return r;
}

DESTRUCTOR(breakpoint_t) {
  if(r == NULL) return;
  free(r);
}

breakpoint_t* get_breakpoint(virtual_machine_t *env, uint32_t bp_pos) {
  return hash_get(env->bps, bp_pos);
}

int add_breakpoint(
  virtual_machine_t *env,
  uint32_t bp_pos,
  const uint8_t *code,
  uint32_t code_size
) {
  uint32_t 
    bp_id = 1000000000 + env->bps->full, // start from big to avoid collision
    code_pos = -1,
    new_size = env->code_size;
  if (code != NULL) {
    if (code[code_size - 1] == ENDVM)
      code_size--;
    if (!(
      code[code_size - 1] == MEM_FREE &&
      code[code_size - 2] != MEM_FREE
    ))
      return -1;

    code_pos = env->code_size;
    new_size = env->code_size + code_size + 1;
    env->code = (uint8_t*) realloc(env->code, new_size);
    memcpy(env->code + code_pos, code, code_size);
    // insert BREAKOUT before MEM_FREE to remember result
    env->code[new_size - 2] = BREAKOUT;
    env->code[new_size - 1] = MEM_FREE;
    code_size++;
  } else {
    if (code_size != 0)
      return -1;
  }

  remove_breakpoint(env, bp_pos);
  env->code[bp_pos] = BREAK;
  env->code_size = new_size;
  breakpoint_t *bp = breakpoint_t_new(bp_id, bp_pos, code_pos, code_size);
  hash_put(env->bps, bp_pos, bp);

  return bp_id;
}

int remove_breakpoint(virtual_machine_t *env, uint32_t bp_pos) {
  breakpoint_t *bp = get_breakpoint(env, bp_pos);
  if(bp == NULL)
    return -1;
  env->code[bp->bp_pos] = NOOP;
  hash_remove(env->bps, bp->id);
  // we do not remove code to enable breakout
  return 0;
}

int enable_breakpoint(virtual_machine_t *env, uint32_t bp_pos, int enabled) {
  breakpoint_t *bp = get_breakpoint(env, bp_pos);
  if(bp == NULL)
    return -1;
  if (enabled)
    env->code[bp->bp_pos] = BREAK;
  else
    env->code[bp->bp_pos] = NOOP;
  return 0;
}

int get_dynamic_bp_id(virtual_machine_t *env, uint32_t bp_pos) {
  breakpoint_t *bp = get_breakpoint(env, bp_pos);
  return bp == NULL ? 0 : bp->id;
}

int execute_breakpoint_condition(virtual_machine_t *env) {
  uint32_t bp_pos = env->pc - 1;
  breakpoint_t *bp = get_breakpoint(env, bp_pos);
  if(!bp) // static breakpoint
    return -10;
  if (bp->code_pos == -1) { // dynamic breakpoint without condition
    uint32_t f = 1;
    for (int t = 0; t < env->n_thr; t++) {
      if (env->thr[t]->returned)
        continue;
      _PUSH(f, 4);
    }
    return -10;
  }
  if (env->a_thr == 0)
    return -10;

  stack_t *active = stack_t_new();
  for (int t = 0; t < env->n_thr; t++) {
    thread_t *thr = env->thr[t];
    if (thr->returned)
      continue;
    thr->refcnt++;
    stack_t_push(active, (void *)(&(thr)), sizeof(thread_t *));
  }
  stack_t_push(env->threads, (void *)(&active), sizeof(stack_t *));
  env->thr = STACK(active, thread_t *);
  env->n_thr = STACK_SIZE(active, thread_t *);
  env->a_thr = env->n_thr;

  int pc = env->pc, stored_pc = env->stored_pc;
  env->pc = bp->code_pos;
  int resp = execute(env, -1, 1, 0); // TODO stop_on_bp set to 1
  env->pc = pc;
  env->stored_pc = stored_pc;

  printf("break execute resp %d\n", resp);
  return resp;
}

int execute(virtual_machine_t *env, int limit, int trace_on, int stop_on_bp) {
  while (1) {
    uint8_t opcode = lval(env->code + env->pc, uint8_t);
    if (limit > 0) limit--;
    if (limit == 0) return 0;
    if (trace_on) {
      printf("\n");
      if (env->debug_info) {
        int i = code_map_find(env->debug_info->source_items_map, env->pc);
        if (i > -1) {
          int it = env->debug_info->source_items_map->val[i];
          if (it > -1) {
            item_info_t *item = &env->debug_info->items[it];
            printf("%s:%d,%d ", env->debug_info->files[item->fileid], item->fl,
                   item->fc);
          }
        }
      }
      printf("%3d: %s", env->pc, instr_names[opcode]);
      switch (opcode) {
        case PUSHC:
        case JMP:
        case JOIN_JMP:
          printf(" %d", lval(&env->code[env->pc + 1], int32_t));
          break;
        case CALL:
          printf(" %d", lval(&env->code[env->pc + 1], uint32_t));
          break;
        case PUSHB:
        case IDX:
          printf(" %d", lval(&env->code[env->pc + 1], uint8_t));
          break;
        case ENDVM:
          printf("ENDVM\n");
          printf("\n\n");
          break;
      }
      printf("\n");
    }
    int res = instruction(env, stop_on_bp);
    if (res < 0) return res;  // error/ENDVM
    if (res > 0) return res;  // breakpoint

    if (trace_on) {
      printf("thread groups: ");
      for (int i = 0; i < STACK_SIZE(env->threads, stack_t *); i++)
        printf(" %lu ",
               STACK_SIZE(STACK(env->threads, stack_t *)[i], thread_t *));
      printf("\nenv->n_thr=%2d env->a_thr=%2d env->virtual_grps=%d\n",
             env->n_thr, env->a_thr, env->virtual_grps);
      if (env->a_thr > 0) {
        printf("fbase=%d\n", env->frame->base);
        for (int t = 0; t < env->n_thr; t++) {
          printf("mem_base=%d size=%d parent=%lu", env->thr[t]->mem_base,
                 env->thr[t]->mem->top, (unsigned long)env->thr[t]->parent);
          printf("     [");
          for (int i = 0; i < env->thr[t]->op_stack->top; i++)
            printf("%d ", env->thr[t]->op_stack->data[i]);
          printf("]\n");
        }
        printf("W=%d T=%d\n\n", env->W, env->T);
      } else
        printf("\n");
    }
  }  // end of while
}

int instruction(virtual_machine_t *env, int stop_on_bp) {
  ___pc___ = env->pc;
  env->stored_pc = env->pc;
  if (env->frame->base == 0) env->last_global_pc = env->pc;

  env->state = VM_RUNNING;
  for (int t = 0; t < env->n_thr; t++) env->thr[t]->bp_hit = 0;
  uint8_t opcode = lval(env->code + env->pc, uint8_t);
  printf("%3d: %s\n", env->pc, instr_names[opcode]);
  if (opcode == ENDVM) {
    env->state = VM_OK;
    return -1;
  }
  env->pc++;

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
    case NOOP:
      break;
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
            uint32_t a;
            int32_t n;
            _POP(a, 4);
            _POP(n, 4);
            // is this really needed?
            /*
            if (n <= 0) {
              throw("no threads to  FORK\n");
              return -1;
            }
            */
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
        stack_t *nonzero = stack_t_new();
        stack_t *zero = stack_t_new();
        for (int t = 0; t < env->n_thr; t++)
          if (!env->thr[t]->returned) {
            int32_t a;
            _POP(a, 4);
            env->thr[t]->refcnt++;
            if (a == 0)
              stack_t_push(zero, (void *)(&(env->thr[t])), sizeof(thread_t *));
            else
              stack_t_push(nonzero, (void *)(&(env->thr[t])),
                           sizeof(thread_t *));
          }
        stack_t_push(env->threads, (void *)(&zero), sizeof(stack_t *));
        stack_t_push(env->threads, (void *)(&nonzero), sizeof(stack_t *));
        env->thr = STACK(nonzero, thread_t *);
        env->n_thr = STACK_SIZE(nonzero, thread_t *);
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
        for (int t = 0; t < env->n_thr; t++) env->thr[t]->returned = 1;
        env->a_thr = 0;
        for (int t = 0; t < env->n_thr; t++) // TODO wtf, this is redundant
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
      } else
        env->pc += 4;
      break;

    case BREAK: {
      uint32_t bp_pos = env->pc - 1;
      int bp_id = get_dynamic_bp_id(env, bp_pos);
      if(bp_id) {
        int resp = execute_breakpoint_condition(env);
        if(resp != -10)
          return resp;
      } else
        bp_id = bp_pos;
      int hits = 0;
      for (int t = 0; t < env->n_thr; t++) {
        if (env->thr[t]->returned)
          continue;
        uint32_t f;
        _POP(f, 4);
        if (f) {
          /*printf("breakpoint %d hit in thread %d\n",
                  lval(env->code + env->pc, uint32_t), env->thr[t]->tid);*/
          env->thr[t]->bp_hit = 1;
          hits++;
        }
      }
      if (hits && (stop_on_bp & 1)) {
        // if (env->pc < env->code_size &&
        //     lval(env->code + env->pc, uint8_t) == STEP_IN)
        //   env->pc += 1;
        return bp_id;
      }
    } break;

    case BREAKOUT: {
      uint32_t *fs = malloc(sizeof(uint32_t) * env->n_thr);
      uint32_t f;
      for (int t = 0; t < env->n_thr; t++) {
        if (!env->thr[t]->returned) {
          fs[t] = 0;
          printf("breakpoint did not return in thread %d\n", t);
        } else {
          _POP(f, 4); // take result from stack
          fs[t] = f;
          env->thr[t]->returned = 0;
          printf("breakout hit in thread %d with value %d\n", t, f);
        }
      }
      instruction(env, 0); // execute final MEM_FREE
      perform_join(env);
      uint ai = 0;
      for (int t = 0; t < env->n_thr; t++) {
        if (env->thr[t]->returned)
          continue;
        f = fs[ai++];
        _PUSH(f, 4); // push result back on stack
      }
      free(fs);
      return -10;
    } break;

    case STEP_IN: {
      if (stop_on_bp & 2)
        return -11;
    } break;

    case STEP_OUT: {
      if (stop_on_bp & 4)
        return -11;
    } break;

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

            case FBASE: {
              uint32_t a;
              _POP(a, 4);
              a += env->frame->base;
              _PUSH(a, 4);
            } break;

            case SIZE: {
              uint32_t a, d;
              _POP(a, 4);
              _POP(d, 4);
              uint32_t max = lval(get_addr(env->thr[t], a + 4, 4), uint32_t);
              if (d >= max) {
                throw("bad array dimension\n");
                env->state = VM_ERROR;
                return -2;
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
              if (!check_read_mem(env, mem_used, addr)) return -5;
            } break;

            case LDB: {
              uint32_t a;
              _POP(a, 4);
              void *addr = get_addr(env->thr[t], a, 1);
              int32_t w = lval(addr, uint8_t);
              _PUSH(w, 4);
              if (!check_read_mem(env, mem_used, addr)) return -5;
            } break;

            case STC: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              void *addr = get_addr(env->thr[t], a, 4);
              lval(addr, int32_t) = v;
              if (!check_write_mem(env, mem_used, addr, v)) return -5;
            } break;

            case STB: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              void *addr = get_addr(env->thr[t], a, 1);
              lval(addr, uint8_t) = (uint8_t)v;
              if (!check_write_mem(env, mem_used, addr, v)) return -5;
            } break;

            case LDCH: {
              uint32_t a;
              _POP(a, 4);
              void *addr = (void *)(env->heap->data + a);
              stack_t_push(env->thr[t]->op_stack, addr, 4);
              if (!check_read_mem(env, mem_used, addr)) return -5;
            } break;

            case LDBH: {
              uint32_t a;
              _POP(a, 4);
              void *addr = (void *)(env->heap->data + a);
              int32_t w = lval(addr, uint8_t);
              _PUSH(w, 4);
              if (!check_read_mem(env, mem_used, addr)) return -5;
            } break;

            case STCH: {
              uint32_t a;
              int32_t v;
              _POP(a, 4);
              _POP(v, 4);
              void *addr = (void *)(env->heap->data + a);
              lval(addr, int32_t) = v;
              if (!check_write_mem(env, mem_used, addr, v)) return -5;
            } break;

            case STBH: {
              uint32_t a;
              int32_t v;
              uint8_t w;
              _POP(a, 4);
              _POP(v, 4);
              w = v;
              void *addr = (void *)(env->heap->data + a);
              lval(addr, int32_t) = w;
              if (!check_write_mem(env, mem_used, addr, v)) return -5;
            } break;

            case IDX: {
              uint8_t nd = lval(&env->code[env->pc], uint8_t);
              uint32_t addr;
              _POP(addr, 4);
              uint32_t nd2 = lval(get_addr(env->thr[t], addr + 4, 4), uint32_t);
              if (nd != nd2) {
                throw("mismatch in dimensions %d %d (%d)", nd, nd2, ___pc___);
                env->state = VM_ERROR;
                return -3;
              }

              for (int i = 0; i < nd; i++) {
                env->arr_sizes[i] = lval(
                    get_addr(env->thr[t], addr + 4 * (i + 2), 4), uint32_t);
                uint32_t v;
                _POP(v, 4);
                env->arr_offs[i] = v;
                if (v >= env->arr_sizes[i]) {
                  throw("range check error %d (%d).", addr, ___pc___);
                  env->state = VM_ERROR;
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
              stack_t_push(env->thr[t]->acc_stack,
                           (void *)(&STACK_TOP(env->thr[t]->op_stack, int32_t)),
                           4);
              break;

            case RVA: {
              int n = env->thr[t]->acc_stack->top / 4;
              for (int i = 0; i < (int)(n / 2); i++) {
                int32_t a = lval(env->thr[t]->acc_stack->data + 4 * i, int32_t);
                lval(env->thr[t]->acc_stack->data + 4 * i, int32_t) = lval(
                    env->thr[t]->acc_stack->data + 4 * (n - i - 1), int32_t);
                lval(env->thr[t]->acc_stack->data + 4 * (n - i - 1), int32_t) =
                    a;
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
              if (!b) {
                throw("division by zero");
                return -6;
              }
              a /= b;
              _PUSH(a, 4);
            } break;

            case MOD_INT: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              if (!b) {
                throw("modulo by zero");
                return -6;
              }
              a %= b;
              _PUSH(a, 4);
            } break;

            case BIT_AND: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a &= b;
              _PUSH(a, 4);
            } break;

            case BIT_OR: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a |= b;
              _PUSH(a, 4);
            } break;

            case BIT_XOR: {
              int32_t a, b;
              _POP(a, 4);
              _POP(b, 4);
              a ^= b;
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
              if (!b) {
                throw("division by zero");
                return -6;
              }
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
              if (a != 0)
                while (a % 2 == 0) {
                  b++;
                  a >>= 1;
                }
              _PUSH(b, 4);
            } break;

            case LOGF: {
              float a;
              _POP(a, 4);
              a = logf(a) / logf(2);
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
              if (b * b != a) b++;
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
              if (!check_write_mem(env, mem_used, base, 1)) return -5;
              qsort(base, n, size, sort_compare);
            } break;

            default:
              throw("unknown instruction %s (opcode %0x) at %u\n",
                    instr_names[opcode], opcode, env->pc - 1);
              env->state = VM_ERROR;
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

  return 0;
}
#undef _PUSH
#undef _POP

void print_types(writer_t *w, virtual_machine_t *env) {
  if (!env->debug_info) return;
  // if (env->debug_info->n_types <= 4) return;
  out_text(w, "types [%d]:\n", env->debug_info->n_types);
  for (int i = 0; i < env->debug_info->n_types; i++) {
    int nm = env->debug_info->types[i].n_members;
    if (nm == 0) continue;  // don't write basic types (?)
    out_text(w, "  %s ", env->debug_info->types[i].name);
    if (nm > 0) out_text(w, " : {");
    for (int j = 0; j < nm; j++)
      out_text(w, "%c %s %s", (j == 0) ? ' ' : ',',
               env->debug_info->types[env->debug_info->types[i].member_types[j]]
                   .name,
               env->debug_info->types[i].member_names[j]);
    if (nm > 0) out_text(w, " }");
    out_text(w, "\n");
  }
}

void print_var_name(writer_t *w, virtual_machine_t *env, int addr) {
  if (!env->debug_info) return;
  for (int j = 0; j < env->debug_info->scopes[0].n_vars; j++)
    if (env->debug_info->scopes[0].vars[j].addr == addr) {
      out_text(
          w, "  %s %s",
          env->debug_info->types[env->debug_info->scopes[0].vars[j].type].name,
          env->debug_info->scopes[0].vars[j].name);
      int nd = env->debug_info->scopes[0].vars[j].num_dim;
      if (nd > 0) {
        out_text(w, "[");
        for (int i = 0; i < nd; i++) {
          if (i > 0) out_text(w, ",");
          out_text(w, "_");
        }
        out_text(w, "]");
      }
      break;
    }
}

void print_var_layout(writer_t *w, input_layout_item_t *it) {
  for (int j = 0; j < it->n_elems; j++) switch (it->elems[j]) {
      case TYPE_INT:
        out_text(w, "int ");
        break;
      case TYPE_FLOAT:
        out_text(w, "float ");
        break;
      case TYPE_CHAR:
        out_text(w, "char ");
        break;
    }
}

void print_io_vars(writer_t *w, virtual_machine_t *env, int n,
                   input_layout_item_t *vars) {
  for (int i = 0; i < n; i++) {
    out_text(w, "%010u (%08x) ", vars[i].addr, vars[i].addr);
    if (env->debug_info)
      print_var_name(w, env, vars[i].addr);
    else {
      if (vars[i].num_dim > 0)
        out_text(w, "(%d)", vars[i].num_dim);
      else
        out_text(w, "   ");
    }
    out_text(w, " : ");
    print_var_layout(w, &vars[i]);
    out_text(w, "\n");
  }
}

void print_root_vars(writer_t *w, virtual_machine_t *env) {
  if (!env->debug_info) return;
  for (int j = 0; j < env->debug_info->scopes[0].n_vars; j++) {
    int addr = env->debug_info->scopes[0].vars[j].addr;
    char c = ' ';
    for (int i = 0; i < env->n_in_vars; i++)
      if (env->in_vars[i].addr == addr) c = 'i';
    for (int i = 0; i < env->n_out_vars; i++)
      if (env->out_vars[i].addr == addr) c = 'o';
    out_text(w, " %010u (%08x) %c", addr, addr, c);
    print_var_name(w, env, addr);
    out_text(w, "\n");
  }
}

int count_size(input_layout_item_t *var) {
  int n = 0;
  for (int i = 0; i < var->n_elems; i++) switch (var->elems[i]) {
      case TYPE_INT:
        n += 4;
        break;
      case TYPE_FLOAT:
        n += 4;
        break;
      case TYPE_CHAR:
        n += 4;
        break;
    }
  return n;
}

void print_code(writer_t *w, uint8_t *code, int size) {
  int n_instr = 0;
  for (; strcmp(instr_names[n_instr], "???"); n_instr++)
    ;
  for (int i = 0; i < size; i++) {
    int instr = code[i];
    if (instr > n_instr) {
      out_text(w, "???\n");
      continue;
    }
    out_text(w, "%010d (%08x) %s", i, i, instr_names[instr]);
    switch (instr) {
      case PUSHC:
      case JMP:
      case JOIN_JMP:
        out_text(w, " %d", lval(&code[i + 1], int32_t));
        i += 4;
        break;
      case CALL:
        out_text(w, " %d", lval(&code[i + 1], uint32_t));
        i += 4;
        break;
      case PUSHB:
      case IDX:
        out_text(w, " %d", lval(&code[i + 1], uint8_t));
        i += 1;
        break;
    }
    out_text(w, "\n");
  }
}

int read_var(reader_t *r, uint8_t *base, input_layout_item_t *var) {
  int offs = 0;
  int res;

  if (var->n_elems > 1)
    for (char c = '0'; c != '{';) {
      in_text(r, res, "%c", &c);
      if (res != 1) {
        throw("wrong input");
        return -1;
      }
    }

  for (int i = 0; i < var->n_elems; i++) switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x;
        in_text(r, res, "%d", &x);
        if (res != 1) {
          throw("wrong input");
          return -1;
        }
        lval(base + offs, uint32_t) = x;
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x;
        in_text(r, res, "%f", &x);
        if (res != 1) {
          throw("wrong input");
          return -1;
        }
        lval(base + offs, float) = x;
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x;
        in_text(r, res, "%c", &x);
        if (res != 1) {
          throw("wrong input");
          return -1;
        }
        lval(base + offs, uint8_t) = x;
        offs += 1;
      } break;
    }

  if (var->n_elems > 1)
    for (char c = '0'; c != '}';) {
      in_text(r, res, "%c", &c);
      if (res != 1) {
        throw("wrong input");
        return -1;
      }
    }
  return 0;
}

int scan_array(reader_t *r, writer_t *w, input_layout_item_t *var, int *sizes,
               int current, int first) {
  int res;
  int elem_size = count_size(var);

  for (char c = '0'; c != '[';) {
    in_text(r, res, "%c", &c);
    if (res != 1) {
      throw("wrong input");
      return -1;
    }
  }

  int cnt = 0;

  do {
    if (current < var->num_dim - 1) {
      if (scan_array(r, w, var, sizes, current + 1,
                     (first && cnt == 0) ? 1 : 0) != 0)
        return -1;
    } else {
      out_text(w, " ");
      uint8_t *buf = malloc(elem_size);
      if (read_var(r, buf, var) != 0) {
        free(buf);
        return -1;
      }
      print_var(w, buf, var);
      out_text(w, " ");
      free(buf);
    }

    cnt++;

    char c = ' ';
    for (; c == ' ' || c == '\n' || c == '\t';) {
      in_text(r, res, "%c", &c);
      if (res != 1) {
        throw("wrong input");
        return -1;
      }
    }
    if (c == ']') {
      if (first == 1)
        sizes[current] = cnt;
      else if (sizes[current] != cnt) {
        throw("wrong input size");
        return -1;
      }

      break;
    }
    in_ungetc(r, c);
  } while (1);
  return 0;
}

int read_input(reader_t *r, virtual_machine_t *env) {
  thread_t *tt = STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0];
  printf("reader %s\n", r->str.base);
  for (int i = 0; i < env->n_in_vars; i++) {
    input_layout_item_t *var = &(env->in_vars[i]);
    int elem_size = count_size(var);

    if (var->num_dim > 0) {
      int sizes[var->num_dim];
      writer_t *w = writer_t_new(WRITER_STRING);
      if (scan_array(r, w, var, sizes, 0, 1) != 0) {
        writer_t_delete(w);
        return -1;
      }

      int n_elem = 1;
      for (int i = 0; i < var->num_dim; i++) n_elem *= sizes[i];

      uint32_t base = env->heap->top;
      stack_t_alloc(env->heap, n_elem * elem_size);

      lval(get_addr(tt, env->in_vars[i].addr, 4), uint32_t) = base;
      lval(get_addr(tt, env->in_vars[i].addr + 4, 4), uint32_t) = var->num_dim;
      for (int j = 0; j < var->num_dim; j++)
        lval(get_addr(tt, env->in_vars[i].addr + 4 * (j + 2), 4), uint32_t) =
            (uint32_t)(sizes[j]);

      reader_t *rw = reader_t_new(READER_STRING, w->str.base);
      for (int j = 0; j < n_elem; j++)
        if (read_var(rw, env->heap->data + base + j * elem_size, var) != 0) {
          writer_t_delete(w);
          reader_t_delete(rw);
          return -1;
        }

      reader_t_delete(rw);
      writer_t_delete(w);
    } else {
      if (read_var(r, get_addr(tt, var->addr, elem_size), var) != 0) return -1;
    }
  }
  return 0;
}

void print_var(writer_t *w, uint8_t *addr, input_layout_item_t *var) {
  int offs = 0;
  if (var->n_elems > 1) out_text(w, "{ ");
  for (int i = 0; i < var->n_elems; i++) {
    if (i > 0) out_text(w, " ");
    switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x = lval(addr + offs, uint32_t);
        out_text(w, "%d", x);
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x = lval(addr + offs, float);
        out_text(w, "%f", x);
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x = lval(addr + offs, uint8_t);
        out_text(w, "%c", x);
        offs += 1;
      } break;
      default:
        out_text(w, "????");
    }
  }
  if (var->n_elems > 1) out_text(w, " }");
  // out_text(w," ;");
}

void print_array(writer_t *w, virtual_machine_t *env, input_layout_item_t *var,
                 int nd, int *sizes, uint32_t base, int from_dim, int offs) {
  out_text(w, "[");
  if (from_dim == nd - 1) {
    int s = count_size(var);
    for (int i = 0; i < sizes[nd - 1]; i++) {
      if (i > 0) out_text(w, " ");
      print_var(w, env->heap->data + (base + (offs + i) * s), var);
    }
  } else {
    int o = 0;
    for (int i = 0; i < sizes[from_dim]; i++) {
      if (i > 0) out_text(w, " ");
      print_array(w, env, var, nd, sizes, base, from_dim + 1, offs + o);
      o += sizes[from_dim + 1];
    }
  }
  out_text(w, "]");
}

void write_output(writer_t *w, virtual_machine_t *env, int i) {
  input_layout_item_t *var = &env->out_vars[i];
  uint8_t *global_mem =
      STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data;
  uint8_t *base = global_mem + var->addr;
  if (var->num_dim > 0) {
    uint8_t nd = var->num_dim;
    int *sizes = (int *)malloc(nd * sizeof(int));
    for (int j = 0; j < nd; j++)
      sizes[j] = lval(base + 4 * (j + 2), uint32_t);
    print_array(w, env, var, nd, sizes, lval(base, uint32_t), 0, 0);
    free(sizes);
  } else
    print_var(w, base, var);
  out_text(w, "\n");
}

void dump_header(writer_t *w, virtual_machine_t *env) {
  out_text(w, "data segment:       %d B\n", env->global_size);
  out_text(w, "memory mode:        %s\n", mode_name(env->mem_mode));
  out_text(w, "input variables:\n");
  print_io_vars(w, env, env->n_in_vars, env->in_vars);
  out_text(w, "output variables:\n");
  print_io_vars(w, env, env->n_out_vars, env->out_vars);
  out_text(w, "function addresses:\n");
  for (uint32_t i = 0; i < env->fcnt; i++) {
    out_text(w, "%03d %010u (%08x)", i, env->fnmap[i], env->fnmap[i]);
    if (env->debug_info) {
      out_text(w, " %s (%s:%d.%d)", env->debug_info->fn_names[i],
               env->debug_info
                   ->files[env->debug_info->items[env->debug_info->fn_items[i]]
                               .fileid],
               env->debug_info->items[env->debug_info->fn_items[i]].fl,
               env->debug_info->items[env->debug_info->fn_items[i]].fc);
    }
    out_text(w, "\n");
  }
}

void dump_debug_info(writer_t *w, virtual_machine_t *env) {
  if (!env->debug_info) return;
  out_text(w, "source files:\n");
  for (int i = 0; i < env->debug_info->n_files; i++)
    out_text(w, "  %s\n", env->debug_info->files[i]);
  print_types(w, env);
  out_text(w, "root variables:\n");
  print_root_vars(w, env);
}

char *mode_name(int mode) {
  static char *erew = "EREW";
  static char *crew = "CREW";
  static char *ccrcw = "cCRCW";

  switch (mode) {
    case MEM_MODE_EREW:
      return erew;
    case MEM_MODE_CREW:
      return crew;
    case MEM_MODE_CCRCW:
      return ccrcw;
    default:
      return NULL;
  }
}

void include_layout_type(input_layout_item_t *it, virtual_machine_t *env,
                         int type) {
  type_info_t *t = &(env->debug_info->types[type]);

  if (t->n_members == 0) {
    it->n_elems++;
    it->elems = realloc(it->elems, it->n_elems);
    if (!strcmp(t->name, "int"))
      it->elems[it->n_elems - 1] = TYPE_INT;
    else if (!strcmp(t->name, "float"))
      it->elems[it->n_elems - 1] = TYPE_FLOAT;
    else if (!strcmp(t->name, "char"))
      it->elems[it->n_elems - 1] = TYPE_CHAR;
    else
      exit(123);
  } else
    for (int m = 0; m < t->n_members; m++)
      include_layout_type(it, env, t->member_types[m]);
}

input_layout_item_t get_layout(variable_info_t *var, virtual_machine_t *env) {
  input_layout_item_t r;
  r.addr = var->addr;
  r.num_dim = var->num_dim;
  r.n_elems = 0;
  r.elems = NULL;
  include_layout_type(&r, env, var->type);
  return r;
}
