/**
 * @file driver.h
 * @brief Driver that takes care of feeding the parser with proper input.
 *
 * The driver stores a set of files, and uses them to parse the input. The content of 
 * some files can be preloaded as string by #driver_set_file. The main entrypoint is 
 * #driver_parse that internally uses #driver_push_file and starts parsing using 
 * `yyparse`. #driver_push_file  pushes a new file to the stack of used files; if the 
 * content has been preloaded by #driver_set_file, uses the string as buffer, otherwise 
 * tries to open the file. The #driver_push_file (and #driver_pop_file)  are also used 
 * from the `scanner.l` when an `#include <file>` directive is encountered.
 *
 * The filenames input to #driver_push_file and #driver_parse are normalized relative
 * to currently active path.
 *
 */
#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <ast.h>
#include <parser.h>
#include <scanner.h>

typedef struct {
    int lineno;
} yyextra_t;

//! structure to store included files
typedef struct _include_file_t {
  char *name;           //!< normalized name
  char *content;        //!< content, if preloaded by #driver_set_file
  FILE *f;              //!< if there is no content, open this file
  YY_BUFFER_STATE buf;  //!< if the parsing was interupted by inseting a new
                        //!< file, save the state here
  int lineno,           //!< current line
      col;              //!< current column
  int included;  //!< if the file was already included using #driver_push_file
  struct _include_file_t *next,  //!< next file in the linked list
      *included_from;  //!< pointer to where this file was included from
  yyscan_t scanner; //TODO remove scanner dependency
} include_file_t;

CONSTRUCTOR(include_file_t, const char *name);

DESTRUCTOR(include_file_t);

typedef struct _include_project_t {
  include_file_t *files;    //!< the list of known included files
  include_file_t *current;  //!< pointer to the list of included files
} include_project_t;

CONSTRUCTOR(include_project_t);

DESTRUCTOR(include_project_t);

//! Allocate memory and iitialize the driver
void driver_init();

/**
 * @brief Preload contents of a file.
 * @note driver makes a local copy of both strings, caller should free 
 * the parameters
 */
void driver_set_file(include_project_t *ip, const char *filename, const char *content);

/**
 * @brief Parse file and return the result.
 *
 * Main parsing procedure. The driver allocates the \ref ast_t struct
 * which is the result of the parsing (it wil have the `error_occured`
 * flag set if there were errors; and the errors will be in the error log
 * accessed from errors.h ). Caller should free the returned
 * structure using #ast_t_delete
 *
 */
ast_t *driver_parse(const char *filename);

/**
 * @brief Switch to a new file
 *
 * if `only_once` is set, the file will not be pushed if it was already included
 *
 * @todo When in web mode, and the content of a file is not preloaded, it should
 * not try to open the file (there is no filesystem present)
 */
void driver_push_file(include_project_t *ip, const char *filename, int only_once, void* scanner);

/**
 * @brief Remove current file from the stack of included files.
 * Returns 0 if there are no more files, 1 otherwise.
 */
int driver_pop_file();

//! Return the filename of the current file
const char *driver_current_file(include_file_t *current);
//! Currently scanned line in the current file
int driver_current_line(include_file_t *current);
//! Currently scanned column in the current line
int driver_current_column(include_file_t *current);
//! Set the stored position in the current file 
void driver_set_current_pos(include_file_t *current,int l,int col);

//! Deallocate all memory
void driver_destroy();

#endif
