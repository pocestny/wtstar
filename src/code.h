/**
 * @file code.h
 * @brief Instruction set and helpers
 *
 * ### Structure of the binary file ###
 *
 * The file consists of sections. The first section is HEADER
 *
 *  type    | meaning
 *  --------|-------
 *   uint8  | `SECTION_HEADER`
 *   uint8  | version byte
 *   uint32 | size of static memory
 *   uint8  | memory mode
 *
 * Following the header are in arbitrary order sections INPUT, OUTPUT, CODE, and optionally
 * DEBUG.
 *
 * The INPUT and OUTPUT sections describe the input/output variables and
 * have the same structure:
 *
 *  type    | meaning
 *  --------|-------
 *   uint8  | `SECTION_INPUT` or `SECTION_OUTPUT`
 *   uint32 | `n_var` - number of input/output variables
 *
 * followed by `n_var` descriptions of variables. A descriptor is:
 *
 *  type    | meaning
 *  --------|-------
 *   uint32 | address in the static memory
 *   uint32 | number of dimensions
 *   uint8  | `type_layout_size`
 *
 * followed by a sequence of `type_layout_size` bytes from #type_descriptor_t
 *
 * @note The restriction means that there are at 
 * most 256 subtypes in a type.
 *
 * The FNMAP section maps functions to addresses:
 *
 *  type    | meaning
 *  --------|-------
 *   uint8  | `SECTION_FNMAP`
 *   uint32 | `n_fn` - number of functions
 *
 * followed by `n_fn` function descriptors:
 *
 *  type    | meaning
 *  --------|-------
 *   uint32 | address in the code segment
 *   int32  | `stack_change`: how should the `op_stack` change after the call
 *  
 * `stack_change` = `out_type_size` - `overall_size_of_parameters`
 *
 * The DEBUG section is optional, and has the following structure:
 *
 *  type                                | meaning
 *  ------------------------------------|-------
 *  uint8                               | `SECTION_DEBUG`
 *  uint32                              | `n_files`
 *  `n_files` 0-terminated strings      | names of the included files
 *  uint32                              | `n_fn`
 *  `n_fn` pairs uint32, string         | references to items, and names of functions
 *  uint32                              | `n_src_items`
 *  `n_src_items` descriptors           | descriptors of source entities that generated code
 *  uint32                              | `n_src_map`: source item map size
 *  `n_scr_map` pairs of uint32,int32   | each pair is (breakpoint,item_id) 
 *  uint32                              | `n_types`
 *  `n_types` type descriptors          | name, n_members, for each member name, id
 *  uint32                              | `n_scope_map` : scope map size
 *  `n_scope_map` pairs of uint32,int32 |  each pair is (breakpoint,scope_id)
 *  uint32_t                            | `n_scopes`
 *  `n_scopes` scope_info               | for each scope its descriptor
 *
 *
 *  The source item descriptor consist of 5 `uint32` numbers: `fileid` (index to the
 *  `files` table), `first_line`, `first_column`, `last_line`, `last_column` 
 *
 *  The scope descriptor contains parent, n_vars, and for each variable
 *  name, type, num_dim, start_code, address
 *
 *  For a pair `(bp,item)` in the source map, the code starting from `bp` up to
 *  the next breakpoint was generated from the entity `item`
 *
 *  ### Storage ###
 *
 *  `int`, `float`, `char` are stored directly in memory as `int32_t`, `float`, `uint8_t`.
 *
 *  Arrays have header in static memory, and the contents is allocated on heap
 *  when the array is created. The header has the following structure
 *
 *  type                   | meaning
 *  -----------------------|-------
 *  uint32                 |  base address (relative to heap)
 *  uint32                 |  `n_dim`
 *  `n_dim`  uint32 ranges |  size in i-th dimension
 *
 * ### Function calls ###
 *
 * on `call x`
 * :  copy non_returned threads from the active group to newly created group
 *
 * on `return x`
 * :   `push x`, `ret_join` which sets `returned` flag and joins the current group
 *
 * on end of function
 * :   call `return` clear `returned` flag, join current group, set ret_addr
 */


#ifndef __CODE__H__
#define __CODE__H__
#include <utils.h>

/**
 * @brief instruction set
 *
 * Stack stores 4B values (`int32_t`, `uint32_t`, or `float`).
 * Memory has `int` and `float` values of 4B and  `unit8_t` chars
 */
typedef enum {
NOOP        =0x0U, //!< empty instruction

PUSHC,  //!<  followed by c  : push c (4 B) to stack 
PUSHB,  //!<  followed by b  : push b (1 B) to stack (it will be 4B on stack) 
FBASE,  //!<  add the `FBASE` register (uint32) to top of stack 
SIZE,   /*!<  ` a , d  ... -> s,...`
              where `a` = array address, `d` = dim number, `s` = size in dimension `d`
        */
LDC,    //!<  `a,... -> val(a),...`  where `a`, `val(a)` are 4B
LDB,    //!<  same as `LDC`, `val(a)` is 1B converted to 4B on stack
STC,    //!<  `a,val,.. -> ...` `a` is 4B in memory
STB,    //!<  same as `STC`, `val` 4B on stack converted to 1B in memory

LDCH,   //!< same as `LDC`,  but address is relative to heap
LDBH,   //!< same as `LDB`,  but address is relative to heap 
STCH,   //!< same as `STC`,  but address is relative to heap
STBH,   //!< same as `STB`,  but address is relative to heap

IDX,   /*!< followed by 1B `n`, makes `addr,i1,...,in,... -> hoffs ...` where
        `addr`      = address of the header block of array variable, 
        `i1`..`in`  = indices in dimensions,
        `hoffs`     = offset (in number of elements)
        */

SWS,    //!< swap `op_stack`: `a,b,...  -> b,a,...`    
POP,    //!<discard top stack

A2S,     //!< copy top acc to top stack
POPA,    //!< discard top accumulator
S2A,     //!< top stack copy to acc
RVA,     //!< reverse acc
SWA,     //!< swap two top elements of acc

ADD_INT,      //!<  `a,b,... -> a+b,...` (int32_t)
SUB_INT,      //!<  `a,b,... -> a-b,...` (int32_t)
MULT_INT,     //!<  `a,b,... -> a*b...` (int32_t)
DIV_INT,      //!<  `a,b,... -> a/b...` (int32_t)
MOD_INT,      //!<  `a,b,... -> a%b...` (int32_t)
ADD_FLOAT,    //!<  `a,b,... -> a+b...` (float)
SUB_FLOAT,    //!<  `a,b,... -> a-b...` (float)
MULT_FLOAT,   //!<  `a,b,... -> a*b...` (float)
DIV_FLOAT,    //!<  `a,b,... -> a/b...` (float)
POW_INT,      //!<  `a,b,... -> a^b...` (int32_t)
POW_FLOAT,    //!<  `a,b,... -> a^b...` (float)

NOT,          //!< `a,... -> 1-a,...`
OR,           //!< `a,b,... -> x,...`  x = a OR b
AND,          //!< `a,b,... -> x,...`  x = a AND b

BIT_OR,
BIT_AND,
BIT_XOR,

EQ_INT,       //!< `a,b.... -> x,....` x=1 if a=b (int32_t)
EQ_FLOAT,     //!< `a,b.... -> x,....` x=1 if a=b (float)
GT_INT,       //!< `a,b.... -> x,....` x=1 if a>b (int32_t)
GT_FLOAT,     //!< `a,b.... -> x,....` x=1 if a>b (float)
GEQ_INT,      //!< `a,b.... -> x,....` x=1 if a>=b (int32_t)
GEQ_FLOAT,    //!< `a,b.... -> x,....` x=1 if a>=b (float)
LT_INT,       //!< `a,b.... -> x,....` x=1 if a<b (int32_t)
LT_FLOAT,     //!< `a,b.... -> x,....` x=1 if a<b (float)
LEQ_INT,      //!< `a,b.... -> x,....` x=1 if a<=b (int32_t)
LEQ_FLOAT,    //!< `a,b.... -> x,....` x=1 if a<=b (float)

JMP,          //!<  followed by `x` (4B)   : jump to addr x (relative, x int32_t)

CALL,         /*!<  followed by `n` (4B)  : call function n (from ftable) :
               Cretes new callframe, and jumps to the address.
               The function code transfers parameters from stack
                          `p1,p2,...,pn,s... -> s,...`
              */            
RETURN,       //!<  removes the callframe (top of stack is return value) and jumps

FLOAT2INT,    //!<  cast top of stack
INT2FLOAT,    //!<  cast top of stack

FORK,         /*!<  `a,n,... -> ....` forks n new processes,
                  `a` is the address of the driving variable 
              */    
SPLIT,        /*!<  `c,... -> ...` split current group based on stack top: 
                Ccreates two new groups (first for nonzero, second for zero),
                each continues until join.
                Empty group causes the PC to move to the next join
                */
JOIN,         //!<  remove active group 
JOIN_JMP,     //!<  followed by `a` (4B): remove active group and add `a` to pc (default 4)
SETR,         //!<  set the "returned" flag in current group

MEM_MARK,    //!< mark both static memory and heap
MEM_FREE,    //!< deallocate heap from last mark

ALLOC,        /*!<  `c,... -> addr,....` (c,addr:uint32_t):
               give address (relative to heap) to block of size `c`
               */

ENDVM,      //!< halt the machine

LAST_BIT,   /*!< `c,... -> d,...` (int32):
              d : position (from right) of the last non-zero bit
            */

SORT,       /*!< `addr, size, offs, type, ... -> ...` :
              `addr` is a 1-dimensional array of elements of `size`, 
               sort it based on a key of `type`, located at `offs` in
               the record; 
               type = `TYPE_INT`, `TYPE_FLOAT`, or `TYPE_CHAR`
             */  

LOGF,       //!<  `a... -> b...` (a,b:float) b=log2
LOG,        //!<  `a... -> b...` (a,b:int) b = ceiling log2
SQRT,       //!<  `a... -> b...` (a,b:int) b = ceiling(sqrt(a))
SQRTF,      //!<  `a... -> b...` (a,b:float) b = sqrt(a)
BREAK,      //!<  followed by x (4B)  : `a ... -> ....` if `(a)`, fire breakpoint number `x`
BREAKOUT,
BREAKSLOT,  /*!<  NOOP hinting the debugger that the previous instruction is
                  also NOOP, and can be used for inserting breakpoints. It is
                  also used to mark halt points for debugger stepping.
             */
} instruction_t;


//! section headers
typedef enum {
SECTION_HEADER  =0x77U, //!< HEADER
SECTION_INPUT   =0x88U, //!< INPUT
SECTION_OUTPUT  =0x99U, //!< OUTPUT
SECTION_FNMAP   =0xaaU, //!< FNMAP
SECTION_CODE    =0xbbU, //!< CODE
SECTION_DEBUG   =0xccU  //!< DEBUG
} section_header_t;

//! type descriptors
typedef enum {
TYPE_INT   =0U, //!< `int`
TYPE_FLOAT,     //!< `float`
TYPE_CHAR       //!< `char`
} type_descriptor_t;

//! supported memory modes
typedef enum {
MEM_MODE_EREW=0x75U,//!< EREW 
MEM_MODE_CREW,      //!< CREW (default)
MEM_MODE_CCRCW      //!< common CRCW
} memory_mode_t;


//! returns true if `oper` (token value) is assignment operator
#define assign_oper(oper) ( \
      (oper) == '=' || (oper) == TOK_PLUS_ASSIGN || (oper) == TOK_MINUS_ASSIGN || \
      (oper) == TOK_TIMES_ASSIGN || (oper) == TOK_DIV_ASSIGN ||\
      (oper) == TOK_MOD_ASSIGN)

//! returns true if `oper` (token value) is numeric operator
#define numeric_oper(oper) (\
      (oper) == '+' || (oper) == '-' || (oper) == '*' || (oper) == '^' || (oper) == '/' ||\
      (oper) == '%' || (oper) =='|' || (oper)=='&' || (oper)=='~')

//! returns true if `oper` (token value) is comparison operator
#define comparison_oper(oper) (\
      (oper) == TOK_EQ || (oper) == TOK_NEQ || (oper) == TOK_GEQ || (oper) == TOK_LEQ || \
      (oper) == '<' || (oper) == '>')


#endif
