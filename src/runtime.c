#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <runtime.h>
#include <reader.h>


extern const char *const instr_names[];

void print_io_vars(writer_t *w, int n, input_layout_item_t *vars) {
  for (int i = 0; i < n; i++) {
    out_text(w,"%04d %c : ", vars[i].addr, ((vars[i].num_dim > 0) ? 'A' : ' '));
    for (int j = 0; j < vars[i].n_elems; j++) switch (vars[i].elems[j]) {
        case TYPE_INT:
          out_text(w,"int ");
          break;
        case TYPE_FLOAT:
          out_text(w,"float ");
          break;
        case TYPE_CHAR:
          out_text(w,"char ");
          break;
      }
    out_text(w,"\n");
  }
}

void print_code(writer_t *w, uint8_t *code, int size) {
  int n_instr = 0;
  for (; strcmp(instr_names[n_instr], "???"); n_instr++)
    ;
  for (int i = 0; i < size; i++) {
    int instr = code[i];
    if (instr > n_instr) {
      out_text(w,"???\n");
      continue;
    }
    out_text(w,"%04d %s", i, instr_names[instr]);
    switch (instr) {
      case PUSHC:
      case JMP:
        out_text(w," %d", lval(&code[i + 1], int32_t));
        i += 4;
        break;
      case CALL:
        out_text(w," %d", lval(&code[i + 1], uint32_t));
        i += 4;
        break;
      case PUSHB:
      case IDX:
        out_text(w," %d", lval(&code[i + 1], uint8_t));
        i += 1;
        break;
    }
    out_text(w,"\n");
  }
}

void read_var(reader_t *r, runtime_t *env, thread_t *tt, input_layout_item_t *var) {
  int offs = 0;
  for (int i = 0; i < var->n_elems; i++) switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x;
        in_text(r,"%d ", &x);
        lval(get_addr(tt, var->addr + offs, 4), uint32_t) = x;
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x;
        in_text(r,"%f ", &x);
        lval(get_addr(tt, var->addr + offs, 4), float) = x;
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x;
        in_text(r,"%c ", &x);
        lval(get_addr(tt, var->addr + offs, 1), uint8_t) = x;
        offs += 1;
      } break;
    }
}

void print_var(writer_t *w, runtime_t *env, uint8_t *addr, input_layout_item_t *var) {
  int offs = 0;
  for (int i = 0; i < var->n_elems; i++) {
    if (i > 0) out_text(w," ");
    switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x = lval(addr + offs, uint32_t);
        out_text(w,"%d", x);
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x = lval(addr + offs, float);
        out_text(w,"%f", x);
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x = lval(addr + offs, uint8_t);
        out_text(w,"%c", x);
        offs += 1;
      } break;
    }
  }
  // out_text(w," ;");
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

void print_array(writer_t *w, runtime_t *env, input_layout_item_t *var, int nd, int *sizes,
                 uint32_t base, int from_dim, int offs) {
  if (from_dim == nd - 1) {
    for (int i = 0; i < sizes[nd - 1]; i++)  // FIXME other than ints
      out_text(w,"%d ", lval(env->heap->data + (base + (offs + i) * 4), int32_t));
  } else {
    int o = 0;
    for (int i = 0; i < sizes[from_dim]; i++) {
      print_array(w,env, var, nd, sizes, base, from_dim + 1, offs + o);
      o += sizes[from_dim + 1];
    }
  }
}

void read_input(reader_t *r, runtime_t *env) {
  thread_t *tt = STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0];

  for (int i = 0; i < env->n_in_vars; i++) {
    if (env->in_vars[i].num_dim > 0) {
      // FIXME one-dimensional array of ints only
      int n_elem, elem_size = count_size(&(env->in_vars[i]));
      in_text(r,"%d ", &n_elem);

      uint32_t base = env->heap->top;
      stack_t_alloc(env->heap, n_elem * elem_size);

      lval(get_addr(tt, env->in_vars[i].addr, 4), uint32_t) = base;
      lval(get_addr(tt, env->in_vars[i].addr + 4, 4), uint32_t) = n_elem;

      for (int j = 0; j < n_elem; j++) {
        int x;
        in_text(r,"%d ", &x);
        lval(env->heap->data + base + j * 4, uint32_t) = x;
      }

    } else
      read_var(r,env, tt, &(env->in_vars[i]));
  }
}

void write_output(writer_t *w, runtime_t *env) {
  for (int i = 0; i < env->n_out_vars; i++) {
    if (env->out_vars[i].num_dim > 0) {
      int elem_size = count_size(&(env->out_vars[i]));
      uint8_t *global_mem =
          STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data;
      uint32_t base = lval(global_mem + env->out_vars[i].addr, uint32_t);
      
      uint8_t nd = env->out_vars[i].num_dim;
      int *sizes = (int *)malloc(nd * sizeof(int));
      for (int j = 0; j < nd; j++)
        sizes[j] = lval(global_mem +  env->out_vars[i].addr + 4*(j+1), uint32_t) ;
      print_array(w,env, &(env->out_vars[i]), nd, sizes, base, 0, 0);
      free(sizes);
    } else
      print_var(
          w,
          env,
          STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data +
              env->out_vars[i].addr,
          &(env->out_vars[i]));
    out_text(w,"\n");
  }
}


void dump_memory(writer_t *w, runtime_t *env) {
  out_text(w,"mem (size=%d): ",env->heap->size);
  for (int i = 0; i < env->heap->size; i++) out_text(w,"%02u ", env->heap->data[i]);
  out_text(w,"\n");
}



