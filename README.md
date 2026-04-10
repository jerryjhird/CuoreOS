# CuoreOS
Hobby x86-64 ELF Kernel

### Configuring
```
make menuconfig
```
note: if its your first time configuring it will fill out the config for you with the default values

### Compiling: (-Otarget isnt required)
```bash
make -j$(nproc) -Otarget
```

### Qemu example:

```bash
QEMU_FIRMWARE=PATH_TO_OVMF make run
```
