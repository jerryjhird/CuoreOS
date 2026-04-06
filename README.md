# CuoreOS
Hobby x86-64 ELF Kernel

### Compiling:
```bash
make -j$(nproc)
```

### Qemu example:

```bash
QEMU_FIRMWARE=PATH_TO_OVMF make run
```