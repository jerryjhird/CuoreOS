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
- cpio
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

### Other make modes:
- make clean (removes compiled files)
- make fullclean (removes compiled files and the downloaded git repos / requirements (limine))
- make buildutils (compiles src/buildutils/hash.c for linux userspace so you can generate command hashes if you wanted to expand on the command parser)

- make initramfs (rebuilds initramfs image)
- make mount (mounts the initramfs image)
- make umount (unmounts the initramfs)


### the source code of this project is released under the MIT license granting permission to use copy modify merge publish distribute sublicense and or sell copies of the software. the projects fetched externally specifcally from the makefile will follow different licenses please check and follow these before attempting to do something with them