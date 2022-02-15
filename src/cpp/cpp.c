#include "../config.h"

#include <string.h>

#include "preprocessor.h"
#include "util.h"

int main(int argc, char *argv[]) {
  FILE *ofp = stdout;
  init_preprocessor(ofp);

  // Predefeined macros.
  define_macro("__XCC");
#if defined(__XV6)
  define_macro("__XV6");
#elif defined(__linux__)
  define_macro("__linux__");
#elif defined(__APPLE__)
  define_macro("__APPLE__");
#endif
#if defined(__NO_FLONUM)
  define_macro("__NO_FLONUM");
#endif

  enum {
    OPT_ISYSTEM = 128,
    OPT_IDIRAFTER,
  };

  static const struct option options[] = {
    {"I", required_argument},  // Add include path
    {"isystem", required_argument, OPT_ISYSTEM},  // Add system include path
    {"idirafter", required_argument, OPT_IDIRAFTER},  // Add include path (after)
    {"D", required_argument},  // Define macro
    {"-version", no_argument, 'V'},
    {0},
  };
  int opt;
  Vector *idirafter = new_vector();
  while ((opt = optparse(argc, argv, options)) != -1) {
    switch (opt) {
    case 'V':
      show_version("cpp");
      return 0;
    case 'I':
      add_system_inc_path(optarg);
      break;
    case OPT_ISYSTEM:
      add_system_inc_path(optarg);
      break;
    case OPT_IDIRAFTER:
      vec_push(idirafter, optarg);
      break;
    case 'D':
      define_macro(optarg);
      break;
    }
  }

  for (int i = 0; i < idirafter->len; ++i)
    add_system_inc_path(idirafter->data[i]);

  int iarg = optind;
  if (iarg < argc) {
    for (int i = iarg; i < argc; ++i) {
      const char *filename = argv[i];
      FILE *fp = fopen(filename, "r");
      if (fp == NULL)
        error("Cannot open file: %s\n", filename);
      fprintf(ofp, "# 1 \"%s\" 1\n", filename);
      preprocess(fp, filename);
      fclose(fp);
    }
  } else {
    preprocess(stdin, "*stdin*");
  }
  return 0;
}
