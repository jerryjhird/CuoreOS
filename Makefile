# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at 
# https://mozilla.org/MPL/2.0/.

DL = build/downloads
OVMF_PATH = /usr/share/OVMF/OVMF_CODE.fd

LIMINE_URL = --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git
HOST_ARCH := $(shell uname -m)

QEMU_CPU ?= max
QEMU_MEM ?= -m 4G
IMAGE_FORMAT ?= disk

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros
COMMON_INCLUDES = -Iinclude
COMMON_CFLAGS = -mno-red-zone -ffreestanding -mcmodel=large -nostdinc -fno-pie $(ENABLED_WARNINGS) $(COMMON_INCLUDES)

KERNEL_SRC_FILES := \
    src/kernel/kmem.c \
    src/klibc/string.c \
    src/klibc/stdio.c \
    src/kernel/x86.c \
    src/kernel/cpir.c \
    src/kernel/kentry.c \
    src/kernel/ktests.c \
    src/kernel/bmp_render.c \
    src/kernel/ps2.c \
    src/kernel/serial.c

# clone repo($1) at specific commit($2) into folder($3)
define git_clone
	( \
		mkdir -p $2; \
		test -d $2/.git || git clone $1 $2; \
	)
endef

OBJ_FILES := $(patsubst src/%.c,build/%.o,$(KERNEL_SRC_FILES))

ifeq ($(HOST_ARCH),x86_64)
	MAKE_CC := gcc
	MAKE_LD := ld
else
	MAKE_CC := x86_64-elf-gcc
	MAKE_LD := x86_64-elf-ld
endif

.PHONY: main build/bootimg-disk.img clean fullclean run run-kvm buildutils cloc cc_commands deps
main: build/bootimg-$(IMAGE_FORMAT).img
all: clean main buildutils

deps:
	mkdir -p $(DL)/Limine
	$(call git_clone,$(LIMINE_URL),$(DL)/Limine)

build/%.o: src/%.c deps
	@mkdir -p $(dir $@)
	$(MAKE_CC) $(COMMON_CFLAGS) -c $< -o $@

# link kernel into kernel.elf
build/kernel.elf: $(OBJ_FILES)
	$(MAKE_LD) -T src/kernel.ld -o build/kernel.elf $(OBJ_FILES)

# fat16 uefi limine disk image
build/bootimg-disk.img: build/kernel.elf build/initramfs.img
	dd if=/dev/zero of=build/bootimg-disk.img bs=1M count=16

	parted -s build/bootimg-disk.img mklabel gpt
	parted -s build/bootimg-disk.img mkpart ESP fat16 1MiB 6MiB
	parted -s build/bootimg-disk.img set 1 esp on

	mformat -i build/bootimg-disk.img ::/

	mmd -i build/bootimg-disk.img ::EFI
	mmd -i build/bootimg-disk.img ::EFI/BOOT
	mcopy -i build/bootimg-disk.img $(DL)/Limine/BOOTX64.EFI ::EFI/BOOT/
	mmd -i build/bootimg-disk.img ::boot
	mcopy -i build/bootimg-disk.img build/kernel.elf ::
	mcopy -i build/bootimg-disk.img src/limine.conf ::

	mcopy -i build/bootimg-disk.img build/initramfs.img ::

# put initramfs together
build/initramfs.img:
	mkdir -p build/initramfs-dump
	echo "hello world" > build/initramfs-dump/hworld.txt
	cp resources/panic.bmp build/initramfs-dump/

	rm -f build/archiver build/initramfs.img
	$(MAKE_CC) src/buildutils/archiver.c -o build/archiver -Wall -Wextra -O2

	build/archiver build/initramfs-dump > build/initramfs.img

# run qemu with 
# $(QEMU_CPU) default: max
# $(QEMU_MEM) default: -m 4G
# $(OVMF_PATH) Firmware (path may need adjusting based on system or just make sure its installed)
run:
	qemu-system-x86_64 \
		$(if $(QEMU_CPU),-cpu $(QEMU_CPU)) \
		$(QEMU_MEM) \
	    -bios $(OVMF_PATH) \
	    -drive if=virtio,format=raw,file=build/bootimg-disk.img \
	    -serial stdio \

# run qemu with 
# Host CPU using KVM (some pc's dont support kvm)
# $(QEMU_MEM) default: -m 4G
# $(OVMF_PATH) Firmware (path may need adjusting based on system or just make sure its installed)
# fallback to run: if kvm / qemu fails
run-kvm:
	@qemu-system-x86_64 \
		$(QEMU_MEM) \
		-enable-kvm \
		-cpu host \
		-bios $(OVMF_PATH) \
		-drive if=virtio,format=raw,file=build/bootimg-disk.img \
		-serial stdio || $(MAKE) run-max

buildutils:
	gcc -O2 -o build/hashcalc src/buildutils/hash.c

# remove build files
clean:
	rm -rf build/kernel
	rm -rf build/klibc
	rm -rf build/initramfs-dump

# removes downloaded files as well
fullclean:
	rm -rf $(DL) build

# loc counter (may need to be installed)
cloc:
	cloc . --exclude-list-file=exclude.cloc

# generate compile_commands.json for clangd
cc_commands:
	bear -- make all