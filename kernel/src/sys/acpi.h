#ifndef _ACPI_H
#define _ACPI_H
#include <stdlib.h>

/*
a lot of the code here is from https://github.com/RickleAndMortimer/MakenOS
*/

typedef struct rsdp_descriptor
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__ ((packed)) rsdp_descriptor_t;

typedef struct rsdp_descriptor20
{
    struct rsdp_descriptor descriptor10;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__ ((packed)) rsdp_descriptor20_t;

typedef struct acpi_std_header
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__ ((packed)) acpi_std_header_t;

typedef struct rsdt
{
    struct acpi_std_header h;
    uint32_t other_sdt[];
} __attribute__ ((packed)) rsdt_t;

typedef struct xsdt
{
    struct acpi_std_header h;
    uint64_t other_sdt[];
} __attribute__ ((packed)) xsdt_t;

#endif // _ACPI_H