QEMU_USE_CXL ?= false
QEMU_USE_SHM ?= false

QEMU_MACHINE ?= q35

GENERIC_QEMU_FLAGS := -display sdl -cpu qemu64,+rdrand,+rdseed -smp 6 -m 256M -serial stdio -cdrom $(BOOT_ISO) -boot d
DISK_QEMU_FLAG := -drive file="$(DISK_IMG)",format=raw,index=0,media=disk
QEMU_AUDIO_FLAGS := -audiodev sdl,id=snd0 -device ac97,audiodev=snd0
NET_QEMU_FLAG := -netdev user,id=u1 -device rtl8139,netdev=u1 -object filter-dump,id=f1,netdev=u1,file=network_capture.pcap

MACHINE_ARGS := $(QEMU_MACHINE)
EXTRA_OBJECTS_AND_DEVICES :=

ifeq ($(QEMU_USE_CXL),true)
    MACHINE_ARGS := $(MACHINE_ARGS),cxl=on

    EXTRA_OBJECTS_AND_DEVICES += -object memory-backend-ram,id=cxl-mem0,size=256M \
                                 -device pxb-cxl,bus_nr=64,bus=pcie.0,id=cxl.1 \
                                 -device cxl-rp,id=rp0,bus=cxl.1,chassis=0,slot=0 \
                                 -device cxl-type3,bus=rp0,volatile-memdev=cxl-mem0,id=cxl-dev0 \
                                 -machine cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=256M
endif

ifeq ($(QEMU_USE_SHM),true)
    SHM_FILE := /dev/shm/CUORE_SHM
    EXTRA_OBJECTS_AND_DEVICES += -object memory-backend-file,id=mb1,size=4M,mem-path=$(SHM_FILE),share=on -device ivshmem-plain,memdev=mb1

    define CONSTRUCT_SHM_BACKING
        @rm -f $(SHM_FILE)
        @truncate -s 4M $(SHM_FILE)
        @echo "$(SHM_FILE) (4MB) setup for ivshmem"
    endef
else
    define CONSTRUCT_SHM_BACKING
    endef
endif

GENERIC_QEMU_FLAGS += -machine $(MACHINE_ARGS)

.PHONY: runu runb

runu:
	$(CONSTRUCT_SHM_BACKING)
	qemu-system-x86_64 -bios $(QEMU_UEFI_FIRMWARE) $(GENERIC_QEMU_FLAGS) $(DISK_QEMU_FLAG) $(QEMU_AUDIO_FLAGS) $(NET_QEMU_FLAG) $(EXTRA_OBJECTS_AND_DEVICES)

runb:
	$(CONSTRUCT_SHM_BACKING)
	qemu-system-x86_64 $(GENERIC_QEMU_FLAGS) $(DISK_QEMU_FLAG) $(QEMU_AUDIO_FLAGS) $(NET_QEMU_FLAG) $(EXTRA_OBJECTS_AND_DEVICES)
