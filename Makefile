.PHONY: main uefi run clean

DL = build/downloads
COMMON_INCLUDES = -Iinclude -I$(DL)/Flanterm/src/
COMMON_CFLAGS = -ffreestanding -fno-pie -mno-red-zone -Wall -Wextra $(COMMON_INCLUDES)

LIBC_SOURCES = src/libc/mem.c

main: build/kernel.elf

build/kernel.elf: src/kernel.c
	mkdir -p build $(DL)/Flanterm $(DL)/Limine

	@test -d $(DL)/Flanterm/.git || git clone https://codeberg.org/Mintsuki/Flanterm.git $(DL)/Flanterm 
	@test -d $(DL)/Limine/.git || git clone --branch=v10.x-binary --depth=1 https://codeberg.org/Limine/Limine.git $(DL)/Limine

	gcc $(COMMON_CFLAGS) -mcmodel=large -fPIC -c $(DL)/Flanterm/src/flanterm.c -o build/flanterm_core.o
	gcc $(COMMON_CFLAGS) -mcmodel=large -fPIC -c $(DL)/Flanterm/src/flanterm_backends/fb.c -o build/flanterm_fb.o
	ar rcs build/libflanterm.a build/flanterm_core.o build/flanterm_fb.o

	gcc $(COMMON_CFLAGS) -c $(LIBC_SOURCES) -o build/libc.o

	gcc $(COMMON_CFLAGS) -mcmodel=large -c src/kernel.c -o build/kernel.o
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
	sudo umount build/mnt; \
	sudo losetup -d $${LOOP}; \
	}


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

clean:
	rm build/* 2>/dev/null || true

fullclean:
	rm -rf $(DL) build
