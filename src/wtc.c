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

#include <utils.h>
#include <code_generation.h>
#include <driver.h>
#include <errors.h>
#include <writer.h>

// extern int yydebug;
#include <ast_debug_print.h>

char *outf,  //!< name of output file
    *inf,    //!< name of input file
    *infs[10];    //!< names of input files
int ninfs = 0;

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
      infs[ninfs++] = argv[i];
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
  parse_options(argc, argv);
  printf("ninfs=%d outf=%s\n", ninfs, outf);
  if (!ninfs) print_help(argc, argv);
  for(int i = 0; i < ninfs; ++i)
    printf("infs[%d]=%s\n", i, infs[i]);

  register_error_handler(&error_handler);
  //   yydebug=1;

  int num_was_error = 0;
  for(int i = 0; i < ninfs; ++i) {
    inf = infs[i];
    if(ninfs > 1) {
      if(outf) free(outf);
      char *suff = ".out";
      outf = (char *) malloc(1 + strlen(inf) + strlen(suff));
      strcpy(outf, inf);
      strcat(outf, suff);
    }
    
    include_project_t ip;
    driver_init(&ip);
    ast_t *r = driver_parse(&ip, inf);

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

    driver_destroy(&ip);
    ast_t_delete(r);

    writer_t_delete(out);
    num_was_error += was_error;
  }
  return num_was_error;
}
