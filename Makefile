# CuoreOS Build System
CUOREOS_VERSION_NAME := ALPHA-prebin-000
SYSTEM_CONFIG_VERSION := 0007

WHITELIST_GOALS := configsync clean clean-cache format
ifeq ($(wildcard Config.mk),)
    ifeq ($(filter $(WHITELIST_GOALS),$(MAKECMDGOALS)),)
        $(error [!] Config.mk is missing. Run 'make configsync' first)
    endif
else
    -include Config.mk
    ifneq ($(CONFIG_STORED_VERSION),$(SYSTEM_CONFIG_VERSION))
        ifeq ($(filter $(WHITELIST_GOALS),$(MAKECMDGOALS)),)
            $(error [!] Config.mk is outdated (Version $(CONFIG_STORED_VERSION) vs Required $(SYSTEM_CONFIG_VERSION)). Run 'make configsync')
        endif
    endif
endif

TOOLS_NEEDED := qemu-img xorriso xxd git sed
MISSING_TOOLS := $(strip $(foreach tool,$(TOOLS_NEEDED),\
    $(if $(shell command -v $(tool) 2>/dev/null),,$(tool))))

ifneq ($(MISSING_TOOLS),)
$(error Install the following utilities: $(MISSING_TOOLS))
endif

QEMU_UEFI_FIRMWARE ?= /usr/share/OVMF/OVMF_CODE.fd

SRCDIR := src
BUILDDIR := build
CACHEDIR := cache
TOOLS := tools


# GIT CONSTANTS
GIT_LIMINE = "https://github.com/Limine-Bootloader/Limine.git" --depth=1 --branch v11.x-binary
GIT_LIMINE_PROTOCOL = "https://github.com/Limine-Bootloader/limine-protocol.git" --depth=1
LIMINE_DIR := $(CACHEDIR)/limine
LIMINE_PROTOCOL_DIR := $(CACHEDIR)/limine-protocol
ALL_REPOS_DIR := $(LIMINE_DIR) $(LIMINE_PROTOCOL_DIR)

DISK_IMG := $(CACHEDIR)/qemu-disk.img
C_SRCS := $(shell find $(SRCDIR) -name "*.c")
ASM_SRCS := $(shell find $(SRCDIR) -name "*.S")
TOOL_SRCS := $(wildcard $(TOOLS)/*.c)
OBJS := $(C_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o) $(ASM_SRCS:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
DEPS := $(OBJS:.o=.d)

KERNEL_ELF := $(BUILDDIR)/kernel.elf
BOOT_ISO := $(BUILDDIR)/Cuore.x86-64.iso
ISO_ROOT := $(BUILDDIR)/iso_root
SYMTABLE := $(BUILDDIR)/symtable.data
INITRD := $(BUILDDIR)/initrd.img

# flags
# -fno-pie/-fno-pic for systems that auto enable PIE/PIC like microslop windows subsystem for linux
CFLAGS := -fno-pie -fno-pic -std=c23 -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fno-lto -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-bmi -mno-bmi2 -I$(SRCDIR) -I$(LIMINE_PROTOCOL_DIR)/include -I. -MMD -MP
CC_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wmissing-declarations
GCC_WARNINGS := -Wlogical-op

LDFLAGS := -T kernel.ld -nostdlib -static -z max-page-size=0x1000 -no-pie
CFLAGS += $(CC_WARNINGS) $(CONFIG_ADDITIONAL_CFLAGS)
LDFLAGS += $(CONFIG_ADDITIONAL_LDFLAGS)

SIG := 0x$(shell head -c 8 /dev/urandom | xxd -p)
CFLAGS += -DKERNEL_BUILD_SIGNATURE=$(SIG)

.PHONY: all clean run style format compile_commands.json configsync
all: deps_setup $(OBJS) $(KERNEL_ELF) $(DISK_IMG) $(INITRD) $(BOOT_ISO) compile_commands.json

$(OBJS): | deps_setup

FIRMWARE_SUPPORT_LIST = 

ifeq ($(CONFIG_UEFI_SUPPORT),true)
	FIRMWARE_SUPPORT_LIST += UEFI
endif

ifeq ($(CONFIG_BIOS_SUPPORT),true)
	FIRMWARE_SUPPORT_LIST += BIOS
endif

configsync:
	@python3 $(TOOLS)/configsync.py $(SYSTEM_CONFIG_VERSION)
	@echo -e "it is recommended to run 'make clean' after regenerating the config"

deps_setup:
	@mkdir -p $(ALL_REPOS_DIR)
	@if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
		git clone $(GIT_LIMINE) $(LIMINE_DIR); \
	fi
	@if [ ! -d "$(LIMINE_PROTOCOL_DIR)/.git" ]; then \
		git clone $(GIT_LIMINE_PROTOCOL) $(LIMINE_PROTOCOL_DIR); \
	fi

	make -C $(LIMINE_DIR)

compile_commands.json: $(C_SRCS) $(ASM_SRCS) $(TOOL_SRCS) $(FORCE_REBUILD)
	@echo "[" > $@
	@comma=""; \
	for src in $(C_SRCS) $(ASM_SRCS); do \
		rel_path="$${src#$(SRCDIR)/}"; \
		obj="$(BUILDDIR)/$${rel_path%.*}.o"; \
		printf "%s\n  {\n    \"directory\": \"%s\",\n    \"command\": \"%s %s -c %s\",\n    \"file\": \"%s\",\n    \"output\": \"%s\"\n  }" \
			"$$comma" "$(CURDIR)" "$(CONFIG_CC)" "$(subst ",\",$(CFLAGS))" "$$src" "$$src" "$$obj" >> $@; \
		comma=","; \
	done
	@printf "\n]\n" >> $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CONFIG_CC) ] $<"
	@$(CONFIG_CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.S $(FORCE_REBUILD)
	@mkdir -p $(dir $@)
	@echo " [ $(CONFIG_CC) ] $<"
	@$(CONFIG_CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/$(TOOLS)/mkramfs: $(TOOLS)/mkramfs.c
	@mkdir -p $(dir $@)
	@echo " [ $(CONFIG_CC) ] [ TOOL ] $<"
	@$(CONFIG_CC) -std=c11 -I./ $< -o $@

$(SYMTABLE): $(KERNEL_ELF)
	python3 $(TOOLS)/mksymtable.py $(BUILDDIR)/kernel.sym $(SYMTABLE)

$(INITRD): $(BUILDDIR)/$(TOOLS)/mkramfs $(SYMTABLE)
	$(BUILDDIR)/$(TOOLS)/mkramfs $@ $(SYMTABLE)

$(DISK_IMG):
	@echo " [ qemu-img ] $(DISK_IMG)"
	@qemu-img create -f raw $(DISK_IMG) 1024M > /dev/null

$(KERNEL_ELF): $(OBJS)
	@echo " [ LINK ] $@"
	@$(CONFIG_CC) $(LDFLAGS) $(OBJS) -o $@
	@cp $@ $(BUILDDIR)/kernel.sym
	@objcopy --strip-debug --strip-unneeded -R .eh_frame -R .note.gnu.property -R .note.gnu.build-id -R .comment $@

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

include QEMU.mk

format:
	./format src/
	./format tools/

clean:
	rm -rf $(BUILDDIR) network_capture.pcap

clean-cache: clean
	rm -rf $(CACHEDIR)
	rm -rf .cache

fullclean: clean clean-cache

-include $(DEPS)
