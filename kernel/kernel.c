#include "efi.h"
#include "ap_request.h"
#include "ap_services.h"

typedef struct {
    UINT32 *base;
    UINT32 width;
    UINT32 height;
    UINT32 stride;
    UINTN size;
    EFI_GRAPHICS_PIXEL_FORMAT format;
} framebuffer_t;

typedef struct {
    EFI_MEMORY_DESCRIPTOR *descriptors;
    UINTN map_size;
    UINTN map_capacity;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    UINTN descriptor_count;
} efi_memory_map_t;

typedef struct {
    UINT64 cr3;
    UINT64 loaded_cr3;
    UINT64 pool_start;
    UINT64 pool_end;
    UINT64 pool_next;
    UINT64 max_identity_end;
    UINT64 reserved_pages;
    UINT64 allocated_pages;
    UINT64 mapped_pages;
    UINT64 mapped_ranges;
    UINT32 error;
    UINT32 idt_self_test_ok;
} paging_info_t;

#define PAGE_SIZE_4K 4096ULL
#define PAGE_TABLE_ENTRIES 512ULL
#define LOW_CANONICAL_LIMIT 0x0000800000000000ULL
#define PTE_PRESENT 0x001ULL
#define PTE_RW 0x002ULL
#define PTE_PWT 0x008ULL
#define PTE_PCD 0x010ULL
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#ifndef VIBE_FAULT_TEST
#define VIBE_FAULT_TEST 0
#endif

#define VIBE_FAULT_TEST_NONE 0
#define VIBE_FAULT_TEST_UD2 1
#define VIBE_FAULT_TEST_PF 2

#ifndef VIBE_AP_FAULT_TEST
#define VIBE_AP_FAULT_TEST 0
#endif

#define VIBE_AP_FAULT_TEST_NONE 0
#define VIBE_AP_FAULT_TEST_UD2 1
#define VIBE_AP_FAULT_TEST_PF 2

#ifndef VIBE_AP_FIRST_REQUEST_OPCODE
#define VIBE_AP_FIRST_REQUEST_OPCODE AP_REQUEST_OP_PING
#endif

#ifndef VIBE_AP_FIRST_REQUEST_SERVICE_ID
#define VIBE_AP_FIRST_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_PING
#endif

#ifndef VIBE_AP_FIRST_REQUEST_INTERFACE_ID
#define VIBE_AP_FIRST_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_PING
#endif

#ifndef VIBE_AP_SECOND_REQUEST_OPCODE
#define VIBE_AP_SECOND_REQUEST_OPCODE AP_REQUEST_OP_COUNTER
#endif

#ifndef VIBE_AP_SECOND_REQUEST_SERVICE_ID
#define VIBE_AP_SECOND_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_COUNTER
#endif

#ifndef VIBE_AP_SECOND_REQUEST_INTERFACE_ID
#define VIBE_AP_SECOND_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_COUNTER_INCREMENT
#endif

#ifndef VIBE_AP_THIRD_REQUEST_OPCODE
#define VIBE_AP_THIRD_REQUEST_OPCODE AP_REQUEST_OP_COUNTER
#endif

#ifndef VIBE_AP_THIRD_REQUEST_SERVICE_ID
#define VIBE_AP_THIRD_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_COUNTER
#endif

#ifndef VIBE_AP_THIRD_REQUEST_INTERFACE_ID
#define VIBE_AP_THIRD_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_COUNTER_INCREMENT
#endif

#ifndef VIBE_AP_FOURTH_REQUEST_OPCODE
#define VIBE_AP_FOURTH_REQUEST_OPCODE AP_REQUEST_OP_PING
#endif

#ifndef VIBE_AP_FOURTH_REQUEST_SERVICE_ID
#define VIBE_AP_FOURTH_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_PING
#endif

#ifndef VIBE_AP_FOURTH_REQUEST_INTERFACE_ID
#define VIBE_AP_FOURTH_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_FIRST_REQUEST_OPCODE
#define VIBE_AP_SECOND_BATCH_FIRST_REQUEST_OPCODE AP_REQUEST_OP_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_FIRST_REQUEST_SERVICE_ID
#define VIBE_AP_SECOND_BATCH_FIRST_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_FIRST_REQUEST_INTERFACE_ID
#define VIBE_AP_SECOND_BATCH_FIRST_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_SECOND_REQUEST_OPCODE
#define VIBE_AP_SECOND_BATCH_SECOND_REQUEST_OPCODE AP_REQUEST_OP_COUNTER
#endif

#ifndef VIBE_AP_SECOND_BATCH_SECOND_REQUEST_SERVICE_ID
#define VIBE_AP_SECOND_BATCH_SECOND_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_COUNTER
#endif

#ifndef VIBE_AP_SECOND_BATCH_SECOND_REQUEST_INTERFACE_ID
#define VIBE_AP_SECOND_BATCH_SECOND_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_COUNTER_INCREMENT
#endif

#ifndef VIBE_AP_SECOND_BATCH_THIRD_REQUEST_OPCODE
#define VIBE_AP_SECOND_BATCH_THIRD_REQUEST_OPCODE AP_REQUEST_OP_COUNTER
#endif

#ifndef VIBE_AP_SECOND_BATCH_THIRD_REQUEST_SERVICE_ID
#define VIBE_AP_SECOND_BATCH_THIRD_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_COUNTER
#endif

#ifndef VIBE_AP_SECOND_BATCH_THIRD_REQUEST_INTERFACE_ID
#define VIBE_AP_SECOND_BATCH_THIRD_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_COUNTER_INCREMENT
#endif

#ifndef VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_OPCODE
#define VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_OPCODE AP_REQUEST_OP_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_SERVICE_ID
#define VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_SERVICE_ID AP_REQUEST_SERVICE_PING
#endif

#ifndef VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_INTERFACE_ID
#define VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_INTERFACE_ID AP_REQUEST_INTERFACE_PING
#endif

#ifndef VIBE_AP_REQUEST_TARGET_INDEX
#define VIBE_AP_REQUEST_TARGET_INDEX 0
#endif

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define KERNEL_TSS_SELECTOR 0x18
#define CPU_GDT_ENTRIES 5
#define CPU_IST_FAULT 1
#define CPU_IST_DOUBLE_FAULT 2
#define CPU_FAULT_STACK_SIZE (16 * 1024)
#define ACPI_MAX_CPUS 32
#define ACPI_MAX_RSDP_LENGTH 4096U
#define ACPI_MAX_SDT_LENGTH (1024U * 1024U)
#define ACPI_CPU_LIST_OUTPUT_LIMIT 5
#define MAX_AP_CONTEXTS 8
#define AP_BOOT_STACK_SIZE (16 * 1024)
#define AP_TRAMPOLINE_PAGES 1ULL
#define AP_TRAMPOLINE_MAX_ADDRESS 0x9F000ULL
#define AP_TRAMPOLINE_PROT32_OFFSET 0x60U
#define AP_TRAMPOLINE_LONG_OFFSET 0xC0U
#define AP_TRAMPOLINE_GDTR_OFFSET 0x180U
#define AP_TRAMPOLINE_GDT_OFFSET 0x190U
#define AP_TRAMPOLINE_PROT32_SELECTOR 0x18
#define AP_ONLINE_TIMEOUT_LOOPS 100000000U
#ifndef AP_REQUEST_TIMEOUT_LOOPS
#define AP_REQUEST_TIMEOUT_LOOPS 100000000U
#endif
#define AP_REQUEST_SLOT_COUNT 4U
#define AP_REQUEST_HISTORY_COUNT 8U
#define AP_REQUEST_NO_SLOT 0xffffffffU
#define AP_REQUEST_KICK_IPI_VECTOR 0xf0U
#define AP_REQUEST_IPI_VECTOR 0xf1U
#define AP_IDLE_TIMER_VECTOR 0xf2U
#define AP_IDLE_TIMER_INITIAL_COUNT 1000000U
#define BSP_WAIT_TIMER_VECTOR 0xf3U
#define BSP_WAIT_TIMER_INITIAL_COUNT 1000000U
#ifndef BSP_WAIT_TIMER_TIMEOUT_STEP
#define BSP_WAIT_TIMER_TIMEOUT_STEP 100000U
#endif
#define AP_REQUEST_IPI_DRAIN_LOOPS 100000U
#define LAPIC_SPURIOUS_VECTOR 0xffU
#define LAPIC_LVT_MASKED (1U << 16)

enum {
    PAGING_OK = 0,
    PAGING_ERR_LA57 = 1,
    PAGING_ERR_RANGE = 2,
    PAGING_ERR_NO_POOL = 3,
    PAGING_ERR_POOL_EMPTY = 4,
    PAGING_ERR_TABLE_ENTRY = 5,
    PAGING_ERR_MAP_FULL = 6,
};

enum {
    ACPI_OK = 0,
    ACPI_ERR_NO_RSDP = 1,
    ACPI_ERR_BAD_RSDP = 2,
    ACPI_ERR_NO_XSDT = 3,
    ACPI_ERR_BAD_XSDT = 4,
    ACPI_ERR_NO_MADT = 5,
    ACPI_ERR_BAD_MADT = 6,
    ACPI_ERR_BAD_ENTRY = 7,
    ACPI_ERR_RANGE = 8,
};

enum {
    AP_BOOT_OK = 0,
    AP_BOOT_ERR_NO_TRAMPOLINE = 1,
    AP_BOOT_ERR_NO_TARGET = 2,
    AP_BOOT_ERR_X2APIC = 3,
    AP_BOOT_ERR_BAD_TRAMPOLINE = 4,
    AP_BOOT_ERR_HIGH_CR3 = 5,
    AP_BOOT_ERR_LAPIC_RANGE = 6,
    AP_BOOT_ERR_ICR_TIMEOUT = 7,
    AP_BOOT_ERR_ONLINE_TIMEOUT = 8,
};

enum {
    AP_BOOT_STATE_NONE = 0,
    AP_BOOT_STATE_READY = 1,
    AP_BOOT_STATE_SENT_INIT = 2,
    AP_BOOT_STATE_SENT_SIPI = 3,
    AP_BOOT_STATE_ONLINE = 4,
    AP_BOOT_STATE_HALTED = 5,
    AP_BOOT_STATE_FAULTED = 6,
};

enum {
    AP_ENTRY_STATE_NONE = 0,
    AP_ENTRY_STATE_C = 1,
    AP_ENTRY_STATE_TABLES = 2,
    AP_ENTRY_STATE_HALTED = 3,
    AP_ENTRY_STATE_FAULT = 4,
    AP_ENTRY_STATE_LOOP = 5,
    AP_ENTRY_STATE_REQUEST = 6,
};

enum {
    AP_FAULT_PHASE_NONE = 0,
    AP_FAULT_PHASE_STARTUP = 1,
    AP_FAULT_PHASE_LOOP = 2,
    AP_FAULT_PHASE_REQUEST = 3,
};

enum {
    AP_QUEUE_STOP_NONE = 0,
    AP_QUEUE_STOP_DRAINED = 1,
    AP_QUEUE_STOP_BAD_OP = 2,
    AP_QUEUE_STOP_FAULT = 3,
};

static UINT32 color(framebuffer_t *fb, UINT8 r, UINT8 g, UINT8 b) {
    if (fb->format == PixelRedGreenBlueReserved8BitPerColor) {
        return ((UINT32)b << 16) | ((UINT32)g << 8) | r;
    }
    return ((UINT32)r << 16) | ((UINT32)g << 8) | b;
}

static UINT8 glyph_row(char c, int row) {
    static const UINT8 space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const UINT8 *g = space;

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    switch (c) {
    case '0': { static const UINT8 r[7] = {14, 17, 19, 21, 25, 17, 14}; g = r; break; }
    case '1': { static const UINT8 r[7] = {4, 12, 4, 4, 4, 4, 14}; g = r; break; }
    case '2': { static const UINT8 r[7] = {14, 17, 1, 2, 4, 8, 31}; g = r; break; }
    case '3': { static const UINT8 r[7] = {30, 1, 1, 14, 1, 1, 30}; g = r; break; }
    case '4': { static const UINT8 r[7] = {2, 6, 10, 18, 31, 2, 2}; g = r; break; }
    case '5': { static const UINT8 r[7] = {31, 16, 16, 30, 1, 17, 14}; g = r; break; }
    case '6': { static const UINT8 r[7] = {6, 8, 16, 30, 17, 17, 14}; g = r; break; }
    case '7': { static const UINT8 r[7] = {31, 1, 2, 4, 8, 8, 8}; g = r; break; }
    case '8': { static const UINT8 r[7] = {14, 17, 17, 14, 17, 17, 14}; g = r; break; }
    case '9': { static const UINT8 r[7] = {14, 17, 17, 15, 1, 2, 12}; g = r; break; }
    case 'A': { static const UINT8 r[7] = {14, 17, 17, 31, 17, 17, 17}; g = r; break; }
    case 'B': { static const UINT8 r[7] = {30, 17, 17, 30, 17, 17, 30}; g = r; break; }
    case 'C': { static const UINT8 r[7] = {14, 17, 16, 16, 16, 17, 14}; g = r; break; }
    case 'D': { static const UINT8 r[7] = {30, 17, 17, 17, 17, 17, 30}; g = r; break; }
    case 'E': { static const UINT8 r[7] = {31, 16, 16, 30, 16, 16, 31}; g = r; break; }
    case 'F': { static const UINT8 r[7] = {31, 16, 16, 30, 16, 16, 16}; g = r; break; }
    case 'G': { static const UINT8 r[7] = {14, 17, 16, 23, 17, 17, 15}; g = r; break; }
    case 'H': { static const UINT8 r[7] = {17, 17, 17, 31, 17, 17, 17}; g = r; break; }
    case 'I': { static const UINT8 r[7] = {14, 4, 4, 4, 4, 4, 14}; g = r; break; }
    case 'J': { static const UINT8 r[7] = {7, 2, 2, 2, 18, 18, 12}; g = r; break; }
    case 'K': { static const UINT8 r[7] = {17, 18, 20, 24, 20, 18, 17}; g = r; break; }
    case 'L': { static const UINT8 r[7] = {16, 16, 16, 16, 16, 16, 31}; g = r; break; }
    case 'M': { static const UINT8 r[7] = {17, 27, 21, 21, 17, 17, 17}; g = r; break; }
    case 'N': { static const UINT8 r[7] = {17, 25, 21, 19, 17, 17, 17}; g = r; break; }
    case 'O': { static const UINT8 r[7] = {14, 17, 17, 17, 17, 17, 14}; g = r; break; }
    case 'P': { static const UINT8 r[7] = {30, 17, 17, 30, 16, 16, 16}; g = r; break; }
    case 'Q': { static const UINT8 r[7] = {14, 17, 17, 17, 21, 18, 13}; g = r; break; }
    case 'R': { static const UINT8 r[7] = {30, 17, 17, 30, 20, 18, 17}; g = r; break; }
    case 'S': { static const UINT8 r[7] = {15, 16, 16, 14, 1, 1, 30}; g = r; break; }
    case 'T': { static const UINT8 r[7] = {31, 4, 4, 4, 4, 4, 4}; g = r; break; }
    case 'U': { static const UINT8 r[7] = {17, 17, 17, 17, 17, 17, 14}; g = r; break; }
    case 'V': { static const UINT8 r[7] = {17, 17, 17, 17, 17, 10, 4}; g = r; break; }
    case 'W': { static const UINT8 r[7] = {17, 17, 17, 21, 21, 21, 10}; g = r; break; }
    case 'X': { static const UINT8 r[7] = {17, 17, 10, 4, 10, 17, 17}; g = r; break; }
    case 'Y': { static const UINT8 r[7] = {17, 17, 10, 4, 4, 4, 4}; g = r; break; }
    case 'Z': { static const UINT8 r[7] = {31, 1, 2, 4, 8, 16, 31}; g = r; break; }
    case ':': { static const UINT8 r[7] = {0, 4, 4, 0, 4, 4, 0}; g = r; break; }
    case '.': { static const UINT8 r[7] = {0, 0, 0, 0, 0, 12, 12}; g = r; break; }
    case ',': { static const UINT8 r[7] = {0, 0, 0, 0, 0, 4, 8}; g = r; break; }
    case '-': { static const UINT8 r[7] = {0, 0, 0, 31, 0, 0, 0}; g = r; break; }
    case '_': { static const UINT8 r[7] = {0, 0, 0, 0, 0, 0, 31}; g = r; break; }
    case '/': { static const UINT8 r[7] = {1, 2, 2, 4, 8, 8, 16}; g = r; break; }
    case '(': { static const UINT8 r[7] = {2, 4, 8, 8, 8, 4, 2}; g = r; break; }
    case ')': { static const UINT8 r[7] = {8, 4, 2, 2, 2, 4, 8}; g = r; break; }
    case '+': { static const UINT8 r[7] = {0, 4, 4, 31, 4, 4, 0}; g = r; break; }
    case '@': { static const UINT8 r[7] = {14, 17, 23, 21, 23, 16, 14}; g = r; break; }
    case ' ': g = space; break;
    }

    return g[row];
}

static void put_pixel(framebuffer_t *fb, UINT32 x, UINT32 y, UINT32 c) {
    if (x < fb->width && y < fb->height) {
        fb->base[(UINTN)y * fb->stride + x] = c;
    }
}

static void fill_rect(framebuffer_t *fb, UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 c) {
    for (UINT32 yy = 0; yy < h; yy++) {
        for (UINT32 xx = 0; xx < w; xx++) {
            put_pixel(fb, x + xx, y + yy, c);
        }
    }
}

static void clear_screen(framebuffer_t *fb, UINT32 c) {
    for (UINT32 y = 0; y < fb->height; y++) {
        for (UINT32 x = 0; x < fb->width; x++) {
            fb->base[(UINTN)y * fb->stride + x] = c;
        }
    }
}

static void draw_char(framebuffer_t *fb, UINT32 x, UINT32 y, char ch, UINT32 fg, UINT32 bg, UINT32 scale) {
    fill_rect(fb, x, y, 6 * scale, 8 * scale, bg);
    for (int row = 0; row < 7; row++) {
        UINT8 bits = glyph_row(ch, row);
        for (int col = 0; col < 5; col++) {
            if (bits & (1U << (4 - col))) {
                fill_rect(fb, x + (UINT32)col * scale, y + (UINT32)row * scale, scale, scale, fg);
            }
        }
    }
}

static void draw_text(framebuffer_t *fb, UINT32 x, UINT32 y, const char *s, UINT32 fg, UINT32 bg, UINT32 scale) {
    while (*s) {
        draw_char(fb, x, y, *s, fg, bg, scale);
        x += 6 * scale;
        s++;
    }
}

static void draw_line(framebuffer_t *fb, UINT32 x, UINT32 *y, const char *s, UINT32 fg, UINT32 bg, UINT32 scale) {
    draw_text(fb, x, *y, s, fg, bg, scale);
    *y += 10 * scale;
}

typedef struct {
    char vendor[13];
    char brand[49];
    UINT32 family;
    UINT32 model;
    UINT32 stepping;
    UINT32 logical_processors;
    UINT32 cores_per_package;
    UINT32 physical_address_bits;
    UINT32 virtual_address_bits;
    UINT32 max_basic_leaf;
    UINT32 max_extended_leaf;
    UINT32 leaf1_ecx;
    UINT32 leaf1_edx;
    UINT32 leaf7_ebx;
    UINT32 ext_edx;
} cpu_info_t;

typedef struct {
    UINT16 offset_low;
    UINT16 selector;
    UINT8 ist;
    UINT8 type_attr;
    UINT16 offset_mid;
    UINT32 offset_high;
    UINT32 zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    UINT16 limit;
    UINT64 base;
} __attribute__((packed)) descriptor_table_ptr_t;

typedef struct {
    UINT32 reserved0;
    UINT64 rsp[3];
    UINT64 reserved1;
    UINT64 ist[7];
    UINT64 reserved2;
    UINT16 reserved3;
    UINT16 io_map_base;
} __attribute__((packed)) tss64_t;

typedef struct {
    UINT32 id;
    UINT16 loaded_tr;
    UINT8 tss_ready;
    UINT8 reserved;
    UINT64 gdt[CPU_GDT_ENTRIES];
    tss64_t tss;
    UINT8 fault_stack[CPU_FAULT_STACK_SIZE] __attribute__((aligned(16)));
    UINT8 double_fault_stack[CPU_FAULT_STACK_SIZE] __attribute__((aligned(16)));
} __attribute__((aligned(16))) cpu_local_t;

typedef struct {
    EFI_GUID VendorGuid;
    void *VendorTable;
} efi_configuration_table_t;

typedef struct {
    char signature[8];
    UINT8 checksum;
    char oem_id[6];
    UINT8 revision;
    UINT32 rsdt_address;
    UINT32 length;
    UINT64 xsdt_address;
    UINT8 extended_checksum;
    UINT8 reserved[3];
} __attribute__((packed)) acpi_rsdp_t;
_Static_assert(sizeof(acpi_rsdp_t) == 36, "ACPI RSDP layout must match ACPI 2.0");

typedef struct {
    char signature[4];
    UINT32 length;
    UINT8 revision;
    UINT8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    UINT32 oem_revision;
    UINT32 creator_id;
    UINT32 creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;
_Static_assert(sizeof(acpi_sdt_header_t) == 36, "ACPI SDT header layout must match ACPI");

typedef struct {
    UINT32 acpi_uid;
    UINT32 apic_id;
    UINT32 flags;
    UINT8 x2apic;
} acpi_cpu_t;

typedef struct {
    UINT64 rsdp;
    UINT64 xsdt;
    UINT64 madt;
    UINT64 local_apic_base;
    UINT32 madt_flags;
    UINT32 bsp_apic_id;
    UINT32 rsdp_revision;
    UINT32 xsdt_entries;
    UINT32 enabled_cpu_count;
    UINT32 stored_cpu_count;
    UINT32 error;
    acpi_cpu_t cpus[ACPI_MAX_CPUS];
} acpi_info_t;

typedef struct {
    UINT64 trampoline_base;
    UINT64 stack_top;
    volatile UINT32 online;
    volatile UINT32 ap_state;
    volatile UINT32 entry_state;
    volatile UINT32 gdt_ok;
    volatile UINT32 tss_ok;
    volatile UINT32 idt_ok;
    volatile UINT64 ap_cs;
    volatile UINT64 ap_tr;
    volatile UINT64 ap_ist1;
    volatile UINT64 ap_ist2;
    volatile UINT64 fault_vector;
    volatile UINT64 fault_error_code;
    volatile UINT64 fault_rip;
    volatile UINT64 fault_cs;
    volatile UINT64 fault_rflags;
    volatile UINT64 fault_cr2;
    volatile UINT32 fault_phase;
    volatile UINT32 fault_request_state;
    volatile UINT32 fault_request_opcode;
    volatile UINT32 fault_request_sequence;
    volatile UINT64 fault_request_service_id;
    volatile UINT64 fault_request_interface_id;
    volatile UINT64 fault_request_id_high;
    volatile UINT64 fault_request_id_low;
    UINT32 target_acpi_uid;
    UINT32 target_apic_id;
    UINT32 sipi_vector;
    UINT32 error;
    UINT32 wait_loops;
    UINT32 icr_timeouts;
} ap_boot_info_t;

typedef struct {
    ap_request_slot_t request;
    UINT32 counter_value;
    UINT32 slot;
    UINT32 valid;
} ap_request_history_entry_t;

typedef struct {
    volatile UINT32 current_slot;
    volatile UINT32 handled_count;
    volatile UINT32 last_slot;
    volatile UINT32 last_state;
    volatile UINT32 stop_reason;
    volatile UINT32 planned_count;
} ap_queue_summary_t;

typedef struct {
    volatile UINT32 vector;
    volatile UINT32 count;
    volatile UINT64 rip;
    volatile UINT64 cs;
    volatile UINT64 rflags;
} interrupt_trace_t;
_Static_assert(sizeof(interrupt_trace_t) == 32, "interrupt trace layout must match ISR writes");
_Static_assert(__builtin_offsetof(interrupt_trace_t, count) == 4,
               "interrupt trace count offset must match ISR writes");
_Static_assert(__builtin_offsetof(interrupt_trace_t, rip) == 8,
               "interrupt trace RIP offset must match ISR writes");
_Static_assert(__builtin_offsetof(interrupt_trace_t, cs) == 16,
               "interrupt trace CS offset must match ISR writes");
_Static_assert(__builtin_offsetof(interrupt_trace_t, rflags) == 24,
               "interrupt trace RFLAGS offset must match ISR writes");

typedef struct {
    interrupt_trace_t kick_ipi;
    interrupt_trace_t completion_ipi;
    volatile UINT32 idle_timer_count;
} ap_interrupt_observe_t;
_Static_assert(__builtin_offsetof(ap_interrupt_observe_t, kick_ipi) == 0,
               "AP kick IPI trace offset must match ISR writes");
_Static_assert(__builtin_offsetof(ap_interrupt_observe_t, completion_ipi) == 32,
               "AP completion IPI trace offset must match ISR writes");
_Static_assert(__builtin_offsetof(ap_interrupt_observe_t, idle_timer_count) == 64,
               "AP idle timer count offset must match ISR writes");

typedef struct {
    interrupt_trace_t idt_self_test;
} bsp_interrupt_observe_t;
_Static_assert(__builtin_offsetof(bsp_interrupt_observe_t, idt_self_test) == 0,
               "BSP IDT self-test trace offset must match ISR writes");

typedef struct {
    interrupt_trace_t spurious;
} system_interrupt_observe_t;
_Static_assert(__builtin_offsetof(system_interrupt_observe_t, spurious) == 0,
               "System spurious trace offset must match ISR writes");

typedef struct {
    volatile UINT32 halt_count;
    volatile UINT32 wake_count;
    volatile UINT32 timer_count;
} bsp_wait_observe_t;
_Static_assert(__builtin_offsetof(bsp_wait_observe_t, halt_count) == 0,
               "BSP wait halt count offset must match C writes");
_Static_assert(__builtin_offsetof(bsp_wait_observe_t, wake_count) == 4,
               "BSP wait wake count offset must match C writes");
_Static_assert(__builtin_offsetof(bsp_wait_observe_t, timer_count) == 8,
               "BSP wait timer count offset must match ISR writes");

typedef struct {
    cpu_local_t cpu;
    idt_entry_t idt[256];
} bsp_context_t;

typedef struct {
    framebuffer_t framebuffer;
    UINT32 bg;
    UINT32 fg;
    UINT32 accent;
    UINT32 warn;
} bsp_fault_display_t;

typedef struct {
    ap_boot_info_t boot;
    UINT8 boot_stack[AP_BOOT_STACK_SIZE] __attribute__((aligned(16)));
    cpu_local_t cpu;
    idt_entry_t idt[256];
    ap_interrupt_observe_t interrupts;
    ap_request_slot_t request_slots[AP_REQUEST_SLOT_COUNT];
    ap_request_history_entry_t request_history[AP_REQUEST_HISTORY_COUNT];
    ap_queue_summary_t queue_summary;
    volatile UINT32 current_request_slot;
    volatile UINT32 request_handled_count;
    volatile UINT32 counter_value;
    volatile UINT32 request_kick_ipi_send_count;
    volatile UINT32 request_kick_ipi_send_fail_count;
    volatile UINT32 request_ipi_send_count;
    volatile UINT32 request_ipi_send_fail_count;
    volatile UINT32 idle_halt_count;
    volatile UINT32 idle_wake_count;
    volatile UINT32 completion_target_apic_id;
} ap_context_t;

typedef struct {
    UINT64 vector;
    UINT64 error_code;
    UINT64 rip;
    UINT64 cs;
    UINT64 rflags;
} fault_frame_t;

static bsp_context_t bsp_context;
static bsp_interrupt_observe_t bsp_interrupt_observe;
static system_interrupt_observe_t system_interrupt_observe;
static bsp_fault_display_t bsp_fault_display;
static ap_context_t ap_contexts[MAX_AP_CONTEXTS];
static ap_context_t *ap_request_target_context = &ap_contexts[0];
static ap_interrupt_observe_t *ap_request_interrupts __attribute__((used)) = &ap_contexts[0].interrupts;
static volatile UINT64 system_lapic_base;
static bsp_wait_observe_t bsp_wait_observe;

static ap_context_t *ap0_context(void) {
    return &ap_contexts[0];
}

static UINT32 ap_context_index(ap_context_t *ctx) {
    for (UINT32 i = 0; i < MAX_AP_CONTEXTS; i++) {
        if (ctx == &ap_contexts[i]) {
            return i;
        }
    }
    return 0;
}

static void set_ap_request_target_context(ap_context_t *ctx) {
    ap_request_target_context = ctx;
    ap_request_interrupts = &ctx->interrupts;
}

static int ap_context_is_request_target(ap_context_t *ctx) {
    return ctx == ap_request_target_context;
}

static ap_context_t *select_ap_request_target_context(UINTN context_count) {
    UINTN target_index = VIBE_AP_REQUEST_TARGET_INDEX;
    if (context_count == 0 || target_index >= context_count || target_index >= MAX_AP_CONTEXTS) {
        return ap0_context();
    }
    return &ap_contexts[target_index];
}

static int address_in_range(UINT64 addr, UINT64 start, UINT64 size) {
    return addr >= start && addr < start + size;
}

static ap_context_t *ap_context_from_fault_frame(fault_frame_t *frame) {
    UINT64 frame_addr = (UINT64)(UINTN)frame;

    for (UINTN i = 0; i < MAX_AP_CONTEXTS; i++) {
        cpu_local_t *cpu = &ap_contexts[i].cpu;
        UINT64 fault_stack = (UINT64)(UINTN)cpu->fault_stack;
        UINT64 double_fault_stack = (UINT64)(UINTN)cpu->double_fault_stack;

        if (address_in_range(frame_addr, fault_stack, sizeof(cpu->fault_stack)) ||
            address_in_range(frame_addr, double_fault_stack, sizeof(cpu->double_fault_stack))) {
            return &ap_contexts[i];
        }
    }

    return ap0_context();
}

static int cmpxchg_u32(volatile UINT32 *ptr, UINT32 expected, UINT32 desired);
static void enable_lapic_if_needed(UINT64 base);
static void lapic_write32(UINT64 base, UINT32 reg, UINT32 value);
static int lapic_send_ipi(UINT64 base, UINT32 apic_id, UINT32 command, ap_boot_info_t *boot);
static void ap_notify_bsp_request_complete(ap_context_t *ctx);
static UINT64 read_rflags(void);

extern void isr_breakpoint(void);
extern void isr_fault_ud(void);
extern void isr_fault_df(void);
extern void isr_fault_gp(void);
extern void isr_fault_pf(void);
extern void isr_unhandled(void);
extern void isr_ap_request_kick_ipi(void);
extern void isr_ap_request_ipi(void);
extern void isr_ap_idle_timer(void);
extern void isr_bsp_wait_timer(void);
extern void isr_spurious_interrupt(void);
extern void isr_ap_fault_ud(void);
extern void isr_ap_fault_df(void);
extern void isr_ap_fault_gp(void);
extern void isr_ap_fault_pf(void);
extern void isr_ap_unhandled(void);
void fault_dispatch(fault_frame_t *frame) __attribute__((noreturn));
void ap_fault_dispatch(fault_frame_t *frame) __attribute__((noreturn));

__asm__(
    ".text\n"
    ".global isr_breakpoint\n"
    "isr_breakpoint:\n"
    "    pushq %rax\n"
    "    movq 8(%rsp), %rax\n"
    "    movq %rax, bsp_interrupt_observe+8(%rip)\n"
    "    movq 16(%rsp), %rax\n"
    "    movq %rax, bsp_interrupt_observe+16(%rip)\n"
    "    movq 24(%rsp), %rax\n"
    "    movq %rax, bsp_interrupt_observe+24(%rip)\n"
    "    movl $3, bsp_interrupt_observe(%rip)\n"
    "    movl bsp_interrupt_observe+4(%rip), %eax\n"
    "    addl $1, %eax\n"
    "    movl %eax, bsp_interrupt_observe+4(%rip)\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_fault_ud\n"
    "isr_fault_ud:\n"
    "    pushq $0\n"
    "    pushq $6\n"
    "    jmp isr_fault_common\n"
    ".global isr_fault_df\n"
    "isr_fault_df:\n"
    "    pushq $8\n"
    "    jmp isr_fault_common\n"
    ".global isr_fault_gp\n"
    "isr_fault_gp:\n"
    "    pushq $13\n"
    "    jmp isr_fault_common\n"
    ".global isr_fault_pf\n"
    "isr_fault_pf:\n"
    "    pushq $14\n"
    "    jmp isr_fault_common\n"
    "isr_fault_common:\n"
    "    cld\n"
    "    movq %rsp, %rdi\n"
    "    andq $-16, %rsp\n"
    "    call fault_dispatch\n"
    "2:\n"
    "    cli\n"
    "    hlt\n"
    "    jmp 2b\n"
    ".global isr_unhandled\n"
    "isr_unhandled:\n"
    "    cli\n"
    "1:\n"
    "    hlt\n"
    "    jmp 1b\n"
    ".global isr_ap_request_kick_ipi\n"
    "isr_ap_request_kick_ipi:\n"
    "    pushq %rax\n"
    "    pushq %rdx\n"
    "    movq ap_request_interrupts(%rip), %rax\n"
    "    movq 16(%rsp), %rdx\n"
    "    movq %rdx, 8(%rax)\n"
    "    movq 24(%rsp), %rdx\n"
    "    movq %rdx, 16(%rax)\n"
    "    movq 32(%rsp), %rdx\n"
    "    movq %rdx, 24(%rax)\n"
    "    movl $0xf0, (%rax)\n"
    "    movl 4(%rax), %edx\n"
    "    addl $1, %edx\n"
    "    movl %edx, 4(%rax)\n"
    "    movq system_lapic_base(%rip), %rax\n"
    "    testq %rax, %rax\n"
    "    jz 6f\n"
    "    movl $0, 0xb0(%rax)\n"
    "6:\n"
    "    popq %rdx\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_ap_request_ipi\n"
    "isr_ap_request_ipi:\n"
    "    pushq %rax\n"
    "    pushq %rdx\n"
    "    movq ap_request_interrupts(%rip), %rax\n"
    "    movq 16(%rsp), %rdx\n"
    "    movq %rdx, 40(%rax)\n"
    "    movq 24(%rsp), %rdx\n"
    "    movq %rdx, 48(%rax)\n"
    "    movq 32(%rsp), %rdx\n"
    "    movq %rdx, 56(%rax)\n"
    "    movl $0xf1, 32(%rax)\n"
    "    movl 36(%rax), %edx\n"
    "    addl $1, %edx\n"
    "    movl %edx, 36(%rax)\n"
    "    movq system_lapic_base(%rip), %rax\n"
    "    testq %rax, %rax\n"
    "    jz 5f\n"
    "    movl $0, 0xb0(%rax)\n"
    "5:\n"
    "    popq %rdx\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_ap_idle_timer\n"
    "isr_ap_idle_timer:\n"
    "    pushq %rax\n"
    "    movq ap_request_interrupts(%rip), %rax\n"
    "    addl $1, 64(%rax)\n"
    "    movq system_lapic_base(%rip), %rax\n"
    "    testq %rax, %rax\n"
    "    jz 7f\n"
    "    movl $0, 0xb0(%rax)\n"
    "7:\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_bsp_wait_timer\n"
    "isr_bsp_wait_timer:\n"
    "    pushq %rax\n"
    "    movl bsp_wait_observe+8(%rip), %eax\n"
    "    addl $1, %eax\n"
    "    movl %eax, bsp_wait_observe+8(%rip)\n"
    "    movq system_lapic_base(%rip), %rax\n"
    "    testq %rax, %rax\n"
    "    jz 8f\n"
    "    movl $0, 0xb0(%rax)\n"
    "8:\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_spurious_interrupt\n"
    "isr_spurious_interrupt:\n"
    "    pushq %rax\n"
    "    movq 8(%rsp), %rax\n"
    "    movq %rax, system_interrupt_observe+8(%rip)\n"
    "    movq 16(%rsp), %rax\n"
    "    movq %rax, system_interrupt_observe+16(%rip)\n"
    "    movq 24(%rsp), %rax\n"
    "    movq %rax, system_interrupt_observe+24(%rip)\n"
    "    movl $0xff, system_interrupt_observe(%rip)\n"
    "    movl system_interrupt_observe+4(%rip), %eax\n"
    "    addl $1, %eax\n"
    "    movl %eax, system_interrupt_observe+4(%rip)\n"
    "    popq %rax\n"
    "    iretq\n"
    ".global isr_ap_fault_ud\n"
    "isr_ap_fault_ud:\n"
    "    pushq $0\n"
    "    pushq $6\n"
    "    jmp isr_ap_fault_common\n"
    ".global isr_ap_fault_df\n"
    "isr_ap_fault_df:\n"
    "    pushq $8\n"
    "    jmp isr_ap_fault_common\n"
    ".global isr_ap_fault_gp\n"
    "isr_ap_fault_gp:\n"
    "    pushq $13\n"
    "    jmp isr_ap_fault_common\n"
    ".global isr_ap_fault_pf\n"
    "isr_ap_fault_pf:\n"
    "    pushq $14\n"
    "    jmp isr_ap_fault_common\n"
    "isr_ap_fault_common:\n"
    "    cld\n"
    "    movq %rsp, %rdi\n"
    "    andq $-16, %rsp\n"
    "    call ap_fault_dispatch\n"
    "4:\n"
    "    cli\n"
    "    hlt\n"
    "    jmp 4b\n"
    ".global isr_ap_unhandled\n"
    "isr_ap_unhandled:\n"
    "    cli\n"
    "3:\n"
    "    hlt\n"
    "    jmp 3b\n");

static void cpuid(UINT32 leaf, UINT32 subleaf, UINT32 *a, UINT32 *b, UINT32 *c, UINT32 *d) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf)
        : "memory");
}

static void put_u32_chars(char *dst, UINT32 value) {
    dst[0] = (char)(value & 0xff);
    dst[1] = (char)((value >> 8) & 0xff);
    dst[2] = (char)((value >> 16) & 0xff);
    dst[3] = (char)((value >> 24) & 0xff);
}

static char *append_str(char *p, const char *s) {
    while (*s) {
        *p++ = *s++;
    }
    *p = 0;
    return p;
}

static char *append_dec(char *p, UINT32 value) {
    char tmp[10];
    UINT32 n = 0;
    if (value == 0) {
        *p++ = '0';
        *p = 0;
        return p;
    }
    while (value > 0) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0) {
        *p++ = tmp[--n];
    }
    *p = 0;
    return p;
}

static char *append_dec64(char *p, UINT64 value) {
    char tmp[20];
    UINT32 n = 0;
    if (value == 0) {
        *p++ = '0';
        *p = 0;
        return p;
    }
    while (value > 0) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0) {
        *p++ = tmp[--n];
    }
    *p = 0;
    return p;
}

static char *append_spaces(char *p, UINT32 count) {
    while (count-- > 0) {
        *p++ = ' ';
    }
    *p = 0;
    return p;
}

static char *append_left(char *p, const char *s, UINT32 width) {
    UINT32 n = 0;
    while (*s && n < width) {
        *p++ = *s++;
        n++;
    }
    while (n++ < width) {
        *p++ = ' ';
    }
    *p = 0;
    return p;
}

static char *append_dec64_width(char *p, UINT64 value, UINT32 width) {
    char tmp[21];
    char *end = append_dec64(tmp, value);
    UINT32 n = (UINT32)(end - tmp);
    if (n < width) {
        p = append_spaces(p, width - n);
    }
    return append_str(p, tmp);
}

static char *append_hex64(char *p, UINT64 value) {
    static const char digits[] = "0123456789ABCDEF";
    p = append_str(p, "0X");
    for (int shift = 60; shift >= 0; shift -= 4) {
        *p++ = digits[(value >> shift) & 0xf];
    }
    *p = 0;
    return p;
}

static char *append_hex32(char *p, UINT32 value) {
    static const char digits[] = "0123456789ABCDEF";
    p = append_str(p, "0X");
    for (int shift = 28; shift >= 0; shift -= 4) {
        *p++ = digits[(value >> shift) & 0xf];
    }
    *p = 0;
    return p;
}

static char *append_hex64_width(char *p, UINT64 value, UINT32 width) {
    char tmp[19];
    append_hex64(tmp, value);
    if (width > 18) {
        p = append_spaces(p, width - 18);
    }
    return append_str(p, tmp);
}

static const char *memory_type_name(UINT32 type) {
    switch (type) {
    case EfiReservedMemoryType: return "RSV";
    case EfiLoaderCode: return "LOAD-CODE";
    case EfiLoaderData: return "LOAD-DATA";
    case EfiBootServicesCode: return "BS-CODE";
    case EfiBootServicesData: return "BS-DATA";
    case EfiRuntimeServicesCode: return "RT-CODE";
    case EfiRuntimeServicesData: return "RT-DATA";
    case EfiConventionalMemory: return "CONV";
    case EfiUnusableMemory: return "UNUSABLE";
    case EfiACPIReclaimMemory: return "ACPI-R";
    case EfiACPIMemoryNVS: return "ACPI-NVS";
    case EfiMemoryMappedIO: return "MMIO";
    case EfiMemoryMappedIOPortSpace: return "MMIO-PORT";
    case EfiPalCode: return "PAL";
    case EfiPersistentMemory: return "PERSIST";
    case EfiUnacceptedMemoryType: return "UNACCEPT";
    default: return "UNKNOWN";
    }
}

static char *append_feature(char *p, int enabled, int *first, const char *name) __attribute__((unused));
static char *append_feature(char *p, int enabled, int *first, const char *name) {
    if (!enabled) {
        return p;
    }
    if (!*first) {
        *p++ = ' ';
    }
    *first = 0;
    return append_str(p, name);
}

static void collect_cpu_info(cpu_info_t *cpu) __attribute__((unused));
static void collect_cpu_info(cpu_info_t *cpu) {
    UINT32 a = 0;
    UINT32 b = 0;
    UINT32 c = 0;
    UINT32 d = 0;

    for (UINTN i = 0; i < sizeof(*cpu); i++) {
        ((UINT8 *)cpu)[i] = 0;
    }

    cpuid(0, 0, &a, &b, &c, &d);
    cpu->max_basic_leaf = a;
    put_u32_chars(&cpu->vendor[0], b);
    put_u32_chars(&cpu->vendor[4], d);
    put_u32_chars(&cpu->vendor[8], c);
    cpu->vendor[12] = 0;

    cpuid(0x80000000U, 0, &a, &b, &c, &d);
    cpu->max_extended_leaf = a;

    if (cpu->max_basic_leaf >= 1) {
        cpuid(1, 0, &a, &b, &c, &d);
        UINT32 base_family = (a >> 8) & 0xf;
        UINT32 base_model = (a >> 4) & 0xf;
        UINT32 ext_family = (a >> 20) & 0xff;
        UINT32 ext_model = (a >> 16) & 0xf;
        cpu->stepping = a & 0xf;
        cpu->family = base_family == 0xf ? base_family + ext_family : base_family;
        cpu->model = (base_family == 0x6 || base_family == 0xf) ? base_model + (ext_model << 4) : base_model;
        cpu->logical_processors = (b >> 16) & 0xff;
        cpu->leaf1_ecx = c;
        cpu->leaf1_edx = d;
    }

    if (cpu->max_basic_leaf >= 4) {
        cpuid(4, 0, &a, &b, &c, &d);
        cpu->cores_per_package = ((a >> 26) & 0x3f) + 1;
    }

    if (cpu->max_basic_leaf >= 7) {
        cpuid(7, 0, &a, &b, &c, &d);
        cpu->leaf7_ebx = b;
    }

    if (cpu->max_extended_leaf >= 0x80000001U) {
        cpuid(0x80000001U, 0, &a, &b, &c, &d);
        cpu->ext_edx = d;
    }

    if (cpu->max_extended_leaf >= 0x80000004U) {
        UINT32 *brand_words = (UINT32 *)cpu->brand;
        for (UINT32 leaf = 0; leaf < 3; leaf++) {
            cpuid(0x80000002U + leaf, 0, &brand_words[leaf * 4 + 0], &brand_words[leaf * 4 + 1],
                  &brand_words[leaf * 4 + 2], &brand_words[leaf * 4 + 3]);
        }
        cpu->brand[48] = 0;
        while (cpu->brand[0] == ' ') {
            for (UINTN i = 0; i < 48; i++) {
                cpu->brand[i] = cpu->brand[i + 1];
            }
        }
    } else {
        append_str(cpu->brand, "UNKNOWN");
    }

    if (cpu->max_extended_leaf >= 0x80000008U) {
        cpuid(0x80000008U, 0, &a, &b, &c, &d);
        cpu->physical_address_bits = a & 0xff;
        cpu->virtual_address_bits = (a >> 8) & 0xff;
    }
}

static void make_cpu_line(char *line, const char *label, UINT32 value) __attribute__((unused));
static void make_cpu_line(char *line, const char *label, UINT32 value) {
    char *p = line;
    p = append_str(p, label);
    p = append_dec(p, value);
}

static EFI_MEMORY_DESCRIPTOR *memory_desc_at(efi_memory_map_t *map, UINTN index) {
    return (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map->descriptors + index * map->descriptor_size);
}

static int append_memory_descriptor(efi_memory_map_t *map, EFI_MEMORY_DESCRIPTOR *desc) {
    UINTN used = map->descriptor_count * map->descriptor_size;
    if (used + map->descriptor_size > map->map_capacity) {
        return 0;
    }

    UINT8 *dst = (UINT8 *)map->descriptors + used;
    for (UINTN i = 0; i < map->descriptor_size; i++) {
        dst[i] = 0;
    }

    UINTN copy_size = sizeof(*desc);
    if (copy_size > map->descriptor_size) {
        copy_size = map->descriptor_size;
    }
    for (UINTN i = 0; i < copy_size; i++) {
        dst[i] = ((UINT8 *)desc)[i];
    }

    map->descriptor_count++;
    map->map_size += map->descriptor_size;
    return 1;
}

static UINT64 read_cr3(void) {
    UINT64 value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(value));
    return value;
}

static UINT64 read_cr4(void) {
    UINT64 value;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(value));
    return value;
}

static UINT64 read_cr2(void) {
    UINT64 value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(value));
    return value;
}

static UINT16 read_tr(void) {
    UINT16 value;
    __asm__ __volatile__("str %0" : "=r"(value));
    return value;
}

static void write_cr3(UINT64 value) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(value) : "memory");
}

static UINT64 align_down_4k(UINT64 value) {
    return value & ~(PAGE_SIZE_4K - 1);
}

static int align_up_4k_checked(UINT64 value, UINT64 *out) {
    if (value == 0) {
        *out = 0;
        return 1;
    }
    if (value > (~(UINT64)0) - (PAGE_SIZE_4K - 1)) {
        return 0;
    }
    *out = (value + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    return 1;
}

static UINT64 div_round_up64(UINT64 value, UINT64 divisor) {
    return value == 0 ? 0 : ((value - 1) / divisor) + 1;
}

static int descriptor_end_checked(EFI_MEMORY_DESCRIPTOR *desc, UINT64 *end) {
    if (desc->NumberOfPages > ((~(UINT64)0) - desc->PhysicalStart) / PAGE_SIZE_4K) {
        return 0;
    }
    *end = desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE_4K;
    return 1;
}

static UINT64 framebuffer_size_bytes(framebuffer_t *fb) {
    if (fb->size != 0) {
        return fb->size;
    }
    return (UINT64)fb->stride * (UINT64)fb->height * sizeof(UINT32);
}

static UINT64 estimate_identity_page_table_pages(UINT64 max_end) {
    UINT64 mapped_pages = div_round_up64(max_end, PAGE_SIZE_4K);
    UINT64 pt_pages = div_round_up64(mapped_pages, PAGE_TABLE_ENTRIES);
    UINT64 pd_pages = div_round_up64(mapped_pages, PAGE_TABLE_ENTRIES * PAGE_TABLE_ENTRIES);
    UINT64 pdpt_pages = div_round_up64(mapped_pages, PAGE_TABLE_ENTRIES * PAGE_TABLE_ENTRIES * PAGE_TABLE_ENTRIES);

    return 1 + pdpt_pages + pd_pages + pt_pages + 16;
}

static void zero_page_4k(UINT64 phys) {
    UINT64 *page = (UINT64 *)(UINTN)phys;
    for (UINTN i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        page[i] = 0;
    }
}

static UINT64 alloc_page_table_page(paging_info_t *info) {
    if (info->pool_next > info->pool_end - PAGE_SIZE_4K) {
        info->error = PAGING_ERR_POOL_EMPTY;
        return 0;
    }

    UINT64 phys = info->pool_next;
    info->pool_next += PAGE_SIZE_4K;
    info->allocated_pages++;
    zero_page_4k(phys);
    return phys;
}

static int select_page_table_pool(efi_memory_map_t *map, UINT64 pool_pages, paging_info_t *info) {
    UINT64 selected_start = 0;
    UINT64 selected_end = 0;
    UINTN selected_index = 0;
    int found = 0;

    for (UINTN i = 0; i < map->descriptor_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, i);
        UINT64 end = 0;

        if (desc->Type != EfiConventionalMemory || desc->NumberOfPages < pool_pages) {
            continue;
        }
        if (!descriptor_end_checked(desc, &end)) {
            info->error = PAGING_ERR_RANGE;
            return 0;
        }
        if (!found || desc->PhysicalStart < selected_start) {
            selected_start = desc->PhysicalStart;
            selected_end = end;
            selected_index = i;
            found = 1;
        }
    }

    if (!found) {
        info->error = PAGING_ERR_NO_POOL;
        return 0;
    }

    info->pool_end = selected_end;
    info->pool_start = selected_end - pool_pages * PAGE_SIZE_4K;
    info->pool_next = info->pool_start;
    info->reserved_pages = pool_pages;

    EFI_MEMORY_DESCRIPTOR *selected = memory_desc_at(map, selected_index);
    if (selected->NumberOfPages == pool_pages) {
        selected->Type = EfiLoaderData;
    } else {
        EFI_MEMORY_DESCRIPTOR pool_desc = *selected;
        selected->NumberOfPages -= pool_pages;
        pool_desc.Type = EfiLoaderData;
        pool_desc.PhysicalStart = info->pool_start;
        pool_desc.VirtualStart = 0;
        pool_desc.NumberOfPages = pool_pages;
        if (!append_memory_descriptor(map, &pool_desc)) {
            info->error = PAGING_ERR_MAP_FULL;
            return 0;
        }
    }

    return 1;
}

static int ensure_next_table(paging_info_t *info, UINT64 *table, UINT64 index, UINT64 **next_table) {
    UINT64 entry = table[index];

    if (entry & PTE_PRESENT) {
        *next_table = (UINT64 *)(UINTN)(entry & PTE_ADDR_MASK);
        return 1;
    }

    UINT64 phys = alloc_page_table_page(info);
    if (phys == 0) {
        return 0;
    }

    table[index] = phys | PTE_PRESENT | PTE_RW;
    *next_table = (UINT64 *)(UINTN)phys;
    return 1;
}

static int map_identity_range_4k(paging_info_t *info, UINT64 start, UINT64 bytes, UINT64 flags) {
    if (bytes == 0) {
        return 1;
    }
    if (start > (~(UINT64)0) - bytes) {
        info->error = PAGING_ERR_RANGE;
        return 0;
    }

    UINT64 raw_end = start + bytes;
    UINT64 map_start = align_down_4k(start);
    UINT64 map_end = 0;
    if (!align_up_4k_checked(raw_end, &map_end) || map_end > LOW_CANONICAL_LIMIT) {
        info->error = PAGING_ERR_RANGE;
        return 0;
    }

    info->mapped_ranges++;
    for (UINT64 addr = map_start; addr < map_end; addr += PAGE_SIZE_4K) {
        UINT64 pml4_index = (addr >> 39) & 0x1ff;
        UINT64 pdpt_index = (addr >> 30) & 0x1ff;
        UINT64 pd_index = (addr >> 21) & 0x1ff;
        UINT64 pt_index = (addr >> 12) & 0x1ff;
        UINT64 *pml4 = (UINT64 *)(UINTN)info->cr3;
        UINT64 *pdpt = 0;
        UINT64 *pd = 0;
        UINT64 *pt = 0;

        if (!ensure_next_table(info, pml4, pml4_index, &pdpt) ||
            !ensure_next_table(info, pdpt, pdpt_index, &pd) ||
            !ensure_next_table(info, pd, pd_index, &pt)) {
            return 0;
        }

        UINT64 entry = pt[pt_index];
        if (entry & PTE_PRESENT) {
            if ((entry & PTE_ADDR_MASK) != (addr & PTE_ADDR_MASK)) {
                info->error = PAGING_ERR_TABLE_ENTRY;
                return 0;
            }
            pt[pt_index] = entry | flags;
        } else {
            pt[pt_index] = (addr & PTE_ADDR_MASK) | flags;
            info->mapped_pages++;
        }
    }

    return 1;
}

static UINT64 page_flags_for_memory_type(UINT32 type) {
    UINT64 flags = PTE_PRESENT | PTE_RW;
    if (type == EfiMemoryMappedIO || type == EfiMemoryMappedIOPortSpace) {
        flags |= PTE_PWT | PTE_PCD;
    }
    return flags;
}

static int setup_kernel_paging(efi_memory_map_t *map, framebuffer_t *fb, paging_info_t *info) {
    for (UINTN i = 0; i < sizeof(*info); i++) {
        ((UINT8 *)info)[i] = 0;
    }

    if (read_cr4() & (1ULL << 12)) {
        info->error = PAGING_ERR_LA57;
        return 0;
    }

    UINT64 max_end = 0;
    for (UINTN i = 0; i < map->descriptor_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, i);
        UINT64 end = 0;

        if (!descriptor_end_checked(desc, &end) || end > LOW_CANONICAL_LIMIT) {
            info->error = PAGING_ERR_RANGE;
            return 0;
        }
        if (end > max_end) {
            max_end = end;
        }
    }

    UINT64 fb_start = (UINT64)(UINTN)fb->base;
    UINT64 fb_bytes = framebuffer_size_bytes(fb);
    if (fb_start > (~(UINT64)0) - fb_bytes) {
        info->error = PAGING_ERR_RANGE;
        return 0;
    }
    UINT64 fb_end = fb_start + fb_bytes;
    if (!align_up_4k_checked(fb_end, &fb_end) || fb_end > LOW_CANONICAL_LIMIT) {
        info->error = PAGING_ERR_RANGE;
        return 0;
    }
    if (fb_end > max_end) {
        max_end = fb_end;
    }

    info->max_identity_end = max_end;
    UINT64 pool_pages = estimate_identity_page_table_pages(max_end);
    if (!select_page_table_pool(map, pool_pages, info)) {
        return 0;
    }

    info->cr3 = alloc_page_table_page(info);
    if (info->cr3 == 0) {
        return 0;
    }

    for (UINTN i = 0; i < map->descriptor_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, i);
        UINT64 bytes = desc->NumberOfPages * PAGE_SIZE_4K;
        if (!map_identity_range_4k(info, desc->PhysicalStart, bytes, page_flags_for_memory_type(desc->Type))) {
            return 0;
        }
    }

    if (!map_identity_range_4k(info, fb_start, fb_bytes, PTE_PRESENT | PTE_RW | PTE_PWT | PTE_PCD)) {
        return 0;
    }

    write_cr3(info->cr3);
    info->loaded_cr3 = read_cr3();
    return 1;
}

static const char *paging_error_name(UINT32 error) {
    switch (error) {
    case PAGING_OK: return "OK";
    case PAGING_ERR_LA57: return "LA57";
    case PAGING_ERR_RANGE: return "RANGE";
    case PAGING_ERR_NO_POOL: return "NO-POOL";
    case PAGING_ERR_POOL_EMPTY: return "POOL-EMPTY";
    case PAGING_ERR_TABLE_ENTRY: return "TABLE-ENTRY";
    case PAGING_ERR_MAP_FULL: return "MAP-FULL";
    default: return "UNKNOWN";
    }
}

static const char *fault_vector_name(UINT64 vector) {
    switch (vector) {
    case 6: return "INVALID OPCODE";
    case 8: return "DOUBLE FAULT";
    case 13: return "GENERAL PROTECTION";
    case 14: return "PAGE FAULT";
    default: return "UNKNOWN";
    }
}

static void ap_notify_bsp_request_complete(ap_context_t *ctx) {
    UINT64 base = system_lapic_base;
    UINT32 apic_id = ctx->completion_target_apic_id;
    ap_boot_info_t *boot = &ctx->boot;
    if (base == 0) {
        return;
    }

    __asm__ __volatile__("mfence" : : : "memory");
    enable_lapic_if_needed(base);
    if (lapic_send_ipi(base, apic_id, AP_REQUEST_IPI_VECTOR, boot)) {
        ctx->request_ipi_send_count = ctx->request_ipi_send_count + 1U;
    } else {
        ctx->request_ipi_send_fail_count = ctx->request_ipi_send_fail_count + 1U;
    }
}

static void bsp_notify_ap_request_pending(ap_context_t *ctx) {
    UINT64 base = system_lapic_base;
    ap_boot_info_t *boot = &ctx->boot;
    if (base == 0 || boot->error != AP_BOOT_OK || !boot->online ||
        boot->ap_state == AP_BOOT_STATE_HALTED ||
        boot->ap_state == AP_BOOT_STATE_FAULTED) {
        return;
    }

    __asm__ __volatile__("mfence" : : : "memory");
    enable_lapic_if_needed(base);
    if (lapic_send_ipi(base, boot->target_apic_id, AP_REQUEST_KICK_IPI_VECTOR, boot)) {
        ctx->request_kick_ipi_send_count = ctx->request_kick_ipi_send_count + 1U;
    } else {
        ctx->request_kick_ipi_send_fail_count = ctx->request_kick_ipi_send_fail_count + 1U;
    }
}

void fault_dispatch(fault_frame_t *frame) {
    bsp_fault_display_t *display = &bsp_fault_display;
    framebuffer_t *fb = &display->framebuffer;
    char line[192];
    char *p;
    UINT32 y = 48;

    __asm__ __volatile__("cli" : : : "memory");

    if (!fb->base || fb->width == 0 || fb->height == 0) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }

    clear_screen(fb, display->bg);
    draw_text(fb, 48, y, "BSP FAULT", display->warn, display->bg, 4);
    y += 64;

    p = append_str(line, "VECTOR: ");
    p = append_dec64(p, frame->vector);
    p = append_str(p, "  ");
    p = append_str(p, fault_vector_name(frame->vector));
    p = append_str(p, "  ERROR: ");
    append_hex64(p, frame->error_code);
    draw_line(fb, 48, &y, line, display->fg, display->bg, 2);

    p = append_str(line, "RIP: ");
    p = append_hex64(p, frame->rip);
    p = append_str(p, "  CS: ");
    p = append_hex64(p, frame->cs);
    p = append_str(p, "  RFLAGS: ");
    append_hex64(p, frame->rflags);
    draw_line(fb, 48, &y, line, display->fg, display->bg, 2);

    p = append_str(line, "CR2: ");
    p = append_hex64(p, read_cr2());
    p = append_str(p, "  CR3: ");
    append_hex64(p, read_cr3());
    draw_line(fb, 48, &y, line, display->fg, display->bg, 2);

    cpu_local_t *cpu = &bsp_context.cpu;
    p = append_str(line, "CPU: ");
    p = append_dec(p, cpu ? cpu->id : 0);
    p = append_str(p, "  TR: ");
    p = append_hex64(p, read_tr());
    p = append_str(p, "  LOADED TR: ");
    p = append_hex64(p, cpu ? cpu->loaded_tr : 0);
    p = append_str(p, "  TSS: ");
    append_str(p, cpu && cpu->tss_ready ? "READY" : "NOT-READY");
    draw_line(fb, 48, &y, line, display->fg, display->bg, 2);

    p = append_str(line, "IST1: ");
    p = append_hex64(p, cpu ? cpu->tss.ist[CPU_IST_FAULT - 1] : 0);
    p = append_str(p, "  IST2: ");
    append_hex64(p, cpu ? cpu->tss.ist[CPU_IST_DOUBLE_FAULT - 1] : 0);
    draw_line(fb, 48, &y, line, display->fg, display->bg, 2);

    draw_line(fb, 48, &y, "CURRENT REQUEST: N/A", display->accent, display->bg, 2);

    draw_line(fb, 48, &y, "HALTED", display->warn, display->bg, 2);

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void ap_fault_dispatch(fault_frame_t *frame) {
    __asm__ __volatile__("cli" : : : "memory");

    ap_context_t *ctx = ap_context_from_fault_frame(frame);
    ap_boot_info_t *boot = &ctx->boot;
    UINT32 slot_index = ctx->current_request_slot;
    ap_request_slot_t *slot = slot_index < AP_REQUEST_SLOT_COUNT ?
                              &ctx->request_slots[slot_index] : 0;
    UINT32 request_state = slot ? slot->state : AP_REQUEST_STATUS_EMPTY;
    UINT32 fault_phase = AP_FAULT_PHASE_STARTUP;
    if (slot &&
        (boot->entry_state == AP_ENTRY_STATE_REQUEST ||
         request_state == AP_REQUEST_STATUS_RUNNING)) {
        fault_phase = AP_FAULT_PHASE_REQUEST;
    } else if (boot->entry_state == AP_ENTRY_STATE_LOOP) {
        fault_phase = AP_FAULT_PHASE_LOOP;
    }

    boot->fault_vector = frame->vector;
    boot->fault_error_code = frame->error_code;
    boot->fault_rip = frame->rip;
    boot->fault_cs = frame->cs;
    boot->fault_rflags = frame->rflags;
    boot->fault_cr2 = read_cr2();
    boot->fault_phase = fault_phase;
    boot->fault_request_state = request_state;
    boot->fault_request_opcode = slot ? slot->request.opcode : AP_REQUEST_OP_NONE;
    boot->fault_request_sequence = slot ? slot->request.sequence : 0;
    boot->fault_request_service_id = slot ? slot->request.service_id : 0;
    boot->fault_request_interface_id = slot ? slot->request.interface_id : 0;
    boot->fault_request_id_high = slot ? slot->request.id_high : 0;
    boot->fault_request_id_low = slot ? slot->request.id_low : 0;
    if (slot &&
        (request_state == AP_REQUEST_STATUS_EMPTY ||
         request_state == AP_REQUEST_STATUS_PENDING ||
         request_state == AP_REQUEST_STATUS_RUNNING)) {
        if (request_state == AP_REQUEST_STATUS_PENDING ||
            request_state == AP_REQUEST_STATUS_RUNNING) {
            slot->reply.request_id_high = slot->request.id_high;
            slot->reply.request_id_low = slot->request.id_low;
        }
        slot->reply.fault_code = (UINT32)frame->vector;
    }
    if (slot) {
        ctx->queue_summary.current_slot = slot_index + 1U;
        ctx->queue_summary.last_slot = slot_index + 1U;
        ctx->queue_summary.last_state = AP_REQUEST_STATUS_FAULT;
        ctx->queue_summary.stop_reason = AP_QUEUE_STOP_FAULT;
        __asm__ __volatile__("mfence" : : : "memory");
        if (!cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_PENDING, AP_REQUEST_STATUS_FAULT)) {
            if (!cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_RUNNING, AP_REQUEST_STATUS_FAULT)) {
                if (slot->state == AP_REQUEST_STATUS_EMPTY) {
                    slot->state = AP_REQUEST_STATUS_FAULT;
                }
            }
        }
    }
    __asm__ __volatile__("mfence" : : : "memory");
    boot->entry_state = AP_ENTRY_STATE_FAULT;
    boot->ap_state = AP_BOOT_STATE_FAULTED;
    __asm__ __volatile__("mfence" : : : "memory");
    ap_notify_bsp_request_complete(ctx);
    boot->online = 1;

    for (;;) {
        __asm__ __volatile__("hlt" : : : "memory");
    }
}

static UINT16 read_cs(void) {
    UINT16 cs;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static UINT64 read_rflags(void) {
    UINT64 flags;
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags) : : "memory");
    return flags;
}

static void zero_memory(void *ptr, UINTN size) {
    UINT8 *p = (UINT8 *)ptr;
    for (UINTN i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static void set_tss_descriptor(UINT64 *gdt, UINT32 index, UINT64 base, UINT32 limit) {
    UINT64 low = 0;

    low |= (UINT64)(limit & 0xffff);
    low |= (base & 0x00ffffffULL) << 16;
    low |= 0x89ULL << 40;
    low |= ((UINT64)((limit >> 16) & 0x0f)) << 48;
    low |= ((base >> 24) & 0xffULL) << 56;

    gdt[index] = low;
    gdt[index + 1] = (base >> 32) & 0xffffffffULL;
}

static void init_cpu_local(cpu_local_t *cpu, UINT32 id) {
    zero_memory(cpu, sizeof(*cpu));

    cpu->id = id;
    cpu->gdt[0] = 0x0000000000000000ULL;
    cpu->gdt[1] = 0x00af9a000000ffffULL;
    cpu->gdt[2] = 0x00cf92000000ffffULL;
    cpu->tss.ist[CPU_IST_FAULT - 1] =
        (UINT64)(UINTN)(cpu->fault_stack + sizeof(cpu->fault_stack));
    cpu->tss.ist[CPU_IST_DOUBLE_FAULT - 1] =
        (UINT64)(UINTN)(cpu->double_fault_stack + sizeof(cpu->double_fault_stack));
    cpu->tss.io_map_base = sizeof(cpu->tss);
    set_tss_descriptor(cpu->gdt, KERNEL_TSS_SELECTOR / 8, (UINT64)(UINTN)&cpu->tss,
                       (UINT32)(sizeof(cpu->tss) - 1));
    cpu->tss_ready = 1;
}

static void load_cpu_tables(cpu_local_t *cpu);

static void install_cpu_tables(cpu_local_t *cpu) {
    load_cpu_tables(cpu);
}

static void load_cpu_tables(cpu_local_t *cpu) {
    descriptor_table_ptr_t gdtr;
    UINT16 tss_selector = KERNEL_TSS_SELECTOR;

    gdtr.limit = (UINT16)(sizeof(cpu->gdt) - 1);
    gdtr.base = (UINT64)(UINTN)cpu->gdt;

    __asm__ __volatile__(
        "cli\n"
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        :
        : "m"(gdtr)
        : "rax", "memory");

    __asm__ __volatile__("ltr %0" : : "r"(tss_selector) : "memory");
    cpu->loaded_tr = read_tr();
}

static void store_gdtr(descriptor_table_ptr_t *gdtr) {
    __asm__ __volatile__("sgdt %0" : "=m"(*gdtr) : : "memory");
}

static UINT64 isr_breakpoint_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_breakpoint(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_unhandled_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_unhandled(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_request_ipi_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_request_ipi(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_request_kick_ipi_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_request_kick_ipi(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_idle_timer_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_idle_timer(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_bsp_wait_timer_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_bsp_wait_timer(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_spurious_interrupt_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_spurious_interrupt(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_fault_ud_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_fault_ud(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_fault_df_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_fault_df(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_fault_gp_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_fault_gp(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_fault_pf_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_fault_pf(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_unhandled_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_unhandled(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_fault_ud_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_fault_ud(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_fault_df_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_fault_df(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_fault_gp_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_fault_gp(%%rip), %0" : "=r"(addr));
    return addr;
}

static UINT64 isr_ap_fault_pf_addr(void) {
    UINT64 addr;
    __asm__ __volatile__("leaq isr_ap_fault_pf(%%rip), %0" : "=r"(addr));
    return addr;
}

static void set_idt_gate_in(idt_entry_t *table, UINT32 vector, UINT64 addr, UINT16 selector,
                            UINT8 ist, UINT8 type_attr) {
    table[vector].offset_low = (UINT16)(addr & 0xffff);
    table[vector].selector = selector;
    table[vector].ist = ist & 0x7;
    table[vector].type_attr = type_attr;
    table[vector].offset_mid = (UINT16)((addr >> 16) & 0xffff);
    table[vector].offset_high = (UINT32)(addr >> 32);
    table[vector].zero = 0;
}

static void set_bsp_idt_gate(UINT32 vector, UINT64 addr, UINT16 selector, UINT8 ist,
                             UINT8 type_attr) {
    set_idt_gate_in(bsp_context.idt, vector, addr, selector, ist, type_attr);
}

static void load_idt_entries(idt_entry_t *table) {
    descriptor_table_ptr_t idtr;
    idtr.limit = (UINT16)(sizeof(idt_entry_t) * 256 - 1);
    idtr.base = (UINT64)(UINTN)table;

    __asm__ __volatile__("cli; lidt %0" : : "m"(idtr) : "memory");
}

static void load_bsp_idt_table(void) {
    load_idt_entries(bsp_context.idt);
}

static void store_idtr(descriptor_table_ptr_t *idtr) {
    __asm__ __volatile__("sidt %0" : "=m"(*idtr) : : "memory");
}

static void install_bsp_idt(void) {
    UINT16 cs = read_cs();
    UINT64 unhandled_addr = isr_unhandled_addr();

    for (UINT32 vector = 0; vector < 256; vector++) {
        set_bsp_idt_gate(vector, unhandled_addr, cs, 0, 0x8e);
    }
    set_bsp_idt_gate(3, isr_breakpoint_addr(), cs, 0, 0xef);
    set_bsp_idt_gate(6, isr_fault_ud_addr(), cs, CPU_IST_FAULT, 0x8e);
    set_bsp_idt_gate(8, isr_fault_df_addr(), cs, CPU_IST_DOUBLE_FAULT, 0x8e);
    set_bsp_idt_gate(13, isr_fault_gp_addr(), cs, CPU_IST_FAULT, 0x8e);
    set_bsp_idt_gate(14, isr_fault_pf_addr(), cs, CPU_IST_FAULT, 0x8e);
    set_bsp_idt_gate(AP_REQUEST_KICK_IPI_VECTOR, isr_ap_request_kick_ipi_addr(), cs, 0, 0x8e);
    set_bsp_idt_gate(AP_REQUEST_IPI_VECTOR, isr_ap_request_ipi_addr(), cs, 0, 0x8e);
    set_bsp_idt_gate(BSP_WAIT_TIMER_VECTOR, isr_bsp_wait_timer_addr(), cs, 0, 0x8e);
    set_bsp_idt_gate(LAPIC_SPURIOUS_VECTOR, isr_spurious_interrupt_addr(), cs, 0, 0x8e);

    load_bsp_idt_table();
}

static void init_ap_idt(ap_context_t *ctx) {
    idt_entry_t *table = ctx->idt;
    UINT64 unhandled_addr = isr_ap_unhandled_addr();

    for (UINT32 vector = 0; vector < 256; vector++) {
        set_idt_gate_in(table, vector, unhandled_addr, KERNEL_CODE_SELECTOR, 0, 0x8e);
    }
    set_idt_gate_in(table, 6, isr_ap_fault_ud_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
    set_idt_gate_in(table, 8, isr_ap_fault_df_addr(), KERNEL_CODE_SELECTOR, CPU_IST_DOUBLE_FAULT, 0x8e);
    set_idt_gate_in(table, 13, isr_ap_fault_gp_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
    set_idt_gate_in(table, 14, isr_ap_fault_pf_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
    set_idt_gate_in(table, AP_REQUEST_KICK_IPI_VECTOR, isr_ap_request_kick_ipi_addr(),
                    KERNEL_CODE_SELECTOR, 0, 0x8e);
    set_idt_gate_in(table, AP_IDLE_TIMER_VECTOR, isr_ap_idle_timer_addr(),
                    KERNEL_CODE_SELECTOR, 0, 0x8e);
    set_idt_gate_in(table, LAPIC_SPURIOUS_VECTOR, isr_spurious_interrupt_addr(),
                    KERNEL_CODE_SELECTOR, 0, 0x8e);
}

static void run_ap_fault_test_if_enabled(void) {
#if VIBE_AP_FAULT_TEST == VIBE_AP_FAULT_TEST_UD2
    __asm__ __volatile__("ud2" : : : "memory");
#elif VIBE_AP_FAULT_TEST == VIBE_AP_FAULT_TEST_PF
    volatile UINT64 *unmapped = (volatile UINT64 *)(UINTN)(LOW_CANONICAL_LIMIT - PAGE_SIZE_4K);
    volatile UINT64 value = *unmapped;
    (void)value;
#endif
}

static void reset_ap_request_slot(ap_request_slot_t *slot) {
    slot->state = AP_REQUEST_STATUS_EMPTY;
    slot->request.source_cpu = 0;
    slot->request.target_cpu = 0;
    slot->request.opcode = AP_REQUEST_OP_NONE;
    slot->request.sequence = 0;
    slot->request.service_id = 0;
    slot->request.interface_id = 0;
    slot->request.id_high = 0;
    slot->request.id_low = 0;
    slot->reply.result_code = 0;
    slot->reply.fault_code = 0;
    slot->reply.request_id_high = 0;
    slot->reply.request_id_low = 0;
    slot->reply.result_cs = 0;
    slot->reply.result_tr = 0;
    slot->metrics.handled_count = 0;
    slot->metrics.wait_loops = 0;
}

static void copy_ap_request_slot(ap_request_slot_t *dst, ap_request_slot_t *src) {
    dst->state = src->state;
    dst->request.source_cpu = src->request.source_cpu;
    dst->request.target_cpu = src->request.target_cpu;
    dst->request.opcode = src->request.opcode;
    dst->request.sequence = src->request.sequence;
    dst->request.service_id = src->request.service_id;
    dst->request.interface_id = src->request.interface_id;
    dst->request.id_high = src->request.id_high;
    dst->request.id_low = src->request.id_low;
    dst->reply.result_code = src->reply.result_code;
    dst->reply.fault_code = src->reply.fault_code;
    dst->reply.request_id_high = src->reply.request_id_high;
    dst->reply.request_id_low = src->reply.request_id_low;
    dst->reply.result_cs = src->reply.result_cs;
    dst->reply.result_tr = src->reply.result_tr;
    dst->metrics.handled_count = src->metrics.handled_count;
    dst->metrics.wait_loops = src->metrics.wait_loops;
}

static void reset_ap_request_history(ap_context_t *ctx) {
    for (UINTN i = 0; i < AP_REQUEST_HISTORY_COUNT; i++) {
        reset_ap_request_slot(&ctx->request_history[i].request);
        ctx->request_history[i].counter_value = 0;
        ctx->request_history[i].slot = 0;
        ctx->request_history[i].valid = 0;
    }
}

static int ap_request_is_counter_increment(ap_request_slot_t *slot) {
    return slot->request.service_id == AP_REQUEST_SERVICE_COUNTER &&
           slot->request.interface_id == AP_REQUEST_INTERFACE_COUNTER_INCREMENT &&
           slot->request.opcode == AP_REQUEST_OP_COUNTER;
}

static void record_ap_request_history(ap_context_t *ctx, UINTN index, UINTN slot_index,
                                      ap_request_slot_t *slot, UINT32 counter_value) {
    if (index >= AP_REQUEST_HISTORY_COUNT) {
        return;
    }

    __asm__ __volatile__("mfence" : : : "memory");
    copy_ap_request_slot(&ctx->request_history[index].request, slot);
    ctx->request_history[index].counter_value = counter_value;
    ctx->request_history[index].slot = (UINT32)(slot_index + 1U);
    ctx->request_history[index].valid = 1;
    __asm__ __volatile__("mfence" : : : "memory");
}

static int cmpxchg_u32(volatile UINT32 *ptr, UINT32 expected, UINT32 desired) {
    UINT8 success;
    __asm__ __volatile__(
        "lock cmpxchgl %3, %1\n"
        "sete %0"
        : "=q"(success), "+m"(*ptr), "+a"(expected)
        : "r"(desired)
        : "cc", "memory");
    return success ? 1 : 0;
}

static int ap_request_state_terminal(UINT32 state) {
    return state == AP_REQUEST_STATUS_DONE ||
           state == AP_REQUEST_STATUS_BAD_OP ||
           state == AP_REQUEST_STATUS_FAULT;
}

static int ap_request_state_finished(UINT32 state) {
    return ap_request_state_terminal(state) ||
           state == AP_REQUEST_STATUS_TIMEOUT ||
           state == AP_REQUEST_STATUS_SKIPPED;
}

static void reset_ap_queue_summary(ap_context_t *ctx) {
    ctx->queue_summary.current_slot = 0;
    ctx->queue_summary.handled_count = 0;
    ctx->queue_summary.last_slot = 0;
    ctx->queue_summary.last_state = AP_REQUEST_STATUS_EMPTY;
    ctx->queue_summary.stop_reason = AP_QUEUE_STOP_NONE;
    ctx->queue_summary.planned_count = AP_REQUEST_SLOT_COUNT;
}

static int ap_queue_all_other_slots_done(ap_context_t *ctx, UINTN current_slot) {
    for (UINTN i = 0; i < AP_REQUEST_SLOT_COUNT; i++) {
        if (i == current_slot) {
            continue;
        }
        if (ctx->request_slots[i].state != AP_REQUEST_STATUS_DONE) {
            return 0;
        }
    }
    return 1;
}

static void ap_queue_note_slot_start(ap_context_t *ctx, UINTN slot_index) {
    ctx->queue_summary.current_slot = (UINT32)(slot_index + 1);
    ctx->queue_summary.stop_reason = AP_QUEUE_STOP_NONE;
}

static void ap_queue_note_slot_terminal(ap_context_t *ctx, UINTN slot_index, UINT32 state,
                                        UINT32 stop_reason) {
    ctx->queue_summary.current_slot = 0;
    ctx->queue_summary.handled_count = ctx->queue_summary.handled_count + 1U;
    ctx->queue_summary.last_slot = (UINT32)(slot_index + 1);
    ctx->queue_summary.last_state = state;
    if (stop_reason == AP_QUEUE_STOP_NONE && ap_queue_all_other_slots_done(ctx, slot_index)) {
        stop_reason = AP_QUEUE_STOP_DRAINED;
    }
    ctx->queue_summary.stop_reason = stop_reason;
    __asm__ __volatile__("mfence" : : : "memory");
}

static void begin_ap_reply(ap_request_slot_t *slot) {
    slot->reply.request_id_high = slot->request.id_high;
    slot->reply.request_id_low = slot->request.id_low;
    slot->reply.result_code = 0;
    slot->reply.fault_code = 0;
}

static void complete_ap_request(ap_request_slot_t *slot, UINT32 state) {
    __asm__ __volatile__("mfence" : : : "memory");
    (void)cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_RUNNING, state);
    __asm__ __volatile__("mfence" : : : "memory");
}

static void ap_disarm_idle_timer(void) {
    UINT64 base = system_lapic_base;
    if (base == 0) {
        return;
    }

    lapic_write32(base, 0x380, 0);
    lapic_write32(base, 0x320, AP_IDLE_TIMER_VECTOR | LAPIC_LVT_MASKED);
}

static int ap_arm_idle_timer(void) {
    UINT64 base = system_lapic_base;
    if (base == 0) {
        return 0;
    }

    lapic_write32(base, 0x3e0, 0x3);
    lapic_write32(base, 0x320, AP_IDLE_TIMER_VECTOR);
    lapic_write32(base, 0x380, AP_IDLE_TIMER_INITIAL_COUNT);
    return 1;
}

static void bsp_disarm_wait_timer(void) {
    UINT64 base = system_lapic_base;
    if (base == 0) {
        return;
    }

    lapic_write32(base, 0x380, 0);
    lapic_write32(base, 0x320, BSP_WAIT_TIMER_VECTOR | LAPIC_LVT_MASKED);
}

static int bsp_arm_wait_timer(void) {
    UINT64 base = system_lapic_base;
    if (base == 0) {
        return 0;
    }

    enable_lapic_if_needed(base);
    lapic_write32(base, 0x3e0, 0x3);
    lapic_write32(base, 0x320, BSP_WAIT_TIMER_VECTOR);
    lapic_write32(base, 0x380, BSP_WAIT_TIMER_INITIAL_COUNT);
    return 1;
}

static UINT32 bsp_wait_for_ap_request_event(void) {
    UINT32 timer_count_before = bsp_wait_observe.timer_count;

    if (!bsp_arm_wait_timer()) {
        __asm__ __volatile__("sti; nop; pause; cli" : : : "memory");
        return 1;
    }

    bsp_wait_observe.halt_count = bsp_wait_observe.halt_count + 1U;
    __asm__ __volatile__("sti; hlt; cli" : : : "memory");
    bsp_disarm_wait_timer();
    bsp_wait_observe.wake_count = bsp_wait_observe.wake_count + 1U;
    if (bsp_wait_observe.timer_count != timer_count_before) {
        return BSP_WAIT_TIMER_TIMEOUT_STEP;
    }
    return 1;
}

static void ap_request_loop(ap_context_t *ctx) __attribute__((noreturn));
static void ap_request_loop(ap_context_t *ctx) {
    ap_boot_info_t *boot = &ctx->boot;
    ap_service_context_t service_context = {
        &ctx->request_handled_count,
        &ctx->counter_value,
    };

    boot->ap_state = AP_BOOT_STATE_ONLINE;
    boot->entry_state = AP_ENTRY_STATE_LOOP;
    __asm__ __volatile__("mfence" : : : "memory");
    boot->online = 1;

    for (;;) {
        int handled_work = 0;
        __asm__ __volatile__("cli" : : : "memory");
        for (UINTN i = 0; i < AP_REQUEST_SLOT_COUNT; i++) {
            ap_request_slot_t *slot = &ctx->request_slots[i];
            if (slot->state != AP_REQUEST_STATUS_PENDING) {
                continue;
            }
            if (!cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_PENDING,
                             AP_REQUEST_STATUS_RUNNING)) {
                continue;
            }
            handled_work = 1;

            ctx->current_request_slot = (UINT32)i;
            ap_queue_note_slot_start(ctx, i);
            slot->metrics.handled_count = ctx->request_handled_count;
            begin_ap_reply(slot);
            boot->entry_state = AP_ENTRY_STATE_REQUEST;
            __asm__ __volatile__("mfence" : : : "memory");

            ap_request_handler_t handler = find_ap_request_handler(slot->request.service_id,
                                                                   slot->request.interface_id);
            if (handler) {
                handler(&service_context, slot);
                boot->ap_state = AP_BOOT_STATE_ONLINE;
                boot->entry_state = AP_ENTRY_STATE_LOOP;
                ap_queue_note_slot_terminal(ctx, i, AP_REQUEST_STATUS_DONE, AP_QUEUE_STOP_NONE);
                ctx->current_request_slot = AP_REQUEST_NO_SLOT;
                complete_ap_request(slot, AP_REQUEST_STATUS_DONE);
                ap_notify_bsp_request_complete(ctx);
            } else {
                slot->reply.result_code = ap_dispatch_miss_result_code(slot->request.service_id,
                                                                       slot->request.interface_id);
                slot->reply.fault_code = 0;
                boot->ap_state = AP_BOOT_STATE_HALTED;
                boot->entry_state = AP_ENTRY_STATE_HALTED;
                ap_queue_note_slot_terminal(ctx, i, AP_REQUEST_STATUS_BAD_OP, AP_QUEUE_STOP_BAD_OP);
                ctx->current_request_slot = AP_REQUEST_NO_SLOT;
                complete_ap_request(slot, AP_REQUEST_STATUS_BAD_OP);
                ap_notify_bsp_request_complete(ctx);
                ap_disarm_idle_timer();
                for (;;) {
                    __asm__ __volatile__("cli; hlt" : : : "memory");
                }
            }
        }
        if (handled_work) {
            continue;
        }
        if (!ap_context_is_request_target(ctx) || !ap_arm_idle_timer()) {
            __asm__ __volatile__("sti; nop; pause; cli" : : : "memory");
            continue;
        }
        ctx->idle_halt_count = ctx->idle_halt_count + 1U;
        __asm__ __volatile__("sti; hlt; cli" : : : "memory");
        ap_disarm_idle_timer();
        ctx->idle_wake_count = ctx->idle_wake_count + 1U;
    }
}

static void prepare_ap_request(ap_request_slot_t *slot, UINT32 target_cpu, UINT32 opcode,
                               UINT64 service_id, UINT64 interface_id, UINT32 sequence) {
    slot->metrics.handled_count = 0;
    slot->request.source_cpu = 0;
    slot->request.target_cpu = target_cpu;
    slot->request.opcode = opcode;
    slot->request.sequence = sequence;
    slot->request.service_id = service_id;
    slot->request.interface_id = interface_id;
    slot->request.id_high = 0x41502d50494e4721ULL;
    slot->request.id_low = sequence;
}

static void publish_ap_request(ap_context_t *ctx, ap_request_slot_t *slot, UINT32 opcode,
                               UINT64 service_id, UINT64 interface_id, UINT32 sequence) {
    ap_boot_info_t *boot = &ctx->boot;
    reset_ap_request_slot(slot);
    prepare_ap_request(slot, ap_context_index(ctx) + 1U, opcode, service_id,
                       interface_id, sequence);
    if (boot->ap_state == AP_BOOT_STATE_FAULTED) {
        slot->reply.fault_code = (UINT32)boot->fault_vector;
        slot->state = AP_REQUEST_STATUS_FAULT;
        return;
    }
    if (boot->error != AP_BOOT_OK || !boot->online) {
        slot->state = AP_REQUEST_STATUS_SKIPPED;
        return;
    }

    if (boot->ap_state == AP_BOOT_STATE_HALTED) {
        slot->state = AP_REQUEST_STATUS_SKIPPED;
        return;
    }

    __asm__ __volatile__("mfence" : : : "memory");
    slot->state = AP_REQUEST_STATUS_PENDING;
    __asm__ __volatile__("mfence" : : : "memory");
    bsp_notify_ap_request_pending(ctx);
}

static int ap_request_all_done(ap_request_slot_t *slots, UINTN count) {
    if (count > AP_REQUEST_SLOT_COUNT) {
        count = AP_REQUEST_SLOT_COUNT;
    }
    for (UINTN i = 0; i < count; i++) {
        if (slots[i].state != AP_REQUEST_STATUS_DONE) {
            return 0;
        }
    }
    return 1;
}

static void drain_ap_request_ipis(void) {
    for (UINT32 i = 0; i < AP_REQUEST_IPI_DRAIN_LOOPS; i++) {
        __asm__ __volatile__("pause" : : : "memory");
    }
}

static UINTN ap_request_slot_limit(UINTN count) {
    if (count > AP_REQUEST_SLOT_COUNT) {
        return AP_REQUEST_SLOT_COUNT;
    }
    return count;
}

static void record_ap_stream_slot(ap_context_t *ctx, UINTN history_index, UINTN slot_index,
                                  ap_request_slot_t *slot, UINT32 *counter_value) {
    if (ap_request_is_counter_increment(slot) &&
        slot->state == AP_REQUEST_STATUS_DONE &&
        slot->reply.fault_code == 0) {
        *counter_value = *counter_value + 1U;
    }
    record_ap_request_history(ctx, history_index, slot_index, slot, *counter_value);
}

static void publish_ap_stream_slot(ap_context_t *ctx, ap_request_slot_t *slots, UINT8 *active,
                                   UINTN slot_index,
                                   const ap_request_plan_t *plan) {
    publish_ap_request(ctx, &slots[slot_index], plan->opcode, plan->service_id,
                       plan->interface_id, plan->sequence);
    active[slot_index] = 1;
}

static void stop_ap_request_stream(ap_context_t *ctx, ap_request_slot_t *slots, UINTN count,
                                   UINT8 *active, UINT32 wait_loops) {
    ap_boot_info_t *boot = &ctx->boot;
    for (UINTN slot_index = 0; slot_index < count; slot_index++) {
        if (!active[slot_index]) {
            continue;
        }

        ap_request_slot_t *slot = &slots[slot_index];
        UINT32 state = slot->state;
        if (ap_request_state_finished(state)) {
            continue;
        }

        slot->metrics.wait_loops = wait_loops;
        if (boot->ap_state == AP_BOOT_STATE_FAULTED && state == AP_REQUEST_STATUS_RUNNING) {
            slot->reply.fault_code = (UINT32)boot->fault_vector;
            slot->state = AP_REQUEST_STATUS_FAULT;
        } else if (state == AP_REQUEST_STATUS_PENDING) {
            slot->state = AP_REQUEST_STATUS_SKIPPED;
        }
    }
}

static UINT32 active_ap_request_slot_count(UINT8 *active, UINTN count) {
    UINT32 active_count = 0;
    for (UINTN slot_index = 0; slot_index < count; slot_index++) {
        if (active[slot_index]) {
            active_count++;
        }
    }
    return active_count;
}

static int ap_request_stream_has_finished_slot(ap_request_slot_t *slots, UINT8 *active,
                                               UINTN count) {
    for (UINTN slot_index = 0; slot_index < count; slot_index++) {
        if (active[slot_index] && ap_request_state_finished(slots[slot_index].state)) {
            return 1;
        }
    }
    return 0;
}

static int ap_request_stream_should_stop(ap_context_t *ctx) {
    ap_boot_info_t *boot = &ctx->boot;
    return boot->ap_state == AP_BOOT_STATE_FAULTED ||
           boot->ap_state == AP_BOOT_STATE_HALTED;
}

static int finish_ap_stream_slot(ap_context_t *ctx, ap_request_slot_t *slots, UINT8 *active,
                                 UINTN slot_index, UINT32 wait_loops, UINT32 *counter_value,
                                 UINTN *completed_count) {
    ap_request_slot_t *slot = &slots[slot_index];
    UINT32 state = slot->state;
    if (!active[slot_index] || !ap_request_state_finished(state)) {
        return 0;
    }

    slot->metrics.wait_loops = wait_loops;
    record_ap_stream_slot(ctx, *completed_count, slot_index, slot, counter_value);
    active[slot_index] = 0;
    *completed_count = *completed_count + 1U;
    return state == AP_REQUEST_STATUS_DONE;
}

static UINT32 advance_ap_request_wait_loops(UINT32 wait_loops, UINT32 step) {
    if (step >= AP_REQUEST_TIMEOUT_LOOPS - wait_loops) {
        return AP_REQUEST_TIMEOUT_LOOPS;
    }
    return wait_loops + step;
}

static void finish_timed_out_ap_stream_slots(ap_context_t *ctx, ap_request_slot_t *slots, UINTN count,
                                             UINT8 *active, UINT32 *counter_value,
                                             UINTN *completed_count) {
    for (UINTN slot_index = 0; slot_index < count; slot_index++) {
        if (!active[slot_index]) {
            continue;
        }

        ap_request_slot_t *slot = &slots[slot_index];
        if (!ap_request_state_finished(slot->state)) {
            slot->metrics.wait_loops = AP_REQUEST_TIMEOUT_LOOPS;
            if (!cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_PENDING, AP_REQUEST_STATUS_TIMEOUT)) {
                (void)cmpxchg_u32(&slot->state, AP_REQUEST_STATUS_RUNNING, AP_REQUEST_STATUS_TIMEOUT);
            }
        }
        (void)finish_ap_stream_slot(ctx, slots, active, slot_index, AP_REQUEST_TIMEOUT_LOOPS,
                                    counter_value, completed_count);
    }
}

static int run_ap_request_stream(ap_context_t *ctx, const ap_request_plan_t *plan,
                                 UINTN plan_count) {
    UINT8 active[AP_REQUEST_SLOT_COUNT];
    ap_request_slot_t *slots = ctx->request_slots;
    UINTN count = ap_request_slot_limit(AP_REQUEST_SLOT_COUNT);
    UINTN next_plan = 0;
    UINTN completed_count = 0;
    UINT32 counter_base = ctx->counter_value;
    UINT64 rflags = read_rflags();
    int stopped = 0;

    if (count == 0) {
        return plan_count == 0;
    }

    reset_ap_queue_summary(ctx);
    reset_ap_request_history(ctx);
    ctx->queue_summary.planned_count = (UINT32)plan_count;
    for (UINTN i = 0; i < AP_REQUEST_SLOT_COUNT; i++) {
        active[i] = 0;
        reset_ap_request_slot(&slots[i]);
    }

    for (UINTN slot_index = 0; slot_index < count && next_plan < plan_count; slot_index++) {
        publish_ap_stream_slot(ctx, slots, active, slot_index, &plan[next_plan]);
        next_plan++;
    }

    __asm__ __volatile__("cli" : : : "memory");
    UINT32 wait_loops = 0;
    for (;;) {
        if (ap_request_stream_should_stop(ctx)) {
            stopped = 1;
            stop_ap_request_stream(ctx, slots, count, active, wait_loops);
        }

        for (UINTN slot_index = 0; slot_index < count; slot_index++) {
            if (!active[slot_index]) {
                continue;
            }
            int done = finish_ap_stream_slot(ctx, slots, active, slot_index, wait_loops,
                                             &counter_base, &completed_count);
            if (!active[slot_index] && !done) {
                stopped = 1;
            }
            if (done && !stopped && wait_loops < AP_REQUEST_TIMEOUT_LOOPS &&
                next_plan < plan_count) {
                if (ap_request_stream_should_stop(ctx)) {
                    stopped = 1;
                } else {
                    publish_ap_stream_slot(ctx, slots, active, slot_index, &plan[next_plan]);
                    next_plan++;
                }
            }
        }

        if (stopped) {
            stop_ap_request_stream(ctx, slots, count, active, wait_loops);
        }
        UINT32 active_count = active_ap_request_slot_count(active, count);
        if ((next_plan >= plan_count || stopped) && active_count == 0) {
            break;
        }
        if (stopped) {
            if (ap_request_stream_has_finished_slot(slots, active, count)) {
                continue;
            }
            if (wait_loops >= AP_REQUEST_TIMEOUT_LOOPS) {
                break;
            }
            wait_loops = advance_ap_request_wait_loops(wait_loops, 1);
            __asm__ __volatile__("pause" : : : "memory");
            continue;
        }
        if (wait_loops >= AP_REQUEST_TIMEOUT_LOOPS) {
            break;
        }

        wait_loops = advance_ap_request_wait_loops(wait_loops,
                                                   bsp_wait_for_ap_request_event());
    }

    bsp_disarm_wait_timer();
    __asm__ __volatile__("sti" : : : "memory");
    drain_ap_request_ipis();
    if ((rflags & (1ULL << 9)) == 0) {
        __asm__ __volatile__("cli" : : : "memory");
    }

    finish_timed_out_ap_stream_slots(ctx, slots, count, active, &counter_base, &completed_count);
    return completed_count == plan_count && ap_request_all_done(slots, count);
}

static void init_ap_context(ap_context_t *ctx, ap_boot_info_t *trampoline_boot,
                            UINT32 completion_target_apic_id) {
    zero_memory(ctx, sizeof(*ctx));
    ctx->boot.trampoline_base = trampoline_boot->trampoline_base;
    ctx->boot.sipi_vector = trampoline_boot->sipi_vector;
    ctx->boot.error = trampoline_boot->error;
    for (UINTN i = 0; i < AP_REQUEST_SLOT_COUNT; i++) {
        reset_ap_request_slot(&ctx->request_slots[i]);
    }
    reset_ap_request_history(ctx);
    reset_ap_queue_summary(ctx);
    ctx->current_request_slot = AP_REQUEST_NO_SLOT;
    ctx->completion_target_apic_id = completion_target_apic_id;
}

static void init_ap_contexts(ap_boot_info_t *trampoline_boot, UINT32 completion_target_apic_id) {
    for (UINTN i = 0; i < MAX_AP_CONTEXTS; i++) {
        init_ap_context(&ap_contexts[i], trampoline_boot, completion_target_apic_id);
    }
}

static void ap_entry_one(ap_context_t *ctx) __attribute__((noreturn));
static void ap_entry_one(ap_context_t *ctx) {
    descriptor_table_ptr_t gdtr;
    descriptor_table_ptr_t idtr;
    ap_boot_info_t *boot = &ctx->boot;
    cpu_local_t *cpu = &ctx->cpu;

    __asm__ __volatile__("cli" : : : "memory");
    boot->entry_state = AP_ENTRY_STATE_C;

    init_cpu_local(cpu, ap_context_index(ctx) + 1U);
    load_cpu_tables(cpu);
    init_ap_idt(ctx);
    load_idt_entries(ctx->idt);
    if (system_lapic_base != 0) {
        enable_lapic_if_needed(system_lapic_base);
        ap_disarm_idle_timer();
    }

    store_gdtr(&gdtr);
    store_idtr(&idtr);

    boot->ap_cs = read_cs();
    boot->ap_tr = read_tr();
    boot->ap_ist1 = cpu->tss.ist[CPU_IST_FAULT - 1];
    boot->ap_ist2 = cpu->tss.ist[CPU_IST_DOUBLE_FAULT - 1];
    boot->gdt_ok = (gdtr.base == (UINT64)(UINTN)cpu->gdt &&
                    gdtr.limit == (UINT16)(sizeof(cpu->gdt) - 1)) ? 1U : 0U;
    boot->tss_ok = (boot->ap_tr == KERNEL_TSS_SELECTOR && cpu->tss_ready &&
                    boot->ap_ist1 != 0 && boot->ap_ist2 != 0) ? 1U : 0U;
    boot->idt_ok = (idtr.base == (UINT64)(UINTN)ctx->idt &&
                    idtr.limit == (UINT16)(sizeof(ctx->idt) - 1)) ? 1U : 0U;
    boot->entry_state = AP_ENTRY_STATE_TABLES;
    run_ap_fault_test_if_enabled();
    ap_request_loop(ctx);
}

static int run_idt_self_test(void) {
    interrupt_trace_t *trace = &bsp_interrupt_observe.idt_self_test;
    trace->vector = 0;
    trace->count = 0;
    trace->rip = 0;
    trace->cs = 0;
    trace->rflags = 0;

    __asm__ __volatile__("int3" : : : "memory");

    return trace->vector == 3 && trace->count == 1;
}

static void run_fault_test_if_enabled(void) {
#if VIBE_FAULT_TEST == VIBE_FAULT_TEST_UD2
    __asm__ __volatile__("ud2" : : : "memory");
#elif VIBE_FAULT_TEST == VIBE_FAULT_TEST_PF
    volatile UINT64 *unmapped = (volatile UINT64 *)(UINTN)(LOW_CANONICAL_LIMIT - PAGE_SIZE_4K);
    volatile UINT64 value = *unmapped;
    (void)value;
#endif
}

static int guid_equal(EFI_GUID *a, EFI_GUID *b) {
    if (a->Data1 != b->Data1 || a->Data2 != b->Data2 || a->Data3 != b->Data3) {
        return 0;
    }
    for (UINTN i = 0; i < sizeof(a->Data4); i++) {
        if (a->Data4[i] != b->Data4[i]) {
            return 0;
        }
    }
    return 1;
}

static UINT64 find_acpi_rsdp(EFI_SYSTEM_TABLE *st) {
    static EFI_GUID acpi_20_guid = {
        0x8868e871,
        0xe4f1,
        0x11d3,
        {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81},
    };
    static EFI_GUID acpi_10_guid = {
        0xeb9d2d30,
        0x2d88,
        0x11d3,
        {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d},
    };
    UINT64 fallback = 0;

    if (!st->ConfigurationTable || st->NumberOfTableEntries == 0) {
        return 0;
    }

    efi_configuration_table_t *tables = (efi_configuration_table_t *)st->ConfigurationTable;
    for (UINTN i = 0; i < st->NumberOfTableEntries; i++) {
        if (guid_equal(&tables[i].VendorGuid, &acpi_20_guid)) {
            return (UINT64)(UINTN)tables[i].VendorTable;
        }
        if (!fallback && guid_equal(&tables[i].VendorGuid, &acpi_10_guid)) {
            fallback = (UINT64)(UINTN)tables[i].VendorTable;
        }
    }

    return fallback;
}

static int bytes_equal(const char *a, const char *b, UINTN count) {
    for (UINTN i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static UINT32 read_le32(UINT8 *p) {
    return ((UINT32)p[0]) | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

static UINT64 read_le64(UINT8 *p) {
    return ((UINT64)read_le32(p)) | ((UINT64)read_le32(p + 4) << 32);
}

static int checksum_ok(UINT8 *p, UINT32 length) {
    UINT8 sum = 0;
    for (UINT32 i = 0; i < length; i++) {
        sum = (UINT8)(sum + p[i]);
    }
    return sum == 0;
}

static int memory_map_contains_range(efi_memory_map_t *map, UINT64 start, UINT64 length) {
    if (length == 0) {
        return 1;
    }
    if (start > (~(UINT64)0) - length) {
        return 0;
    }

    UINT64 end = start + length;
    for (UINTN i = 0; i < map->descriptor_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, i);
        UINT64 desc_end = 0;
        if (!descriptor_end_checked(desc, &desc_end)) {
            continue;
        }
        if (start >= desc->PhysicalStart && end <= desc_end) {
            return 1;
        }
    }

    return 0;
}

static UINT32 read_bsp_apic_id(void) {
    UINT32 a = 0;
    UINT32 b = 0;
    UINT32 c = 0;
    UINT32 d = 0;

    cpuid(0, 0, &a, &b, &c, &d);
    if (a >= 0x0bU) {
        cpuid(0x0bU, 0, &a, &b, &c, &d);
        if (b != 0) {
            return d;
        }
    }

    cpuid(1, 0, &a, &b, &c, &d);
    return (b >> 24) & 0xffU;
}

static void add_acpi_cpu(acpi_info_t *info, UINT32 uid, UINT32 apic_id, UINT32 flags, UINT8 x2apic) {
    info->enabled_cpu_count++;
    if (info->stored_cpu_count >= ACPI_MAX_CPUS) {
        return;
    }

    acpi_cpu_t *cpu = &info->cpus[info->stored_cpu_count++];
    cpu->acpi_uid = uid;
    cpu->apic_id = apic_id;
    cpu->flags = flags;
    cpu->x2apic = x2apic;
}

static void parse_madt(efi_memory_map_t *map, acpi_info_t *info, acpi_sdt_header_t *madt_header) {
    UINT8 *madt = (UINT8 *)madt_header;
    UINT32 offset = sizeof(acpi_sdt_header_t) + 8;
    UINT64 madt_addr = (UINT64)(UINTN)madt_header;

    info->madt = madt_addr;
    if (madt_header->length < offset || madt_header->length > ACPI_MAX_SDT_LENGTH) {
        info->error = ACPI_ERR_BAD_MADT;
        return;
    }
    if (!memory_map_contains_range(map, madt_addr, madt_header->length)) {
        info->error = ACPI_ERR_RANGE;
        return;
    }
    if (!checksum_ok(madt, madt_header->length)) {
        info->error = ACPI_ERR_BAD_MADT;
        return;
    }

    info->local_apic_base = read_le32(madt + sizeof(acpi_sdt_header_t));
    info->madt_flags = read_le32(madt + sizeof(acpi_sdt_header_t) + 4);

    while (offset < madt_header->length) {
        UINT8 *entry = madt + offset;
        UINT8 type = entry[0];
        UINT8 length = entry[1];

        if (length < 2 || offset + length > madt_header->length) {
            info->error = ACPI_ERR_BAD_ENTRY;
            return;
        }

        if (type == 0 && length >= 8) {
            UINT32 flags = read_le32(entry + 4);
            if (flags & 1U) {
                add_acpi_cpu(info, entry[2], entry[3], flags, 0);
            }
        } else if (type == 5 && length >= 12) {
            info->local_apic_base = read_le64(entry + 4);
        } else if (type == 9 && length >= 16) {
            UINT32 flags = read_le32(entry + 8);
            if (flags & 1U) {
                add_acpi_cpu(info, read_le32(entry + 12), read_le32(entry + 4), flags, 1);
            }
        }

        offset += length;
    }
}

static void parse_acpi(UINT64 rsdp_addr, efi_memory_map_t *map, acpi_info_t *info) {
    zero_memory(info, sizeof(*info));
    info->bsp_apic_id = read_bsp_apic_id();

    if (rsdp_addr == 0) {
        info->error = ACPI_ERR_NO_RSDP;
        return;
    }
    info->rsdp = rsdp_addr;
    if (!memory_map_contains_range(map, rsdp_addr, 20)) {
        info->error = ACPI_ERR_RANGE;
        return;
    }

    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)(UINTN)rsdp_addr;
    info->rsdp_revision = rsdp->revision;

    if (!bytes_equal(rsdp->signature, "RSD PTR ", 8) || !checksum_ok((UINT8 *)rsdp, 20)) {
        info->error = ACPI_ERR_BAD_RSDP;
        return;
    }
    if (rsdp->revision < 2) {
        info->error = ACPI_ERR_NO_XSDT;
        return;
    }
    if (!memory_map_contains_range(map, rsdp_addr, 24)) {
        info->error = ACPI_ERR_RANGE;
        return;
    }
    UINT32 rsdp_length = read_le32((UINT8 *)rsdp + 20);
    if (rsdp_length < sizeof(acpi_rsdp_t) || rsdp_length > ACPI_MAX_RSDP_LENGTH) {
        info->error = ACPI_ERR_BAD_RSDP;
        return;
    }
    if (!memory_map_contains_range(map, rsdp_addr, rsdp_length)) {
        info->error = ACPI_ERR_RANGE;
        return;
    }
    if (!checksum_ok((UINT8 *)rsdp, rsdp_length)) {
        info->error = ACPI_ERR_BAD_RSDP;
        return;
    }
    if (rsdp->xsdt_address == 0) {
        info->error = ACPI_ERR_NO_XSDT;
        return;
    }
    if (!memory_map_contains_range(map, rsdp->xsdt_address, sizeof(acpi_sdt_header_t))) {
        info->error = ACPI_ERR_RANGE;
        return;
    }

    acpi_sdt_header_t *xsdt = (acpi_sdt_header_t *)(UINTN)rsdp->xsdt_address;
    info->xsdt = rsdp->xsdt_address;
    if (!bytes_equal(xsdt->signature, "XSDT", 4) || xsdt->length < sizeof(*xsdt) ||
        xsdt->length > ACPI_MAX_SDT_LENGTH) {
        info->error = ACPI_ERR_BAD_XSDT;
        return;
    }
    if (!memory_map_contains_range(map, rsdp->xsdt_address, xsdt->length)) {
        info->error = ACPI_ERR_RANGE;
        return;
    }
    if (!checksum_ok((UINT8 *)xsdt, xsdt->length)) {
        info->error = ACPI_ERR_BAD_XSDT;
        return;
    }

    UINT32 entry_count = (xsdt->length - sizeof(*xsdt)) / sizeof(UINT64);
    info->xsdt_entries = entry_count;
    UINT8 *entries = (UINT8 *)xsdt + sizeof(*xsdt);
    for (UINT32 i = 0; i < entry_count; i++) {
        UINT64 table_addr = read_le64(entries + i * sizeof(UINT64));
        if (table_addr == 0) {
            info->error = ACPI_ERR_RANGE;
            return;
        }
        if (!memory_map_contains_range(map, table_addr, sizeof(acpi_sdt_header_t))) {
            info->error = ACPI_ERR_RANGE;
            return;
        }
        acpi_sdt_header_t *header = (acpi_sdt_header_t *)(UINTN)table_addr;
        if (bytes_equal(header->signature, "APIC", 4)) {
            parse_madt(map, info, header);
            return;
        }
    }

    info->error = ACPI_ERR_NO_MADT;
}

static const char *acpi_error_name(UINT32 error) {
    switch (error) {
    case ACPI_OK: return "OK";
    case ACPI_ERR_NO_RSDP: return "NO-RSDP";
    case ACPI_ERR_BAD_RSDP: return "BAD-RSDP";
    case ACPI_ERR_NO_XSDT: return "NO-XSDT";
    case ACPI_ERR_BAD_XSDT: return "BAD-XSDT";
    case ACPI_ERR_NO_MADT: return "NO-MADT";
    case ACPI_ERR_BAD_MADT: return "BAD-MADT";
    case ACPI_ERR_BAD_ENTRY: return "BAD-ENTRY";
    case ACPI_ERR_RANGE: return "RANGE";
    default: return "UNKNOWN";
    }
}

static void draw_acpi_info(framebuffer_t *fb, UINT32 *y, acpi_info_t *acpi, UINT32 fg, UINT32 accent,
                           UINT32 warn, UINT32 bg) {
    char line[192];
    char *p = append_str(line, "ACPI: ");
    p = append_str(p, acpi_error_name(acpi->error));
    p = append_str(p, "  RSDP: ");
    p = append_hex64(p, acpi->rsdp);
    p = append_str(p, "  XSDT: ");
    p = append_hex64(p, acpi->xsdt);
    p = append_str(p, "  ENTRIES: ");
    append_dec(p, acpi->xsdt_entries);
    draw_line(fb, 48, y, line, acpi->error == ACPI_OK ? accent : warn, bg, 2);

    p = append_str(line, "MADT: ");
    p = append_hex64(p, acpi->madt);
    p = append_str(p, "  LAPIC BASE: ");
    p = append_hex64(p, acpi->local_apic_base);
    p = append_str(p, "  FLAGS: ");
    p = append_hex64(p, acpi->madt_flags);
    p = append_str(p, "  BSP APIC ID: ");
    append_dec(p, acpi->bsp_apic_id);
    draw_line(fb, 48, y, line, fg, bg, 2);

    p = append_str(line, "ENABLED CPU/APIC: ");
    p = append_dec(p, acpi->enabled_cpu_count);
    p = append_str(p, "  LIST: ");
    UINT32 output_count = acpi->stored_cpu_count;
    if (output_count > ACPI_CPU_LIST_OUTPUT_LIMIT) {
        output_count = ACPI_CPU_LIST_OUTPUT_LIMIT;
    }
    for (UINT32 i = 0; i < output_count; i++) {
        if (i > 0) {
            *p++ = ' ';
            *p = 0;
        }
        p = append_dec(p, acpi->cpus[i].acpi_uid);
        *p++ = '/';
        *p = 0;
        p = append_dec(p, acpi->cpus[i].apic_id);
        if (acpi->cpus[i].x2apic) {
            p = append_str(p, "(X2)");
        }
    }
    if (acpi->enabled_cpu_count > output_count) {
        p = append_str(p, " +");
        append_dec(p, acpi->enabled_cpu_count - output_count);
    }
    draw_line(fb, 48, y, line, fg, bg, 2);
}

static void write_u16(UINT8 *p, UINT16 value) {
    p[0] = (UINT8)(value & 0xff);
    p[1] = (UINT8)((value >> 8) & 0xff);
}

static void write_u32(UINT8 *p, UINT32 value) {
    p[0] = (UINT8)(value & 0xff);
    p[1] = (UINT8)((value >> 8) & 0xff);
    p[2] = (UINT8)((value >> 16) & 0xff);
    p[3] = (UINT8)((value >> 24) & 0xff);
}

static void write_u64(UINT8 *p, UINT64 value) {
    write_u32(p, (UINT32)(value & 0xffffffffULL));
    write_u32(p + 4, (UINT32)(value >> 32));
}

static void emit8(UINT8 **p, UINT8 value) {
    **p = value;
    *p += 1;
}

static void emit16(UINT8 **p, UINT16 value) {
    write_u16(*p, value);
    *p += 2;
}

static void emit32(UINT8 **p, UINT32 value) {
    write_u32(*p, value);
    *p += 4;
}

static void emit64(UINT8 **p, UINT64 value) {
    write_u64(*p, value);
    *p += 8;
}

static void emit_mov_rax_imm64(UINT8 **p, UINT64 value) {
    emit8(p, 0x48);
    emit8(p, 0xb8);
    emit64(p, value);
}

static EFI_STATUS allocate_ap_trampoline(EFI_BOOT_SERVICES *bs, ap_boot_info_t *boot) {
    zero_memory(boot, sizeof(*boot));
    EFI_PHYSICAL_ADDRESS base = AP_TRAMPOLINE_MAX_ADDRESS;
    EFI_STATUS status = bs->AllocatePages(AllocateMaxAddress, EfiLoaderData, AP_TRAMPOLINE_PAGES, &base);
    if (status != EFI_SUCCESS || base == 0 || base >= 0x100000ULL || (base & (PAGE_SIZE_4K - 1)) != 0) {
        boot->error = AP_BOOT_ERR_NO_TRAMPOLINE;
        return status == EFI_SUCCESS ? (EFI_STATUS)1 : status;
    }

    boot->trampoline_base = base;
    boot->sipi_vector = (UINT32)(base >> 12);
    boot->error = AP_BOOT_OK;
    return EFI_SUCCESS;
}

static int acpi_cpu_is_supported_ap(acpi_info_t *acpi, acpi_cpu_t *cpu) {
    return !cpu->x2apic && cpu->apic_id != acpi->bsp_apic_id && cpu->apic_id < 256;
}

static UINTN count_supported_ap_targets(acpi_info_t *acpi) {
    if (acpi->error != ACPI_OK) {
        return 0;
    }

    UINTN count = 0;
    for (UINT32 i = 0; i < acpi->stored_cpu_count; i++) {
        if (acpi_cpu_is_supported_ap(acpi, &acpi->cpus[i])) {
            count++;
        }
    }

    return count;
}

static int select_ap_target(acpi_info_t *acpi, UINTN target_index, ap_boot_info_t *boot) {
    if (acpi->error != ACPI_OK) {
        return 0;
    }

    UINTN supported_index = 0;
    for (UINT32 i = 0; i < acpi->stored_cpu_count; i++) {
        acpi_cpu_t *cpu = &acpi->cpus[i];
        if (acpi_cpu_is_supported_ap(acpi, cpu)) {
            if (supported_index != target_index) {
                supported_index++;
                continue;
            }
            boot->target_acpi_uid = cpu->acpi_uid;
            boot->target_apic_id = cpu->apic_id;
            return 1;
        }
    }

    return 0;
}

static int build_ap_trampoline(ap_context_t *ctx, paging_info_t *paging) {
    ap_boot_info_t *boot = &ctx->boot;
    if (boot->trampoline_base == 0 || boot->trampoline_base >= 0x100000ULL ||
        (boot->trampoline_base & (PAGE_SIZE_4K - 1)) != 0 || boot->sipi_vector == 0 ||
        boot->sipi_vector > 0xff) {
        boot->error = AP_BOOT_ERR_BAD_TRAMPOLINE;
        return 0;
    }
    if (paging->loaded_cr3 > 0xffffffffULL) {
        boot->error = AP_BOOT_ERR_HIGH_CR3;
        return 0;
    }

    UINT8 *page = (UINT8 *)(UINTN)boot->trampoline_base;
    for (UINT32 i = 0; i < PAGE_SIZE_4K; i++) {
        page[i] = 0;
    }
    boot->stack_top = (UINT64)(UINTN)(ctx->boot_stack + sizeof(ctx->boot_stack));

    UINT8 *p = page;
    emit8(&p, 0xfa);                         /* cli */
    emit8(&p, 0xfc);                         /* cld */
    emit8(&p, 0x8c); emit8(&p, 0xc8);        /* mov ax, cs */
    emit8(&p, 0x8e); emit8(&p, 0xd8);        /* mov ds, ax */
    emit8(&p, 0x8e); emit8(&p, 0xc0);        /* mov es, ax */
    emit8(&p, 0x8e); emit8(&p, 0xd0);        /* mov ss, ax */
    emit8(&p, 0xbc); emit16(&p, 0x0ff0);     /* mov sp, 0x0ff0 */
    emit8(&p, 0x0f); emit8(&p, 0x01); emit8(&p, 0x16);
    emit16(&p, AP_TRAMPOLINE_GDTR_OFFSET);   /* lgdt [gdtr] */
    emit8(&p, 0x0f); emit8(&p, 0x20); emit8(&p, 0xc0); /* mov eax, cr0 */
    emit8(&p, 0x66); emit8(&p, 0x0d); emit32(&p, 0x1); /* or eax, PE */
    emit8(&p, 0x0f); emit8(&p, 0x22); emit8(&p, 0xc0); /* mov cr0, eax */
    emit8(&p, 0x66); emit8(&p, 0xea);
    emit32(&p, (UINT32)(boot->trampoline_base + AP_TRAMPOLINE_PROT32_OFFSET));
    emit16(&p, AP_TRAMPOLINE_PROT32_SELECTOR); /* jmp far 0x18:protected32 */

    p = page + AP_TRAMPOLINE_PROT32_OFFSET;
    emit8(&p, 0x66); emit8(&p, 0xb8); emit16(&p, KERNEL_DATA_SELECTOR); /* mov ax, 0x10 */
    emit8(&p, 0x8e); emit8(&p, 0xd8);        /* mov ds, eax */
    emit8(&p, 0x8e); emit8(&p, 0xc0);        /* mov es, eax */
    emit8(&p, 0x8e); emit8(&p, 0xd0);        /* mov ss, eax */
    emit8(&p, 0x0f); emit8(&p, 0x20); emit8(&p, 0xe0); /* mov eax, cr4 */
    emit8(&p, 0x0d); emit32(&p, 0x20);       /* or eax, PAE */
    emit8(&p, 0x0f); emit8(&p, 0x22); emit8(&p, 0xe0); /* mov cr4, eax */
    emit8(&p, 0xb8); emit32(&p, (UINT32)paging->loaded_cr3); /* mov eax, cr3 */
    emit8(&p, 0x0f); emit8(&p, 0x22); emit8(&p, 0xd8); /* mov cr3, eax */
    emit8(&p, 0xb9); emit32(&p, 0xc0000080U); /* mov ecx, EFER */
    emit8(&p, 0x0f); emit8(&p, 0x32);        /* rdmsr */
    emit8(&p, 0x0d); emit32(&p, 0x100);      /* or eax, LME */
    emit8(&p, 0x0f); emit8(&p, 0x30);        /* wrmsr */
    emit8(&p, 0x0f); emit8(&p, 0x20); emit8(&p, 0xc0); /* mov eax, cr0 */
    emit8(&p, 0x0d); emit32(&p, 0x80000000U); /* or eax, PG */
    emit8(&p, 0x0f); emit8(&p, 0x22); emit8(&p, 0xc0); /* mov cr0, eax */
    emit8(&p, 0xea);
    emit32(&p, (UINT32)(boot->trampoline_base + AP_TRAMPOLINE_LONG_OFFSET));
    emit16(&p, KERNEL_CODE_SELECTOR);        /* jmp far 0x08:long_entry */

    p = page + AP_TRAMPOLINE_LONG_OFFSET;
    emit8(&p, 0x66); emit8(&p, 0xb8); emit16(&p, KERNEL_DATA_SELECTOR); /* mov ax, 0x10 */
    emit8(&p, 0x8e); emit8(&p, 0xd8);        /* mov ds, eax */
    emit8(&p, 0x8e); emit8(&p, 0xc0);        /* mov es, eax */
    emit8(&p, 0x8e); emit8(&p, 0xd0);        /* mov ss, eax */
    emit_mov_rax_imm64(&p, boot->stack_top);
    emit8(&p, 0x48); emit8(&p, 0x89); emit8(&p, 0xc4); /* mov rsp, rax */
    emit_mov_rax_imm64(&p, (UINT64)(UINTN)ctx);
    emit8(&p, 0x48); emit8(&p, 0x89); emit8(&p, 0xc7); /* mov rdi, rax */
    emit_mov_rax_imm64(&p, (UINT64)(UINTN)ap_entry_one);
    emit8(&p, 0xff); emit8(&p, 0xd0);        /* call rax */
    emit8(&p, 0xfa);                         /* cli */
    emit8(&p, 0xf4);                         /* hlt */
    emit8(&p, 0xeb); emit8(&p, 0xfd);        /* jmp hlt */

    UINT8 *gdtr = page + AP_TRAMPOLINE_GDTR_OFFSET;
    write_u16(gdtr, (UINT16)(4 * sizeof(UINT64) - 1));
    write_u32(gdtr + 2, (UINT32)(boot->trampoline_base + AP_TRAMPOLINE_GDT_OFFSET));

    UINT8 *gdt = page + AP_TRAMPOLINE_GDT_OFFSET;
    write_u64(gdt + 0, 0x0000000000000000ULL);
    write_u64(gdt + 8, 0x00af9a000000ffffULL);
    write_u64(gdt + 16, 0x00cf92000000ffffULL);
    write_u64(gdt + 24, 0x00cf9a000000ffffULL);

    boot->ap_state = AP_BOOT_STATE_READY;
    return 1;
}

static UINT64 read_msr(UINT32 msr) {
    UINT32 low = 0;
    UINT32 high = 0;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((UINT64)high << 32) | low;
}

static void delay_loops(UINT32 loops) {
    for (volatile UINT32 i = 0; i < loops; i++) {
        __asm__ __volatile__("pause");
    }
}

static UINT32 lapic_read32(UINT64 base, UINT32 reg) {
    return *(volatile UINT32 *)(UINTN)(base + reg);
}

static void lapic_write32(UINT64 base, UINT32 reg, UINT32 value) {
    *(volatile UINT32 *)(UINTN)(base + reg) = value;
}

static int lapic_wait_icr_idle(UINT64 base, ap_boot_info_t *boot) {
    for (UINT32 i = 0; i < 1000000U; i++) {
        if ((lapic_read32(base, 0x300) & (1U << 12)) == 0) {
            return 1;
        }
        __asm__ __volatile__("pause");
    }

    boot->icr_timeouts++;
    return 0;
}

static int lapic_send_ipi(UINT64 base, UINT32 apic_id, UINT32 command, ap_boot_info_t *boot) {
    if (!lapic_wait_icr_idle(base, boot)) {
        return 0;
    }

    lapic_write32(base, 0x310, apic_id << 24);
    lapic_write32(base, 0x300, command);
    return lapic_wait_icr_idle(base, boot);
}

static void enable_lapic_if_needed(UINT64 base) {
    UINT32 svr = lapic_read32(base, 0x0f0);
    UINT32 desired = (svr & 0xffffff00U) | (1U << 8) | LAPIC_SPURIOUS_VECTOR;
    if (svr != desired) {
        lapic_write32(base, 0x0f0, desired);
    }
}

static void bring_up_one_ap(ap_context_t *ctx, UINTN target_index, acpi_info_t *acpi,
                            efi_memory_map_t *map, paging_info_t *paging) {
    ap_boot_info_t *boot = &ctx->boot;
    boot->online = 0;
    boot->ap_state = AP_BOOT_STATE_NONE;
    boot->entry_state = AP_ENTRY_STATE_NONE;
    boot->gdt_ok = 0;
    boot->tss_ok = 0;
    boot->idt_ok = 0;
    boot->ap_cs = 0;
    boot->ap_tr = 0;
    boot->ap_ist1 = 0;
    boot->ap_ist2 = 0;
    boot->fault_vector = 0;
    boot->fault_error_code = 0;
    boot->fault_rip = 0;
    boot->fault_cs = 0;
    boot->fault_rflags = 0;
    boot->fault_cr2 = 0;
    boot->fault_phase = AP_FAULT_PHASE_NONE;
    boot->fault_request_state = AP_REQUEST_STATUS_EMPTY;
    boot->fault_request_opcode = AP_REQUEST_OP_NONE;
    boot->fault_request_sequence = 0;
    boot->fault_request_service_id = 0;
    boot->fault_request_interface_id = 0;
    boot->fault_request_id_high = 0;
    boot->fault_request_id_low = 0;
    boot->target_acpi_uid = 0;
    boot->target_apic_id = 0;
    boot->wait_loops = 0;
    boot->icr_timeouts = 0;

    if (boot->error != AP_BOOT_OK) {
        return;
    }
    if (!select_ap_target(acpi, target_index, boot)) {
        boot->error = AP_BOOT_ERR_NO_TARGET;
        return;
    }
    if (read_msr(0x1b) & (1ULL << 10)) {
        boot->error = AP_BOOT_ERR_X2APIC;
        return;
    }
    if (!memory_map_contains_range(map, acpi->local_apic_base, PAGE_SIZE_4K)) {
        boot->error = AP_BOOT_ERR_LAPIC_RANGE;
        return;
    }
    if (!build_ap_trampoline(ctx, paging)) {
        return;
    }
    __asm__ __volatile__("mfence" : : : "memory");

    UINT64 lapic_base = acpi->local_apic_base;
    enable_lapic_if_needed(lapic_base);

    if (!lapic_send_ipi(lapic_base, boot->target_apic_id, 0x00004500U, boot)) {
        boot->error = AP_BOOT_ERR_ICR_TIMEOUT;
        return;
    }
    boot->ap_state = AP_BOOT_STATE_SENT_INIT;
    delay_loops(1000000U);

    if (!lapic_send_ipi(lapic_base, boot->target_apic_id, 0x00008500U, boot)) {
        boot->error = AP_BOOT_ERR_ICR_TIMEOUT;
        return;
    }
    delay_loops(1000000U);

    UINT32 sipi = 0x00000600U | (boot->sipi_vector & 0xffU);
    if (!lapic_send_ipi(lapic_base, boot->target_apic_id, sipi, boot)) {
        boot->error = AP_BOOT_ERR_ICR_TIMEOUT;
        return;
    }
    boot->ap_state = AP_BOOT_STATE_SENT_SIPI;
    delay_loops(200000U);

    if (!boot->online) {
        if (!lapic_send_ipi(lapic_base, boot->target_apic_id, sipi, boot)) {
            boot->error = AP_BOOT_ERR_ICR_TIMEOUT;
            return;
        }
    }

    for (UINT32 i = 0; i < AP_ONLINE_TIMEOUT_LOOPS; i++) {
        if (boot->online) {
            if (boot->ap_state != AP_BOOT_STATE_HALTED && boot->ap_state != AP_BOOT_STATE_FAULTED) {
                boot->ap_state = AP_BOOT_STATE_ONLINE;
            }
            boot->error = AP_BOOT_OK;
            boot->wait_loops = i;
            return;
        }
        __asm__ __volatile__("pause");
    }

    boot->wait_loops = AP_ONLINE_TIMEOUT_LOOPS;
    boot->error = AP_BOOT_ERR_ONLINE_TIMEOUT;
}

static const char *ap_boot_error_name(UINT32 error) {
    switch (error) {
    case AP_BOOT_OK: return "OK";
    case AP_BOOT_ERR_NO_TRAMPOLINE: return "NO-TRAMP";
    case AP_BOOT_ERR_NO_TARGET: return "NO-TARGET";
    case AP_BOOT_ERR_X2APIC: return "X2APIC";
    case AP_BOOT_ERR_BAD_TRAMPOLINE: return "BAD-TRAMP";
    case AP_BOOT_ERR_HIGH_CR3: return "HIGH-CR3";
    case AP_BOOT_ERR_LAPIC_RANGE: return "LAPIC-RANGE";
    case AP_BOOT_ERR_ICR_TIMEOUT: return "ICR-TIMEOUT";
    case AP_BOOT_ERR_ONLINE_TIMEOUT: return "ONLINE-TIMEOUT";
    default: return "UNKNOWN";
    }
}

static const char *ap_boot_state_name(UINT32 state) {
    switch (state) {
    case AP_BOOT_STATE_NONE: return "NONE";
    case AP_BOOT_STATE_READY: return "READY";
    case AP_BOOT_STATE_SENT_INIT: return "SENT-INIT";
    case AP_BOOT_STATE_SENT_SIPI: return "SENT-SIPI";
    case AP_BOOT_STATE_ONLINE: return "ONLINE";
    case AP_BOOT_STATE_HALTED: return "HALTED";
    case AP_BOOT_STATE_FAULTED: return "FAULTED";
    default: return "UNKNOWN";
    }
}

static const char *ap_entry_state_name(UINT32 state) {
    switch (state) {
    case AP_ENTRY_STATE_NONE: return "NONE";
    case AP_ENTRY_STATE_C: return "C";
    case AP_ENTRY_STATE_TABLES: return "TABLES";
    case AP_ENTRY_STATE_HALTED: return "HALTED";
    case AP_ENTRY_STATE_FAULT: return "FAULT";
    case AP_ENTRY_STATE_LOOP: return "LOOP";
    case AP_ENTRY_STATE_REQUEST: return "REQUEST";
    default: return "UNKNOWN";
    }
}

static const char *ap_fault_phase_name(UINT32 phase) {
    switch (phase) {
    case AP_FAULT_PHASE_NONE: return "NONE";
    case AP_FAULT_PHASE_STARTUP: return "STARTUP";
    case AP_FAULT_PHASE_LOOP: return "LOOP";
    case AP_FAULT_PHASE_REQUEST: return "REQUEST";
    default: return "UNKNOWN";
    }
}

static const char *ap_request_state_name(UINT32 state) {
    switch (state) {
    case AP_REQUEST_STATUS_EMPTY: return "EMPTY";
    case AP_REQUEST_STATUS_PENDING: return "PENDING";
    case AP_REQUEST_STATUS_RUNNING: return "RUNNING";
    case AP_REQUEST_STATUS_DONE: return "DONE";
    case AP_REQUEST_STATUS_TIMEOUT: return "TIMEOUT";
    case AP_REQUEST_STATUS_BAD_OP: return "BAD-OP";
    case AP_REQUEST_STATUS_FAULT: return "FAULT";
    case AP_REQUEST_STATUS_SKIPPED: return "SKIPPED";
    default: return "UNKNOWN";
    }
}

static const char *ap_request_op_name(UINT32 opcode) {
    switch (opcode) {
    case AP_REQUEST_OP_NONE: return "NONE";
    case AP_REQUEST_OP_PING: return "PING";
    case AP_REQUEST_OP_COUNTER: return "COUNTER";
    default: return "UNKNOWN";
    }
}

static const char *ap_queue_stop_reason_name(UINT32 reason) {
    switch (reason) {
    case AP_QUEUE_STOP_NONE: return "NONE";
    case AP_QUEUE_STOP_DRAINED: return "DRAINED";
    case AP_QUEUE_STOP_BAD_OP: return "BAD-OP";
    case AP_QUEUE_STOP_FAULT: return "FAULT";
    default: return "UNKNOWN";
    }
}

static int ap_request_result_ok(ap_request_slot_t *request, UINT32 counter_value) {
    if (request->state != AP_REQUEST_STATUS_DONE ||
        request->reply.fault_code != 0 ||
        request->request.id_high != request->reply.request_id_high ||
        request->request.id_low != request->reply.request_id_low) {
        return 0;
    }

    if (request->request.service_id == AP_REQUEST_SERVICE_PING &&
        request->request.interface_id == AP_REQUEST_INTERFACE_PING) {
        return request->request.opcode == AP_REQUEST_OP_PING &&
               request->reply.result_code == 0;
    }

    if (request->request.service_id == AP_REQUEST_SERVICE_COUNTER &&
        request->request.interface_id == AP_REQUEST_INTERFACE_COUNTER_INCREMENT) {
        return request->request.opcode == AP_REQUEST_OP_COUNTER &&
               request->reply.result_code == counter_value &&
               counter_value > 0;
    }

    return 0;
}

static void draw_ap_boot_info(framebuffer_t *fb, UINT32 *y, ap_boot_info_t *boot, UINT32 fg,
                              UINT32 accent, UINT32 warn, UINT32 bg) {
    char line[192];
    char *p = append_str(line, "AP START: ");
    p = append_str(p, ap_boot_error_name(boot->error));
    p = append_str(p, "  TARGET UID/APIC: ");
    p = append_dec(p, boot->target_acpi_uid);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, boot->target_apic_id);
    p = append_str(p, "  ONLINE: ");
    append_dec(p, boot->online ? 1U : 0U);
    draw_line(fb, 48, y, line, boot->error == AP_BOOT_OK ? accent : warn, bg, 2);

    p = append_str(line, "AP STATE: ");
    p = append_str(p, ap_boot_state_name(boot->ap_state));
    p = append_str(p, "  TRAMP: ");
    p = append_hex64(p, boot->trampoline_base);
    p = append_str(p, "  SIPI: ");
    p = append_dec(p, boot->sipi_vector);
    p = append_str(p, "  ICR-TO: ");
    append_dec(p, boot->icr_timeouts);
    draw_line(fb, 48, y, line, fg, bg, 2);

    p = append_str(line, "AP ENTRY: ");
    p = append_str(p, ap_entry_state_name(boot->entry_state));
    p = append_str(p, "  GDT: ");
    p = append_str(p, boot->gdt_ok ? "OK" : "NO");
    p = append_str(p, "  TSS: ");
    p = append_str(p, boot->tss_ok ? "OK" : "NO");
    p = append_str(p, "  IDT: ");
    p = append_str(p, boot->idt_ok ? "OK" : "NO");
    p = append_str(p, "  CS: ");
    p = append_hex64(p, boot->ap_cs);
    p = append_str(p, "  TR: ");
    append_hex64(p, boot->ap_tr);
    draw_line(fb, 48, y, line, (boot->gdt_ok && boot->tss_ok && boot->idt_ok) ? accent : fg, bg, 2);

    p = append_str(line, "AP IST1: ");
    p = append_hex64(p, boot->ap_ist1);
    p = append_str(p, "  IST2: ");
    append_hex64(p, boot->ap_ist2);
    draw_line(fb, 48, y, line, fg, bg, 2);

    if (boot->fault_vector != 0) {
        p = append_str(line, "AP FAULT: ");
        p = append_str(p, ap_fault_phase_name(boot->fault_phase));
        p = append_str(p, "  VEC: ");
        p = append_dec64(p, boot->fault_vector);
        p = append_str(p, " ");
        p = append_str(p, fault_vector_name(boot->fault_vector));
        p = append_str(p, "  ERR: ");
        p = append_hex64(p, boot->fault_error_code);
        p = append_str(p, "  CR2: ");
        append_hex64(p, boot->fault_cr2);
        draw_line(fb, 48, y, line, warn, bg, 2);

        p = append_str(line, "AP RIP: ");
        p = append_hex64(p, boot->fault_rip);
        p = append_str(p, "  CS: ");
        p = append_hex64(p, boot->fault_cs);
        p = append_str(p, "  RFLAGS: ");
        append_hex64(p, boot->fault_rflags);
        draw_line(fb, 48, y, line, warn, bg, 2);

        if (boot->fault_phase == AP_FAULT_PHASE_REQUEST) {
            p = append_str(line, "AP FAULT REQ: ");
            p = append_str(p, ap_request_op_name(boot->fault_request_opcode));
            p = append_str(p, "  STATE: ");
            p = append_str(p, ap_request_state_name(boot->fault_request_state));
            p = append_str(p, "  SEQ: ");
            p = append_dec(p, boot->fault_request_sequence);
            p = append_str(p, "  ID: ");
            p = append_hex64(p, boot->fault_request_id_high);
            *p++ = '-';
            *p = 0;
            append_hex64(p, boot->fault_request_id_low);
            draw_line(fb, 48, y, line, warn, bg, 2);

            p = append_str(line, "AP FAULT KEY: SVC: ");
            p = append_hex64(p, boot->fault_request_service_id);
            p = append_str(p, "  IFACE: ");
            append_hex64(p, boot->fault_request_interface_id);
            draw_line(fb, 48, y, line, warn, bg, 2);
        }
    }
}

static void draw_ap_queue_summary(framebuffer_t *fb, UINT32 *y, ap_context_t *ctx,
                                  UINT32 fg, UINT32 accent, UINT32 warn, UINT32 bg) {
    ap_queue_summary_t *queue = &ctx->queue_summary;
    char line[192];
    char *p = append_str(line, "AP QUEUE: HANDLED ");
    p = append_dec(p, queue->handled_count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, queue->planned_count ? queue->planned_count : AP_REQUEST_SLOT_COUNT);
    p = append_str(p, "  LAST: ");
    p = append_dec(p, queue->last_slot);
    *p++ = ' ';
    *p = 0;
    p = append_str(p, ap_request_state_name(queue->last_state));
    p = append_str(p, "  STOP: ");
    p = append_str(p, ap_queue_stop_reason_name(queue->stop_reason));
    p = append_str(p, "  CURRENT: ");
    p = append_dec(p, queue->current_slot);
    p = append_str(p, "  IPI RX/TX/FAIL: ");
    p = append_dec(p, ctx->interrupts.completion_ipi.count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, ctx->request_ipi_send_count);
    *p++ = '/';
    *p = 0;
    append_dec(p, ctx->request_ipi_send_fail_count);

    UINT32 color = fg;
    if (queue->stop_reason == AP_QUEUE_STOP_DRAINED) {
        color = accent;
    } else if (queue->stop_reason == AP_QUEUE_STOP_BAD_OP ||
               queue->stop_reason == AP_QUEUE_STOP_FAULT) {
        color = warn;
    }
    draw_line(fb, 48, y, line, color, bg, 2);
}

static void draw_ap_kick_summary(framebuffer_t *fb, UINT32 *y, ap_context_t *ctx,
                                 UINT32 fg, UINT32 accent, UINT32 warn, UINT32 bg) {
    UINT32 kick_rx_count = ctx ? ctx->interrupts.kick_ipi.count : 0;
    UINT32 kick_send_count = ctx ? ctx->request_kick_ipi_send_count : 0;
    UINT32 kick_send_fail_count = ctx ? ctx->request_kick_ipi_send_fail_count : 0;
    UINT32 idle_halt_count = ctx ? ctx->idle_halt_count : 0;
    UINT32 idle_wake_count = ctx ? ctx->idle_wake_count : 0;
    UINT32 idle_timer_count = ctx ? ctx->interrupts.idle_timer_count : 0;
    char line[192];
    char *p = append_str(line, "AP KICK: RX/TX/FAIL: ");
    p = append_dec(p, kick_rx_count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, kick_send_count);
    *p++ = '/';
    *p = 0;
    append_dec(p, kick_send_fail_count);

    UINT32 color = kick_send_fail_count ? warn : fg;
    if (kick_send_count && !kick_send_fail_count) {
        color = accent;
    }
    draw_line(fb, 48, y, line, color, bg, 2);

    p = append_str(line, "AP IDLE: HALT/WAKE/TIMER: ");
    p = append_dec(p, idle_halt_count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, idle_wake_count);
    *p++ = '/';
    *p = 0;
    append_dec(p, idle_timer_count);
    draw_line(fb, 48, y, line, fg, bg, 2);

    p = append_str(line, "BSP WAIT: HALT/WAKE/TIMER: ");
    p = append_dec(p, bsp_wait_observe.halt_count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, bsp_wait_observe.wake_count);
    *p++ = '/';
    *p = 0;
    append_dec(p, bsp_wait_observe.timer_count);
    draw_line(fb, 48, y, line, fg, bg, 2);
}

static void draw_ap_request_history_entry(framebuffer_t *fb, UINT32 *y,
                                          ap_request_history_entry_t *entry,
                                          UINT32 history_number, UINT32 accent,
                                          UINT32 warn, UINT32 bg) {
    ap_request_slot_t *request = &entry->request;
    char line[256];
    char *p;
    int ok = ap_request_result_ok(request, entry->counter_value);
    UINT32 color = ok ? accent : warn;
    if (request->state == AP_REQUEST_STATUS_SKIPPED ||
        request->state == AP_REQUEST_STATUS_TIMEOUT) {
        color = warn;
    }

    p = append_str(line, "AP HIST ");
    p = append_dec(p, history_number);
    p = append_str(p, " SLOT ");
    p = append_dec(p, entry->slot);
    p = append_str(p, ": ");
    p = append_str(p, ap_request_op_name(request->request.opcode));
    p = append_str(p, ok ? " OK" : " NOT-OK");
    p = append_str(p, "  STATE: ");
    p = append_str(p, ap_request_state_name(request->state));
    p = append_str(p, "  SEQ: ");
    p = append_dec(p, request->request.sequence);
    p = append_str(p, "  WAIT: ");
    p = append_dec(p, request->metrics.wait_loops);
    p = append_str(p, "  COUNT: ");
    p = append_dec(p, request->metrics.handled_count);
    p = append_str(p, "  RESULT: ");
    p = append_dec(p, request->reply.result_code);
    p = append_str(p, "  FAULT: ");
    p = append_dec(p, request->reply.fault_code);
    p = append_str(p, "  S/I: ");
    p = append_hex32(p, (UINT32)request->request.service_id);
    *p++ = '/';
    *p = 0;
    append_hex32(p, (UINT32)request->request.interface_id);
    draw_line(fb, 48, y, line, color, bg, 2);
}

static void draw_ap_request_history(framebuffer_t *fb, UINT32 *y,
                                    ap_request_history_entry_t *history, UINTN count,
                                    UINT32 fg, UINT32 accent, UINT32 warn, UINT32 bg) {
    UINT32 drawn = 0;

    for (UINTN i = 0; i < count; i++) {
        if (!history[i].valid) {
            continue;
        }
        draw_ap_request_history_entry(fb, y, &history[i], (UINT32)(i + 1),
                                      accent, warn, bg);
        drawn++;
    }

    if (!drawn) {
        draw_line(fb, 48, y, "AP HISTORY: NONE", warn, bg, 2);
    }

    (void)fg;
}

static int draw_map_line(framebuffer_t *fb, UINT32 *y, const char *s, UINT32 fg, UINT32 bg) {
    if (*y + 20 > fb->height) {
        return 0;
    }
    draw_line(fb, 48, y, s, fg, bg, 2);
    return 1;
}

static UINT32 memory_line_color(EFI_MEMORY_DESCRIPTOR *desc, UINT32 fg, UINT32 muted, UINT32 accent, UINT32 warn) {
    switch (desc->Type) {
    case EfiConventionalMemory:
        return fg;
    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
        return warn;
    case EfiReservedMemoryType:
    case EfiUnusableMemory:
        return accent;
    default:
        return muted;
    }
}

static void draw_ap_context_summary(framebuffer_t *fb, UINT32 *y, ap_context_t *contexts,
                                    UINTN context_count, ap_context_t *target_context,
                                    UINT32 fg, UINT32 accent, UINT32 warn, UINT32 bg) {
    char line[192];
    char *p = append_str(line, "AP ONLINE: ");
    UINT32 online_count = 0;
    if (context_count > MAX_AP_CONTEXTS) {
        context_count = MAX_AP_CONTEXTS;
    }

    for (UINTN i = 0; i < context_count; i++) {
        if (contexts[i].boot.online) {
            online_count++;
        }
    }

    p = append_dec(p, online_count);
    *p++ = '/';
    *p = 0;
    p = append_dec(p, (UINT32)context_count);
    if (target_context) {
        p = append_str(p, "  TARGET: ");
        p = append_dec(p, ap_context_index(target_context) + 1U);
        *p++ = '/';
        *p = 0;
        p = append_dec(p, target_context->boot.target_apic_id);
    }
    p = append_str(p, "  APIC:");
    if (context_count == 0) {
        p = append_str(p, " NONE");
    }
    for (UINTN i = 0; i < context_count; i++) {
        *p++ = ' ';
        *p = 0;
        p = append_dec(p, contexts[i].boot.target_apic_id);
        if (!contexts[i].boot.online) {
            p = append_str(p, "!");
        }
    }

    UINT32 color = (context_count > 0 && online_count == context_count) ? accent : warn;
    draw_line(fb, 48, y, line, color ? color : fg, bg, 2);
}

static void draw_memory_map(framebuffer_t *fb, efi_memory_map_t *map, paging_info_t *paging,
                            acpi_info_t *acpi, ap_context_t *target_context,
                            ap_context_t *ap_contexts, UINTN ap_context_count,
                            UINT32 fg, UINT32 muted, UINT32 accent, UINT32 warn, UINT32 bg) {
    UINT64 conventional_pages = 0;
    UINT64 loader_pages = 0;
    UINT64 boot_pages = 0;
    UINT64 runtime_pages = 0;
    UINT64 acpi_pages = 0;
    UINT64 mmio_pages = 0;
    UINT64 reserved_pages = 0;
    UINT64 total_pages = 0;
    UINT64 max_end = 0;
    char line[192];
    char *p;
    const UINT32 table_scale = 2;

    for (UINTN i = 0; i < map->descriptor_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, i);
        UINT64 pages = desc->NumberOfPages;
        UINT64 end = desc->PhysicalStart + (pages << 12);
        total_pages += pages;
        if (end > max_end) {
            max_end = end;
        }

        switch (desc->Type) {
        case EfiConventionalMemory:
            conventional_pages += pages;
            break;
        case EfiLoaderCode:
        case EfiLoaderData:
            loader_pages += pages;
            break;
        case EfiBootServicesCode:
        case EfiBootServicesData:
            boot_pages += pages;
            break;
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
            runtime_pages += pages;
            break;
        case EfiACPIReclaimMemory:
        case EfiACPIMemoryNVS:
            acpi_pages += pages;
            break;
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
            mmio_pages += pages;
            break;
        default:
            reserved_pages += pages;
            break;
        }
    }

    UINT32 y = 42;
    draw_text(fb, 48, y, "UEFI MEMORY MAP", fg, bg, 4);
    y += 58;

    if (paging) {
        p = append_str(line, "PAGING: 4K IDENTITY  CR3: ");
        p = append_hex64(p, paging->loaded_cr3);
        p = append_str(p, "  IDT: ");
        append_str(p, paging->idt_self_test_ok ? "OK" : "FAIL");
        draw_line(fb, 48, &y, line, accent, bg, 2);

        p = append_str(line, "PT POOL: ");
        p = append_hex64(p, paging->pool_start);
        p = append_str(p, "-");
        p = append_hex64(p, paging->pool_end - 1);
        p = append_str(p, "  RESERVED: ");
        p = append_dec64(p, paging->reserved_pages);
        p = append_str(p, "  USED: ");
        append_dec64(p, paging->allocated_pages);
        draw_line(fb, 48, &y, line, fg, bg, 2);

        p = append_str(line, "IDENTITY END: ");
        p = append_hex64(p, paging->max_identity_end);
        p = append_str(p, "  MAPPED PAGES: ");
        p = append_dec64(p, paging->mapped_pages);
        p = append_str(p, "  RANGES: ");
        append_dec64(p, paging->mapped_ranges);
        draw_line(fb, 48, &y, line, fg, bg, 2);

        y += 8;
    }

    if (acpi) {
        draw_acpi_info(fb, &y, acpi, fg, accent, warn, bg);
        y += 8;
    }

    if (target_context) {
        if (ap_contexts) {
            draw_ap_context_summary(fb, &y, ap_contexts, ap_context_count, target_context,
                                    fg, accent, warn, bg);
        }
        draw_ap_boot_info(fb, &y, &target_context->boot, fg, accent, warn, bg);
        draw_ap_queue_summary(fb, &y, target_context, fg, accent, warn, bg);
        draw_ap_kick_summary(fb, &y, target_context, fg, accent, warn, bg);
        draw_ap_request_history(fb, &y, target_context->request_history,
                                AP_REQUEST_HISTORY_COUNT, fg, accent, warn, bg);
        y += 8;
    }

    p = append_str(line, "DESCRIPTORS: ");
    p = append_dec64(p, map->descriptor_count);
    p = append_str(p, "  DESC SIZE: ");
    p = append_dec64(p, map->descriptor_size);
    p = append_str(p, "  VERSION: ");
    append_dec(p, map->descriptor_version);
    draw_line(fb, 48, &y, line, accent, bg, 2);

    p = append_str(line, "TOTAL PAGES: ");
    p = append_dec64(p, total_pages);
    p = append_str(p, "  MAX END: ");
    append_hex64(p, max_end);
    draw_line(fb, 48, &y, line, muted, bg, 2);

    p = append_str(line, "CONV: ");
    p = append_dec64(p, conventional_pages);
    p = append_str(p, " PGS (");
    p = append_dec64(p, conventional_pages / 256);
    p = append_str(p, " MIB)  BOOT: ");
    p = append_dec64(p, boot_pages);
    p = append_str(p, "  LOADER: ");
    append_dec64(p, loader_pages);
    draw_line(fb, 48, &y, line, fg, bg, 2);

    p = append_str(line, "RUNTIME: ");
    p = append_dec64(p, runtime_pages);
    p = append_str(p, "  ACPI: ");
    p = append_dec64(p, acpi_pages);
    p = append_str(p, "  MMIO: ");
    p = append_dec64(p, mmio_pages);
    p = append_str(p, "  RESERVED-OTHER: ");
    append_dec64(p, reserved_pages);
    draw_line(fb, 48, &y, line, fg, bg, 2);

    y += 8;
    p = append_left(line, "IDX", 3);
    *p++ = ' ';
    *p = 0;
    p = append_left(p, "TYPE", 10);
    *p++ = ' ';
    *p = 0;
    p = append_left(p, "START", 18);
    *p++ = ' ';
    *p = 0;
    p = append_left(p, "END", 18);
    *p++ = ' ';
    *p = 0;
    p = append_left(p, "PAGES", 8);
    *p++ = ' ';
    *p = 0;
    append_left(p, "ATTR", 18);
    draw_line(fb, 48, &y, line, accent, bg, table_scale);

    UINTN output_count = map->descriptor_count;
    if (output_count > 512) {
        output_count = 512;
        draw_line(fb, 48, &y, "OUTPUT TRUNCATED TO 512 DESCRIPTORS", warn, bg, table_scale);
    }

    UINT8 emitted[512];
    for (UINTN i = 0; i < output_count; i++) {
        emitted[i] = 0;
    }

    UINT64 cursor = 0;

    for (UINTN printed = 0; printed < output_count; printed++) {
        UINTN best_index = output_count;
        UINT64 best_start = ~(UINT64)0;
        for (UINTN i = 0; i < output_count; i++) {
            EFI_MEMORY_DESCRIPTOR *candidate = memory_desc_at(map, i);
            if (!emitted[i] && (candidate->PhysicalStart < best_start || best_index == output_count)) {
                best_index = i;
                best_start = candidate->PhysicalStart;
            }
        }
        if (best_index == output_count) {
            return;
        }
        emitted[best_index] = 1;

        EFI_MEMORY_DESCRIPTOR *desc = memory_desc_at(map, best_index);
        UINT64 start = desc->PhysicalStart;
        UINT64 bytes = desc->NumberOfPages << 12;
        UINT64 end_exclusive = start + bytes;
        UINT64 end_inclusive = end_exclusive == 0 ? 0 : end_exclusive - 1;

        if (start > cursor) {
            UINT64 hole_pages = (start - cursor) >> 12;
            p = append_left(line, "---", 3);
            *p++ = ' ';
            *p = 0;
            p = append_left(p, "HOLE", 10);
            *p++ = ' ';
            *p = 0;
            p = append_hex64_width(p, cursor, 18);
            *p++ = ' ';
            *p = 0;
            p = append_hex64_width(p, start - 1, 18);
            *p++ = ' ';
            *p = 0;
            p = append_dec64_width(p, hole_pages, 8);
            *p++ = ' ';
            *p = 0;
            append_left(p, "", 18);
            if (!draw_map_line(fb, &y, line, warn, bg)) {
                return;
            }
        }

        p = append_dec64_width(line, best_index, 3);
        *p++ = ' ';
        *p = 0;
        p = append_left(p, memory_type_name(desc->Type), 10);
        *p++ = ' ';
        *p = 0;
        p = append_hex64_width(p, start, 18);
        *p++ = ' ';
        *p = 0;
        p = append_hex64_width(p, end_inclusive, 18);
        *p++ = ' ';
        *p = 0;
        p = append_dec64_width(p, desc->NumberOfPages, 8);
        *p++ = ' ';
        *p = 0;
        append_hex64_width(p, desc->Attribute, 18);
        if (!draw_map_line(fb, &y, line, memory_line_color(desc, fg, muted, accent, warn), bg)) {
            return;
        }

        if (end_exclusive > cursor) {
            cursor = end_exclusive;
        }
    }
}

static void draw_paging_failure(framebuffer_t *fb, paging_info_t *paging, UINT32 fg, UINT32 warn, UINT32 bg) {
    char line[192];
    char *p;
    UINT32 y = 48;

    clear_screen(fb, bg);
    draw_text(fb, 48, y, "PAGING FAILED", warn, bg, 4);
    y += 64;

    p = append_str(line, "ERROR: ");
    p = append_str(p, paging_error_name(paging->error));
    p = append_str(p, "  RESERVED PAGES: ");
    p = append_dec64(p, paging->reserved_pages);
    p = append_str(p, "  USED: ");
    append_dec64(p, paging->allocated_pages);
    draw_line(fb, 48, &y, line, fg, bg, 2);

    p = append_str(line, "PT POOL: ");
    p = append_hex64(p, paging->pool_start);
    p = append_str(p, "-");
    append_hex64(p, paging->pool_end);
    draw_line(fb, 48, &y, line, fg, bg, 2);
}

static void halt_forever(void) {
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static EFI_STATUS exit_boot_services(EFI_HANDLE image, EFI_BOOT_SERVICES *bs, efi_memory_map_t *out_map) {
    EFI_STATUS status;
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR *map = 0;

    status = bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    map_size += desc_size * 16;
    status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&map);
    if (status != EFI_SUCCESS) {
        return status;
    }

    for (int i = 0; i < 8; i++) {
        UINTN this_map_size = map_size;
        status = bs->GetMemoryMap(&this_map_size, map, &map_key, &desc_size, &desc_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            return status;
        }
        if (status != EFI_SUCCESS) {
            return status;
        }

        status = bs->ExitBootServices(image, map_key);
        if (status == EFI_SUCCESS) {
            out_map->descriptors = map;
            out_map->map_size = this_map_size;
            out_map->map_capacity = map_size;
            out_map->descriptor_size = desc_size;
            out_map->descriptor_version = desc_version;
            out_map->descriptor_count = this_map_size / desc_size;
            return EFI_SUCCESS;
        }
    }

    return status;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    static EFI_GUID gop_guid = {
        0x9042a9de,
        0x23dc,
        0x4a38,
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a},
    };

    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS status;

    if (bs->SetWatchdogTimer) {
        bs->SetWatchdogTimer(0, 0, 0, 0);
    }

    status = bs->LocateProtocol(&gop_guid, 0, (void **)&gop);
    if (status != EFI_SUCCESS || !gop || !gop->Mode || !gop->Mode->Info) {
        if (st->ConOut) {
            st->ConOut->OutputString(st->ConOut, L"no GOP framebuffer\r\n");
        }
        halt_forever();
    }

    framebuffer_t fb;
    fb.base = (UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
    fb.width = gop->Mode->Info->HorizontalResolution;
    fb.height = gop->Mode->Info->VerticalResolution;
    fb.stride = gop->Mode->Info->PixelsPerScanLine;
    fb.size = gop->Mode->FrameBufferSize;
    fb.format = gop->Mode->Info->PixelFormat;

    UINT64 acpi_rsdp = find_acpi_rsdp(st);
    ap_boot_info_t trampoline_boot;
    allocate_ap_trampoline(bs, &trampoline_boot);

    efi_memory_map_t memory_map;
    memory_map.descriptors = 0;
    memory_map.map_size = 0;
    memory_map.map_capacity = 0;
    memory_map.descriptor_size = 0;
    memory_map.descriptor_version = 0;
    memory_map.descriptor_count = 0;

    status = exit_boot_services(image, bs, &memory_map);
    if (status != EFI_SUCCESS) {
        if (st->ConOut) {
            st->ConOut->OutputString(st->ConOut, L"ExitBootServices failed\r\n");
        }
        halt_forever();
    }

    UINT32 bg = color(&fb, 0x05, 0x07, 0x0b);
    UINT32 fg = color(&fb, 0xf2, 0xf2, 0xf2);
    UINT32 accent = color(&fb, 0x5c, 0xd6, 0x91);
    UINT32 muted = color(&fb, 0xa8, 0xb0, 0xb8);
    UINT32 warn = color(&fb, 0xff, 0xc8, 0x5c);

    bsp_fault_display.framebuffer = fb;
    bsp_fault_display.bg = bg;
    bsp_fault_display.fg = fg;
    bsp_fault_display.accent = accent;
    bsp_fault_display.warn = warn;
    init_cpu_local(&bsp_context.cpu, 0);
    install_cpu_tables(&bsp_context.cpu);
    install_bsp_idt();

    paging_info_t paging;
    if (!setup_kernel_paging(&memory_map, &fb, &paging)) {
        draw_paging_failure(&fb, &paging, fg, warn, bg);
        halt_forever();
    }

    paging.idt_self_test_ok = run_idt_self_test() ? 1U : 0U;
    acpi_info_t acpi;
    parse_acpi(acpi_rsdp, &memory_map, &acpi);
    init_ap_contexts(&trampoline_boot, acpi.bsp_apic_id);
    ap_context_t *ap0 = ap0_context();
    UINTN ap_context_count = count_supported_ap_targets(&acpi);
    if (ap_context_count > MAX_AP_CONTEXTS) {
        ap_context_count = MAX_AP_CONTEXTS;
    }
    ap_context_t *request_target = select_ap_request_target_context(ap_context_count);
    set_ap_request_target_context(request_target);
    system_lapic_base = acpi.local_apic_base;
    zero_memory(&bsp_wait_observe, sizeof(bsp_wait_observe));
    zero_memory(&system_interrupt_observe, sizeof(system_interrupt_observe));
    if (ap_context_count == 0) {
        bring_up_one_ap(ap0, 0, &acpi, &memory_map, &paging);
    } else {
        for (UINTN i = 0; i < ap_context_count; i++) {
            bring_up_one_ap(&ap_contexts[i], i, &acpi, &memory_map, &paging);
        }
    }
    const ap_request_plan_t ap_request_plan[] = {
        {VIBE_AP_FIRST_REQUEST_OPCODE, VIBE_AP_FIRST_REQUEST_SERVICE_ID,
         VIBE_AP_FIRST_REQUEST_INTERFACE_ID, 1},
        {VIBE_AP_SECOND_REQUEST_OPCODE, VIBE_AP_SECOND_REQUEST_SERVICE_ID,
         VIBE_AP_SECOND_REQUEST_INTERFACE_ID, 2},
        {VIBE_AP_THIRD_REQUEST_OPCODE, VIBE_AP_THIRD_REQUEST_SERVICE_ID,
         VIBE_AP_THIRD_REQUEST_INTERFACE_ID, 3},
        {VIBE_AP_FOURTH_REQUEST_OPCODE, VIBE_AP_FOURTH_REQUEST_SERVICE_ID,
         VIBE_AP_FOURTH_REQUEST_INTERFACE_ID, 4},
        {VIBE_AP_SECOND_BATCH_FIRST_REQUEST_OPCODE, VIBE_AP_SECOND_BATCH_FIRST_REQUEST_SERVICE_ID,
         VIBE_AP_SECOND_BATCH_FIRST_REQUEST_INTERFACE_ID, 5},
        {VIBE_AP_SECOND_BATCH_SECOND_REQUEST_OPCODE, VIBE_AP_SECOND_BATCH_SECOND_REQUEST_SERVICE_ID,
         VIBE_AP_SECOND_BATCH_SECOND_REQUEST_INTERFACE_ID, 6},
        {VIBE_AP_SECOND_BATCH_THIRD_REQUEST_OPCODE, VIBE_AP_SECOND_BATCH_THIRD_REQUEST_SERVICE_ID,
         VIBE_AP_SECOND_BATCH_THIRD_REQUEST_INTERFACE_ID, 7},
        {VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_OPCODE, VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_SERVICE_ID,
         VIBE_AP_SECOND_BATCH_FOURTH_REQUEST_INTERFACE_ID, 8},
    };
    (void)run_ap_request_stream(request_target, ap_request_plan,
                                sizeof(ap_request_plan) / sizeof(ap_request_plan[0]));

    clear_screen(&fb, bg);
    draw_memory_map(&fb, &memory_map, &paging, &acpi, request_target,
                    ap_contexts, ap_context_count,
                    fg, muted, accent, warn, bg);
    run_fault_test_if_enabled();

    halt_forever();
    return EFI_SUCCESS;
}
