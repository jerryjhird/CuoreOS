.PHONY: main uefi run clean

DL = build/downloads
OVMF_PATH = /usr/share/OVMF/OVMF_CODE.fd

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros
3PS_ENABLED_WARNINGS = -Wall -Wextra
COMMON_INCLUDES = -Iinclude -Iinclude/libc -I$(DL)/Cuoreterm/src
COMMON_CFLAGS = -mno-red-zone -ffreestanding -mcmodel=large -nostdinc -fno-pie -std=c99 $(ENABLED_WARNINGS) $(COMMON_INCLUDES)

LIMINE_URL = --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git
CUORETERM_URL = https://codeberg.org/jerryjhird/Cuoreterm.git

HOST_ARCH := $(shell uname -m)
QEMU_CPU ?= max
QEMU_MEM ?= -m 4G

ifeq ($(HOST_ARCH),x86_64)
	MAKE_CC := gcc
	MAKE_LD := ld
else
	MAKE_CC := x86_64-elf-gcc
	MAKE_LD := x86_64-elf-ld
endif

# clone repo($1) at specific commit($2) into folder($3)
define git_clone
	( \
		mkdir -p $2; \
		test -d $2/.git || git clone $1 $2; \
	)
endef

main: build/kernel.elf uefi

build/kernel.elf: src/kernel/kentry.c
	mkdir -p build $(DL)/Limine $(DL)/Cuoreterm

	$(call git_clone,$(LIMINE_URL),$(DL)/Limine)
	$(call git_clone,$(CUORETERM_URL),$(DL)/Cuoreterm)
	
	$(MAKE_CC) $(COMMON_CFLAGS) -c $(DL)/Cuoreterm/src/term.c -o build/cuoreterm.o -I$(DL)/Cuoreterm/src

	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/memory.c -o build/libc_mem.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/string.c -o build/libc_string.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/libc/stdio.c -o build/libc_stdio.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/arch/x86.c -o build/other_x86.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/other/cpio_newc.c -o build/other_cpio_newc.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/kentry.c -o build/kernel_kentry.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/tests.c -o build/kernel_tests.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/pma.c -o build/kernel_pma.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/ps2.c -o build/drivers_ps2.o
	$(MAKE_CC) $(COMMON_CFLAGS) -c src/kernel/serial.c -o build/drivers_serial.o

	$(MAKE_LD) -r -o build/libc.o \
		build/libc_mem.o \
	    build/libc_string.o \
	    build/libc_stdio.o \
		build/other_x86.o \
		build/other_cpio_newc.o

	$(MAKE_LD) -r -o build/kernel.o \
		build/kernel_kentry.o \
	    build/kernel_tests.o \
		build/kernel_pma.o \
		build/drivers_ps2.o \
		build/drivers_serial.o 

	$(MAKE_LD) -nostdlib -T src/kernel.ld build/kernel.o build/libc.o build/cuoreterm.o -o build/kernel.elf

uefi: build/uefi.img

build/uefi.img: build/kernel.elf limine.conf
	mkdir -p build

	dd if=/dev/zero of=build/uefi.img bs=1M count=64

	parted -s build/uefi.img mklabel gpt
	parted -s build/uefi.img mkpart ESP fat32 1MiB 100%
	parted -s build/uefi.img set 1 esp on

	mformat -i build/uefi.img ::/

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
	cd build/initramfs && find . -type f -print0 \
		| cpio --null -ov --format=newc \
		> ../initramfs.img

run:
	qemu-system-x86_64 $(if $(QEMU_CPU),-cpu $(QEMU_CPU)) \
		$(QEMU_MEM) \
	    -bios $(OVMF_PATH) \
	    -drive if=virtio,format=raw,file=build/uefi.img \
	    -serial stdio \

run-kvm:
	qemu-system-x86_64 \
		$(QEMU_MEM) \
	 	-enable-kvm \
	    -cpu host \
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