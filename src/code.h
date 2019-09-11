/*
 * instruction set of the virtual machine
 */

#ifndef __CODE__H__
#define __CODE__H__
#include <utils.h>

// stack has 4 B values, memory int/float 4 B, char 1B

#define NOOP        0x0U

// operand stack
#define PUSHC       0x1U  //  PUSHC(c)  : ...   -> c,...  c: 4B 
#define PUSHB       0x2U  //  PUSHB(b)  : ...   -> c,...  b: 1B, c:4B
#define FBASE       0x3U  //  FBASE     : ...   -> frame_base,... (uint32_t)

#define SIZE        0x04  //  SIZE      : a , d  ... -> s,...
                          //            a = array address, d = dim number

#define LDC         0x5U  //  LDC       : a,... -> val(a),... (a, val(a)=4 B)
#define LDB         0x6U  //  same as LDC, val(a) : 1 B converted to 4 B
#define STC         0x7U  //  STC       : a,val,.. -> ... 4 B
#define STB         0x8U  //  same as STC, a,val : val 4 B converted to 1

#define LDCH        0x9U  // same as above,  but addresses are relative to heap
#define LDBH        0xAU 
#define STCH        0xBU 
#define STBH        0xCU 

#define IDX         0xDU  // IDX(n)  addr,i1,...,in,... -> hoffs
                          // addr - address of array variable, 
                          //    i1..in indices of active dimensions
                          // hoffs - offset (in # of elements)

#define SWS         0xEU  //  SWS      : a,b,...  -> b,a,...    
#define POP         0xFU  //  POP      : discard top stack

// accumulator  stack
#define A2S         0x10U  //  A2S      : copy top acc to top stack
#define POPA        0x11U  //  POPA     : discard top accumulator
#define S2A         0x12U  //  S2A      : top stack copy to acc
#define RVA         0x13U  //  RVA      : reverse acc
#define SWA         0x14U  //  SWA      : swap two top elements of acc

#define ADD_INT     0x15U  //  ADD_INT  : a,b,... -> a+b,... (int32_t)
#define SUB_INT     0x16U  //  SUB_INT  : a,b,... -> a-b,... (int32_t)
#define MULT_INT    0x17U
#define DIV_INT     0x18U
#define MOD_INT     0x19U
#define ADD_FLOAT   0x1AU
#define SUB_FLOAT   0x1BU
#define MULT_FLOAT  0x1CU
#define DIV_FLOAT   0x1DU
#define POW_INT     0x1FU
#define POW_FLOAT   0x20U

// a,b:int32_t
#define NOT         0x21U  // 
#define OR          0x22U  // a,b,... -> x,...  x = a OR b
#define AND         0x23U  // a,b,... -> x,...  x = a AND b

#define EQ_INT     0x24U   // a,b.... -> x,.... x=1 if a=b (int32_t)
#define EQ_FLOAT   0x25U   // a,b.... -> x,.... x=1 if a=b (float)
#define GT_INT     0x26U   // a,b.... -> x,.... x=1 if a>b (int32_t)
#define GT_FLOAT   0x27U   // a,b.... -> x,.... x=1 if a>b (float)
#define GEQ_INT    0x28U   // a,b.... -> x,.... x=1 if a>=b (int32_t)
#define GEQ_FLOAT  0x29U   // a,b.... -> x,.... x=1 if a>=b (float)
#define LT_INT     0x2AU   // a,b.... -> x,.... x=1 if a<b (int32_t)
#define LT_FLOAT   0x2BU   // a,b.... -> x,.... x=1 if a<b (float)
#define LEQ_INT    0x2CU   // a,b.... -> x,.... x=1 if a<=b (int32_t)
#define LEQ_FLOAT  0x2DU   // a,b.... -> x,.... x=1 if a<=b (float)

#define JMP         0x2EU  //  JMP(x)   : jump to addr x (relative, x int32_t)

#define CALL        0x2FU  //  CALL(n)  : call function n (from ftable) 
                           //  cretes new callframe, and jumps to the address
                           //  the function code transfers parameters from stack
                           //             p1,p2,...,pn,s... -> s,...
#define RETURN      0x30U  //  removes the callframe (top of stack is return value)    
                           //  and jumps

#define FLOAT2INT   0x31U  //  cast top of stack
#define INT2FLOAT   0x32U  //  cast top of stack

#define FORK        0x33U  //  FORK: a,n,... -> .... forks n new processes
                           //        a is the address of the driving variable 
#define SPLIT       0x34U  //  SPLIT    : c,... -> ... split current group based on stack top 
                           //  create two new groups (first for nonzero, second for zero)
                           //  each continues until join
                           //  empty group causes the PC to move to the next join
#define JOIN        0x35U  //  JOIN remove active group 
#define JOIN_JMP    0x36U  //  JOIN_JMP(a) remove active group and add a to pc (default 4)
#define SETR        0x37U  //  set the "returned" flag in current group

// these instructions do not count, and are executed only by the machine

// mark both static mem and heap
#define MEM_MARK   0x38U  
#define MEM_FREE   0x39U

// give address (relative to heap) to block of size c
#define ALLOC       0x3aU  //  ALLOC    : c,... -> addr,.... (c,addr:uint32_t)
#define ENDVM       0x3bU

#define LAST_BIT    0x3cU  // LAST_BIT: c,... -> d,... (int32)
                           // d : position (from right) of the last non-zero bit
#define SORT        0x3dU  // SORT addr, size, offs, type, ... -> ...
                           // addr is a 1-dimensional array of elements of size
                           // sort it based on a key of type, located at offs in
                           //    the record
                           // type = TYPE_INT, TYPE_FLOAT, TYPE_CHAR

#define LOGF        0x3eU  //  LOGF      : a... -> b... (a,b:float) b=log2
#define LOG         0x3fU  //  LOG       : a... -> b... (a,b:int) b = ceiling log2
#define SQRT        0x40U  //  SQRT      : a... -> b... (a,b:int) b = ceiling(sqrt(a))
#define SQRTF       0x41U  //  SQRTF     : a... -> b... (a,b:float) b = sqrt(a)


#define SECTION_HEADER  0x77U
#define SECTION_INPUT   0x88U
#define SECTION_OUTPUT  0x99U
#define SECTION_FNMAP   0xaaU
#define SECTION_CODE    0xbbU

/*
 
header: 
version     byte (1)
global_size uint32 
memory_mode uint8

input/output section:

n_vars uint32
<var1> ... <varn>

var: 
addr    uint32
num_dim uint8
n_items uint8 (element description)
item1 .. itemn (elem = uint8 - type )

the restriction means that there are at most 256 dimensions in an array, and at most
256 subtypes in a type

fnmap section: 
uint32_t n
addr_1,type_size 1, ... addr_n, type_size 2   
      addr = ddress in the code segment 
      type_size = how much the op_stack size should change after call 
      (out_type size - parameters size)


*/

#define TYPE_INT   0U
#define TYPE_FLOAT 1U
#define TYPE_CHAR  2U

#define MEM_MODE_EREW 0x75U
#define MEM_MODE_CREW 0x76U
#define MEM_MODE_CCRCW 0x77U


/* storage 
 
  int   = int32_t (4 bytes)
  float = float (4 bytes) -> unfortunately, not portable
  char  = int8_t

  array:
    uint32:  base address (relative to heap)
    uint32:  nd
    dim_1 ... dim_nd  uint32 range n (0..n-1) of dimensions 1..nd
*/

/* function calls
 
  call: copy non_returned from the active group to new group
  return x:  push x, ret_join (set returned flag + join current group)
  end_of_function: call return (clear flag, join group, set ret addr)

*/

#define assign_oper(oper) ( \
      (oper) == '=' || (oper) == TOK_PLUS_ASSIGN || (oper) == TOK_MINUS_ASSIGN || \
      (oper) == TOK_TIMES_ASSIGN || (oper) == TOK_DIV_ASSIGN ||\
      (oper) == TOK_MOD_ASSIGN)

#define numeric_oper(oper) (\
      (oper) == '+' || (oper) == '-' || (oper) == '*' || (oper) == '^' || (oper) == '/' ||\
      (oper) == '%')

#define comparison_oper(oper) (\
      (oper) == TOK_EQ || (oper) == TOK_NEQ || (oper) == TOK_GEQ || (oper) == TOK_LEQ || \
      (oper) == '<' || (oper) == '>')


#endif
