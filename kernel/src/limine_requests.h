#ifndef LIMINE_REQS_H
#define LIMINE_REQS_H
#include <limine.h>
#define REQUESTDEF __attribute__((used,section(".limine_requests"))) static volatile
// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.  

REQUESTDEF LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

REQUESTDEF struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

REQUESTDEF struct limine_bootloader_info_request bootloader_info_request  = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
};
REQUESTDEF struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
    .revision = 0,
};

REQUESTDEF struct limine_memmap_request memmap_request = {
  .id = LIMINE_MEMMAP_REQUEST,
  .revision = 0
};

REQUESTDEF struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

REQUESTDEF struct limine_smbios_request smbios_request = {
  .id = LIMINE_SMBIOS_REQUEST,
  .revision = 0,
};

REQUESTDEF struct limine_date_at_boot_request date_request = {
  .id = LIMINE_DATE_AT_BOOT_REQUEST,
  .revision = 0,
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;


// REQUESTDEF struct limine_stack_size_request stack_size_request = {
//     .id = LIMINE_STACK_SIZE_REQUEST,
//     .revision = 0,
// };
__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;
#endif // LIMINE_REQS_H
