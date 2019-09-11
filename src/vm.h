#ifndef ___VM__H__
#define ___VM__H__

#include <inttypes.h>

#include <utils.h>
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
  uint32_t n_in_vars, n_out_vars;
  uint32_t fcnt;
  fnmap_t* fnmap;

  uint8_t *code;
  uint32_t code_size;
  stack_t *heap;

  stack_t *threads;        // stack of stack of thread_t* 
  stack_t *frames;         // stack of frame_t *

  int W,T;
  int pc,virtual_grps,n_thr,a_thr; // a_thr -> active (non-returned threads)

  uint32_t arr_sizes[257], arr_offs[257];

   thread_t **thr;
   frame_t *frame ;
   int mem_mode;
} runtime_t;


// read runtime from input string
CONSTRUCTOR(runtime_t, uint8_t *in, int len);
DESTRUCTOR(runtime_t);

int execute(runtime_t *env, int limit);
#endif
