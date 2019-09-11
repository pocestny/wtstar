#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errors.h>
#include <reader.h>
#include <runtime.h>

#define error(...)                      \
  {                                     \
    error_t *err = error_t_new();       \
    append_error_msg(err, __VA_ARGS__); \
    emit_error(err);                    \
  }

extern const char *const instr_names[];

void print_io_vars(writer_t *w, int n, input_layout_item_t *vars) {
  for (int i = 0; i < n; i++) {
    out_text(w, "%04d %c : ", vars[i].addr,
             ((vars[i].num_dim > 0) ? 'A' : ' '));
    for (int j = 0; j < vars[i].n_elems; j++) switch (vars[i].elems[j]) {
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
    out_text(w, "%04d %s", i, instr_names[instr]);
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
      if (res != 1) {error("wrong input");return -1;}
    }

  for (int i = 0; i < var->n_elems; i++) switch (var->elems[i]) {
      case TYPE_INT: {
        uint32_t x;
        in_text(r, res, "%d", &x);
        if (res != 1) {error("wrong input");return -1;}
        lval(base + offs, uint32_t) = x;
        offs += 4;
      } break;
      case TYPE_FLOAT: {
        float x;
        in_text(r, res, "%f", &x);
        if (res != 1) {error("wrong input"); return -1;}
        lval(base + offs, float) = x;
        offs += 4;
      } break;
      case TYPE_CHAR: {
        uint8_t x;
        in_text(r, res, "%c", &x);
        if (res != 1) {error("wrong input"); return -1;}
        lval(base + offs, uint8_t) = x;
        offs += 1;
      } break;
    }

  if (var->n_elems > 1)
    for (char c = '0'; c != '}';) {
      in_text(r, res, "%c", &c);
      if (res != 1) {error("wrong input");return -1;}
    }
  return 0;
}

int scan_array(reader_t *r, writer_t *w, input_layout_item_t *var, int *sizes, int current,
                int first) {
  int res;
  int elem_size = count_size(var);

  for (char c = '0'; c != '[';) {
    in_text(r, res, "%c", &c);
    if (res != 1) {error("wrong input");return -1;}
  }

  int cnt = 0;

  do {
    if (current<var->num_dim-1) {
      if (scan_array(r,w,var,sizes,current+1, (first&&cnt==0)?1:0)!=0) return -1;
    } else {
      out_text(w," ");
      uint8_t *buf = malloc(elem_size);
      if (read_var(r,buf,var)!=0) {
        free(buf);
        return -1;
      }
      print_var(w,buf,var);
      out_text(w," ");
      free(buf);
    }

    cnt++;
    
    char c =' ';
    for (; c == ' ' || c=='\n' || c=='\t';) {
      in_text(r, res, "%c", &c);
      if (res != 1) {error("wrong input");return -1;}
    }
    if (c==']') {
      if (first==1) sizes[current]=cnt;
      else if (sizes[current]!=cnt)
      {error("wrong input size");return -1;}

      break;
    }
    in_ungetc(r,c);
  } while (1);
  return 0;
}

int read_input(reader_t *r, runtime_t *env) {
  thread_t *tt = STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0];

  for (int i = 0; i < env->n_in_vars; i++) {
    input_layout_item_t *var = &(env->in_vars[i]);
    int elem_size = count_size(var);

    if (var->num_dim > 0) {
      int sizes[var->num_dim];
      writer_t *w = writer_t_new(WRITER_STRING);
      if (scan_array(r, w, var, sizes, 0, 1)!=0) {
        writer_t_delete(w);
        return -1;
      }

      int n_elem = 1;
      for (int i=0;i<var->num_dim;i++)
        n_elem*=sizes[i];

      uint32_t base = env->heap->top;
      stack_t_alloc(env->heap, n_elem * elem_size);

      lval(get_addr(tt, env->in_vars[i].addr, 4), uint32_t) = base;
      lval(get_addr(tt, env->in_vars[i].addr+4, 4), uint32_t) = var->num_dim;
      for (int j=0;j<var->num_dim;j++)
        lval(get_addr(tt, env->in_vars[i].addr + 4*(j+2), 4), uint32_t) = (uint32_t)(sizes[j]);

      reader_t *rw = reader_t_new(READER_STRING,w->str.base);
      for (int j = 0; j < n_elem; j++) 
        if (read_var(rw,env->heap->data+j*elem_size,var)!=0) {
          writer_t_delete(w);
          reader_t_delete(rw);
          return -1;
        }
      
      reader_t_delete(rw);
      writer_t_delete(w);
    } else {
      if (read_var(r, get_addr(tt, var->addr, elem_size), var)!=0) return -1;
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

void print_array(writer_t *w, runtime_t *env, input_layout_item_t *var, int nd,
                 int *sizes, uint32_t base, int from_dim, int offs) {
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
        sizes[j] =
            lval(global_mem + env->out_vars[i].addr + 4 * (j + 2), uint32_t);
      print_array(w, env, &(env->out_vars[i]), nd, sizes, base, 0, 0);
      free(sizes);
    } else
      print_var(
          w, 
          STACK(STACK(env->threads, stack_t *)[0], thread_t *)[0]->mem->data +
              env->out_vars[i].addr,
          &(env->out_vars[i]));
    out_text(w, "\n");
  }
}

void dump_memory(writer_t *w, runtime_t *env) {
  out_text(w, "mem (size=%d): ", env->heap->size);
  for (int i = 0; i < env->heap->size; i++)
    out_text(w, "%02u ", env->heap->data[i]);
  out_text(w, "\n");
}
