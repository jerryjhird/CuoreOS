# CuoreOS
---
Hobby x86-64 ELF Kernel

[How to Contribute](#contributing)

[How to Build](#how-to-build)

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
make configsync
```
note: everytime you change the config.toml file re-run this command

### Compiling: (-Otarget isnt required)
```bash
make -j$(nproc) -Otarget
```

### Qemu example:
uefi:
```bash
QEMU_FIRMWARE=PATH_TO_OVMF make runu
```
bios:
```bash
make run
```
