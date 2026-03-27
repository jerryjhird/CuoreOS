# CuoreOS
Hobby x86-64 ELF Kernel

### Compiling:
```bash
meson setup build
meson compile -C build
```

### Qemu example:

(replace /usr/share/OVMF/OVMF_CODE.fd with your own OVMF/UEFI firmware):
```bash
qemu-system-x86_64 -bios /usr/share/OVMF/OVMF_CODE.fd -cdrom build/Cuore.x86-64.iso -m 256M -serial stdio
```