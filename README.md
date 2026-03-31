# CuoreOS
Hobby x86-64 ELF Kernel

### Compiling:
```bash
make
```

### Qemu example:

(replace the value of QEMU_FIRMWARE with your own OVMF firmware)
```bash
QEMU_FIRMWARE=/usr/share/OVMF/OVMF_CODE.fd make run
```