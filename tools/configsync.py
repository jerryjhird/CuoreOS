import sys, os

if sys.version_info >= (3, 11):
    import tomllib as toml
else:
    import toml

is_silent = "--silent" in sys.argv

kernel_conf_defaults = """
[makefile]
CONFIG_STORED_VERSION = "0007"
CONFIG_CC = "clang"
CONFIG_ADDITIONAL_CFLAGS = "-DDEBUG"
CONFIG_ADDITIONAL_LDFLAGS = ""
CONFIG_UEFI_SUPPORT = "true"
CONFIG_BIOS_SUPPORT = "true"
CONFIG_LIMINE = "https://github.com/Limine-Bootloader/Limine.git"
CONFIG_LIMINE_FLAGS = "--depth=1 --branch v11.x-binary"
CONFIG_FLANTERM = "https://github.com/Mintsuki/Flanterm.git"
CONFIG_FLANTERM_FLAGS = "--depth=1"

[header]
KERNEL_MOD_IDE_ENABLED = "true"
KERNEL_MOD_AHCI_ENABLED = "true"
KERNEL_MOD_AC97_ENABLED = "true"
KERNEL_MOD_RTL8139_ENABLED = "true"
KERNEL_MOD_IVSHMEM_ENABLED = "true"
KERNEL_MOD_E1000_ENABLED = "true"
KERNEL_MOD_CXL_ENABLED = "true"
AP_STACK_SIZE = "16384"
SMP_MAX_CORES = "255"
DO_KDEVTESTS = "false"

"""

def log(fmt):
    if not is_silent:
        print(fmt, end='')

def generate_configs(toml_file):
    if not os.path.exists(toml_file):
        with open(toml_file, "w") as f:
            f.write(kernel_conf_defaults)

    with open(toml_file, 'rb') as f:
        data = toml.load(f)

    with open("Config.mk", "w") as f:
        for key, value in data.get("makefile", {}).items():
            f.write(f"{key} := {value}\n")

    with open("src/Config.h", "w") as f:
        f.write("#pragma once\n\n")
        for key, value in data.get("header", {}).items():
            val = str(value).lower()
            if val in ["true", "false"]:
                directive = "#define" if val == "true" else "#undef"
                f.write(f"{directive} {key}\n")
            else:
                f.write(f"#define {key} {value}\n")

def main():
    if len(sys.argv) < 2:
        sys.exit(1)

    format_version = sys.argv[1]

    generate_configs('config.toml')

if __name__ == "__main__":
    main()
