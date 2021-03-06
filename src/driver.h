/**
 * @file driver.h
 * @brief Driver that takes care of feeding the parser with proper input.
 *
 * The driver stores a set of files, and uses them to parse the input. The content of 
 * some files can be preloaded as string by #driver_set_file. The main entrypoint is 
 * #driver_parse that internally uses #driver_push_file and starts parsing using 
 * `yyparse`. #driver_push_file  pushes a new file to ste stack of used files; if the 
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

//! Allocate memory and iitialize the driver
void driver_init();

/**
 * @brief Preload contents of a file.
 * @note driver makes a local copy of both strings, caller should free 
 * the parameters
 */
void driver_set_file(const char *filename, const char *content);

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
void driver_push_file(const char *filename, int only_once);

/**
 * @brief Remove current file from the stack of included files.
 * Returns 0 if there are no more files, 1 otherwise.
 */
int driver_pop_file();

//! Return the filename of the current file
const char *driver_current_file();
//! Currently scanned line in the current file
int driver_current_line();
//! Currently scanned column in the current line
int driver_current_column();
//! Set the stored position in the current file 
void driver_set_current_pos(int l,int col);

//! Deallocate all memory
void driver_destroy();

#endif
