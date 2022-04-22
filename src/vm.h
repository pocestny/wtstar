/**
 * @file vm.h
 * @brief virtual machine
 */
#ifndef ___VM__H__
#define ___VM__H__

#include <inttypes.h>

#include <code.h>
#include <debug.h>
#include <hash.h>
#include <reader.h>
#include <utils.h>
#include <writer.h>

//! layout of a variable value
typedef struct {
  uint32_t addr;     //!< address in data
  uint8_t num_dim;   //!< number of dimensions
  uint32_t n_elems;  //!< number of elements in the basic type
  uint8_t
      *elems;  //!< descriptions of elements of basic type (#type_descriptor_t)
} input_layout_item_t;

//! growing stack of values
typedef struct {
  uint8_t *data;  //!< memory
  uint32_t top,   //!< current top of stack
      size;       //!< allocated size
} stack_t;

//! constuctor
CONSTRUCTOR(stack_t);
//! destructor
DESTRUCTOR(stack_t);

//! low level push
void stack_t_push(stack_t *s, void *data, uint32_t len);
//! just push empty space
void stack_t_alloc(stack_t *s, uint32_t len);
//! low level pop
void stack_t_pop(stack_t *s, void *data, uint32_t len);

//! number of elements of given type
#define STACK_SIZE(s, type) ((s)->top / sizeof(type))
//! return stack as an array of given type
#define STACK(s, type) ((type *)((s)->data))
//! return top of stack of given type
#define STACK_TOP(s, type) (((type *)((s)->data))[STACK_SIZE(s, type) - 1])

//! info about a runtime thread
typedef struct _thread_t {
  uint32_t mem_base;  //!< where the memory starts (the index variable is here)
  stack_t *op_stack,  //!< operands stack
      *acc_stack,     //!< accumulator stack
      *mem;           //!< data
  struct _thread_t *parent;  //!< parent
  int refcnt;    //!< threads are refcounted, in constructor, destructor clears
                 //!< the last
  int returned;  //!< flag if return was called within a function
  int bp_hit;    //!< if the breakpoint was currently hit
  uint64_t tid;  //!< id of the thread (unique id assigned in constructor)
} thread_t;

//! constructor
CONSTRUCTOR(thread_t);
//! destructor
DESTRUCTOR(thread_t);

//! get the (real) memory of an address within thread's data; make sure at least
//! len bytes are allocated
void *get_addr(thread_t *thr, uint32_t addr, uint32_t len);

//! find thread by id
thread_t *get_thread(uint64_t tid);

//! create a child copy
thread_t *clone_thread(thread_t *src);

//! info about call frame
typedef struct {
  uint32_t base,       //!< base in the data (equal in all threads)
      ret_addr;        //!< return address
  stack_t *heap_mark,  //!< use this to mark/free heap memory in current frame
      *mem_mark;       //!< use this to mark/free data memory in current frame
  //! where the operand stack should end after the call, i.e.
  //! after removing from stack the parameters, and inserting the return value
  int op_stack_end;    
} frame_t;

//! constructor
CONSTRUCTOR(frame_t, uint32_t base);
//! destructor
DESTRUCTOR(frame_t);

//! used to index functions
typedef struct {
  uint32_t addr; //!< address in the code
  int32_t out_size; //!< size of the output value
} fnmap_t;

typedef struct {
  uint32_t id;
  uint32_t bp_pos;
  uint32_t code_pos;
  uint32_t code_size;
} breakpoint_t;

CONSTRUCTOR(
  breakpoint_t,
  uint32_t id,
  uint32_t bp_pos,
  uint32_t code_pos,
  uint32_t code_size
);

DESTRUCTOR(breakpoint_t);

//! virtual machine
typedef struct {
  input_layout_item_t *in_vars, //!< input variables
                      *out_vars; //!< output variables
  uint32_t n_in_vars,  //!< number of input variables
           n_out_vars, //!< number of output variables
           global_size; //!< size of the global variables (allocated at start)
  uint32_t fcnt; //!< number of functions
  fnmap_t *fnmap; //!< starting addresses and return value sizes of fuctions

  uint8_t *code; //!< binary code
  uint32_t code_size; //!< size of binary code
  stack_t *heap; //!< global heap

  stack_t *threads;  //!< stack of stack of thread_t*
  stack_t *frames;   //!< stack of frame_t *

  int W, T; //!< keep track of work and time
  int pc, //!< pc
      stored_pc, //!< pc of the last operation 
      virtual_grps, //!< empty groups of threads at the top of thread stack
      n_thr, //!< number of threads
      last_global_pc,//!< last time the pc was in global scope
      a_thr;  //!< a_thr -> active (non-returned threads)

  uint32_t arr_sizes[257],  //!< used internally when indexing an array
           arr_offs[257]; //!<  used internally when indexing an array

  thread_t **thr; //!< top thread group from threads for convenience
  frame_t *frame; //!< current frame from frames for convenience
  int mem_mode; //!< memory mode

  debug_info_t *debug_info; //!< debugging info (if present)

  hash_table_t *bps;

  enum { VM_READY = 0, VM_RUNNING, VM_OK, VM_ERROR } state; //!< current state

} virtual_machine_t;

//! read runtime from input string
CONSTRUCTOR(virtual_machine_t, uint8_t *in, int len);
//! destructor
DESTRUCTOR(virtual_machine_t);

//! add runtime breakpoint with condition
int add_breakpoint(
  virtual_machine_t *env,
  uint32_t bp_pos,
  uint8_t *code,
  uint32_t code_size
);

//! remove runtime breakpoint
int remove_breakpoint(virtual_machine_t *env, uint32_t bp_pos);

//! enable/disable runtime breakpoint
int enable_breakpoint(virtual_machine_t *env, uint32_t bp_pos, int enabled);

/**
 * Return breakpoint id associated with dynamically inserted BREAK instruction
 * numbered bp_pos or 0 if given instruction is not dynamically inserted BREAK
 */
int get_dynamic_bp_id(virtual_machine_t *env, uint32_t bp_pos);

/**
 * @brief evaluate condition of breakpoint
 * 
 * executes instructions associated with given breakpoint in given thread and
 * puts result onto stack
 * does nothing if breakpoint was present at compile time
 */
int execute_breakpoint_condition(
  virtual_machine_t *env,
  uint32_t bp_pos,
  int thr_id
);

/** 
 * @brief execute the virtual machine
 *
 * perform at mose limit (-1 for unlimited) instructions, and maybe stop on breakpoints
 * (trace_on is used for debugging)
 *
 * return status: <-1 error, -1 endvm, 0 limit exceeded, >0 breakpoint hit
 */
int execute(virtual_machine_t *env, int limit, int trace_on, int stop_on_bp);

/**
 * @brief perform one instruction
 *
 * return value: endvm = -1, breakpoint x = x (>0), error <-1, ok = 0
 */
int instruction(virtual_machine_t *env, int stop_on_bp);

//! if there is debug info about types, get layout
input_layout_item_t get_layout(variable_info_t *var, virtual_machine_t *env);

//----------------------------
// io support

//! print basic info about types
void print_types(writer_t *w, virtual_machine_t *env);
//! print name of a variable at given address
void print_var_name(writer_t *w, virtual_machine_t *env, int addr);
//! print layout of the variable's type
void print_var_layout(writer_t *w, input_layout_item_t *it);

//! read variable from input
int read_var(reader_t *r, uint8_t *base, input_layout_item_t *var);
//! read all input variables
int read_input(reader_t *r, virtual_machine_t *env);

//! recursively read all dimensions of an array from input
int scan_array(reader_t *r, writer_t *w, input_layout_item_t *var, int *sizes,
               int current, int first);
//! print input/output variables
void print_io_vars(writer_t *w, virtual_machine_t *env, int n,
                   input_layout_item_t *vars);

//! dump the  code
void print_code(writer_t *w, uint8_t *code, int size);
//! print a variable
void print_var(writer_t *w, uint8_t *addr, input_layout_item_t *var);
//! print an array
void print_array(writer_t *w, virtual_machine_t *env, input_layout_item_t *var,
                 int nd, int *sizes, uint32_t base, int from_dim, int offs);
//! print all output variables
void write_output(writer_t *w, virtual_machine_t *env, int i);
//! count the size of a type layout
int count_size(input_layout_item_t *var);

//! write debug information
void dump_debug_info(writer_t *w, virtual_machine_t *env);
//! write file header
void dump_header(writer_t *w, virtual_machine_t *env);

//! return the memory mode in human readable form
char *mode_name(int mode);
#endif
