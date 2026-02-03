# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at 
# https://mozilla.org/MPL/2.0/.

DL = .cache/downloads
OVMF_PATH = /usr/share/OVMF/OVMF_CODE.fd

LIMINE_URL = --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git
FLANTERM_URL = --depth=1 https://codeberg.org/Mintsuki/Flanterm.git
HOST_ARCH := $(shell uname -m)

QEMU_CPU ?= max
QEMU_MEM ?= -m 4G
IMAGE_FORMAT ?= disk

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros
KERNEL_INCLUDES = -Iinclude/kernel -Iinclude/ -I$(DL)/Flanterm/src
KERNEL_CFLAGS = -mno-red-zone -ffreestanding -mcmodel=large -fno-pie $(ENABLED_WARNINGS) $(KERNEL_INCLUDES)

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros

# userland includes (probably none unless you have headers)
USER_INCLUDES = -Iinclude/userspace
USER_CFLAGS = -mno-red-zone -ffreestanding -mcmodel=large -fno-pie $(USER_INCLUDES) $(ENABLED_WARNINGS) -fno-stack-protector -fno-builtin

OBJ_FILES := \
    build/kernel/kmem.o \
    build/kernel/string.o \
    build/kernel/stdio.o \
    build/kernel/x86.o \
    build/kernel/cpir.o \
    build/kernel/kentry.o \
    build/kernel/ktests.o \
    build/kernel/serial.o \
	build/kernel/paging.o \
    build/kernel/flanterm.o \
    build/kernel/fb.o \
	# build/kernel/ps2.o (not being used currently)

VPATH := src/kernel $(DL)/Flanterm/src $(DL)/Flanterm/src/flanterm_backends

# clone repo($1) at specific commit($2) into folder($3)
define git_clone
	( \
		mkdir -p $2; \
		test -d $2/.git || git clone $1 $2; \
	)
endef


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
	mkdir -p $(DL)/Limine $(DL)/Flanterm
	$(call git_clone,$(LIMINE_URL),$(DL)/Limine)
	$(call git_clone,$(FLANTERM_URL),$(DL)/Flanterm)

build/kernel/%.o: %.c deps
	@mkdir -p $(dir $@)
	$(MAKE_CC) $(KERNEL_CFLAGS) -c $< -o $@

build/kernel/flanterm.o: KERNEL_CFLAGS += -w
build/kernel/fb.o:       KERNEL_CFLAGS += -w

build/userspace/init: src/userspace/init.c
	mkdir -p build/userspace
	$(MAKE_CC) $(USER_CFLAGS) -c $< -o build/userspace/init.o
	ld -T src/userspace/userspace.ld -nostdlib -o build/userspace/init.elf build/userspace/init.o
	objcopy -O binary build/userspace/init.elf build/userspace/init

# link kernel into kernel.elf
build/kernel.elf: $(OBJ_FILES)
	$(MAKE_LD) -T src/kernel/kernel.ld -o build/kernel.elf $(OBJ_FILES)

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
build/initramfs.img: build/userspace/init
	rm -rf build/initramfs-dump
	mkdir -p build/initramfs-dump

	echo "hello world" > build/initramfs-dump/hworld.txt
	cp build/userspace/init build/initramfs-dump/

	rm -f build/archiver build/initramfs.img
	$(MAKE_CC) src/buildutils/archiver.c -o build/archiver -Wall -Wextra -O2

	build/archiver build/initramfs-dump build/initramfs.img

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

# remove build files
clean:
	rm -rf build

# removes downloaded files / cache as well
fullclean: clean
	rm -rf .cache compile_commands.json

# loc counter (may need to be installed)
cloc:
	cloc . --exclude-list-file=exclude.cloc

# generate compile_commands.json for clangd
cc_commands:
	bear -- make all