# CuoreOS
Hobby x86-64 ELF Kernel

## Build Reqiurements ("make")
- GNU Cross Compiler (GCC) (>= 10)
- GNU linker (ld)
- GNU find
- Make
- mtools
- Git
- ar
- DD
- Parted
- echo 
- test
- 
- losetup (for mounting/unmounting (not required for building or running))
- Mount (for mounting/unmounting)
- umount (for mounting/unmounting)
- Sudo (for mounting/unmounting)

## Requirements ("make run")
- qemu-system-x86_64
- OVMF UEFI firmware

### other make recipes:
- clean (removes compiled files)

- fullclean (removes compiled files and the downloaded git repos / requirements (limine,cuoreterm,etc))

- buildutils (sets up helper scripts in src/buildutils for the developer (not a runtime, buildtime etc dependency))

### This Repo and all of its code is subject to the terms of the Mozilla Public License, v. 2.0. the license can be found in the {Project root}/LICENSE or at https://mozilla.org/MPL/2.0/