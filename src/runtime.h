#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include <vm.h>
#include <reader.h>
#include <writer.h>

int read_var(reader_t *r, uint8_t *base, input_layout_item_t *var);
int read_input(reader_t *r, runtime_t *env);

int scan_array(reader_t *r, writer_t *w, input_layout_item_t *var, int *sizes, int current,
                int first);
void print_io_vars(writer_t *w, int n, input_layout_item_t *vars);
void print_code(writer_t *w, uint8_t *code, int size);
void print_var(writer_t *w, uint8_t *addr, input_layout_item_t *var);
void print_array(writer_t *w, runtime_t *env, input_layout_item_t *var, int nd, int *sizes,
                 uint32_t base, int from_dim, int offs);
void write_output(writer_t *w, runtime_t *env);
void dump_memory(writer_t *w, runtime_t *env);

int count_size(input_layout_item_t *var);


#endif
