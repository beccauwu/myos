#define _XOPEN_SOURCE 500
#include <stddef.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"
#define GLOB_IMPLEMENTATION
#include "config.h"
#include "glob.h"
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static Cmd   cmd   = { 0 };
static Procs procs = { 0 };
/* check for matching filename with suffix */
#define CC_END(n, keep, ...)               \
  if(ends_with(input, (n))) {              \
    if(!(keep)) {                          \
      cmd.count = 0;                       \
      continue;                            \
    }                                      \
    cmd_append(&cmd, (keep), __VA_ARGS__); \
  }
/* check for matching filename using glob */
#define CC_GLOB(g, keep, ...)              \
  if(glob_utf8((g), input) == 0) {         \
    if(!(keep)) {                          \
      cmd.count = 0;                       \
      continue;                            \
    }                                      \
    cmd_append(&cmd, (keep), __VA_ARGS__); \
  }

static struct {
  bool   debug;
  bool   force;
  bool   uefi;
  bool   run;
  size_t cores;
  char  *gdb;
  char  *cc;
  char  *linker;
} args = { 0 };

#define streq(s1, s2) strcmp((s1), (s2)) == 0

static inline bool ends_with(const char *str, const char *suffix) {
  if(!str || !suffix)
    return false;
  size_t lenstr    = strlen(str);
  size_t lensuffix = strlen(suffix);
  if(lensuffix > lenstr)
    return false;
  return strcmp(str + lenstr - lensuffix, suffix) == 0;
}

bool _ends_with_any(const char *str, char **suffixes) {
  for(; *suffixes != NULL; ++suffixes) {
    if(ends_with(str, *suffixes))
      return true;
  }
}
#define ends_with_any(str, ...) \
  _ends_with_any(str, ((char *[]) { __VA_ARGS__, NULL }))

bool starts_with(const char *str, const char *prefix) {
  if(!str || !prefix)
    return false;
  size_t lenstr    = strlen(str);
  size_t lenprefix = strlen(prefix);
  if(lenprefix > lenstr)
    return false;
  return strncmp(str, prefix, lenprefix) == 0;
}

bool read_dir_recurse(const char *path, File_Paths *children) {
  File_Paths fps = { 0 };
  if(!nob_read_entire_dir(path, &fps))
    return false;
  for(size_t i = 0; i < fps.count; ++i) {
    if(fps.items[i][0] == '.')
      continue;
    char     *joined_path = temp_sprintf("%s/%s", path, fps.items[i]);
    File_Type type        = get_file_type(joined_path);
    switch(type) {
      case NOB_FILE_DIRECTORY: {
        if(!read_dir_recurse(joined_path, children))
          return false;
      } break;
      case NOB_FILE_REGULAR: {
        da_append(children, joined_path);
      } break;
      default: {
        nob_log(WARNING,
                "list_dir_recurse: unhandled file type %d; skipping %s",
                type,
                fps.items[i]);
      } break;
    }
  }
  return true;
}

bool _mkdir_if_not_exists(const char *path) {
  // 0755 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
  int result = mkdir(path, 0755);
  if(result < 0) {
    if(errno == EEXIST) {
      return true;
    }
    nob_log(
      NOB_ERROR, "could not create directory `%s`: %s", path, strerror(errno));
    return false;
  }
  return true;
}

bool mkdir_if_not_exists_p(const char *path) {
  // 0755 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
  // we try to create it once so we can log if full path exists
  int result = mkdir(path, 0755);
  if(result < 0) {
    if(errno == EEXIST) {
      nob_log(INFO, "directory `%s` already exists", path);
      return true;
    }
  }
  bool           isabs = path[0] == '/';
  String_View    sv    = sv_from_cstr(!isabs ? path : path + 1);
  String_Builder curr  = { 0 };
  if(isabs)
    da_append(&curr, '/');
  Log_Level prev_log_level = minimal_log_level;
  // we don't need to log every parent directory created
  if(prev_log_level < WARNING)
    minimal_log_level = WARNING;
  while(sv.count > 0) {
    String_View seg = sv_chop_by_delim(&sv, '/');
    if(seg.count <= 0)
      break;
    sb_append_buf(&curr, seg.data, seg.count);
    da_append(&curr, '/');
    sb_append_null(&curr);
    if(!mkdir_if_not_exists(curr.items))
      return false;
    curr.count--;  // removing null
  }
  minimal_log_level = prev_log_level;
  nob_log(INFO, "created directory `%s`", path);
  return true;
}

bool parse_deps(const char *depfile, File_Paths *deps) {
  String_Builder sb = { 0 };
  if(!nob_read_entire_file(depfile, &sb))
    return false;
  String_View sv     = sb_to_sv(sb);
  String_View target = nob_sv_chop_by_delim(&sv, ':');
  while(true) {
    sv = nob_sv_trim_left(sv);
    if(nob_sv_starts_with(sv, nob_sv_from_cstr("\\"))) {
      nob_sv_chop_left(&sv, 1);
      sv = nob_sv_trim_left(sv);
    }
    String_View dep = nob_sv_chop_by_delim(&sv, ' ');
    if(sv.count == 0) {
      // last dependency ends with newline
      dep = nob_sv_chop_by_delim(&dep, '\n');
    }
    dep = nob_sv_trim(dep);
    if(nob_sv_end_with(dep, ":") || dep.count == 0)
      break;
    da_append(deps, nob_temp_sv_to_cstr(dep));
  }
  return true;
}
int __unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
                struct FTW *ftwbuf) {
  UNUSED(sb);
  UNUSED(typeflag);
  UNUSED(ftwbuf);
  int ret = remove(fpath);

  if(ret != 0) {
    nob_log(ERROR, "could not unlink file `%s`: %s", fpath, strerror(errno));
  }

  return ret;
}
bool remove_file(const char *path) {
  if(!file_exists(path))
    return true;
  File_Type t = get_file_type(path);
  switch(t) {
    case FILE_DIRECTORY: {
      int ret = nftw(path, __unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
      if(ret == 0) {
        nob_log(INFO, "deleted `%s`", path);
        return true;
      }
      nob_log(ERROR, "failed to delete `%s`", path);
    } break;
    default: {
      if(remove(path) != 0) {
        nob_log(ERROR, "could not unlink file `%s`: %s", path, strerror(errno));
        return false;
      }
      nob_log(INFO, "deleted `%s`", path);
      return true;
    }
  }
  return false;
}

bool _remove_files(char **paths) {
  for(; *paths != NULL; ++paths) {
    if(!remove_file(*paths))
      return false;
  }
  return true;
}

bool _copy_files(const char *dst_dir, char **files) {
  for(; *files != NULL; ++files) {
    if(!copy_file(*files,
                  temp_sprintf("%s/%s", dst_dir, basename(strdup(*files)))))
      return false;
  }
  return true;
}
bool _mkdirs_if_not_exist(char **dirs) {
  for(; *dirs != NULL; ++dirs) {
    if(!mkdir_if_not_exists_p(*dirs))
      return false;
  }
  return true;
}

#define copy_files(dst, ...) \
  _copy_files((dst), ((char *[]) { __VA_ARGS__, NULL }))
#define mkdirs_if_not_exist(...) \
  _mkdirs_if_not_exist(((char *[]) { __VA_ARGS__, NULL }))
#define remove_files(...) _remove_files(((char *[]) { __VA_ARGS__, NULL }))

bool build_src(File_Paths *objs) {
  File_Paths src_files = { 0 };
  File_Paths dep_files = { 0 };
  Procs      cprocs    = { 0 };
  // fasm seems to be causing race conditions that make terminal silly so need
  // to do it separately :3
  Procs          asmprocs = { 0 };
  String_Builder sb       = { 0 };
  bool           result   = true;
  if(!read_dir_recurse("external", &src_files))
    return_defer(false);
  if(!read_dir_recurse("src", &src_files))
    return_defer(false);
  for(size_t i = 0; i < src_files.count; ++i) {
    dep_files.count   = 0;
    const char *input = src_files.items[i];
    if(!ends_with_any(input, ".c", ".asm", ".S"))
      continue;
    const char *output  = temp_sprintf("obj/%s.o", input);
    const char *depfile = temp_sprintf("obj/%s.d", input);
    // apparently dirname fucks with the string :3
    const char *dir = dirname(strdup(output));
    if(args.force || !file_exists(depfile))
      goto commands;
    if(!parse_deps(depfile, &dep_files))
      return_defer(false);
    if(!needs_rebuild(output, dep_files.items, dep_files.count))
      goto loop_end;
  commands:
    int curr_ll       = minimal_log_level;
    minimal_log_level = WARNING;
    mkdir_if_not_exists_p(dir);
    minimal_log_level = curr_ll;
    if(ends_with(input, ".asm")) {
      cmd_append(&cmd, "fasm", input, output);
      if(!cmd_run(&cmd, .async = &asmprocs, .max_procs = args.cores))
        return_defer(false);
    } else {
      cmd_append(&cmd, args.cc);
      cmd_append(&cmd, SRC_CFLAGS);

      CC_EXTRAS

      cmd_append(&cmd, "-o", output, "-c", input);
      if(!cmd_run(&cmd, .async = &cprocs, .max_procs = args.cores))
        return_defer(false);
    }
  loop_end:
    da_append(objs, output);
  }
  if(!procs_flush(&cprocs))
    return_defer(false);
  if(!procs_flush(&asmprocs))
    return_defer(false);
defer:
  return result;
}

bool build_kernel(void) {
  const char *cwd = get_current_dir_temp();
  if(!set_current_dir("kernel"))
    return false;
  bool       result = true;
  File_Paths objs   = { 0 };
  if(!build_src(&objs))
    return_defer(false);
  if(!args.force && !needs_rebuild("bin/kernel", objs.items, objs.count)) {
    nob_log(INFO, "kernel is up to date!");
    return_defer(true);
  }
  if(!mkdir_if_not_exists("bin"))
    return_defer(false);
  cmd_append(&cmd, args.linker, KERNEL_LDFLAGS);
  da_append_many(&cmd, objs.items, objs.count);
  cmd_append(&cmd, "-o", "bin/kernel");
  return_defer(cmd_run(&cmd, .async = &procs, .max_procs = args.cores));
defer:
  if(!set_current_dir(cwd))
    return false;
  return result;
}

bool build_limine(void) {
  if(file_exists(LIMINE_BIN)) {
    nob_log(INFO, "limine is up to date!");
    return true;
  }
  if(!file_exists(LIMINE_SRC)) {
    remove_file("limine");
    nob_log(INFO, "Getting limine binaries...");
    cmd_append(&cmd,
               "git",
               "clone",
               "https://codeberg.org/Limine/Limine.git",
               "--branch=v9.x-binary",
               "--depth=1",
               "limine");
    if(!cmd_run(&cmd))
      return false;
  }
  cmd_append(&cmd,
             args.cc,
             "-std=c99",
             "-ggdb",
             "-O2",
             "-pipe",
             "-o",
             LIMINE_BIN,
             LIMINE_SRC);
  return cmd_run(&cmd, .async = &procs, .max_procs = args.cores);
}

bool get_edk2_ovmf_firmware(void) {
  if(file_exists("ovmf/ovmf-code-x86_64.fd"))
    return true;
  if(!mkdir_if_not_exists("ovmf"))
    return false;
  cmd_append(&cmd,
             "curl",
             "-Lo",
             OVMF_FIRMWARE,
             "https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/"
             "download/ovmf-code-x86_64.fd");
  return cmd_run(&cmd);
}

bool build_iso(void) {
  if(!needs_rebuild(
       KERNEL_ISO, (const char *[]) { KERNEL_BIN, LIMINE_BIN }, 2)) {
    nob_log(INFO, "'%s' is up to date!", KERNEL_ISO);
    return true;
  }
  if(!remove_file("iso_root"))
    return false;
  if(!mkdirs_if_not_exist("iso_root/boot/limine", "iso_root/EFI/BOOT"))
    return false;
  if(!copy_file(KERNEL_BIN, "iso_root/boot/kernel"))
    return false;
  if(!copy_files("iso_root/boot/limine",
                 "limine.conf",
                 "limine/limine-bios.sys",
                 "limine/limine-bios-cd.bin",
                 "limine/limine-uefi-cd.bin"))
    return false;
  if(!copy_files(
       "iso_root/EFI/BOOT", "limine/BOOTX64.EFI", "limine/BOOTIA32.EFI"))
    return false;
  cmd_append(&cmd, "xorriso", XORRISO_FLAGS);
  cmd_append(&cmd, "iso_root", "-o", KERNEL_ISO);
  if(!cmd_run(&cmd))
    return false;
  cmd_append(&cmd, "./" LIMINE_BIN, "bios-install", KERNEL_ISO);
  if(!cmd_run(&cmd))
    return false;
  if(!remove_file("iso_root"))
    return false;
  return true;
}
bool build_all(void) {
  if(!build_kernel())
    return false;
  if(!build_limine())
    return false;
  if(!procs_flush(&procs))
    return false;
  if(!build_iso())
    return false;
  return true;
}

bool run_qemu(void) {
  if(args.uefi) {
    if(!get_edk2_ovmf_firmware())
      return false;
    cmd_append(&cmd, "qemu-system-x86_64", QEMU_FLAGS_UEFI);
  } else
    cmd_append(&cmd, "qemu-system-x86_64", QEMU_FLAGS_ISO);
  if(args.debug)
    cmd_append(&cmd, "-s", "-S");

  return cmd_run(&cmd, .async = &procs, .max_procs = args.cores);
}

bool run_debugger() {
  static const char *gdb_commands[] = { "file " KERNEL_BIN,
                                        "break kmain",
                                        "target remote localhost:1234" };
  cmd_append(&cmd, args.gdb);
  for(size_t i = 0; i < ARRAY_LEN(gdb_commands); ++i) {
    cmd_append(&cmd, "-ex", gdb_commands[i]);
  }
  return cmd_run(&cmd, .async = &procs, .max_procs = args.cores);
}

#define OUTPUTS \
  "iso_root/", KERNEL_ISO, "kernel/bin/", "kernel/obj/", "limine/", "ovmf/"

bool remove_outputs(Flag_List *keeps) {
  static const char *outputs[] = { OUTPUTS };
  for(size_t i = 0; i < ARRAY_LEN(outputs); ++i) {
    for(size_t j = 0; j < keeps->count; ++j) {
      if(glob_utf8(keeps->items[j], outputs[i]))
        goto loop_end;
    }
    if(!remove_file(outputs[i]))
      return false;
  loop_end:
  }
  return true;
}

static char *_program_name = NULL;

static inline char *program_name(void) {
  if(_program_name == NULL) {
    _program_name = realpath(flag_program_name(), NULL);
  }
  return _program_name;
}

void print_usage(FILE *);

void run_subcmd_usage(void *ctx, FILE *stream) {
  fprintf(stream, "Usage: %s run [OPTIONS]\n\n", program_name());
  fprintf(stream, "Launches kernel in qemu after building\n");
  fprintf(stream, "OPTIONS:\n");
  flag_c_print_options(ctx, stream);
}

void clean_subcmd_usage(void *ctx, FILE *stream) {
  static const char *outputs[] = { OUTPUTS };
  fprintf(stream, "Usage: %s clean [OPTIONS]\n\n", program_name());
  fprintf(stream, "Removes output files and exits afterwards\n");
  fprintf(stream, "Files removed:\n");
  for(size_t i = 0; i < ARRAY_LEN(outputs); ++i) {
    fprintf(stream, "    - '%s'\n", outputs[i]);
  }
  fprintf(stream, "\nOPTIONS:\n");
  flag_c_print_options(ctx, stream);
}

bool run_subcmd_flags(void *ctx, int argc, char **argv) {
  bool  *uefi  = flag_c_bool(ctx, "uefi", false, "Run in UEFI mode");
  bool  *debug = flag_c_bool(ctx, "debug", false, "Attach debugger");
  char **gdb   = flag_c_str(ctx, "gdb", "gf2", "GDB path");
  bool  *help  = flag_c_bool(ctx, "help", false, "Print this help message");

  if(!flag_c_parse(ctx, argc, argv)) {
    run_subcmd_usage(ctx, stderr);
    flag_c_print_error(ctx, stderr);
    return false;
  }
  if(*help) {
    run_subcmd_usage(ctx, stderr);
    exit(0);
  }
  args.gdb   = *gdb;
  args.debug = *debug;
  args.uefi  = *uefi;
  args.run   = true;
  return true;
}

bool help_subcmd_flags(void *ctx, int argc, char **argv) {
  print_usage(stderr);
  exit(0);
}

bool clean_subcmd_flags(void *ctx, int argc, char **argv) {
  Flag_List *keeps = flag_c_list(ctx, "k", "Files to keep");
  bool      *help  = flag_c_bool(ctx, "help", false, "Print this help message");
  if(!flag_c_parse(ctx, argc, argv)) {
    run_subcmd_usage(ctx, stderr);
    flag_c_print_error(ctx, stderr);
    return false;
  }
  if(*help) {
    clean_subcmd_usage(ctx, stderr);
    return false;
  }
  if(!remove_outputs(keeps)) {
    nob_log(ERROR, "Failed to remove outputs");
  } else {
    nob_log(INFO, "Removed all output files!");
  }
  exit(0);
}

static const struct {
  const char *n;
  bool (*f)(void *, int, char **);
  const char *d;
} subs[] = {
  { "run", run_subcmd_flags, "run kernel after building" },
  { "help", help_subcmd_flags },
  { "clean", clean_subcmd_flags, "clean output files produced" }
};

#define max(a, b) ((a) < (b) ? (b) : (a))

void print_usage(FILE *stream) {
  fprintf(stream, "\nBuild tool for ~insert os name~\n\n", program_name());
  fprintf(stream, "Usage: %s [OPTIONS] <command>\n", program_name());
  fprintf(stream, "\nOPTIONS:\n");
  flag_print_options(stream);
  fprintf(stream, "\nCOMMANDS:\n");
  size_t maxlen = 0;
  for(size_t i = 0; i < ARRAY_LEN(subs); ++i) {
    if(subs[i].d)
      maxlen = max(maxlen, strlen(subs[i].n));
  }

  for(size_t i = 0; i < ARRAY_LEN(subs); ++i) {
    if(subs[i].d) {
      fprintf(stream, "    %-*s - %s\n", (int)maxlen, subs[i].n, subs[i].d);
    }
  }
  fprintf(stream,
          "(see `<command> -help` for more details on specific commands)\n");
}

bool dispatch_subcmd(int *argc, char ***argv) {
  const char *sub = shift(*argv, *argc);
  for(size_t i = 0; i < ARRAY_LEN(subs); ++i) {
    if(streq(sub, subs[i].n)) {
      void *ctx = flag_c_new(sub);
      bool  res = subs[i].f(ctx, *argc, *argv);
      *argc     = flag_c_rest_argc(ctx);
      *argv     = flag_c_rest_argv(ctx);
      return res;
    }
  }
  nob_log(ERROR, "invalid command '%s'", sub);
  print_usage(stderr);
  return false;
}

bool parse_options(int argc, char **argv) {
  char  **cc      = flag_str("cc", "gcc", "C compiler path");
  char  **linker  = flag_str("linker", "ld", "Linker path");
  bool   *verbose = flag_bool("v", false, "Print with verbose log level");
  bool   *force   = flag_bool("B", false, "Force rebuild kernel");
  size_t *cores   = flag_uint64("j", nprocs() + 1, "Number of parallel jobs");
  bool   *help    = flag_bool("help", false, "Print this help message");
  // if (!*verbose) {
  //   minimal_log_level = WARNING;
  // }
  if(!flag_parse(argc, argv)) {
    print_usage(stderr);
    flag_print_error(stderr);
    return false;
  }
  if(*help) {
    print_usage(stderr);
    return false;
  }
  int    rest_argc = flag_rest_argc();
  char **rest_argv = flag_rest_argv();
  bool   result    = true;
  while(rest_argc > 0) {
    if(!dispatch_subcmd(&rest_argc, &rest_argv))
      return false;
  }
  args.cc     = *cc;
  args.force  = *force;
  args.linker = *linker;
  args.cores  = *cores;
  return result;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "config.h");
  if(!parse_options(argc, argv))
    return false;
  if(!build_all())
    return 1;
  if(args.run) {
    if(!run_qemu())
      return 1;
  }
  if(args.debug) {
    if(!run_debugger())
      return 1;
  }
  procs_flush(&procs);

  return 0;
}
