#include <limine.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/interrupts.h>
#include <sys/pic.h>
#include "limine_requests.h"

#define FB_AT(fb, row, col)                                                    \
  ((uint32_t *)(fb->address))[(row) * (fb->pitch / sizeof(uint32_t)) + (col)]

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.

static const char *mmap_typenames[] = {
    [0] = "LIMINE_MEMMAP_USABLE",
    [1] = "LIMINE_MEMMAP_RESERVED",
    [2] = "LIMINE_MEMMAP_ACPI_RECLAIMABLE",
    [3] = "LIMINE_MEMMAP_ACPI_NVS",
    [4] = "LIMINE_MEMMAP_BAD_MEMORY",
    [5] = "LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE",
    [6] = "LIMINE_MEMMAP_EXECUTABLE_AND_MODULES",
    [7] = "LIMINE_MEMMAP_FRAMEBUFFER",
};

__attribute__((interrupt)) void dummy_isr(struct interrupt_frame *frame) {
  printf("INTERRUPT CALLED\n");
}

typedef struct limine_bootloader_info_response *bootldr_info_res;
typedef struct limine_memmap_response *memmap_res;
typedef struct limine_memmap_entry *memmap_entry;
typedef struct limine_framebuffer_response *framebuffer_res;

typedef struct rsdp_descriptor {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__ ((packed)) rsdp_descriptor;

typedef struct kernel_ctx {
  dtr_t *gdtr;
  dtr_t *idtr;
  bootldr_info_res bootloader;
  memmap_res mmap;
  framebuffer_res fb;
  rsdp_descriptor *rsdp;
};

struct kernel_ctx ctx = {0};

/**
 * @brief initialise context
 * to not have to deal with long typenames
 */
void init_ctx(void) {
  ctx.gdtr = get_gdtr();
  ctx.bootloader = bootloader_info_request.response;
  ctx.mmap = memmap_request.response;
  ctx.fb = framebuffer_request.response;
}

bool validate_rsdp(const char *byte_array, size_t size) {
    uint32_t sum = 0;
    for(int i = 0; i < size; ++i) {
        sum += byte_array[i];
    }
    return (sum & 0xFF) == 0;
}

void kmain(void) {
  // Ensure the bootloader actually understands our base revision (see
  // spec).
  
  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    abort();
  }
  init_ctx();
  // Ensure we got a framebuffer.
  if (ctx.fb == NULL || ctx.fb->framebuffer_count < 1) {
    abort();
  }

  // Fetch the first framebuffer.
  struct limine_framebuffer *framebuffer = ctx.fb->framebuffers[0];

  init_io(framebuffer);
  assert(rsdp_request.response != NULL);
  ctx.rsdp = (rsdp_descriptor *)rsdp_request.response->address;
  
  // if(!validate_rsdp((char*)ctx.rsdp, sizeof(rsdp_descriptor))) {
  //   kpanic("couldn't validate rsdp at address %p\n", ctx.rsdp);
  // }
  printf("\e[1;34m%s v%s\e[0m\n", ctx.bootloader->name, ctx.bootloader->version);
  init_handlers();
  
  kinfo("%ld framebuffers present\n", ctx.fb->framebuffer_count);

  kinfo("%ld mmap entries present\n", ctx.mmap->entry_count);
  uint64_t total_mem = 0;
  memmap_entry usable = NULL;
  for (size_t i = 0; i < ctx.mmap->entry_count; ++i) {
    memmap_entry entry = ctx.mmap->entries[i];
    if(entry->type == LIMINE_MEMMAP_USABLE) {
      if(usable && usable->length < entry->length) usable = entry;
      else if(usable == NULL) usable = entry;
      // todo grab all the chunks of usable memory
    }
    total_mem += entry->length;
    printf("\t0x%08X - 0x%08X (%06ld): %s\n", entry->base,
           entry->base + entry->length, entry->length, mmap_typenames[entry->type]);
  }
  printf("rsdp at %p\n",8, ctx.rsdp);
  kinfo("total mapped memory %ldM\n", total_mem/1024/1024);
  kinfo("located usable memory at 0x%08X - 0x%08X (%ldM)\n", usable->base, usable->base+usable->length, usable->length/1024/1024);
  // init_handlers(dummy_isr);
  kinfo("GDTR at 0x%08X with limit %ull \n", ctx.gdtr->base,
         ctx.gdtr->limit);

  // Note: we assume the framebuffer model is RGB with 32-bit pixels.
  // for (size_t i = 0; i < 100; i++) {
  //   volatile uint32_t *fb_ptr = framebuffer->address;
  //   fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
  // }

  // We're done, just hang...
  abort();
}
