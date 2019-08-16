#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "vm.h"

int EXEC_DEBUG = 0;

extern const char *const instr_names[];

int print_file = 0, print_io = 0, dump_heap=0;
char *inf;

void print_io_vars(int n, input_layout_item_t *vars) {
  for (int i = 0; i < n; i++) {
    printf("%04d %c : ", vars[i].addr, ((vars[i].num_dim > 0) ? 'A' : ' '));
    for (int j = 0; j < vars[i].n_elems; j++) switch (vars[i].elems[j]) {
        case TYPE_INT:
          printf("int ");
          break;
        case TYPE_FLOAT:
          printf("float ");
          break;
        case TYPE_CHAR:
          printf("char ");
          break;
      }
    printf("\n");
  }
}

void print_code(uint8_t *code, int size) {
  int n_instr = 0;
  for (; strcmp(instr_names[n_instr], "???"); n_instr++)
    ;
  for (int i = 0; i < size; i++) {
    int instr = code[i];
    if (instr > n_instr) {
      printf("???\n");
      continue;
    }
    printf("%04d %s", i, instr_names[instr]);
    switch (instr) {
      case PUSHC:
      case JMP:
        printf(" %d", lval(&code[i + 1], int32_t));
        i += 4;
        break;
      case CALL:
        printf(" %d", lval(&code[i + 1], uint32_t));
        i += 4;
        break;
      case PUSHB:
      case IDX:
        printf(" %d", lval(&code[i + 1], uint8_t));
        i += 1;
        break;
    }
    printf("\n");
  }
}

void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-i            dump io structure \n");
  printf("-D            dump file and exit\n");
  printf("-m            dump heap after finish\n");
  printf("-t            trace run\n");

  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else if (!strcmp(argv[i], "-D")) {
      print_file = 1;
    } else if (!strcmp(argv[i], "-i")) {
      print_io = 1;
    } else if (!strcmp(argv[i], "-m")) {
      dump_heap = 1;
    } else if (!strcmp(argv[i], "-t")) {
      EXEC_DEBUG = 1;
    } else
      inf = argv[i];
}

void read_var(runtime_t *env, thread_t *tt, input_layout_item_t *var) {
  int offs = 0;
  for (int i = 0; i < var->n_elems; i++) switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x;
        scanf("%d ", &x);
        lval(get_addr(tt, var->addr + offs, 4), uint32_t) = x;
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x;
        scanf("%f ", &x);
        lval(get_addr(tt, var->addr + offs, 4), float) = x;
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x;
        scanf("%c ", &x);
        lval(get_addr(tt, var->addr + offs, 1), uint8_t) = x;
        offs += 1;
      } break;
    }
}

void print_var(runtime_t *env, uint8_t *addr, input_layout_item_t *var) {
  int offs = 0;
  for (int i = 0; i < var->n_elems; i++) {
    if (i > 0) printf(" ");
    switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x = lval(addr + offs, uint32_t);
        printf("%d", x);
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x = lval(addr + offs, float);
        printf("%f", x);
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x = lval(addr + offs, uint8_t);
        printf("%c", x);
        offs += 1;
      } break;
    }
  }
  // printf(" ;");
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

void print_array(runtime_t *env, input_layout_item_t *var, int nd, int *sizes,
                 uint32_t base, int from_dim, int offs) {
  if (from_dim == nd - 1) {
    for (int i = 0; i < sizes[nd - 1]; i++)  // FIXME other than ints
      printf("%d ", lval(env->heap->data + (base + (offs + i) * 4), int32_t));
  } else {
    int o = 0;
    for (int i = 0; i < sizes[from_dim]; i++) {
      print_array(env, var, nd, sizes, base, from_dim + 1, offs + o);
      o += sizes[from_dim + 1];
    }
  }
}

void read_input(runtime_t *env) {
  thread_t *tt = STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0];

  for (int i = 0; i < env->n_in_vars; i++) {
    if (env->in_vars[i].num_dim > 0) {
      // FIXME one-dimensional array of ints only
      int n_elem, elem_size = count_size(&(env->in_vars[i]));
      scanf("%d ", &n_elem);

      uint32_t base = env->heap->top;
      stack_t_alloc(env->heap, n_elem * elem_size);
      uint32_t hdr = env->heap->top;
      stack_t_alloc(env->heap, 11);

      lval(get_addr(tt, env->in_vars[i].addr, 4), uint32_t) = base;
      lval(get_addr(tt, env->in_vars[i].addr + 4, 4), uint32_t) = hdr;

      lval(env->heap->data + hdr, uint8_t) = 1;
      lval(env->heap->data + hdr + 1, uint8_t) = 1;
      lval(env->heap->data + hdr + 2, uint32_t) = 0;
      lval(env->heap->data + hdr + 6, uint32_t) = n_elem - 1;
      lval(env->heap->data + hdr + 10, uint8_t) = 0;

      for (int j = 0; j < n_elem; j++) {
        int x;
        scanf("%d ", &x);
        lval(env->heap->data + base + j * 4, uint32_t) = x;
      }

    } else
      read_var(env, tt, &(env->in_vars[i]));
  }
}

void write_output(runtime_t *env) {
  for (int i = 0; i < env->n_out_vars; i++) {
    if (env->out_vars[i].num_dim > 0) {
      int elem_size = count_size(&(env->out_vars[i]));
      uint8_t *global_mem =
          STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data;
      uint32_t base = lval(global_mem + env->out_vars[i].addr, uint32_t),
               hdr = lval(global_mem + env->out_vars[i].addr + 4, uint32_t);

       //printf(">> base:%u=%u hdr:%u=%u\n",env->out_vars[i].addr,base,env->out_vars[i].addr + 4,hdr);
      uint8_t nd = lval(env->heap->data + hdr, uint8_t);
      int *sizes = (int *)malloc(nd * sizeof(int));
      for (int i = 0; i < nd; i++)
        sizes[i] = lval(env->heap->data + hdr + 2 + 8 * i + 4, uint32_t) + 1;
      print_array(env, &(env->out_vars[i]), nd, sizes, base, 0, 0);
      free(sizes);
    } else
      print_var(
          env,
          STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data +
              env->out_vars[i].addr,
          &(env->out_vars[i]));
    printf("\n");
  }
}


void dump_memory(runtime_t *env) {
  printf("mem (size=%d): ",env->heap->size);
  for (int i = 0; i < env->heap->size; i++) printf("%02u ", env->heap->data[i]);
  printf("\n");
}



int main(int argc, char **argv) {
  inf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  FILE *f = fopen(inf, "rb");
  fseek(f, 0, SEEK_END);
  int len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *in = (uint8_t *)malloc(len);
  fread(in, 1, len, f);
  fclose(f);

  if (in[0] != SECTION_HEADER || in[1] != 1) {
    printf("invalid input file\n");
    exit(1);
  }

  runtime_t *env = runtime_t_new(in, len);
  free(in);

  if (print_file) {
    printf("input variables:\n");
    print_io_vars(env->n_in_vars, env->in_vars);
    printf("\n");
    printf("output variables:\n");
    print_io_vars(env->n_out_vars, env->out_vars);
    printf("\n");
    printf("function addresses:\n");
    for (uint32_t i = 0; i < env->fcnt; i++)
      printf("%3u %05u\n", i, env->fnmap[i]);
    printf("\n");
    printf("code:\n");
    print_code(env->code, env->code_size);
  } else if (print_io) {
    printf("input variables:\n");
    print_io_vars(env->n_in_vars, env->in_vars);
    printf("\n");
    printf("output variables:\n");
    print_io_vars(env->n_out_vars, env->out_vars);
  } else {
    read_input(env);
    int W, T;
    execute(env, &W, &T);
    write_output(env);
    printf("W/T: %d %d\n", W, T);
    if (dump_heap) dump_memory(env); 
  }
}
