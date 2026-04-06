# CuoreOS Build System

# change QEMU_FIRMWARE if needed
QEMU_FIRMWARE ?= /usr/share/OVMF/OVMF_CODE.fd

CC      := gcc
HOST_CC := gcc

SRCDIR   := src
BUILDDIR := build
CACHEDIR := cache
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

LIMINE_DIR := $(CACHEDIR)/limine
FLANTERM_DIR := $(CACHEDIR)/flanterm
INITRD := $(BUILDDIR)/initrd.img

CFLAGS := -std=c11 -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-lto -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-bmi -mno-bmi2 -I$(SRCDIR) -I$(FLANTERM_DIR)/src -I. -MMD -MP
CC_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wlogical-op -Wmissing-declarations

ADD_CFLAGS ?=
LDFLAGS := -T kernel.ld -nostdlib -static -z max-page-size=0x1000
ADD_LDFLAGS ?=

CFLAGS += $(CC_WARNINGS)
CFLAGS += $(ADD_CFLAGS)
LDFLAGS += $(ADD_LDFLAGS)

ifeq ($(filter clean format style,$(MAKECMDGOALS)),)
    OLD_KMAIN_FLAGS := $(shell [ -f compile_commands.json ] && \
        grep -A 1 "src/kmain.c" compile_commands.json | \
        grep "command" | sed 's/.*"command": "\(.*\)".*/\1/' || true)
    
    CURRENT_KMAIN_CMD := $(CC) $(CFLAGS) -c src/kmain.c

    ifneq ($(strip $(OLD_KMAIN_FLAGS)), $(strip $(CURRENT_KMAIN_CMD)))
        ifneq ($(OLD_KMAIN_FLAGS),)
            $(info [BUILD] CFLAGS changed. Forcing full recompile...)
            FORCE_REBUILD := .FORCE_REBUILD
        endif
    endif
endif

DISK_IMG := $(CACHEDIR)/qemu-disk.img

.PHONY: all clean run style format compile_commands

all: deps_setup $(OBJS) $(KERNEL_ELF) compile_commands.json $(DISK_IMG) $(INITRD) $(BOOT_ISO)
$(OBJS): | deps_setup

compile_commands.json: $(C_SRCS) $(ASM_SRCS) $(TOOL_SRCS) $(FORCE_REBUILD)
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
	@mkdir -p $(FLANTERM_DIR) $(LIMINE_DIR)
	@if [ ! -d "$(FLANTERM_DIR)/.git" ]; then \
		git clone --depth=1 https://codeberg.org/Mintsuki/Flanterm.git $(FLANTERM_DIR); \
	fi
	@if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
		git clone --depth=1 --branch v10.x-binary https://codeberg.org/Limine/Limine.git $(LIMINE_DIR); \
	fi

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CC) ] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.S $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CC) ] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

.PHONY: .FORCE_REBUILD
.FORCE_REBUILD:

# tools {

# / compiling

$(BUILDDIR)/$(TOOLS)/mkramfs: $(TOOLS)/mkramfs.c
	@mkdir -p $(dir $@)
	@echo " [ $(HOST_CC) ] [ TOOL ] $<"
	@$(HOST_CC) -std=c11 -I./ $< -o $@

$(BUILDDIR)/$(TOOLS)/mkconfig: $(TOOLS)/mkconfig.c
	@mkdir -p $(dir $@)
	@echo " [ $(HOST_CC) ] [ TOOL ] $<"
	@$(HOST_CC) -std=c11 -I./ $< -o $@

# / running

$(CONFIG_BIN): $(BUILDDIR)/$(TOOLS)/mkconfig
	$(BUILDDIR)/$(TOOLS)/mkconfig cuore.conf $@

$(INITRD): $(CONFIG_BIN) $(BUILDDIR)/$(TOOLS)/mkramfs
	$(BUILDDIR)/$(TOOLS)/mkramfs $@ $(CONFIG_BIN)

DISK_SIZE_MB      := 1024

$(DISK_IMG):
	@echo " [ qemu-img ] $(DISK_IMG)"
	@qemu-img create -f raw $(DISK_IMG) $(DISK_SIZE_MB)M > /dev/null

# }

$(KERNEL_ELF): $(OBJS)
	@echo " [ LINK ] $@"
	@$(CC) $(LDFLAGS) $(OBJS) -o $@
	@cp $@ build/kernel.sym
	@objcopy --strip-all -R .eh_frame -R .note.gnu.property -R .note.gnu.build-id -R .comment $@

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
	qemu-system-x86_64 -machine pc \
	-smp 6 \
	-bios $(QEMU_FIRMWARE) \
	-drive file="$(DISK_IMG)",format=raw,index=0,media=disk \
	-cdrom $(BOOT_ISO) \
	-m 256M \
	-serial stdio \
	-device rtl8139,netdev=u1 \
	-netdev user,id=u1

format:
	./format src/
	./format tools/

clean:
	rm -rf $(BUILDDIR)
