# CuoreOS Build System
CUOREOS_VERSION_NAME := ALPHA-prebin-000

WHITELIST_GOALS := menuconfig clean
ifeq ($(wildcard Config.mk),)
    ifeq ($(filter $(WHITELIST_GOALS),$(MAKECMDGOALS)),)
        $(error [!] Config.mk is missing. Run 'make menuconfig' first)
    endif
endif

-include Config.mk

CC := $(CONFIG_CC)
QEMU_UEFI_FIRMWARE ?= /usr/share/OVMF/OVMF_CODE.fd

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
BOOT_ISO   := $(BUILDDIR)/Cuore.x86-64.iso
ISO_ROOT   := $(BUILDDIR)/iso_root
INITRD     := $(BUILDDIR)/initrd.img

LIMINE_DIR   := $(CACHEDIR)/limine
FLANTERM_DIR := $(CACHEDIR)/flanterm
DISK_IMG     := $(CACHEDIR)/qemu-disk.img

# colors
BLUE         := \033[1;34m
CYAN         := \033[1;36m
GREEN        := \033[1;32m
RESET        := \033[0m

# flags
CFLAGS := -std=c11 -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-lto -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-bmi -mno-bmi2 -I$(SRCDIR) -I$(FLANTERM_DIR)/src -I. -MMD -MP
CC_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wlogical-op -Wmissing-declarations
LDFLAGS := -T kernel.ld -nostdlib -static -z max-page-size=0x1000
CFLAGS += $(CC_WARNINGS) $(CONFIG_ADDITIONAL_CFLAGS)
LDFLAGS += $(CONFIG_ADDITIONAL_LDFLAGS)

ifeq ($(CONFIG_IDE_SUPPORT),true)
    CFLAGS += -DKERNEL_MOD_IDE_ENABLED
endif

CFLAGS += -DAP_STACK_SIZE=$(CONFIG_AP_STACK_SIZE) -DSMP_MAX_CORES=$(CONFIG_MAX_CORES) -DMAX_CHAR_DEVICES=$(CONFIG_MAX_CHAR_DEVICES) -DMAX_DISK_DEVICES=$(CONFIG_MAX_DISK_DEVICES)

.PHONY: all clean run style format compile_commands menuconfig print_config
all: print_config deps_setup $(OBJS) $(KERNEL_ELF) compile_commands.json $(DISK_IMG) $(INITRD) $(BOOT_ISO)

$(OBJS): | deps_setup

FIRMWARE_SUPPORT_LIST = 

ifeq ($(CONFIG_UEFI_SUPPORT),true)
	FIRMWARE_SUPPORT_LIST += UEFI
endif

ifeq ($(CONFIG_BIOS_SUPPORT),true)
	FIRMWARE_SUPPORT_LIST += BIOS
endif

print_config:
	@echo -e "$(BLUE)CuoreOS Main Config:$(RESET)"
	@echo -e "$(CYAN)  Firmware: $(RESET)$(FIRMWARE_SUPPORT_LIST)"
	@echo -e "$(CYAN)  Compiler: $(RESET)$(CC)"
	@echo -e "$(CYAN)  Additional CFLAGS: $(RESET)$(CONFIG_ADDITIONAL_CFLAGS)"
	@echo -e "$(CYAN)  Additional LDFLAGS: $(RESET)$(CONFIG_ADDITIONAL_LDFLAGS)"
	@echo -e "$(CYAN)  Limine: $(RESET)$(CONFIG_LIMINE_FLAGS) $(CONFIG_LIMINE)"
	@echo -e "$(CYAN)  Flanterm: $(RESET)$(CONFIG_FLANTERM_FLAGS) $(CONFIG_FLANTERM)"

menuconfig:
	@python3 $(TOOLS)/configscreen.py $(CUOREOS_VERSION_NAME)
	@echo -e "it is recommended to run 'make clean' after regenerating the config"

deps_setup:
	@mkdir -p $(FLANTERM_DIR) $(LIMINE_DIR)
	@if [ ! -d "$(FLANTERM_DIR)/.git" ]; then \
		git clone $(CONFIG_FLANTERM_FLAGS) $(CONFIG_FLANTERM) $(FLANTERM_DIR); \
	fi
	@if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
		git clone $(CONFIG_LIMINE_FLAGS) $(CONFIG_LIMINE) $(LIMINE_DIR); \
	fi

	make -C $(LIMINE_DIR)

compile_commands.json: $(C_SRCS) $(ASM_SRCS) $(TOOL_SRCS) $(FORCE_REBUILD)
	@echo "Generating compile_commands.json"
	@echo "[" > compile_commands.json
	@for src in $(C_SRCS) $(ASM_SRCS); do \
		echo "  {" >> compile_commands.json; \
		echo "    \"directory\": \"$(CURDIR)\"," >> compile_commands.json; \
		echo "    \"command\": \"$(CC) $(CFLAGS) -c $$src\"," >> compile_commands.json; \
		echo "    \"file\": \"$$src\"" >> compile_commands.json; \
		echo "  }," >> compile_commands.json; \
	done
	@sed -i '$$d' compile_commands.json
	@echo "  }" >> compile_commands.json
	@echo "]" >> compile_commands.json

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CC) ] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.S $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CC) ] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/$(TOOLS)/mkramfs: $(TOOLS)/mkramfs.c
	@mkdir -p $(dir $@)
	@echo " [ $(CC) ] [ TOOL ] $<"
	@$(CC) -std=c11 -I./ $< -o $@

$(INITRD): $(BUILDDIR)/$(TOOLS)/mkramfs
	$(BUILDDIR)/$(TOOLS)/mkramfs $@ kernel.ld

$(DISK_IMG):
	@echo " [ qemu-img ] $(DISK_IMG)"
	@qemu-img create -f raw $(DISK_IMG) 1024M > /dev/null

$(KERNEL_ELF): $(OBJS)
	@echo " [ LINK ] $@"
	@$(CC) $(LDFLAGS) $(OBJS) -o $@
	@cp $@ $(BUILDDIR)/kernel.sym
	@objcopy --strip-all -R .eh_frame -R .note.gnu.property -R .note.gnu.build-id -R .comment $@

$(BOOT_ISO): $(KERNEL_ELF) $(INITRD)
	@echo " [ISO] Generating $@"
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)/boot
	@cp $(KERNEL_ELF) $(ISO_ROOT)/kernel.elf
	@cp $(INITRD) $(ISO_ROOT)/boot/initrd.img
	@cp limine.conf $(ISO_ROOT)/limine.conf
	
	$(eval XORRISO_FLAGS := -as mkisofs -R -J -pad -V "CUOREOS")

ifeq ($(CONFIG_BIOS_SUPPORT),true)
	@cp $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/
	$(eval XORRISO_FLAGS += -b boot/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table)
endif

ifeq ($(CONFIG_UEFI_SUPPORT),true)
	@mkdir -p $(ISO_ROOT)/EFI/BOOT
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	$(eval XORRISO_FLAGS += --efi-boot boot/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label)
endif

	@xorriso $(XORRISO_FLAGS) $(ISO_ROOT) -o $@ > /dev/null 2>&1

ifeq ($(CONFIG_BIOS_SUPPORT),true)
	@$(LIMINE_DIR)/limine bios-install $@
endif

run: uefi-run

GENERIC_QEMU_CFLAGS = -cpu qemu64,+rdrand,+rdseed -smp 6 -m 256M -serial stdio -cdrom $(BOOT_ISO) -drive file="$(DISK_IMG)",format=raw,index=0,media=disk -machine pc -boot d

uefi-run:
	qemu-system-x86_64 -bios $(QEMU_UEFI_FIRMWARE) $(GENERIC_QEMU_CFLAGS)

bios-run:
	qemu-system-x86_64 $(GENERIC_QEMU_CFLAGS)

format:
	./format src/
	./format tools/

clean:
	rm -rf $(BUILDDIR)
