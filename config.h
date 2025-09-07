#ifndef CONFIG_H
#define CONFIG_H

/* per-file extra compiler arguments */
#define CC_EXTRAS                                            \
  CC_END("olive.c",                                          \
         "-DOLIVECDEF=extern",                               \
         "-DOLIVEC_NO_SSE",                                  \
         "-Wno-missing-braces",                              \
         "-DOLIVEC_IMPLEMENTATION",                          \
         "-O3")                                              \
  CC_END("stdio.c", "-DOLIVECDEF=extern", "-DOLIVEC_NO_SSE") \
  CC_END("printf.c",                                         \
         "-DPRINTF_DISABLE_SUPPORT_EXPONENTIAL",             \
         "-DPRINTF_DISABLE_SUPPORT_FLOAT")

#define KERNEL_ISO    "kernel.iso"
#define LIMINE_BIN    "limine/limine"
#define LIMINE_SRC    "limine/limine.c"
#define KERNEL_BIN    "kernel/bin/kernel"
#define OVMF_FIRMWARE "ovmf/ovmf-code-x86_64.fd"
#define LINKER_SCRIPT "linker-scripts/x86_64.lds"
#define SRC_CFLAGS                                                            \
  "-Wall", "-Wextra", "-std=gnu23", "-nostdinc", "-ffreestanding",            \
    "-fno-stack-protector", "-fno-stack-check", "-fno-lto", "-fno-PIC",       \
    "-ffunction-sections", "-fdata-sections", "-m64", "-march=x86-64",        \
    "-mabi=sysv", "-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2",          \
    "-mno-red-zone", "-ggdb", "-mcmodel=kernel", "-O2", "-pipe", "-I", "src", \
    "-I", "limine-protocol/include", "-I", "external", "-isystem",            \
    "freestnd-c-hdrs/include", "-DLIMINE_API_REVISION=3", "-MMD", "-MP"
#define KERNEL_LDFLAGS                                                      \
  "-m", "elf_x86_64", "-nostdlib", "-static", "-z", "max-page-size=0x1000", \
    "--gc-sections", "-T", LINKER_SCRIPT
#define XORRISO_FLAGS                                                          \
  "-as", "mkisofs", "-R", "-r", "-J", "-b", "boot/limine/limine-bios-cd.bin",  \
    "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table", "-hfsplus",   \
    "-apm-block-size", "2048", "--efi-boot", "boot/limine/limine-uefi-cd.bin", \
    "-efi-boot-part", "--efi-boot-image", "--protective-msdos-label"
#define QEMU_FLAGS_ISO \
  "-M", "q35", "-cdrom", KERNEL_ISO, "-boot", "d", "-m", "2G"

#define QEMU_FLAGS_UEFI                                               \
  "-M", "q35", "-drive",                                              \
    "if=pflash,unit=0,format=raw,file=" OVMF_FIRMWARE ",readonly=on", \
    "-cdrom", KERNEL_ISO, "-boot", "d"
#endif  // CONFIG_H