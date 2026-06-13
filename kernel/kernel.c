#include "efi.h"

typedef struct {
    UINT32 *base;
    UINT32 width;
    UINT32 height;
    UINT32 stride;
    EFI_GRAPHICS_PIXEL_FORMAT format;
} framebuffer_t;

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
    volatile UINT32 vector;
    volatile UINT32 count;
    volatile UINT64 rip;
    volatile UINT64 cs;
    volatile UINT64 rflags;
} interrupt_trace_t;

static idt_entry_t idt[256];
static UINT64 gdt[3] __attribute__((aligned(8))) = {
    0x0000000000000000ULL,
    0x00af9a000000ffffULL,
    0x00cf92000000ffffULL,
};
static interrupt_trace_t interrupt_trace;

extern void isr_breakpoint(void);
extern void isr_unhandled(void);

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
    ".global isr_unhandled\n"
    "isr_unhandled:\n"
    "    cli\n"
    "1:\n"
    "    hlt\n"
    "    jmp 1b\n");

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

static char *append_hex64(char *p, UINT64 value) {
    static const char digits[] = "0123456789ABCDEF";
    p = append_str(p, "0X");
    for (int shift = 60; shift >= 0; shift -= 4) {
        *p++ = digits[(value >> shift) & 0xf];
    }
    *p = 0;
    return p;
}

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

static void make_cpu_line(char *line, const char *label, UINT32 value) {
    char *p = line;
    p = append_str(p, label);
    p = append_dec(p, value);
}

static UINT16 read_cs(void) {
    UINT16 cs;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void install_gdt(void) {
    descriptor_table_ptr_t gdtr;
    gdtr.limit = (UINT16)(sizeof(gdt) - 1);
    gdtr.base = (UINT64)(UINTN)gdt;

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

static void set_idt_gate(UINT32 vector, UINT64 addr, UINT16 selector, UINT8 type_attr) {
    idt[vector].offset_low = (UINT16)(addr & 0xffff);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (UINT16)((addr >> 16) & 0xffff);
    idt[vector].offset_high = (UINT32)(addr >> 32);
    idt[vector].zero = 0;
}

static void install_idt(void) {
    UINT16 cs = read_cs();
    UINT64 unhandled_addr = isr_unhandled_addr();

    for (UINT32 vector = 0; vector < 256; vector++) {
        set_idt_gate(vector, unhandled_addr, cs, 0x8e);
    }
    set_idt_gate(3, isr_breakpoint_addr(), cs, 0xef);

    descriptor_table_ptr_t idtr;
    idtr.limit = (UINT16)(sizeof(idt) - 1);
    idtr.base = (UINT64)(UINTN)idt;

    __asm__ __volatile__("cli; lidt %0" : : "m"(idtr) : "memory");
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

static void halt_forever(void) {
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static EFI_STATUS exit_boot_services(EFI_HANDLE image, EFI_BOOT_SERVICES *bs) {
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
    fb.format = gop->Mode->Info->PixelFormat;

    status = exit_boot_services(image, bs);
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
    UINT32 scale = 5;
    UINT32 info_scale = 3;
    cpu_info_t cpu;
    char line[160];
    char *p;
    int first;
    int idt_ok;

    install_gdt();
    install_idt();
    idt_ok = run_idt_self_test();

    collect_cpu_info(&cpu);
    clear_screen(&fb, bg);
    draw_text(&fb, 48, 48, "hello from bare kernel", fg, bg, scale);
    draw_text(&fb, 48, 112, "pxe ok", accent, bg, scale);

    UINT32 y = 190;
    draw_line(&fb, 48, &y, "MODE NOW: X86-64 LONG MODE", accent, bg, info_scale);
    draw_line(&fb, 48, &y, "ARCHITECTURE: X86_64", fg, bg, info_scale);
    draw_line(&fb, 48, &y, "CPU OP-MODES: 32-BIT 64-BIT", fg, bg, info_scale);
    draw_line(&fb, 48, &y, "BYTE ORDER: LITTLE ENDIAN", fg, bg, info_scale);
    draw_line(&fb, 48, &y, "GDT: INSTALLED", accent, bg, info_scale);
    draw_line(&fb, 48, &y, "IDT: INSTALLED", accent, bg, info_scale);
    draw_line(&fb, 48, &y, idt_ok ? "IDT SELF-TEST: BP HANDLED" : "IDT SELF-TEST: FAILED", accent, bg, info_scale);
    draw_line(&fb, 48, &y, idt_ok ? "AFTER INT3: YES" : "AFTER INT3: NO", accent, bg, info_scale);

    p = append_str(line, "IDT VECTOR: ");
    p = append_dec(p, interrupt_trace.vector);
    p = append_str(p, "  COUNT: ");
    append_dec(p, interrupt_trace.count);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "INT3 RIP: ");
    append_hex64(p, interrupt_trace.rip);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "INT3 CS: ");
    p = append_hex64(p, interrupt_trace.cs);
    p = append_str(p, "  RFLAGS: ");
    append_hex64(p, interrupt_trace.rflags);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "VENDOR ID: ");
    append_str(p, cpu.vendor);
    draw_line(&fb, 48, &y, line, fg, bg, info_scale);

    p = append_str(line, "MODEL NAME: ");
    append_str(p, cpu.brand);
    draw_line(&fb, 48, &y, line, fg, bg, info_scale);

    make_cpu_line(line, "CPU FAMILY: ", cpu.family);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);
    make_cpu_line(line, "MODEL: ", cpu.model);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);
    make_cpu_line(line, "STEPPING: ", cpu.stepping);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "LOGICAL CPU(S): ");
    p = append_dec(p, cpu.logical_processors);
    p = append_str(p, "  CORE(S) PER PACKAGE: ");
    append_dec(p, cpu.cores_per_package);
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "ADDRESS SIZES: ");
    p = append_dec(p, cpu.physical_address_bits);
    p = append_str(p, " BITS PHYSICAL, ");
    p = append_dec(p, cpu.virtual_address_bits);
    append_str(p, " BITS VIRTUAL");
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    p = append_str(line, "FLAGS: ");
    first = 1;
    p = append_feature(p, (cpu.ext_edx & (1U << 29)) != 0, &first, "LM");
    p = append_feature(p, (cpu.leaf1_edx & (1U << 9)) != 0, &first, "APIC");
    p = append_feature(p, (cpu.leaf1_edx & (1U << 28)) != 0, &first, "HTT");
    p = append_feature(p, (cpu.leaf1_edx & (1U << 23)) != 0, &first, "MMX");
    p = append_feature(p, (cpu.leaf1_edx & (1U << 25)) != 0, &first, "SSE");
    p = append_feature(p, (cpu.leaf1_edx & (1U << 26)) != 0, &first, "SSE2");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 0)) != 0, &first, "SSE3");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 9)) != 0, &first, "SSSE3");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 19)) != 0, &first, "SSE4_1");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 20)) != 0, &first, "SSE4_2");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 21)) != 0, &first, "X2APIC");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 25)) != 0, &first, "AES");
    p = append_feature(p, (cpu.leaf1_ecx & (1U << 28)) != 0, &first, "AVX");
    p = append_feature(p, (cpu.leaf7_ebx & (1U << 5)) != 0, &first, "AVX2");
    p = append_feature(p, (cpu.leaf7_ebx & (1U << 7)) != 0, &first, "SMEP");
    p = append_feature(p, (cpu.leaf7_ebx & (1U << 20)) != 0, &first, "SMAP");
    if (first) {
        append_str(p, "NONE");
    }
    draw_line(&fb, 48, &y, line, muted, bg, info_scale);

    halt_forever();
    return EFI_SUCCESS;
}
