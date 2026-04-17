# CuoreOS
---
Hobby x86-64 ELF Kernel

[How to Contribute](#contributing)

[How to Build](#how-to-build)

---
## Project Information

### Features in Development
- binary loading
- offloading tasks to cores other than the BSP
- core specific log buffers

### Planned Features
- userspace
- multi-core scheduling
- serial terminal (serial input is already functional so this will be easy to implement i just need to get around to it)

---
## Contributing
- All contributions are welcome. Before committing, please run make format to keep the code clean. PRs that dont follow the style guide may be asked to reformat

- contributions should be sent via github or codeberg unless you arrange something with me personally

### Contributing (LICENSING)
By contributing code to this project, you agree to the following: 

1. You grant the project maintainer a non-exclusive, perpetual, irrevocable, royalty-free, worldwide license to use, modify, and distribute your contributions.

2. You agree that the project maintainer reserves the right to relicense the project (including your contributions) under any Open Source Initiative (OSI) approved license or the UNLICENSE in the future without additional notice.
---
## How to Build

### Configuring
```
make menuconfig
```
note: If a config dosent exist it will fill out the config with the default values so you can immediately save and exit and have a functional config

### Compiling: (-Otarget isnt required)
```bash
make -j$(nproc) -Otarget
```

### Qemu example:
uefi (Recommended):
```bash
QEMU_FIRMWARE=PATH_TO_OVMF make run
```
bios:
```bash
make bios-run
```
