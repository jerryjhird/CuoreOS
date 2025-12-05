.PHONY: main uefi run clean

DL = build/downloads

ENABLED_WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wunused-macros
3PS_ENABLED_WARNINGS = -Wall -Wextra

COMMON_INCLUDES = -Iinclude -Iinclude/libc -I$(DL)/Flanterm/src/
COMMON_CFLAGS = -ffreestanding -mcmodel=large -nostdinc -fno-pie -mno-red-zone -std=c99 $(ENABLED_WARNINGS) $(COMMON_INCLUDES)

FLANTERM_CFLAGS = -ffreestanding -nostdinc -fno-pie -mno-red-zone $(3PS_ENABLED_WARNINGS) -mcmodel=large -Iinclude -Iinclude/libc -I$(DL)/Flanterm/src/

main: build/kernel.elf uefi

build/kernel.elf: src/kernel/kentry.c
	mkdir -p build $(DL)/Flanterm $(DL)/Limine

	@test -d $(DL)/Flanterm/.git || git clone https://codeberg.org/Mintsuki/Flanterm.git $(DL)/Flanterm 
	@test -d $(DL)/Limine/.git || git clone --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git $(DL)/Limine

	gcc $(FLANTERM_CFLAGS) -c $(DL)/Flanterm/src/flanterm.c -o build/flanterm_core.o
	gcc $(FLANTERM_CFLAGS) -c $(DL)/Flanterm/src/flanterm_backends/fb.c -o build/flanterm_fb.o
	ar rcs build/libflanterm.a build/flanterm_core.o build/flanterm_fb.o

	gcc $(COMMON_CFLAGS) -c src/libc/mem.c    -o build/libc_mem.o
	gcc $(COMMON_CFLAGS) -c src/libc/string.c -o build/libc_string.o
	gcc $(COMMON_CFLAGS) -c src/libc/stdio.c  -o build/libc_stdio.o
	gcc $(COMMON_CFLAGS) -c src/other/x86.c  -o build/other_x86.o

	ld -r -o build/libc.o \
		build/libc_mem.o \
	    build/libc_string.o \
	    build/libc_stdio.o \
		build/other_x86.o

	gcc $(COMMON_CFLAGS) -c src/kernel/kentry.c -o build/kernel_kentry.o
	gcc $(COMMON_CFLAGS) -c src/kernel/tests.c  -o build/kernel_tests.o
	gcc $(COMMON_CFLAGS) -c src/other/limineabs.c  -o build/other_limineabs.o
	gcc $(COMMON_CFLAGS) -c src/drivers/ps2.c  -o build/drivers_ps2.o
	gcc $(COMMON_CFLAGS) -c src/drivers/serial.c  -o build/drivers_serial.o

	ld -r -o build/kernel.o \
		build/kernel_kentry.o \
	    build/kernel_tests.o \
		build/other_limineabs.o \
		build/drivers_ps2.o \
		build/drivers_serial.o

	ld -nostdlib -T src/kernel.ld build/kernel.o build/libc.o build/libflanterm.a -o build/kernel.elf

uefi: build/uefi.img

build/uefi.img: build/kernel.elf limine.conf
	mkdir -p build
	dd if=/dev/zero of=build/uefi.img bs=1M count=64
	parted -s build/uefi.img mklabel gpt
	parted -s build/uefi.img mkpart ESP fat32 1MiB 100%
	parted -s build/uefi.img set 1 esp on

	{ \
	LOOP=$$(sudo losetup -Pf --show build/uefi.img); \
	sudo mkfs.vfat -F 32 $${LOOP}p1; \
	mkdir -p build/mnt; \
	sudo mount $${LOOP}p1 build/mnt; \
	sudo mkdir -p build/mnt/EFI/BOOT; \
	sudo cp $(DL)/Limine/BOOTX64.EFI build/mnt/EFI/BOOT/; \
	sudo mkdir -p build/mnt/boot; \
	sudo cp build/kernel.elf build/mnt/boot/; \
	sudo cp limine.conf build/mnt/limine.conf; \
	$(MAKE) initramfs; \
	sudo cp build/initramfs_cpio.img build/mnt/initramfs; \
	sudo umount build/mnt; \
	sudo losetup -d $${LOOP}; \
	}

initramfs:
	mkdir -p build/initramfs
	echo "hello world" > build/initramfs/hworld.txt
	find build/initramfs -mindepth 1 -print0 \
		| cpio --null -ov --format=newc \
		> build/initramfs_cpio.img

run:
	qemu-system-x86_64 \
	    -bios /usr/share/OVMF/OVMF_CODE.fd \
    	-drive if=virtio,format=raw,file=build/uefi.img \
    	-serial stdio \
    	-m 512M

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