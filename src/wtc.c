/**
 * @file  wtc.c
 * @brief Compiler from the WT language to the virtual machine binary.
 *
 * Command line compiler. Supported options:
 *      option   | meaning
 *  -------------|-------------
 *   -o file     | write output to file (default is a.out)
 *   -x          | don't write debug info
 *   -D          | print intermediate AST instead of code
 *
 * @deprecated The -D option uses ast_debug_print.h which is terribly outdated
 * and incoplete. Not intended for use.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code_generation.h>
#include <driver.h>
#include <errors.h>
#include <writer.h>

// extern int yydebug;
#include <ast_debug_print.h>

char *outf,  //!< name of output file
    *inf;    //!< name of input file

int ast_debug = 0,  //!< flag: -D option enabled
    no_debug  = 0,  //!< flag: -x option enabled
    outf_spec = 0;  //!< flag: -o option enabled

//! Print usage options.
void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D][-o file] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-o file       write output to file \n");
  printf("-x            don't write debug info \n");
  printf("-D            print intermediate AST instead of code \n");
  exit(0);
}

//! Parse command line options and set global variables.
void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else if ((!strcmp(argv[i], "-o"))) {
      if (++i < argc)
        outf = argv[i];
      else {
        print_help(argc, argv);
        exit(1);
      }
      outf_spec = 1;
    } else if (!strcmp(argv[i], "-D")) {
      ast_debug = 1;
    } else if (!strcmp(argv[i], "-x")) {
      no_debug = 1;
    } else
      inf = argv[i];
}

//! Error handler used in errors.h Here just prints the error to stderr.
void error_handler(error_t *err) {
  fprintf(stderr, "%s\n", err->msg->str.base);
}

/**
 * @brief Main entry.
 *
 * Uses errors.h for error handling. Not needed here, since we only print the errors
 * to stdout, but the copmilation should work also in web mode, where the errors 
 * need to be stored.
 *
 * It initializes the driver from driver.h and feeds it the file from the command line
 * to parse.
 *
 * It sets the \ref writer_t from writer.h to the appropriate file, and calls #emit_code
 * from code_generation.h
 * 
 */
int main(int argc, char **argv) {
  outf = NULL;
  inf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  register_error_handler(&error_handler);
  //   yydebug=1;

  driver_init();
  ast_t *r = driver_parse(inf);

  writer_t *out;
  out = writer_t_new(WRITER_FILE);

  if (ast_debug) {
    if (!outf || !strcmp(outf, "-"))
      out->f = stdout;
    else
      out->f = fopen(outf, "wt");
  } else {
    if (outf && !strcmp(outf, "-"))
      out->f = stdout;
    else {
      if (!outf) outf = "a.out";
      out->f = fopen(outf, "wb");
    }
  }

  int was_error = 0;
  if (r->error_occured) {
    was_error = 1;
    error_t *err = error_t_new();
    append_error_msg(err, "there were errors");
    emit_error(err);
  } else if (ast_debug)
    ast_debug_print(r, out);
  else if (emit_code(r, out, no_debug)) {
    was_error = 1;
    error_t *err = error_t_new();
    append_error_msg(err, "there were errors");
    emit_error(err);
  }

  driver_destroy();
  ast_t_delete(r);

  writer_t_delete(out);
  return was_error;
}
