#ifndef __DRIVER_H__
#define __DRIVER_H__

#include "ast.h"

/* driver constructor */
void driver_init();

/* preload contents of a file;
 * driver makes a copy, caller should free
 */
void driver_set_file(const char *filename, const char *content);

/* parse file and return result
 *
 * driver allocates ast_t, 
 * caller should free it using ast_t_delete()
 *
 */
ast_t *driver_parse(const char *filename);

void driver_push_file(const char *filename);

/* returns 0 if there are no more files */
int driver_pop_file();

const char *driver_current_file();
int driver_current_line();
int driver_current_column();
void driver_set_current_pos(int l,int col);

/* destructor */
void driver_destroy();

#endif
