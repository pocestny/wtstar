#ifndef ___VM__H__
#define ___VM__H__

#include <inttypes.h>

#include <utils.h>
#include <code.h>
#include <reader.h>
#include <writer.h>
#include <debug.h>
#include <code.h>

typedef struct {
  uint32_t addr;
  uint8_t  num_dim;
  uint32_t n_elems;
  uint8_t *elems;
} input_layout_item_t;


typedef struct {
  uint8_t *data;
  uint32_t top,size;
} stack_t;

CONSTRUCTOR(stack_t);
DESTRUCTOR(stack_t);

// low level push/pop
void stack_t_push(stack_t *s, void *data, uint32_t len);
void stack_t_alloc(stack_t *s, uint32_t len); // just push empty space
void stack_t_pop(stack_t *s, void *data, uint32_t len);

#define STACK_SIZE(s,type) ((s)->top/sizeof(type))
#define STACK(s,type) ((type*)((s)->data))
#define STACK_TOP(s,type) (((type*)((s)->data))[STACK_SIZE(s,type)-1])

typedef struct _thread_t {
  uint32_t mem_base;
  stack_t *op_stack, *acc_stack, *mem;
  struct _thread_t *parent;
  int refcnt;
  int returned; // flag if return was called within a function
  int bp_hit;
  int tid;
} thread_t;

CONSTRUCTOR(thread_t);
DESTRUCTOR(thread_t);

void *get_addr(thread_t *thr, uint32_t addr, uint32_t len);

// create a child copy  
thread_t *clone_thread(thread_t *src);

typedef struct {
  uint32_t base, ret_addr;
  stack_t *heap_mark,*mem_mark; // use this to mark/free memory in current frame
  int op_stack_end; // asset - all active thread should have the same size of opstack
} frame_t;

CONSTRUCTOR(frame_t, uint32_t base);
DESTRUCTOR(frame_t);

typedef struct {
  uint32_t addr;
  int32_t out_size;
} fnmap_t;


typedef struct {
  input_layout_item_t *in_vars, *out_vars;
  uint32_t n_in_vars, n_out_vars, global_size;
  uint32_t fcnt;
  fnmap_t* fnmap;

  uint8_t *code;
  uint32_t code_size;
  stack_t *heap;

  stack_t *threads;        // stack of stack of thread_t* 
  stack_t *frames;         // stack of frame_t *

  int W,T;
  int pc,stored_pc,virtual_grps,n_thr,a_thr; // a_thr -> active (non-returned threads)

  uint32_t arr_sizes[257], arr_offs[257];

   thread_t **thr;
   frame_t *frame ;
   int mem_mode;

   debug_info_t *debug_info;

   enum {
     VM_READY,
     VM_RUNNING,
     VM_OK,
     VM_ERROR
   } state;

} virtual_machine_t;


// read runtime from input string
CONSTRUCTOR(virtual_machine_t, uint8_t *in, int len);
DESTRUCTOR(virtual_machine_t);

// TODO execute status: <-1 error, -1 endvm, 0 limit exceeded, >0 breakpoint hit

int execute(virtual_machine_t *env, int limit, int trace_on, int stop_on_bp);

// return:
// endvm = -1
// breakpoint x = x (>0)
// error <-1
// ok = 0
int instruction(virtual_machine_t *env,int stop_on_bp);
// io support


void print_types(writer_t *w,virtual_machine_t *env);
void print_var_name(writer_t *w, virtual_machine_t *env, int addr) ;
void print_var_layout(writer_t *w, input_layout_item_t *it) ;

int read_var(reader_t *r, uint8_t *base, input_layout_item_t *var);
int read_input(reader_t *r, virtual_machine_t *env);

int scan_array(reader_t *r, writer_t *w, input_layout_item_t *var, int *sizes, int current,
                int first);
void print_io_vars(writer_t *w, virtual_machine_t *env, int n, input_layout_item_t *vars);

void print_code(writer_t *w, uint8_t *code, int size);
void print_var(writer_t *w, uint8_t *addr, input_layout_item_t *var);
void print_array(writer_t *w, virtual_machine_t *env, input_layout_item_t *var, int nd, int *sizes,
                 uint32_t base, int from_dim, int offs);
void write_output(writer_t *w, virtual_machine_t *env, int i);

int count_size(input_layout_item_t *var);

void dump_debug_info(writer_t *w, virtual_machine_t *env);
void dump_header(writer_t *w, virtual_machine_t *env);

char *mode_name(int mode);
#endif
