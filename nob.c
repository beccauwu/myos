#define _XOPEN_SOURCE 500
#include <stddef.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"
#define KERNEL_ISO "kernel.iso"
#define LIMINE_BIN "limine/limine"
#define LIMINE_SRC "limine/limine.c"
#define KERNEL_BIN "bin/kernel"
#define OVMF_FIRMWARE "ovmf/ovmf-code-x86_64.fd"
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static Cmd cmd = {0};
static Procs procs = {0};
#define CC_EXTRA(n, keep, ...)                                                 \
  if (ends_with(input, (n))) {                                                 \
    if (!(keep)) {                                                             \
      cmd.count = 0;                                                           \
      continue;                                                                \
    }                                                                          \
    cmd_append(&cmd, (keep), __VA_ARGS__);                                     \
  }
#define CC_EXTRAS                                                              \
  CC_EXTRA("olive.c", "-DOLIVECDEF=extern", "-DOLIVEC_NO_SSE",                 \
           "-Wno-missing-braces", "-DOLIVEC_IMPLEMENTATION", "-O3")            \
  CC_EXTRA("stdio.c", "-DOLIVECDEF=extern", "-DOLIVEC_NO_SSE")                 \
  CC_EXTRA("printf.c", "-DPRINTF_DISABLE_SUPPORT_EXPONENTIAL",                 \
           "-DPRINTF_DISABLE_SUPPORT_FLOAT")

static struct {
  bool debug;
  bool dry_run;
  bool force;
  bool uefi;
  bool run;
  size_t cores;
  char *gdb;
  char *cc;
  char *linker;
} args = {0};
bool _run_cmd(Cmd *_cmd, Nob_Cmd_Opt opt) {
  if (args.dry_run) {
    static String_Builder sb = {0};
    sb.count = 0;
    sb_append_cstr(&sb, "-> ");
    cmd_render(*_cmd, &sb);
    sb_append_null(&sb);
    printf("%s\n\n", sb.items);
    _cmd->count = 0;
    return true;
  }
  Log_Level curr_ll = minimal_log_level;
  minimal_log_level = INFO;
  bool res = cmd_run_opt(_cmd, opt);
  minimal_log_level = curr_ll;
  return res;
}
// #define cmd_run_dry(cmd, ...) _cmd_run_dry((cmd), (Nob_Cmd_Opt){__VA_ARGS__})

#define run_cmd(cmd, ...) _run_cmd((cmd), (Nob_Cmd_Opt){__VA_ARGS__})

#define streq(s1, s2) strcmp((s1), (s2)) == 0

static inline bool ends_with(const char *str, const char *suffix) {
  if (!str || !suffix)
    return false;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return false;
  return strcmp(str + lenstr - lensuffix, suffix) == 0;
}

bool _ends_with_any(const char *str, char **suffixes) {
  for (; *suffixes != NULL; ++suffixes) {
    if (ends_with(str, *suffixes))
      return true;
  }
}
#define ends_with_any(str, ...)                                                \
  _ends_with_any(str, ((char *[]){__VA_ARGS__, NULL}))

bool starts_with(const char *str, const char *prefix) {
  if (!str || !prefix)
    return false;
  size_t lenstr = strlen(str);
  size_t lenprefix = strlen(prefix);
  if (lenprefix > lenstr)
    return false;
  return strncmp(str, prefix, lenprefix) == 0;
}

bool read_dir_recurse(const char *path, File_Paths *children) {
  File_Paths fps = {0};
  if (!nob_read_entire_dir(path, &fps))
    return false;
  for (size_t i = 0; i < fps.count; ++i) {
    if (fps.items[i][0] == '.')
      continue;
    char *joined_path = temp_sprintf("%s/%s", path, fps.items[i]);
    File_Type type = get_file_type(joined_path);
    switch (type) {
    case NOB_FILE_DIRECTORY: {
      if (!read_dir_recurse(joined_path, children))
        return false;
    } break;
    case NOB_FILE_REGULAR: {
      da_append(children, joined_path);
    } break;
    default: {
      nob_log(WARNING, "list_dir_recurse: unhandled file type %d; skipping %s",
              type, fps.items[i]);
    } break;
    }
  }
  return true;
}

bool _mkdir_if_not_exists(const char *path) {
  // 0755 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
  int result = mkdir(path, 0755);
  if (result < 0) {
    if (errno == EEXIST) {
      return true;
    }
    nob_log(NOB_ERROR, "could not create directory `%s`: %s", path,
            strerror(errno));
    return false;
  }
  return true;
}

bool mkdir_if_not_exists_p(const char *path) {
  // 0755 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
  // we try to create it once so we can log if full path exists
  int result = mkdir(path, 0755);
  if (result < 0) {
    if (errno == EEXIST) {
      nob_log(INFO, "directory `%s` already exists", path);
      return true;
    }
  }
  bool isabs = path[0] == '/';
  String_View sv = sv_from_cstr(!isabs ? path : path + 1);
  String_Builder curr = {0};
  if (isabs)
    da_append(&curr, '/');
  Log_Level prev_log_level = minimal_log_level;
  // we don't need to log every parent directory created
  if (prev_log_level < WARNING)
    minimal_log_level = WARNING;
  while (sv.count > 0) {
    String_View seg = sv_chop_by_delim(&sv, '/');
    if (seg.count <= 0)
      break;
    sb_append_buf(&curr, seg.data, seg.count);
    da_append(&curr, '/');
    sb_append_null(&curr);
    if (!mkdir_if_not_exists(curr.items))
      return false;
    curr.count--; // removing null
  }
  minimal_log_level = prev_log_level;
  nob_log(INFO, "created directory `%s`", path);
  return true;
}

bool parse_deps(const char *depfile, File_Paths *deps) {
  String_Builder sb = {0};
  if (!nob_read_entire_file(depfile, &sb))
    return false;
  String_View sv = sb_to_sv(sb);
  String_View target = nob_sv_chop_by_delim(&sv, ':');
  while (true) {
    sv = nob_sv_trim_left(sv);
    if (nob_sv_starts_with(sv, nob_sv_from_cstr("\\"))) {
      nob_sv_chop_left(&sv, 1);
      sv = nob_sv_trim_left(sv);
    }
    String_View dep = nob_sv_chop_by_delim(&sv, ' ');
    if (sv.count == 0) {
      // last dependency ends with newline
      dep = nob_sv_chop_by_delim(&dep, '\n');
    }
    dep = nob_sv_trim(dep);
    if (nob_sv_end_with(dep, ":") || dep.count == 0)
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

  if (ret != 0) {
    nob_log(ERROR, "could not unlink file `%s`: %s", fpath, strerror(errno));
  }

  return ret;
}
bool remove_dir_recurse(const char *path) {
  if (!file_exists(path))
    return true;

  int ret = nftw(path, __unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
  if (ret == 0) {
    nob_log(INFO, "deleted directory `%s`", path);
    return true;
  }
  nob_log(ERROR, "failed to delete directory `%s`", path);
  return false;
}

bool _copy_files(const char *dst_dir, char **files) {
  for (; *files != NULL; ++files) {
    if (!copy_file(*files,
                   temp_sprintf("%s/%s", dst_dir, basename(strdup(*files)))))
      return false;
  }
  return true;
}
bool _mkdirs_if_not_exist(char **dirs) {
  for (; *dirs != NULL; ++dirs) {
    if (!mkdir_if_not_exists_p(*dirs))
      return false;
  }
  return true;
}
#define copy_files(dst, ...) _copy_files((dst), ((char *[]){__VA_ARGS__, NULL}))
#define mkdirs_if_not_exist(...)                                               \
  _mkdirs_if_not_exist(((char *[]){__VA_ARGS__, NULL}))

#define CFLAGS                                                                 \
  "-Wall", "-Wextra", "-std=gnu23", "-nostdinc", "-ffreestanding",             \
      "-fno-stack-protector", "-fno-stack-check", "-fno-lto", "-fno-PIC",      \
      "-ffunction-sections", "-fdata-sections", "-m64", "-march=x86-64",       \
      "-mabi=sysv", "-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2",         \
      "-mno-red-zone", "-ggdb", "-mcmodel=kernel", "-O2", "-pipe", "-I",       \
      "src", "-I", "limine-protocol/include", "-I", "external", "-isystem",    \
      "freestnd-c-hdrs/include", "-DLIMINE_API_REVISION=3", "-MMD", "-MP"
#define LDFLAGS                                                                \
  "-m", "elf_x86_64", "-nostdlib", "-static", "-z", "max-page-size=0x1000",    \
      "--gc-sections", "-T", "linker-scripts/x86_64.lds"
bool build_src(File_Paths *objs) {
  File_Paths fps = {0};
  File_Paths deps = {0};
  Procs cprocs = {0};
  // fasm seems to be causing race conditions that make terminal silly so need
  // to do it separately :3
  Procs asmprocs = {0};
  String_Builder sb = {0};
  bool result = true;
  if (!read_dir_recurse("external", &fps))
    return_defer(false);
  if (!read_dir_recurse("src", &fps))
    return_defer(false);
  for (size_t i = 0; i < fps.count; ++i) {
    deps.count = 0;
    const char *input = fps.items[i];
    if (!ends_with_any(fps.items[i], ".c", ".asm", ".S"))
      continue;
    const char *output = temp_sprintf("obj/%s.o", input);
    const char *depfile = temp_sprintf("obj/%s.d", input);
    // apparently dirname fucks with the string :3
    const char *dir = dirname(strdup(output));

    if (args.force)
      goto commands;
    if (file_exists(depfile)) {
      if (!parse_deps(depfile, &deps))
        return_defer(false);
      if (!needs_rebuild(output, deps.items, deps.count))
        continue;
    } else if (!needs_rebuild1(output, input))
      continue;
  commands:
    int curr_ll = minimal_log_level;
    minimal_log_level = WARNING;
    mkdir_if_not_exists_p(dir);
    minimal_log_level = curr_ll;
    if (ends_with(input, ".asm")) {
      // build rules useless for fasm since there are no options
      cmd_append(&cmd, "fasm", input, output);
      if (!run_cmd(&cmd, .async = &asmprocs, .max_procs = args.cores))
        return_defer(false);
    } else {
      cmd_append(&cmd, args.cc);
      cmd_append(&cmd, CFLAGS);

      CC_EXTRAS

      cmd_append(&cmd, "-o", output, "-c", input);
      if (!run_cmd(&cmd, .async = &cprocs, .max_procs = args.cores))
        return_defer(false);
    }
    da_append(objs, output);
  }

  if (!procs_flush(&cprocs))
    return_defer(false);
  if (!procs_flush(&asmprocs))
    return_defer(false);
defer:
  return result;
}

bool build_kernel(void) {
  bool result = true;
  const char *cwd = get_current_dir_temp();
  if (!set_current_dir("kernel"))
    return_defer(false);
  File_Paths objs = {0};
  if (!build_src(&objs))
    return_defer(false);
  if (!args.force && !needs_rebuild(KERNEL_BIN, objs.items, objs.count)){
    nob_log(INFO, "kernel up to date!");
    return_defer(true);
  }
  if (!args.dry_run && !mkdir_if_not_exists(dirname(strdup(KERNEL_BIN))))
    return_defer(false);
  cmd_append(&cmd, args.linker, LDFLAGS);
  da_append_many(&cmd, objs.items, objs.count);
  cmd_append(&cmd, "-o", KERNEL_BIN);
  return_defer(run_cmd(&cmd, .async = &procs, .max_procs = args.cores));
defer:
  if (!set_current_dir(cwd))
    return false;
  return result;
}

bool build_limine(void) {
  if (!file_exists("limine")) {
    cmd_append(&cmd, "git", "clone",
               "https://github.com/limine-bootloader/limine.git",
               "--branch=v9.x-binary", "--depth=1");
    if (!run_cmd(&cmd))
      return false;
  }
  if (file_exists("limine/limine")){
    nob_log(INFO, "limine up to date!");
    return true;
  }
  cmd_append(&cmd, args.cc, "-std=c99", "-ggdb", "-O2", "-pipe", "-o",
             LIMINE_BIN, LIMINE_SRC);
  return run_cmd(&cmd, .async = &procs, .max_procs = args.cores);
}

bool get_edk2_ovmf_firmware(void) {
  if (file_exists("ovmf/ovmf-code-x86_64.fd"))
    return true;
  if (!mkdir_if_not_exists("ovmf"))
    return false;
  cmd_append(&cmd, "curl", "-Lo", OVMF_FIRMWARE,
             "https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/"
             "download/ovmf-code-x86_64.fd");
  return run_cmd(&cmd);
}

#define XORRISO_FLAGS                                                          \
  "-as", "mkisofs", "-R", "-r", "-J", "-b", "boot/limine/limine-bios-cd.bin",  \
      "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table", "-hfsplus", \
      "-apm-block-size", "2048", "--efi-boot",                                 \
      "boot/limine/limine-uefi-cd.bin", "-efi-boot-part", "--efi-boot-image",  \
      "--protective-msdos-label"

bool build_iso(void) {
  if (!needs_rebuild("kernel.iso",
                     (const char *[]){"kernel/" KERNEL_BIN, "limine/limine"},
                     2)) {
    nob_log(INFO, "%s up to date!", KERNEL_ISO);
    return true;
  }
  if (!args.dry_run) {
    if (!remove_dir_recurse("iso_root"))
      return false;
    if (!mkdirs_if_not_exist("iso_root/boot/limine", "iso_root/EFI/BOOT"))
      return false;
    if (!copy_file("kernel/" KERNEL_BIN, "iso_root/boot/kernel"))
      return false;
    if (!copy_files("iso_root/boot/limine", "limine.conf",
                    "limine/limine-bios.sys", "limine/limine-bios-cd.bin",
                    "limine/limine-uefi-cd.bin"))
      return false;
    if (!copy_files("iso_root/EFI/BOOT", "limine/BOOTX64.EFI",
                    "limine/BOOTIA32.EFI"))
      return false;
  }
  cmd_append(&cmd, "xorriso", XORRISO_FLAGS);
  cmd_append(&cmd, "iso_root", "-o", KERNEL_ISO);
  if (!run_cmd(&cmd))
    return false;
  cmd_append(&cmd, "./limine/limine", "bios-install", KERNEL_ISO);
  if (!run_cmd(&cmd))
    return false;
  if (!remove_dir_recurse("iso_root"))
    return false;
  return true;
}
bool build(void) {
  if (!build_kernel())
    return false;
  if (!build_limine())
    return false;
  if (!procs_flush(&procs))
    return false;
  if (!build_iso())
    return false;
  return true;
}

#define QEMU_FLAGS_ISO                                                         \
  "-M", "q35", "-cdrom", KERNEL_ISO, "-boot", "d", "-m", "2G"

#define QEMU_FLAGS_UEFI                                                        \
  "-M", "q35", "-drive",                                                       \
      "if=pflash,unit=0,format=raw,file=" OVMF_FIRMWARE ",readonly=on",        \
      "-cdrom", KERNEL_ISO, "-boot", "d"
bool run_qemu(void) {
  if (args.uefi) {
    if (!get_edk2_ovmf_firmware())
      return false;
    cmd_append(&cmd, "qemu-system-x86_64", QEMU_FLAGS_UEFI);
  } else
    cmd_append(&cmd, "qemu-system-x86_64", QEMU_FLAGS_ISO);
  if (args.debug)
    cmd_append(&cmd, "-s", "-S");

  return run_cmd(&cmd, .async = &procs, .max_procs = args.cores);
}

bool run_debugger() {
  static const char *gdb_commands[] = {"file "
                                       "kernel/" KERNEL_BIN,
                                       "b kmain",
                                       "target remote localhost:1234"};
  cmd_append(&cmd, args.gdb);
  for (size_t i = 0; i < ARRAY_LEN(gdb_commands); ++i) {
    cmd_append(&cmd, "-ex", gdb_commands[i]);
  }
  return run_cmd(&cmd, .async = &procs, .max_procs = args.cores);
}

void usage(FILE *stream) {

  fprintf(stream, "Usage: %s [OPTIONS] [run]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}
void run_usage(void *ctx, FILE *stream) {

  fprintf(stream, "Usage: %s run [OPTIONS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_c_print_options(ctx, stream);
}

bool run_options(void *ctx, int argc, char **argv) {

  bool *uefi = flag_c_bool(ctx, "uefi", false, "Run in UEFI mode");
  bool *debug = flag_c_bool(ctx, "debug", false, "Attach debugger");
  char **gdb = flag_c_str(ctx, "gdb", "gf2", "GDB path");
  bool *help = flag_c_bool(ctx, "help", false, "Print this help message");

  if (!flag_c_parse(ctx, argc, argv)) {
    run_usage(ctx, stderr);
    flag_c_print_error(ctx, stderr);
    return false;
  }
  if (*help) {
    run_usage(ctx, stderr);
    return false;
  }
  args.gdb = *gdb;
  args.debug = *debug;
  args.uefi = *uefi;
  return true;
}

bool help_options(void *ctx, int argc, char **argv) {
  usage(stderr);
  return false;
}

static const struct {
  const char *n;
  bool (*f)(void *, int, char **);
} subs[] = {{"run", run_options}, {"help", help_options}};

bool dispatch_subcmd(int *argc, char ***argv) {
  const char *sub = shift(*argv, *argc);
  for (size_t i = 0; i < ARRAY_LEN(subs); ++i) {
    if (streq(sub, subs[i].n)) {
      void *ctx = flag_c_new(sub);
      bool res = subs[i].f(ctx, *argc, *argv);
      *argc = flag_c_rest_argc(ctx);
      *argv = flag_c_rest_argv(ctx);
      return res;
    }
  }
  nob_log(ERROR, "invalid option '%s'", sub);
  usage(stderr);
  return false;
}

bool parse_options(int argc, char **argv) {
  char **cc = flag_str("cc", "gcc", "C compiler path");
  char **linker = flag_str("linker", "ld", "Linker path");

  size_t *cores = flag_uint64("j", nob_nprocs() + 1, "Number of parallel jobs");
  bool *dry_run = flag_bool("dry-run", false, "Dry run the compilation");

  bool *verbose = flag_bool("v", false, "Print with verbose log level");
  bool *force = flag_bool("B", false, "Force rebuild kernel");
  bool *help = flag_bool("help", false, "Print this help message");
  // if (!*verbose) {
  //   minimal_log_level = WARNING;
  // }
  if (!flag_parse(argc, argv)) {
    usage(stderr);
    flag_print_error(stderr);
    return false;
  }
  if (*help) {
    usage(stderr);
    return false;
  }
  int rest_argc = flag_rest_argc();
  char **rest_argv = flag_rest_argv();
  bool result = true;
  while (rest_argc > 0) {
    if (!dispatch_subcmd(&rest_argc, &rest_argv))
      return false;
  }
  args.cc = *cc;
  args.dry_run = *dry_run;
  args.force = *force;
  args.linker = *linker;
  args.cores = *cores;
  return result;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);
  if (!parse_options(argc, argv))
    return false;
  if (!build())
    return 1;
  if (args.debug || args.run) {
    if (!run_qemu())
      return 1;
  }
  if (args.debug) {
    if (!run_debugger())
      return 1;
  }
  procs_flush(&procs);

  return 0;
}
