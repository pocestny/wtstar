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
//! structure to store included files
typedef struct _include_file_t include_file_t;

CONSTRUCTOR(include_file_t, const char *name);

DESTRUCTOR(include_file_t);

typedef struct _include_project_t {
  include_file_t *files;    //!< the list of known included files
  include_file_t *current;  //!< pointer to the list of included files
} include_project_t;

CONSTRUCTOR(include_project_t);

DESTRUCTOR(include_project_t);

typedef struct _extra_t {
  include_project_t *ip;
} yyextra_t;


//! Allocate memory and iitialize the driver
void driver_init(include_project_t *ip);

/**
 * @brief Preload contents of a file.
 * @note driver makes a local copy of both strings, caller should free 
 * the parameters
 */
void driver_set_file(include_project_t *ip, const char *filename, const char *content);

ast_t *driver_parse_from(
  ast_t *ast,
  include_project_t *_ip,
  const char *filename
);

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
ast_t *driver_parse(include_project_t *ip, const char *filename, error_handler_t error_handler, void *error_handler_data);

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
void driver_destroy(include_project_t *ip);

#endif
