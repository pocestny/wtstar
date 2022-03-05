/**
 * @file debug.h
 * @brief take care of handling debugging info
 *
 * #emit_debug_section writes the debug info into binary. The #debug_info_t
 * structure is part of #virtual_machine_t and is read from binary in the
 * constructor.
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <ast.h>
#include <utils.h>
#include <writer.h>

#define MAP_SENTINEL 0xfffffffeU

//! basic info about a syntactic element
typedef struct {
  uint32_t fileid,  //!< index of the source file
      fl,           //!< first line
      fc,           //!< first column
      ll,           //!< last line
      lc;           //!< last column
} item_info_t;

/**
 * @brief compressed information about code
 *
 * Stores a list of values. There are n breakpoints, such that for each position
 * between breakpoint bp[i], and bp[i+1] the value is val[i]
 */
typedef struct {
  uint32_t *bp,  //!< breapoints
      n;         //!< number of breakpoints
  int32_t *val;  //!< values for breapoints
} code_map_t;

//! constructor
CONSTRUCTOR(code_map_t, const uint8_t *in, int *pos, const int len);
//! destructor
DESTRUCTOR(code_map_t);
//! returns the index of the breakpoint that corresponds to the position pos, or
//! -1
int code_map_find(code_map_t *m, uint32_t pos);

//! info about static types
typedef struct {
  char *name;           //!< type name
  uint32_t n_members,   //!< number of members
      *member_types;    //!< indices of member types
  char **member_names;  //!< names of member types
} type_info_t;

//! read in the type_info_t from a binary secion in[*pos] of length len,
//! and increment update *pos
int populate_type_info(type_info_t *t, const uint8_t *in, int *pos,
                       const int len);
//! destructor
void clear_type_info(type_info_t *t);

//! info about variables
typedef struct {
  char *name;     //!< name
  uint32_t type,  //!< index of type (type_info_t)
      num_dim,    //!< number of dimensions
      from_code,  //!< starting position of the initializer code
      addr;       //!< address in data
} variable_info_t;

//! info about lexical scope
typedef struct {
  uint32_t parent,        //!< index of parent scope
      n_vars;             //!< number of variables
  variable_info_t *vars;  //!< info about local variables
} scope_info_t;

//! debugging info
typedef struct {
  char **files;      //!< names of source files
  uint32_t n_files;  //!< number of source files

  char **fn_names;     //!< names of functions
  uint32_t *fn_items;  //!< item info representing functions
  uint32_t n_fn;       //!< number of functions

  item_info_t *items;  //!< lexical items
  uint32_t n_items;    //!< number of lexical items

  code_map_t
      *source_items_map;  //!< maps code position to lexical item in #items

  uint32_t n_types;    //!< number of types
  type_info_t *types;  //!< info about types

  code_map_t *scope_map;  //!< maps code position to scope

  uint32_t n_scopes;     //!< number of scopes
  scope_info_t *scopes;  //!< info about scopes

} debug_info_t;

//! constructor, read the info from binary in[*pos] of length len
//! and update *pos
CONSTRUCTOR(debug_info_t, const uint8_t *in, int *pos, const int len);
//! destructor
DESTRUCTOR(debug_info_t);

//! given an ast_t, write the debug info section to binary writer #out
void emit_debug_section(writer_t *out, ast_t *ast, int _code_size);

#endif
