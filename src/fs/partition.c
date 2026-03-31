#include "partition.h"
#include "mem/mem.h" // IWYU pragma: keep
#include "guid_list.h"
#include "kstate.h"

static partition_t* head = NULL;

typedef struct {
    uint8_t guid[16];
    const char* name;
} gpt_name_lookup_t;

static const partition_type_lookup_t partition_lookup_table[] = {
    {0x00, GUID_EMPTY, "Empty"},
    {0x06, GUID_MS_BASIC_DATA, "FAT16"},
    {0x0B, GUID_MS_BASIC_DATA, "FAT32 (CHS)"},
    {0x0C, GUID_MS_BASIC_DATA, "FAT32 (LBA)"},
    {0x07, GUID_MS_BASIC_DATA, "NTFS/exFAT"},
    {0x83, GUID_LINUX_FILESYSTEM, "Linux Native"},
    {0x82, GUID_LINUX_SWAP, "Linux Swap"},
    {0xEF, GUID_EFI_SYSTEM, "EFI System Partition"},
    {0x21, GUID_BIOS_BOOT,  "BIOS Boot Partition"},
    {0, GUID_EMPTY, NULL}
};

static const gpt_name_lookup_t guid_name_table[] = {
    {GUID_EMPTY, "Empty/Unknown"},
    {GUID_EFI_SYSTEM, "EFI System Partition"},
    {GUID_BIOS_BOOT, "BIOS Boot Partition"},
    {GUID_MS_BASIC_DATA, MICROSOFT_STRING " Basic Data"},
    {GUID_LINUX_SWAP, "Linux Swap"},
    {GUID_LINUX_FILESYSTEM, "Linux Filesystem"},
    {GUID_MS_RESERVED, MICROSOFT_STRING " Reserved"},
    {GUID_WIN_RECOVERY, "Windows Recovery Environment"},
    #ifndef MINIMAL_BUILD
        {GUID_MBR_SCHEME, "MBR Partition Scheme"},
        {GUID_INTEL_FAST_FLASH, "Intel Fast Flash"},
        {GUID_SONY_BOOT, "Sony Boot Partition"},
        {GUID_LENOVO_BOOT, "Lenovo Boot Partition"},
        {GUID_POWERPC_PREP, "PowerPC PReP Boot"},
        {GUID_ONIE_BOOT, "ONIE Boot Partition"},
        {GUID_ONIE_CONFIG, "ONIE Config Partition"},
        {GUID_MS_LDM_METADATA, "Logical Disk Manager Metadata"},
        {GUID_MS_LDM_DATA, "Logical Disk Manager Data"},
        {GUID_MS_STORAGE_SPACES, "Microsoft Storage Spaces"},
        {GUID_HP_UX_DATA, "HP-UX Data"},
        {GUID_HP_UX_SERVICE, "HP-UX Service"},
        {GUID_IBM_GPFS, "IBM General Parallel FS"},
    #endif
    {{0}, NULL}
};

const char* partition_get_name(partition_t* part) {
    if (part->is_gpt) {
        for (int i = 0; guid_name_table[i].name != NULL; i++) {
            if (memcmp(guid_name_table[i].guid, part->guid, 16) == 0) {
                return guid_name_table[i].name;
            }
        }
    } else {
        for (int i = 0; partition_lookup_table[i].name != NULL; i++) {
            if (partition_lookup_table[i].mbr_type == part->type_id) {
                return partition_lookup_table[i].name;
            }
        }
    }

    return "Unknown";
}

void partition_register(partition_t* new_part) {
    if (!new_part) return;

    if (head == NULL) {
        head = new_part;
    } else {
        partition_t* current = head;
        while (current->next != NULL) {
            current = (partition_t*)current->next;
        }
        current->next = (struct partition_s*)new_part;
    }
}