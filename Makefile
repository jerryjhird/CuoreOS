CC      := gcc
HOST_CC := gcc

SRCDIR   := src
BUILDDIR := build
TOOLS    := tools

C_SRCS := $(shell find $(SRCDIR) -name "*.c")
ASM_SRCS := $(shell find $(SRCDIR) -name "*.S")
TOOL_SRCS := $(wildcard $(TOOLS)/*.c)
OBJS := $(C_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o) $(ASM_SRCS:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
DEPS := $(OBJS:.o=.d)

KERNEL_ELF := $(BUILDDIR)/kernel.elf
BOOT_ISO := $(BUILDDIR)/Cuore.x86-64.iso
CONFIG_BIN := $(BUILDDIR)/cuore.conf.bin
ISO_ROOT := $(BUILDDIR)/iso_root
LIMINE_DIR := $(BUILDDIR)/limine
INITRD := $(BUILDDIR)/initrd.img

CFLAGS := -std=c11 -O0 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-lto -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-bmi -mno-bmi2 -I$(SRCDIR) -I$(BUILDDIR)/flanterm -I. -Wall -Wextra -MMD -MP
LDFLAGS := -T kernel.ld -nostdlib -static -z max-page-size=0x1000

DISK_IMG := $(BUILDDIR)/qemu-disk.img
QEMU_FIRMWARE ?= /usr/share/OVMF/OVMF_CODE.fd

.PHONY: all clean run style format compile_commands

all:
	@$(MAKE) deps_setup
	@$(MAKE) $(OBJS)
	@$(MAKE) $(KERNEL_ELF)
	@$(MAKE) compile_commands
	@$(MAKE) $(DISK_IMG)
	@$(MAKE) $(INITRD)
	@$(MAKE) $(BOOT_ISO)

compile_commands:
	@echo "Generating compile_commands.json"
	@echo "[" > compile_commands.json

	@for src in $(C_SRCS); do \
		echo "  {" >> compile_commands.json; \
		echo "    \"directory\": \"$(CURDIR)\"," >> compile_commands.json; \
		echo "    \"command\": \"$(CC) $(CFLAGS) -c $$src\"," >> compile_commands.json; \
		echo "    \"file\": \"$$src\"" >> compile_commands.json; \
		echo "  }," >> compile_commands.json; \
	done

	@for src in $(ASM_SRCS); do \
		echo "  {" >> compile_commands.json; \
		echo "    \"directory\": \"$(CURDIR)\"," >> compile_commands.json; \
		echo "    \"command\": \"$(CC) $(CFLAGS) -c $$src\"," >> compile_commands.json; \
		echo "    \"file\": \"$$src\"" >> compile_commands.json; \
		echo "  }," >> compile_commands.json; \
	done

	@for src in $(TOOL_SRCS); do \
		echo "  {" >> compile_commands.json; \
		echo "    \"directory\": \"$(CURDIR)\"," >> compile_commands.json; \
		echo "    \"command\": \"$(HOST_CC) -std=c11 -I. -c $$src\"," >> compile_commands.json; \
		echo "    \"file\": \"$$src\"" >> compile_commands.json; \
		echo "  }," >> compile_commands.json; \
	done

	@sed -i '$$d' compile_commands.json
	@echo "  }" >> compile_commands.json
	@echo "]" >> compile_commands.json

deps_setup:
	@mkdir -p $(BUILDDIR)/flanterm $(LIMINE_DIR)
	@if [ ! -d "$(BUILDDIR)/flanterm/.git" ]; then \
		git clone --depth=1 https://codeberg.org/Mintsuki/Flanterm.git $(BUILDDIR)/flanterm; \
	fi
	@if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
		git clone --depth=1 --branch v10.x-binary https://codeberg.org/Limine/Limine.git $(LIMINE_DIR); \
	fi

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# tools {

# / compiling

$(BUILDDIR)/$(TOOLS)/mkramfs: $(TOOLS)/mkramfs.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=c11 -I./ $< -o $@

$(BUILDDIR)/$(TOOLS)/mkconfig: $(TOOLS)/mkconfig.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=c11 -I./ $< -o $@

# / running

$(CONFIG_BIN): $(BUILDDIR)/$(TOOLS)/mkconfig
	$(BUILDDIR)/$(TOOLS)/mkconfig cuore.conf $@

$(INITRD): $(CONFIG_BIN) $(BUILDDIR)/$(TOOLS)/mkramfs
	$(BUILDDIR)/$(TOOLS)/mkramfs $@ $(CONFIG_BIN)

DISK_SIZE_MB      := 1024
PART_START_SECTOR := 2048

DISK_FILES := $(CONFIG_BIN) cuore.conf

$(DISK_IMG):
	@echo " [ DISK ] Creating Empty $(DISK_SIZE_MB)MB image..."
	@qemu-img create -f raw $(DISK_IMG) $(DISK_SIZE_MB)M > /dev/null

# }

$(KERNEL_ELF): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BOOT_ISO): $(KERNEL_ELF) $(INITRD)
	@echo " [ISO] Generating $@"
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)/EFI/BOOT $(ISO_ROOT)/boot
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	@cp $(KERNEL_ELF) $(ISO_ROOT)/kernel.elf
	@cp $(INITRD) $(ISO_ROOT)/boot/initrd.img
	@cp limine.conf $(ISO_ROOT)/limine.conf
	@xorriso -as mkisofs -R -J -pad -efi-boot-part --efi-boot-image --efi-boot boot/limine-uefi-cd.bin -no-emul-boot -o $@ $(ISO_ROOT) > /dev/null 2>&1

-include $(DEPS)

run:
	qemu-system-x86_64 -machine pc -smp 6 -bios $(QEMU_FIRMWARE) -drive file="$(DISK_IMG)",format=raw,index=0,media=disk -cdrom $(BOOT_ISO) -m 256M -serial stdio -device rtl8139,netdev=u1 -netdev user,id=u1

format:
	./format src/
	./format tools/

style: format

clean:
	rm -rf $(BUILDDIR)

cleand:
	rm -f build/qemu-disk.img