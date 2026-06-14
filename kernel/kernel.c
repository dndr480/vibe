#include "efi.h"

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
#define AP_BOOT_STACK_SIZE (16 * 1024)
#define AP_TRAMPOLINE_PAGES 1ULL
#define AP_TRAMPOLINE_MAX_ADDRESS 0x9F000ULL
#define AP_TRAMPOLINE_PROT32_OFFSET 0x60U
#define AP_TRAMPOLINE_LONG_OFFSET 0xC0U
#define AP_TRAMPOLINE_GDTR_OFFSET 0x180U
#define AP_TRAMPOLINE_GDT_OFFSET 0x190U
#define AP_TRAMPOLINE_PROT32_SELECTOR 0x18
#define AP_ONLINE_TIMEOUT_LOOPS 100000000U

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
    UINT32 target_acpi_uid;
    UINT32 target_apic_id;
    UINT32 sipi_vector;
    UINT32 error;
    UINT32 wait_loops;
    UINT32 icr_timeouts;
} ap_boot_info_t;

typedef struct {
    volatile UINT32 vector;
    volatile UINT32 count;
    volatile UINT64 rip;
    volatile UINT64 cs;
    volatile UINT64 rflags;
} interrupt_trace_t;

typedef struct {
    UINT64 vector;
    UINT64 error_code;
    UINT64 rip;
    UINT64 cs;
    UINT64 rflags;
} fault_frame_t;

typedef struct {
    UINT64 high;
    UINT64 low;
} uuid128_t;

static idt_entry_t idt[256];
static idt_entry_t ap_idt[256];
static cpu_local_t cpu0;
static cpu_local_t cpu1;
static cpu_local_t *current_cpu = &cpu0;
static interrupt_trace_t interrupt_trace;
static framebuffer_t kernel_framebuffer;
static UINT32 kernel_bg;
static UINT32 kernel_fg;
static UINT32 kernel_accent;
static UINT32 kernel_warn;
static uuid128_t current_request_uuid;
static ap_boot_info_t ap_boot;
static UINT8 ap_boot_stack[AP_BOOT_STACK_SIZE] __attribute__((aligned(16)));

extern void isr_breakpoint(void);
extern void isr_fault_ud(void);
extern void isr_fault_df(void);
extern void isr_fault_gp(void);
extern void isr_fault_pf(void);
extern void isr_unhandled(void);
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
    "    movq %rax, interrupt_trace+8(%rip)\n"
    "    movq 16(%rsp), %rax\n"
    "    movq %rax, interrupt_trace+16(%rip)\n"
    "    movq 24(%rsp), %rax\n"
    "    movq %rax, interrupt_trace+24(%rip)\n"
    "    movl $3, interrupt_trace(%rip)\n"
    "    movl interrupt_trace+4(%rip), %eax\n"
    "    addl $1, %eax\n"
    "    movl %eax, interrupt_trace+4(%rip)\n"
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

static char *append_uuid128(char *p, uuid128_t uuid) {
    p = append_hex64(p, uuid.high);
    *p++ = '-';
    *p = 0;
    return append_hex64(p, uuid.low);
}

void fault_dispatch(fault_frame_t *frame) {
    framebuffer_t *fb = &kernel_framebuffer;
    char line[192];
    char *p;
    UINT32 y = 48;

    __asm__ __volatile__("cli" : : : "memory");

    if (!fb->base || fb->width == 0 || fb->height == 0) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }

    clear_screen(fb, kernel_bg);
    draw_text(fb, 48, y, "BSP FAULT", kernel_warn, kernel_bg, 4);
    y += 64;

    p = append_str(line, "VECTOR: ");
    p = append_dec64(p, frame->vector);
    p = append_str(p, "  ");
    p = append_str(p, fault_vector_name(frame->vector));
    p = append_str(p, "  ERROR: ");
    append_hex64(p, frame->error_code);
    draw_line(fb, 48, &y, line, kernel_fg, kernel_bg, 2);

    p = append_str(line, "RIP: ");
    p = append_hex64(p, frame->rip);
    p = append_str(p, "  CS: ");
    p = append_hex64(p, frame->cs);
    p = append_str(p, "  RFLAGS: ");
    append_hex64(p, frame->rflags);
    draw_line(fb, 48, &y, line, kernel_fg, kernel_bg, 2);

    p = append_str(line, "CR2: ");
    p = append_hex64(p, read_cr2());
    p = append_str(p, "  CR3: ");
    append_hex64(p, read_cr3());
    draw_line(fb, 48, &y, line, kernel_fg, kernel_bg, 2);

    cpu_local_t *cpu = current_cpu;
    p = append_str(line, "CPU: ");
    p = append_dec(p, cpu ? cpu->id : 0);
    p = append_str(p, "  TR: ");
    p = append_hex64(p, read_tr());
    p = append_str(p, "  LOADED TR: ");
    p = append_hex64(p, cpu ? cpu->loaded_tr : 0);
    p = append_str(p, "  TSS: ");
    append_str(p, cpu && cpu->tss_ready ? "READY" : "NOT-READY");
    draw_line(fb, 48, &y, line, kernel_fg, kernel_bg, 2);

    p = append_str(line, "IST1: ");
    p = append_hex64(p, cpu ? cpu->tss.ist[CPU_IST_FAULT - 1] : 0);
    p = append_str(p, "  IST2: ");
    append_hex64(p, cpu ? cpu->tss.ist[CPU_IST_DOUBLE_FAULT - 1] : 0);
    draw_line(fb, 48, &y, line, kernel_fg, kernel_bg, 2);

    p = append_str(line, "CURRENT REQUEST: ");
    append_uuid128(p, current_request_uuid);
    draw_line(fb, 48, &y, line, kernel_accent, kernel_bg, 2);

    draw_line(fb, 48, &y, "HALTED", kernel_warn, kernel_bg, 2);

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void ap_fault_dispatch(fault_frame_t *frame) {
    __asm__ __volatile__("cli" : : : "memory");

    ap_boot.fault_vector = frame->vector;
    ap_boot.fault_error_code = frame->error_code;
    ap_boot.fault_rip = frame->rip;
    ap_boot.fault_cs = frame->cs;
    ap_boot.fault_rflags = frame->rflags;
    ap_boot.fault_cr2 = read_cr2();
    ap_boot.entry_state = AP_ENTRY_STATE_FAULT;
    ap_boot.ap_state = AP_BOOT_STATE_FAULTED;
    __asm__ __volatile__("mfence" : : : "memory");
    ap_boot.online = 1;

    for (;;) {
        __asm__ __volatile__("hlt" : : : "memory");
    }
}

static UINT16 read_cs(void) {
    UINT16 cs;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    return cs;
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
    current_cpu = cpu;
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

static void set_idt_gate(UINT32 vector, UINT64 addr, UINT16 selector, UINT8 ist, UINT8 type_attr) {
    set_idt_gate_in(idt, vector, addr, selector, ist, type_attr);
}

static void load_idt_entries(idt_entry_t *table) {
    descriptor_table_ptr_t idtr;
    idtr.limit = (UINT16)(sizeof(idt_entry_t) * 256 - 1);
    idtr.base = (UINT64)(UINTN)table;

    __asm__ __volatile__("cli; lidt %0" : : "m"(idtr) : "memory");
}

static void load_idt_table(void) {
    load_idt_entries(idt);
}

static void store_idtr(descriptor_table_ptr_t *idtr) {
    __asm__ __volatile__("sidt %0" : "=m"(*idtr) : : "memory");
}

static void install_idt(void) {
    UINT16 cs = read_cs();
    UINT64 unhandled_addr = isr_unhandled_addr();

    for (UINT32 vector = 0; vector < 256; vector++) {
        set_idt_gate(vector, unhandled_addr, cs, 0, 0x8e);
    }
    set_idt_gate(3, isr_breakpoint_addr(), cs, 0, 0xef);
    set_idt_gate(6, isr_fault_ud_addr(), cs, CPU_IST_FAULT, 0x8e);
    set_idt_gate(8, isr_fault_df_addr(), cs, CPU_IST_DOUBLE_FAULT, 0x8e);
    set_idt_gate(13, isr_fault_gp_addr(), cs, CPU_IST_FAULT, 0x8e);
    set_idt_gate(14, isr_fault_pf_addr(), cs, CPU_IST_FAULT, 0x8e);

    load_idt_table();
}

static void init_ap_idt(void) {
    UINT64 unhandled_addr = isr_ap_unhandled_addr();

    for (UINT32 vector = 0; vector < 256; vector++) {
        set_idt_gate_in(ap_idt, vector, unhandled_addr, KERNEL_CODE_SELECTOR, 0, 0x8e);
    }
    set_idt_gate_in(ap_idt, 6, isr_ap_fault_ud_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
    set_idt_gate_in(ap_idt, 8, isr_ap_fault_df_addr(), KERNEL_CODE_SELECTOR, CPU_IST_DOUBLE_FAULT, 0x8e);
    set_idt_gate_in(ap_idt, 13, isr_ap_fault_gp_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
    set_idt_gate_in(ap_idt, 14, isr_ap_fault_pf_addr(), KERNEL_CODE_SELECTOR, CPU_IST_FAULT, 0x8e);
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

static void ap_entry_one(void) __attribute__((noreturn));
static void ap_entry_one(void) {
    descriptor_table_ptr_t gdtr;
    descriptor_table_ptr_t idtr;

    __asm__ __volatile__("cli" : : : "memory");
    ap_boot.entry_state = AP_ENTRY_STATE_C;

    init_cpu_local(&cpu1, 1);
    load_cpu_tables(&cpu1);
    init_ap_idt();
    load_idt_entries(ap_idt);

    store_gdtr(&gdtr);
    store_idtr(&idtr);

    ap_boot.ap_cs = read_cs();
    ap_boot.ap_tr = read_tr();
    ap_boot.ap_ist1 = cpu1.tss.ist[CPU_IST_FAULT - 1];
    ap_boot.ap_ist2 = cpu1.tss.ist[CPU_IST_DOUBLE_FAULT - 1];
    ap_boot.gdt_ok = (gdtr.base == (UINT64)(UINTN)cpu1.gdt &&
                      gdtr.limit == (UINT16)(sizeof(cpu1.gdt) - 1)) ? 1U : 0U;
    ap_boot.tss_ok = (ap_boot.ap_tr == KERNEL_TSS_SELECTOR && cpu1.tss_ready &&
                      ap_boot.ap_ist1 != 0 && ap_boot.ap_ist2 != 0) ? 1U : 0U;
    ap_boot.idt_ok = (idtr.base == (UINT64)(UINTN)ap_idt &&
                      idtr.limit == (UINT16)(sizeof(ap_idt) - 1)) ? 1U : 0U;
    ap_boot.entry_state = AP_ENTRY_STATE_TABLES;
    run_ap_fault_test_if_enabled();
    ap_boot.ap_state = AP_BOOT_STATE_HALTED;
    ap_boot.entry_state = AP_ENTRY_STATE_HALTED;
    __asm__ __volatile__("mfence" : : : "memory");
    ap_boot.online = 1;

    for (;;) {
        __asm__ __volatile__("cli; hlt" : : : "memory");
    }
}

static int run_idt_self_test(void) {
    interrupt_trace.vector = 0;
    interrupt_trace.count = 0;
    interrupt_trace.rip = 0;
    interrupt_trace.cs = 0;
    interrupt_trace.rflags = 0;

    __asm__ __volatile__("int3" : : : "memory");

    return interrupt_trace.vector == 3 && interrupt_trace.count == 1;
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

static int select_ap_target(acpi_info_t *acpi, ap_boot_info_t *boot) {
    if (acpi->error != ACPI_OK) {
        return 0;
    }

    for (UINT32 i = 0; i < acpi->stored_cpu_count; i++) {
        acpi_cpu_t *cpu = &acpi->cpus[i];
        if (!cpu->x2apic && cpu->apic_id != acpi->bsp_apic_id && cpu->apic_id < 256) {
            boot->target_acpi_uid = cpu->acpi_uid;
            boot->target_apic_id = cpu->apic_id;
            return 1;
        }
    }

    return 0;
}

static int build_ap_trampoline(ap_boot_info_t *boot, paging_info_t *paging) {
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
    boot->stack_top = (UINT64)(UINTN)(ap_boot_stack + sizeof(ap_boot_stack));

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
    if ((svr & (1U << 8)) == 0) {
        lapic_write32(base, 0x0f0, (svr & 0xffffff00U) | 0x1ffU);
    }
}

static void bring_up_one_ap(ap_boot_info_t *boot, acpi_info_t *acpi, efi_memory_map_t *map, paging_info_t *paging) {
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
    boot->wait_loops = 0;
    boot->icr_timeouts = 0;

    if (boot->error != AP_BOOT_OK) {
        return;
    }
    if (!select_ap_target(acpi, boot)) {
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
    if (!build_ap_trampoline(boot, paging)) {
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
    default: return "UNKNOWN";
    }
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
    }
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

static void draw_memory_map(framebuffer_t *fb, efi_memory_map_t *map, paging_info_t *paging,
                            acpi_info_t *acpi, ap_boot_info_t *ap, UINT32 fg, UINT32 muted,
                            UINT32 accent, UINT32 warn, UINT32 bg) {
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

    if (ap) {
        draw_ap_boot_info(fb, &y, ap, fg, accent, warn, bg);
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
    allocate_ap_trampoline(bs, &ap_boot);

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

    kernel_framebuffer = fb;
    kernel_bg = bg;
    kernel_fg = fg;
    kernel_accent = accent;
    kernel_warn = warn;
    current_request_uuid.high = 0;
    current_request_uuid.low = 0;

    init_cpu_local(&cpu0, 0);
    install_cpu_tables(&cpu0);
    install_idt();

    paging_info_t paging;
    if (!setup_kernel_paging(&memory_map, &fb, &paging)) {
        draw_paging_failure(&fb, &paging, fg, warn, bg);
        halt_forever();
    }

    paging.idt_self_test_ok = run_idt_self_test() ? 1U : 0U;
    acpi_info_t acpi;
    parse_acpi(acpi_rsdp, &memory_map, &acpi);
    bring_up_one_ap(&ap_boot, &acpi, &memory_map, &paging);

    clear_screen(&fb, bg);
    draw_memory_map(&fb, &memory_map, &paging, &acpi, &ap_boot, fg, muted, accent, warn, bg);
    run_fault_test_if_enabled();

    halt_forever();
    return EFI_SUCCESS;
}
