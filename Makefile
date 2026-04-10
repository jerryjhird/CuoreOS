# CuoreOS Build System
CUOREOS_VERSION_NAME := v0.0.1

WHITELIST_GOALS := menuconfig clean
ifeq ($(wildcard Config.mk),)
    ifeq ($(filter $(WHITELIST_GOALS),$(MAKECMDGOALS)),)
        $(error [!] Config.mk is missing. Run 'make menuconfig' first)
    endif
endif

-include Config.mk

CC := $(strip $(CONFIG_CC))
LIMINE_URL := $(strip $(CONFIG_LIMINE))
LIMINE_FLAGS := $(strip $(CONFIG_LIMINE_FLAGS))
FLANTERM_URL := $(strip $(CONFIG_FLANTERM))
FLANTERM_FLAGS := $(strip $(CONFIG_FLANTERM_FLAGS))
ADD_CFLAGS := $(strip $(CONFIG_ADDITIONAL_CFLAGS))
ADD_LDFLAGS := $(strip $(CONFIG_ADDITIONAL_LDFLAGS))
QEMU_FIRMWARE ?= /usr/share/OVMF/OVMF_CODE.fd

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
CFLAGS += $(CC_WARNINGS) $(ADD_CFLAGS)
LDFLAGS += $(ADD_LDFLAGS)

ifeq ($(CONFIG_PROFILE),debug)
    CFLAGS += -DDEBUG
endif

ifeq ($(CONFIG_MODULE_IDE),true)
    CFLAGS += -DKERNEL_MOD_IDE_ENABLED
endif

CFLAGS += -DSMP_MAX_CORES=$(CONFIG_MAX_CORES) -DMAX_CHAR_DEVICES=$(CONFIG_MAX_CHAR_DEVICES) -DMAX_DISK_DEVICES=$(CONFIG_MAX_DISK_DEVICES)

.PHONY: all clean run style format compile_commands menuconfig print_config
all: print_config deps_setup $(OBJS) $(KERNEL_ELF) compile_commands.json $(DISK_IMG) $(INITRD) $(BOOT_ISO)

$(OBJS): | deps_setup

print_config:
	@echo -e "$(BLUE)CuoreOS Main Config:$(RESET)"
	@echo -e "$(CYAN)  Profile: $(RESET)$(CONFIG_PROFILE)"
	@echo -e "$(CYAN)  Compiler: $(RESET)$(CC)"
	@echo -e "$(CYAN)  Additional CFLAGS: $(RESET)$(ADD_CFLAGS)"
	@echo -e "$(CYAN)  Additional LDFLAGS: $(RESET)$(ADD_LDFLAGS)"
	@echo -e "$(CYAN)  Limine: $(RESET)$(LIMINE_FLAGS) $(LIMINE_URL)"
	@echo -e "$(CYAN)  Flanterm: $(RESET)$(FLANTERM_FLAGS) $(FLANTERM_URL)"

menuconfig:
	@python3 $(TOOLS)/configscreen.py $(CUOREOS_VERSION_NAME)
	@echo -e "it is recommended to run 'make clean' before regenerating the build config"

deps_setup:
	@mkdir -p $(FLANTERM_DIR) $(LIMINE_DIR)
	@if [ ! -d "$(FLANTERM_DIR)/.git" ]; then \
		git clone $(FLANTERM_FLAGS) $(FLANTERM_URL) $(FLANTERM_DIR); \
	fi
	@if [ ! -d "$(LIMINE_DIR)/.git" ]; then \
		git clone $(LIMINE_FLAGS) $(LIMINE_URL) $(LIMINE_DIR); \
	fi

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

# --- Tools & Assets ---
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
	-netdev user,id=u1

format:
	./format src/
	./format tools/

clean:
	rm -rf $(BUILDDIR)
