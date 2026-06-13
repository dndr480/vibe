# vibe

## Kernel Address Space

The kernel intentionally does not follow the conventional x86-64 split where
userspace owns the low canonical half and the kernel lives in the high canonical
half.

For this kernel, the design direction is:

- Low canonical half: reserved for the kernel.
- Kernel mappings: keep physical memory identity-mapped.
- High canonical half: reserved for future userspace mappings.

For 48-bit canonical addressing, that means:

```text
kernel: 0x0000000000000000 - 0x00007FFFFFFFFFFF
hole:   0x0000800000000000 - 0xFFFF7FFFFFFFFFFF
user:   0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
```

This is a deliberate design constraint, not a placeholder. Future paging,
allocator, syscall, ELF loading, and userspace work should reason from this
layout instead of copying the mainstream higher-half-kernel model.

Initial paging uses 4 KiB pages only. The low canonical half is kernel-owned
and identity-mapped where mappings exist, but the kernel must not prebuild page
tables for the entire 128 TiB low half. It should materialize only the physical
ranges it currently needs, using 2 MiB or 1 GiB large pages only as future
optimizations.
