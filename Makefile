.PHONY: main uefi run clean

DL = build/downloads
OVMF_PATH = /usr/share/OVMF/OVMF_CODE.fd

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros
3PS_ENABLED_WARNINGS = -Wall -Wextra

COMMON_INCLUDES = -Iinclude -Iinclude/libc -I$(DL)/Flanterm/src/
COMMON_CFLAGS = -ffreestanding -mcmodel=large -nostdinc -fno-pie -mno-red-zone -std=c99 $(ENABLED_WARNINGS) $(COMMON_INCLUDES)
FLANTERM_CFLAGS = -ffreestanding -nostdinc -fno-pie -mno-red-zone $(3PS_ENABLED_WARNINGS) -mcmodel=large -Iinclude -Iinclude/libc -I$(DL)/Flanterm/src/

HOST_ARCH := $(shell uname -m)

ifeq ($(HOST_ARCH),x86_64)
    MAKE_CC := gcc
	MAKE_LD := ld
else
    MAKE_CC := x86_64-elf-gcc
	MAKE_LD := x86_64-elf-ld
endif

main: build/kernel.elf uefi

build/kernel.elf: src/kernel/kentry.c
	mkdir -p build $(DL)/Flanterm $(DL)/Limine

	@test -d $(DL)/Flanterm/.git || git clone https://codeberg.org/Mintsuki/Flanterm.git $(DL)/Flanterm 
	@test -d $(DL)/Limine/.git || git clone --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git $(DL)/Limine

	$(MAKE_CC) $(FLANTERM_CFLAGS) -c $(DL)/Flanterm/src/flanterm.c -o build/flanterm_core.o
	$(MAKE_CC) $(FLANTERM_CFLAGS) -c $(DL)/Flanterm/src/flanterm_backends/fb.c -o build/flanterm_fb.o
	ar rcs build/libflanterm.a build/flanterm_core.o build/flanterm_fb.o

	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/mem.c    -o build/libc_mem.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/string.c -o build/libc_string.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/stdio.c  -o build/libc_stdio.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/other/x86.c  -o build/other_x86.o

	$(MAKE_LD) -r -o build/libc.o \
		build/libc_mem.o \
	    build/libc_string.o \
	    build/libc_stdio.o \
		build/other_x86.o

	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/kentry.c -o build/kernel_kentry.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/tests.c  -o build/kernel_tests.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/other/limineabs.c  -o build/other_limineabs.o

	$(MAKE_CC) $(COMMON_CFLAGS) -c src/drivers/ps2.c  -o build/drivers_ps2.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/drivers/serial.c  -o build/drivers_serial.o

	$(MAKE_LD) -r -o build/kernel.o \
		build/kernel_kentry.o \
	    build/kernel_tests.o \
		build/other_limineabs.o \
		build/drivers_ps2.o \
		build/drivers_serial.o

	$(MAKE_LD) -nostdlib -T src/kernel.ld build/kernel.o build/libc.o build/libflanterm.a -o build/kernel.elf

uefi: build/uefi.img

build/uefi.img: build/kernel.elf limine.conf
	mkdir -p build

	dd if=/dev/zero of=build/uefi.img bs=1M count=64

	parted -s build/uefi.img mklabel gpt
	parted -s build/uefi.img mkpart ESP fat32 1MiB 100%
	parted -s build/uefi.img set 1 esp on

	mkfs.vfat -F 32 -n EFI build/uefi.img

	mmd -i build/uefi.img ::EFI
	mmd -i build/uefi.img ::EFI/BOOT
	mcopy -i build/uefi.img $(DL)/Limine/BOOTX64.EFI ::EFI/BOOT/
	mmd -i build/uefi.img ::boot
	mcopy -i build/uefi.img build/kernel.elf ::boot/
	mcopy -i build/uefi.img limine.conf ::

	$(MAKE) initramfs
	mcopy -i build/uefi.img build/initramfs.img ::

initramfs:
	mkdir -p build/initramfs
	echo "hello world" > build/initramfs/hworld.txt
	find build/initramfs -mindepth 1 -print0 \
		| cpio --null -ov --format=newc \
		> build/initramfs.img
	
run:
	qemu-system-x86_64 \
	    -bios $(OVMF_PATH) \
    	-drive if=virtio,format=raw,file=build/uefi.img \
    	-serial stdio \

mount:
	sudo losetup -Pf --show build/uefi.img > build/loopdev
	sudo mkdir -p build/mnt
	sudo mount $$(cat build/loopdev)p1 build/mnt

umount:
	if mountpoint -q build/mnt; then sudo umount build/mnt; fi
	if [ -f build/loopdev ]; then sudo losetup -d $$(cat build/loopdev); fi

buildutils:
	gcc -O2 -o build/hashcalc src/buildutils/hash.c

clean:
	rm build/* 2>/dev/null || true

fullclean:
	rm -rf $(DL) build