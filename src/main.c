#define _XOPEN_SOURCE 700
#include "chibicc.h"

typedef enum {
  FILE_NONE, FILE_C, FILE_HEADER, FILE_ASM, FILE_OBJ, FILE_AR, FILE_DSO, FILE_IR,
} FileType;

typedef enum { BACKEND_X86_64, BACKEND_LLVM } Backend;

StringArray include_paths;
bool opt_fcommon = true;
bool opt_fpic;
PpcDispatchMode opt_ppc_dispatch;

static FileType opt_x;
static StringArray opt_include;
static bool opt_E;
static bool opt_M;
static bool opt_MD;
static bool opt_MMD;
static bool opt_MP;
static bool opt_S;
static bool opt_emit_llvm;
static bool opt_closed_program;
static StringArray opt_closed_families;
static char *opt_closed_report;
static Backend opt_backend;
static bool opt_backend_explicit;
static char *opt_optimization = "0";
static bool opt_c;
static bool opt_cc1;
static bool opt_hash_hash_hash;
static bool opt_static;
static bool opt_shared;
static char *opt_MF;
static char *opt_MT;
static char *opt_o;

static StringArray ld_extra_args;
static StringArray std_include_paths;

char *base_file;
static char *output_file;

static StringArray input_paths;
static StringArray tmpfiles;
static char *closed_tmpdir;

static char *closed_object_header = "; ppchibicc closed-program object v1\n";

static void usage(int status) {
  fprintf(stderr, "ppchibicc [ -o <path> ] <file>\n");
  fprintf(stderr, "  -S -emit-llvm  emit raw experimental LLVM IR (.ll), without LLVM tools\n");
  fprintf(stderr, "  --backend=llvm  compile C or .ll/.bc using external opt and llc\n");
  fprintf(stderr, "  --backend=x86-64  use the original backend (default)\n");
  fprintf(stderr, "  --ppc-dispatch=auto|runtime|table|switch  guarded closed-family dispatch\n");
  fprintf(stderr, "  --closed-program  build a complete LLVM program; -c stores raw IR objects\n");
  fprintf(stderr, "  --closed-family=NAME  require a resolved closed family (repeatable)\n");
  fprintf(stderr, "  --closed-report=PATH  final closure manifest (default: OUTPUT.closed.json)\n");
  fprintf(stderr, "  -O0/-O1/-O2/-O3/-Os/-Oz  LLVM optimization level (default: -O0)\n");
  fprintf(stderr, "  PPCHIBICC_LLVM_OPT / PPCHIBICC_LLVM_LLC override tool paths\n");
  fprintf(stderr, "  .c/.ppc are source files; .h/.pph are headers (-E/-M, or explicit -x c)\n");
  exit(status);
}

static bool take_arg(char *arg) {
  char *x[] = {
    "-o", "-I", "-idirafter", "-include", "-x", "-MF", "-MT", "-Xlinker",
  };

  for (int i = 0; i < sizeof(x) / sizeof(*x); i++)
    if (!strcmp(arg, x[i]))
      return true;
  return false;
}

static void add_default_include_paths(char *argv0) {
  char path[4096];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (0 < len && len < sizeof(path) - 1) {
    path[len] = '\0';
    argv0 = path;
  }

  // We expect that ppchibicc-specific include files are installed to
  // ../include relative to the compiler executable.
  strarray_push(&include_paths, format("%s/../include", dirname(strdup(argv0))));

  // Add standard include paths.
  strarray_push(&include_paths, "/usr/local/include");
  strarray_push(&include_paths, "/usr/include/x86_64-linux-gnu");
  strarray_push(&include_paths, "/usr/include");

  // Keep a copy of the standard include paths for -MMD option.
  for (int i = 0; i < include_paths.len; i++)
    strarray_push(&std_include_paths, include_paths.data[i]);
}

static void define(char *str) {
  char *eq = strchr(str, '=');
  if (eq)
    define_macro(strndup(str, eq - str), eq + 1);
  else
    define_macro(str, "1");
}

static FileType parse_opt_x(char *s) {
  if (!strcmp(s, "c"))
    return FILE_C;
  if (!strcmp(s, "assembler"))
    return FILE_ASM;
  if (!strcmp(s, "ir"))
    return FILE_IR;
  if (!strcmp(s, "none"))
    return FILE_NONE;
  error("<command line>: unknown argument for -x: %s", s);
}

static char *quote_makefile(char *s) {
  char *buf = calloc(1, strlen(s) * 2 + 1);

  for (int i = 0, j = 0; s[i]; i++) {
    switch (s[i]) {
    case '$':
      buf[j++] = '$';
      buf[j++] = '$';
      break;
    case '#':
      buf[j++] = '\\';
      buf[j++] = '#';
      break;
    case ' ':
    case '\t':
      for (int k = i - 1; k >= 0 && s[k] == '\\'; k--)
        buf[j++] = '\\';
      buf[j++] = '\\';
      buf[j++] = s[i];
      break;
    default:
      buf[j++] = s[i];
      break;
    }
  }
  return buf;
}

static void parse_args(int argc, char **argv) {
  // Make sure that all command line options that take an argument
  // have an argument.
  for (int i = 1; i < argc; i++)
    if (take_arg(argv[i]))
      if (!argv[++i])
        usage(1);

  StringArray idirafter = {};

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-###")) {
      opt_hash_hash_hash = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1")) {
      opt_cc1 = true;
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0);

    if (!strcmp(argv[i], "-o")) {
      opt_o = argv[++i];
      continue;
    }

    if (!strncmp(argv[i], "-o", 2)) {
      opt_o = argv[i] + 2;
      continue;
    }

    if (!strcmp(argv[i], "-S")) {
      opt_S = true;
      continue;
    }

    if (!strcmp(argv[i], "-emit-llvm")) {
      opt_emit_llvm = true;
      continue;
    }

    if (!strcmp(argv[i], "--closed-program")) {
      opt_closed_program = true;
      continue;
    }
    if (!strncmp(argv[i], "--closed-family=", 16)) {
      if (!argv[i][16]) error("empty --closed-family");
      strarray_push(&opt_closed_families, argv[i] + 16);
      continue;
    }
    if (!strncmp(argv[i], "--closed-report=", 16)) {
      opt_closed_report = argv[i] + 16;
      if (!*opt_closed_report) error("empty --closed-report");
      continue;
    }

    if (!strncmp(argv[i], "--ppc-dispatch=", 15)) {
      char *mode = argv[i] + 15;
      if (!strcmp(mode, "auto")) opt_ppc_dispatch = PPC_DISPATCH_AUTO;
      else if (!strcmp(mode, "runtime")) opt_ppc_dispatch = PPC_DISPATCH_RUNTIME;
      else if (!strcmp(mode, "table")) opt_ppc_dispatch = PPC_DISPATCH_TABLE;
      else if (!strcmp(mode, "switch")) opt_ppc_dispatch = PPC_DISPATCH_SWITCH;
      else error("unknown PPC dispatch mode: %s (expected auto, runtime, table or switch)", mode);
      continue;
    }

    if (!strncmp(argv[i], "--backend=", 10)) {
      char *name = argv[i] + 10;
      if (!strcmp(name, "llvm"))
        opt_backend = BACKEND_LLVM;
      else if (!strcmp(name, "x86-64"))
        opt_backend = BACKEND_X86_64;
      else
        error("unknown backend: %s (expected x86-64 or llvm)", name);
      opt_backend_explicit = true;
      continue;
    }

    if (!strncmp(argv[i], "-O", 2)) {
      opt_optimization = argv[i][2] ? argv[i] + 2 : "1";
      continue;
    }

    if (!strcmp(argv[i], "-fcommon")) {
      opt_fcommon = true;
      continue;
    }

    if (!strcmp(argv[i], "-fno-common")) {
      opt_fcommon = false;
      continue;
    }

    if (!strcmp(argv[i], "-c")) {
      opt_c = true;
      continue;
    }

    if (!strcmp(argv[i], "-E")) {
      opt_E = true;
      continue;
    }

    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(&include_paths, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-D")) {
      define(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-D", 2)) {
      define(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-U")) {
      undef_macro(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-U", 2)) {
      undef_macro(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-include")) {
      strarray_push(&opt_include, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-x")) {
      opt_x = parse_opt_x(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-x", 2)) {
      opt_x = parse_opt_x(argv[i] + 2);
      continue;
    }

    if (!strncmp(argv[i], "-l", 2) || !strncmp(argv[i], "-Wl,", 4)) {
      strarray_push(&input_paths, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-Xlinker")) {
      strarray_push(&ld_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-s")) {
      strarray_push(&ld_extra_args, "-s");
      continue;
    }

    if (!strcmp(argv[i], "-M")) {
      opt_M = true;
      continue;
    }

    if (!strcmp(argv[i], "-MF")) {
      opt_MF = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-MP")) {
      opt_MP = true;
      continue;
    }

    if (!strcmp(argv[i], "-MT")) {
      if (opt_MT == NULL)
        opt_MT = argv[++i];
      else
        opt_MT = format("%s %s", opt_MT, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-MD")) {
      opt_MD = true;
      continue;
    }

    if (!strcmp(argv[i], "-MQ")) {
      if (opt_MT == NULL)
        opt_MT = quote_makefile(argv[++i]);
      else
        opt_MT = format("%s %s", opt_MT, quote_makefile(argv[++i]));
      continue;
    }

    if (!strcmp(argv[i], "-MMD")) {
      opt_MD = opt_MMD = true;
      continue;
    }

    if (!strcmp(argv[i], "-fpic") || !strcmp(argv[i], "-fPIC")) {
      opt_fpic = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1-input")) {
      base_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-output")) {
      output_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-idirafter")) {
      strarray_push(&idirafter, argv[i++]);
      continue;
    }

    if (!strcmp(argv[i], "-static")) {
      opt_static = true;
      strarray_push(&ld_extra_args, "-static");
      continue;
    }

    if (!strcmp(argv[i], "-shared")) {
      opt_shared = true;
      strarray_push(&ld_extra_args, "-shared");
      continue;
    }

    if (!strcmp(argv[i], "-L")) {
      strarray_push(&ld_extra_args, "-L");
      strarray_push(&ld_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-L", 2)) {
      strarray_push(&ld_extra_args, "-L");
      strarray_push(&ld_extra_args, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-hashmap-test")) {
      hashmap_test();
      exit(0);
    }

    // These options are ignored for now.
    if (!strncmp(argv[i], "-W", 2) ||
        !strncmp(argv[i], "-g", 2) ||
        !strncmp(argv[i], "-std=", 5) ||
        !strcmp(argv[i], "-ffreestanding") ||
        !strcmp(argv[i], "-fno-builtin") ||
        !strcmp(argv[i], "-fno-omit-frame-pointer") ||
        !strcmp(argv[i], "-fno-stack-protector") ||
        !strcmp(argv[i], "-fno-strict-aliasing") ||
        !strcmp(argv[i], "-m64") ||
        !strcmp(argv[i], "-mno-red-zone") ||
        !strcmp(argv[i], "-w"))
      continue;

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(&input_paths, argv[i]);
  }

  for (int i = 0; i < idirafter.len; i++)
    strarray_push(&include_paths, idirafter.data[i]);

  if (input_paths.len == 0)
    error("no input files");

  if (opt_closed_program) {
    if (opt_backend_explicit && opt_backend != BACKEND_LLVM)
      error("--closed-program requires the LLVM backend");
    if (opt_shared)
      error("--closed-program builds executables, not shared libraries");
    if (opt_c && (opt_closed_families.len || opt_closed_report))
      error("--closed-family and --closed-report are link-stage options");
    opt_backend = BACKEND_LLVM;
  } else if (opt_closed_families.len || opt_closed_report) {
    error("--closed-family and --closed-report require --closed-program");
  }

  if (opt_emit_llvm && (!opt_S || opt_c || opt_E || opt_M))
    error("-emit-llvm requires -S and cannot be combined with -c, -E or -M");
  if (opt_emit_llvm && opt_backend_explicit && opt_backend != BACKEND_LLVM)
    error("-emit-llvm conflicts with --backend=x86-64");
  if (opt_backend == BACKEND_LLVM && !opt_emit_llvm &&
      (strlen(opt_optimization) != 1 || !strchr("0123sz", opt_optimization[0])))
    error("unsupported LLVM optimization level: -O%s (use -O0, -O1, -O2, -O3, -Os or -Oz)", opt_optimization);

  // -E implies that the input is the C macro language.
  if (opt_E)
    opt_x = FILE_C;
}

static FILE *open_file(char *path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;

  FILE *out = fopen(path, "w");
  if (!out)
    error("cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

static bool endswith(char *p, char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

// Replace file extension
static char *replace_extn(char *tmpl, char *extn) {
  char *filename = basename(strdup(tmpl));
  char *dot = strrchr(filename, '.');
  if (dot)
    *dot = '\0';
  return format("%s%s", filename, extn);
}

static void cleanup(void) {
  for (int i = 0; i < tmpfiles.len; i++)
    unlink(tmpfiles.data[i]);
  if (closed_tmpdir)
    rmdir(closed_tmpdir);
}

static char *create_tmpfile(void) {
  char *path = strdup("/tmp/chibicc-XXXXXX");
  int fd = mkstemp(path);
  if (fd == -1)
    error("mkstemp failed: %s", strerror(errno));
  close(fd);

  strarray_push(&tmpfiles, path);
  return path;
}

static char *create_closed_tmpfile(char *suffix) {
  static int id;
  if (!closed_tmpdir) {
    char *dir = strdup("/tmp/chibicc-closed-XXXXXX");
    if (!mkdtemp(dir)) error("mkdtemp: %s", strerror(errno));
    closed_tmpdir = dir;
  }
  char *path = format("%s/%d%s", closed_tmpdir, id++, suffix);
  strarray_push(&tmpfiles, path);
  return path;
}

static void run_subprocess(char **argv) {
  // If -### is given, dump the subprocess's command line.
  if (opt_hash_hash_hash) {
    fprintf(stderr, "%s", argv[0]);
    for (int i = 1; argv[i]; i++)
      fprintf(stderr, " %s", argv[i]);
    fprintf(stderr, "\n");
  }

  if (fork() == 0) {
    // Child process. Run a new command.
    execvp(argv[0], argv);
    fprintf(stderr, "exec failed: %s: %s\n", argv[0], strerror(errno));
    _exit(1);
  }

  // Wait for the child process to finish.
  int status;
  while (wait(&status) > 0);
  if (status != 0)
    exit(1);
}

static void run_cc1(int argc, char **argv, char *input, char *output) {
  char **args = calloc(argc + 10, sizeof(char *));
  memcpy(args, argv, argc * sizeof(char *));
  args[argc++] = "-cc1";

  if (input) {
    args[argc++] = "-cc1-input";
    args[argc++] = input;
  }

  if (output) {
    args[argc++] = "-cc1-output";
    args[argc++] = output;
  }

  run_subprocess(args);
}

// Print tokens to stdout. Used for -E.
static void print_tokens(Token *tok) {
  FILE *out = open_file(opt_o ? opt_o : "-");

  int line = 1;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (line > 1 && tok->at_bol)
      fprintf(out, "\n");
    if (tok->has_space && !tok->at_bol)
      fprintf(out, " ");
    fprintf(out, "%.*s", tok->len, tok->loc);
    line++;
  }
  fprintf(out, "\n");
}

static bool in_std_include_path(char *path) {
  for (int i = 0; i < std_include_paths.len; i++) {
    char *dir = std_include_paths.data[i];
    int len = strlen(dir);
    if (strncmp(dir, path, len) == 0 && path[len] == '/')
      return true;
  }
  return false;
}

// If -M options is given, the compiler write a list of input files to
// stdout in a format that "make" command can read. This feature is
// used to automate file dependency management.
static void print_dependencies(void) {
  char *path;
  if (opt_MF)
    path = opt_MF;
  else if (opt_MD)
    path = replace_extn(opt_o ? opt_o : base_file, ".d");
  else if (opt_o)
    path = opt_o;
  else
    path = "-";

  FILE *out = open_file(path);
  if (opt_MT)
    fprintf(out, "%s:", opt_MT);
  else
    fprintf(out, "%s:", quote_makefile(replace_extn(base_file, ".o")));

  File **files = get_input_files();

  for (int i = 0; files[i]; i++) {
    if (opt_MMD && in_std_include_path(files[i]->name))
      continue;
    fprintf(out, " \\\n  %s", quote_makefile(files[i]->name));
  }

  fprintf(out, "\n\n");

  if (opt_MP) {
    for (int i = 1; files[i]; i++) {
      if (opt_MMD && in_std_include_path(files[i]->name))
        continue;
      fprintf(out, "%s:\n\n", quote_makefile(files[i]->name));
    }
  }
}

static Token *must_tokenize_file(char *path) {
  Token *tok = tokenize_file(path);
  if (!tok)
    error("%s: %s", path, strerror(errno));
  return tok;
}

static Token *append_tokens(Token *tok1, Token *tok2) {
  if (!tok1 || tok1->kind == TK_EOF)
    return tok2;

  Token *t = tok1;
  while (t->next->kind != TK_EOF)
    t = t->next;
  t->next = tok2;
  return tok1;
}

static void cc1(void) {
  Token *tok = NULL;

  // Process -include option
  for (int i = 0; i < opt_include.len; i++) {
    char *incl = opt_include.data[i];

    char *path;
    if (file_exists(incl)) {
      path = incl;
    } else {
      path = search_include_paths(incl);
      if (!path)
        error("-include: %s: %s", incl, strerror(errno));
    }

    Token *tok2 = must_tokenize_file(path);
    tok = append_tokens(tok, tok2);
  }

  // Tokenize and parse.
  Token *tok2 = must_tokenize_file(base_file);
  tok = append_tokens(tok, tok2);
  tok = preprocess(tok);

  // If -M or -MD are given, print file dependencies.
  if (opt_M || opt_MD) {
    print_dependencies();
    if (opt_M)
      return;
  }

  // If -E is given, print out preprocessed C code as a result.
  if (opt_E) {
    print_tokens(tok);
    return;
  }

  if (opt_ppc_dispatch == PPC_DISPATCH_AUTO)
    opt_ppc_dispatch = opt_emit_llvm || opt_backend == BACKEND_LLVM ?
                       PPC_DISPATCH_SWITCH : PPC_DISPATCH_RUNTIME;
  Obj *prog = parse(tok);

  // Open a temporary output buffer.
  char *buf;
  size_t buflen;
  FILE *output_buf = open_memstream(&buf, &buflen);

  // Both backends consume the same parsed and typed program.
  if (opt_emit_llvm || opt_backend == BACKEND_LLVM)
    codegen_llvm(prog, output_buf);
  else
    codegen(prog, output_buf);
  fclose(output_buf);

  // Write the generated text only after successful code generation.
  FILE *out = open_file(output_file);
  fwrite(buf, buflen, 1, out);
  fclose(out);
}

static void assemble(char *input, char *output) {
  char *cmd[] = {"as", "-c", input, "-o", output, NULL};
  run_subprocess(cmd);
}

static char *llvm_tool(char *variable, char *fallback) {
  char *path = getenv(variable);
  return path && *path ? path : fallback;
}

// Finish both external stages before opening the requested output. Temporary
// bitcode and machine output are removed by cleanup, including on tool failure.
static char *compile_llvm(char *input, bool assembly) {
  char *bitcode = create_tmpfile();
  char *output = create_tmpfile();
  bool size = strchr("sz", opt_optimization[0]);
  char *level = size ? "2" : opt_optimization;
  StringArray opt_cmd = {};
  strarray_push(&opt_cmd, llvm_tool("PPCHIBICC_LLVM_OPT", "opt"));
  strarray_push(&opt_cmd, "-mcpu=x86-64");
  strarray_push(&opt_cmd, size ? "-passes=forceattrs,default<O2>" :
                format("-passes=default<O%s>", level));
  strarray_push(&opt_cmd, "-verify-each");
  // LLVM 23 removed default<Os>/default<Oz>. Size is now expressed by
  // function attributes; forceattrs also handles supplied .ll/.bc inputs.
  if (size)
    strarray_push(&opt_cmd, "-force-attribute=optsize");
  if (!strcmp(opt_optimization, "z"))
    strarray_push(&opt_cmd, "-force-attribute=minsize");
  strarray_push(&opt_cmd, input);
  strarray_push(&opt_cmd, "-o");
  strarray_push(&opt_cmd, bitcode);
  strarray_push(&opt_cmd, NULL);
  run_subprocess(opt_cmd.data);

  char *llc_cmd[] = {
    llvm_tool("PPCHIBICC_LLVM_LLC", "llc"), "-mcpu=x86-64",
    format("-O=%s", level), "-relocation-model=pic",
    assembly ? "-filetype=asm" : "-filetype=obj", bitcode, "-o", output, NULL,
  };
  run_subprocess(llc_cmd);
  return output;
}

static void copy_output(char *input, char *output) {
  FILE *in = fopen(input, "rb");
  if (!in)
    error("cannot read LLVM output: %s: %s", input, strerror(errno));
  FILE *out = open_file(output);
  char buf[8192];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), in)))
    if (fwrite(buf, 1, len, out) != len)
      error("cannot write output: %s: %s", output, strerror(errno));
  if (ferror(in))
    error("cannot read LLVM output: %s: %s", input, strerror(errno));
  fclose(in);
  if (fflush(out))
    error("cannot write output: %s: %s", output, strerror(errno));
  if (out != stdout && fclose(out))
    error("cannot close output: %s: %s", output, strerror(errno));
}

static char *find_file(char *pattern) {
  char *path = NULL;
  glob_t buf = {};
  glob(pattern, 0, NULL, &buf);
  if (buf.gl_pathc > 0)
    path = strdup(buf.gl_pathv[buf.gl_pathc - 1]);
  globfree(&buf);
  return path;
}

// Returns true if a given file exists.
bool file_exists(char *path) {
  struct stat st;
  return !stat(path, &st);
}

static char *find_libpath(void) {
  if (file_exists("/usr/lib/x86_64-linux-gnu/crti.o"))
    return "/usr/lib/x86_64-linux-gnu";
  if (file_exists("/usr/lib64/crti.o"))
    return "/usr/lib64";
  error("library path is not found");
}

static char *find_gcc_libpath(void) {
  char *paths[] = {
    "/usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o",
    "/usr/lib/gcc/x86_64-pc-linux-gnu/*/crtbegin.o", // For Gentoo
    "/usr/lib/gcc/x86_64-redhat-linux/*/crtbegin.o", // For Fedora
  };

  for (int i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    char *path = find_file(paths[i]);
    if (path)
      return dirname(path);
  }

  error("gcc library path is not found");
}

static void run_linker(StringArray *inputs, char *output) {
  StringArray arr = {};

  strarray_push(&arr, "ld");
  strarray_push(&arr, "-o");
  strarray_push(&arr, output);
  strarray_push(&arr, "-m");
  strarray_push(&arr, "elf_x86_64");

  char *libpath = find_libpath();
  char *gcc_libpath = find_gcc_libpath();

  if (opt_shared) {
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbeginS.o", gcc_libpath));
  } else {
    strarray_push(&arr, format("%s/crt1.o", libpath));
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbegin.o", gcc_libpath));
  }

  strarray_push(&arr, format("-L%s", gcc_libpath));
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib64");
  strarray_push(&arr, "-L/lib64");
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-pc-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-redhat-linux");
  strarray_push(&arr, "-L/usr/lib");
  strarray_push(&arr, "-L/lib");

  if (!opt_static) {
    strarray_push(&arr, "-dynamic-linker");
    strarray_push(&arr, "/lib64/ld-linux-x86-64.so.2");
  }

  for (int i = 0; i < ld_extra_args.len; i++)
    strarray_push(&arr, ld_extra_args.data[i]);

  for (int i = 0; i < inputs->len; i++)
    strarray_push(&arr, inputs->data[i]);

  if (opt_static) {
    strarray_push(&arr, "--start-group");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "-lgcc_eh");
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "--end-group");
  } else {
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "--as-needed");
    strarray_push(&arr, "-lgcc_s");
    strarray_push(&arr, "--no-as-needed");
  }

  if (opt_shared)
    strarray_push(&arr, format("%s/crtendS.o", gcc_libpath));
  else
    strarray_push(&arr, format("%s/crtend.o", gcc_libpath));

  strarray_push(&arr, format("%s/crtn.o", libpath));
  strarray_push(&arr, NULL);

  run_subprocess(arr.data);
}

static FileType get_file_type(char *filename) {
  if (opt_x != FILE_NONE)
    return opt_x;

  if (endswith(filename, ".a"))
    return FILE_AR;
  if (endswith(filename, ".so"))
    return FILE_DSO;
  if (endswith(filename, ".o"))
    return FILE_OBJ;
  if (endswith(filename, ".c") || endswith(filename, ".ppc"))
    return FILE_C;
  if (endswith(filename, ".h") || endswith(filename, ".pph"))
    return FILE_HEADER;
  if (endswith(filename, ".s"))
    return FILE_ASM;
  if (endswith(filename, ".ll") || endswith(filename, ".bc"))
    return FILE_IR;

  error("<command line>: unknown file extension: %s", filename);
}

static bool is_closed_object(char *path) {
  FILE *in = fopen(path, "rb");
  if (!in) return false;
  char buf[128] = {};
  size_t n = fread(buf, 1, strlen(closed_object_header), in);
  fclose(in);
  return n == strlen(closed_object_header) && !memcmp(buf, closed_object_header, n);
}

static char *closed_helper(char *argv0) {
  char *override = getenv("PPCHIBICC_STATIC_DISPATCH");
  if (override && *override) return override;
  char executable[4096];
  ssize_t len = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  if (len > 0 && len < sizeof(executable) - 1) {
    executable[len] = '\0';
    argv0 = executable;
  }
  char *dir = dirname(strdup(argv0));
  char *installed = format("%s/../share/ppchibicc/ppc-static-dispatch.py", dir);
  if (file_exists(installed)) return installed;
  char *source = format("%s/../cmake/ppc-static-dispatch.py", dir);
  if (file_exists(source)) return source;
  error("closed-program helper not found; set PPCHIBICC_STATIC_DISPATCH");
}

static char *canonical_output(char *path) {
  char *resolved = realpath(path, NULL);
  if (resolved) return resolved;
  char *parent = realpath(dirname(strdup(path)), NULL);
  return parent ? format("%s/%s", parent, basename(strdup(path))) : path;
}

static char *output_temporary(char *path) {
  struct stat st;
  if (!stat(path, &st) && S_ISDIR(st.st_mode)) error("output is a directory: %s", path);
  char *temporary = format("%s.ppchibicc-XXXXXX", path);
  int fd = mkstemp(temporary);
  if (fd < 0) error("cannot prepare output %s: %s", path, strerror(errno));
  close(fd);
  strarray_push(&tmpfiles, temporary);
  return temporary;
}

static void build_closed_program(int argc, char **argv) {
  if (opt_MD && !opt_c)
    error("--closed-program dependency generation requires separate -c compilation");
  for (int i = 0; i < ld_extra_args.len; i++)
    if (strcmp(ld_extra_args.data[i], "-static") && strcmp(ld_extra_args.data[i], "-s"))
      error("unsupported closed-program linker option: %s", ld_extra_args.data[i]);
  char *output = opt_o ? opt_o : opt_c ? NULL : opt_S ?
    (input_paths.len == 1 ? replace_extn(input_paths.data[0], opt_emit_llvm ? ".ll" : ".s") :
     opt_emit_llvm ? "a.ll" : "a.s") : "a.out";
  if (output && !strcmp(output, "-") && !opt_S && !opt_c)
    error("closed-program executable requires a file output");
  char *report = opt_c ? NULL : opt_closed_report ? opt_closed_report :
    strcmp(output, "-") ? format("%s.closed.json", output) : NULL;
  if (report && !strcmp(report, "-")) error("closed report requires a file output");
  if (report && !strcmp(canonical_output(report), canonical_output(output)))
    error("closed report and program output must be distinct");
  for (int i = 0; i < input_paths.len; i++) {
    char *input = input_paths.data[i];
    if ((output && strcmp(output, "-") && !strcmp(canonical_output(output), canonical_output(input))) ||
        (report && !strcmp(canonical_output(report), canonical_output(input))))
      error("closed-program output would overwrite an input: %s", input);
  }
  StringArray modules = {}, libraries = {};
  for (int i = 0; i < input_paths.len; i++) {
    char *input = input_paths.data[i];
    if (!strcmp(input, "-lm")) {
      if (opt_c || opt_S) error("-lm requires executable linking");
      strarray_push(&libraries, input);
      continue;
    }
    if (input[0] == '-' && strcmp(input, "-"))
      error("unsupported closed-program input/linker flag: %s", input);
    FileType type = get_file_type(input);
    if (opt_c && type != FILE_C)
      error("closed-program -c requires C/4P source: %s", input);
    if (type == FILE_C) {
      char *ir = create_closed_tmpfile(".ll");
      run_cc1(argc, argv, input, ir);
      if (opt_c) {
        char *dest = output ? output : replace_extn(input, ".o");
        if (strcmp(dest, "-") && !strcmp(canonical_output(dest), canonical_output(input)))
          error("closed-program output would overwrite an input: %s", input);
        char *object = create_closed_tmpfile(".o");
        FILE *in = fopen(ir, "rb");
        FILE *out = fopen(object, "wb");
        if (!in || !out) error("cannot prepare closed-program object");
        fputs(closed_object_header, out);
        char buf[8192];
        size_t count;
        while ((count = fread(buf, 1, sizeof(buf), in)))
          if (fwrite(buf, 1, count, out) != count) error("cannot write closed-program object");
        if (ferror(in) || fclose(out)) error("cannot finish closed-program object");
        fclose(in);
        copy_output(object, dest);
      } else {
        strarray_push(&modules, ir);
      }
    } else if (type == FILE_IR && endswith(input, ".ll")) {
      strarray_push(&modules, input);
    } else if (type == FILE_OBJ && is_closed_object(input)) {
      strarray_push(&modules, input);
    } else {
      error("closed program requires source, raw .ll, or --closed-program -c objects: %s", input);
    }
  }
  if (opt_c) return;
  if (!modules.len) error("closed program has no user modules");
  char *ir = create_closed_tmpfile(".ll");
  char *audit = create_closed_tmpfile(".json");
  StringArray command = {};
  strarray_push(&command, llvm_tool("PPCHIBICC_PYTHON", "python3"));
  strarray_push(&command, closed_helper(argv[0]));
  strarray_push(&command, "--closed-program");
  strarray_push(&command, "--allow-empty");
  strarray_push(&command, "--quiet");
  strarray_push(&command, "--llvm-as");
  strarray_push(&command, llvm_tool("PPCHIBICC_LLVM_AS", "llvm-as"));
  strarray_push(&command, "--llvm-opt");
  strarray_push(&command, llvm_tool("PPCHIBICC_LLVM_OPT", "opt"));
  strarray_push(&command, "--llvm-link");
  strarray_push(&command, llvm_tool("PPCHIBICC_LLVM_LINK", "llvm-link"));
  if (!opt_emit_llvm) {
    strarray_push(&command, "--llvm-llc");
    strarray_push(&command, llvm_tool("PPCHIBICC_LLVM_LLC", "llc"));
  }
  strarray_push(&command, "-o");
  strarray_push(&command, ir);
  strarray_push(&command, "--report");
  strarray_push(&command, audit);
  for (int i = 0; i < opt_closed_families.len; i++) {
    strarray_push(&command, "--family");
    strarray_push(&command, opt_closed_families.data[i]);
  }
  for (int i = 0; i < modules.len; i++) strarray_push(&command, modules.data[i]);
  strarray_push(&command, NULL);
  run_subprocess(command.data);
  char *result = opt_emit_llvm ? ir : compile_llvm(ir, opt_S);
  char *temporary_output = strcmp(output, "-") ? output_temporary(output) : NULL;
  char *temporary_report = report ? output_temporary(report) : NULL;
  if (opt_S) {
    copy_output(result, temporary_output ? temporary_output : "-");
  } else {
    StringArray objects = {};
    strarray_push(&objects, result);
    for (int i = 0; i < libraries.len; i++) strarray_push(&objects, libraries.data[i]);
    run_linker(&objects, temporary_output);
  }
  if (report) copy_output(audit, temporary_report);
  if (temporary_output && rename(temporary_output, output)) error("cannot publish output: %s", strerror(errno));
  if (temporary_report && rename(temporary_report, report)) error("cannot publish report: %s", strerror(errno));
}

int main(int argc, char **argv) {
  atexit(cleanup);
  init_macros();
  parse_args(argc, argv);

  if (opt_cc1) {
    add_default_include_paths(argv[0]);
    cc1();
    return 0;
  }

  if (input_paths.len > 1 && opt_o && (opt_c || (!opt_closed_program && opt_S) || opt_E))
    error("cannot specify '-o' with '-c,' '-S' or '-E' with multiple files");

  if (opt_closed_program && !opt_E && !opt_M) {
    build_closed_program(argc, argv);
    return 0;
  }

  StringArray ld_args = {};

  for (int i = 0; i < input_paths.len; i++) {
    char *input = input_paths.data[i];

    if (opt_emit_llvm &&
        (!strncmp(input, "-l", 2) || !strncmp(input, "-Wl,", 4) ||
         get_file_type(input) != FILE_C))
      error("-emit-llvm requires C source inputs: %s", input);

    if (!strncmp(input, "-l", 2)) {
      strarray_push(&ld_args, input);
      continue;
    }

    if (!strncmp(input, "-Wl,", 4)) {
      char *s = strdup(input + 4);
      char *arg = strtok(s, ",");
      while (arg) {
        strarray_push(&ld_args, arg);
        arg = strtok(NULL, ",");
      }
      continue;
    }

    char *output;
    if (opt_o)
      output = opt_o;
    else if (opt_S)
      output = replace_extn(input, opt_emit_llvm ? ".ll" : ".s");
    else
      output = replace_extn(input, ".o");

    FileType type = get_file_type(input);

    // Headers are include inputs, not independent object files or PCH files.
    // -x c can deliberately override this classification.
    if (type == FILE_HEADER && !opt_E && !opt_M)
      error("header input requires -E, -M, or -x c to compile as a source: %s", input);

    // Handle .o or .a
    if (type == FILE_OBJ || type == FILE_AR || type == FILE_DSO) {
      if (type == FILE_OBJ && is_closed_object(input))
        error("closed-program IR object requires --closed-program: %s", input);
      strarray_push(&ld_args, input);
      continue;
    }

    // Handle .s
    if (type == FILE_ASM) {
      if (opt_S || opt_M)
        continue;
      if (opt_c)
        assemble(input, output);
      else {
        char *tmp = create_tmpfile();
        assemble(input, tmp);
        strarray_push(&ld_args, tmp);
      }
      continue;
    }

    if (type == FILE_IR && opt_backend != BACKEND_LLVM)
      error("LLVM IR input requires --backend=llvm: %s", input);

    // Just preprocess
    if (opt_E || opt_M) {
      if (type != FILE_C && type != FILE_HEADER)
        error("preprocessing requires C source: %s", input);
      run_cc1(argc, argv, input, NULL);
      continue;
    }

    if (opt_backend == BACKEND_LLVM && !opt_emit_llvm) {
      char *ir = input;
      if (type == FILE_C) {
        ir = create_tmpfile();
        run_cc1(argc, argv, input, ir);
      }
      char *machine_output = compile_llvm(ir, opt_S);
      if (opt_S || opt_c)
        copy_output(machine_output, output);
      else
        strarray_push(&ld_args, machine_output);
      continue;
    }

    assert(type == FILE_C);

    // Compile
    if (opt_S) {
      run_cc1(argc, argv, input, output);
      continue;
    }

    // Compile and assemble
    if (opt_c) {
      char *tmp = create_tmpfile();
      run_cc1(argc, argv, input, tmp);
      assemble(tmp, output);
      continue;
    }

    // Compile, assemble and link
    char *tmp1 = create_tmpfile();
    char *tmp2 = create_tmpfile();
    run_cc1(argc, argv, input, tmp1);
    assemble(tmp1, tmp2);
    strarray_push(&ld_args, tmp2);
    continue;
  }

  if (ld_args.len > 0 && !opt_c && !opt_S && !opt_E && !opt_M)
    run_linker(&ld_args, opt_o ? opt_o : "a.out");
  return 0;
}
