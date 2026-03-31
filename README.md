# CuoreOS
Hobby x86-64 ELF Kernel

### Compiling:
```bash
meson setup build
meson compile -C build
```

### Qemu example:

(replace the value of QEMU_FIRMWARE in the `./runqemu` script with your own OVMF/UEFI firmware if the default is wrong (it probably is))
```bash
./runqemu
```